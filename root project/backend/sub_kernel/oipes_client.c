/**
 * ServEcosys SED - OIPES Client
 *
 * 职责：
 * - 与 OIPES 服务端进行安全通信
 * - 认证令牌管理
 * - AI 推理请求代理
 * - 推送通知接收
 *
 * Open Internet Public Ecosystem Services
 * 运行在 sys_dom_t 域
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <time.h>

#define OIPES_VERSION         "0.1.0"
#define TOKEN_PATH            "/system/backend/data/oipes/token.store"
#define API_BASE_URL          "https://api.oipes.io/v1"
#define MAX_TOKEN_SIZE        4096
#define PID_FILE              "/var/run/oipes_client.pid"

typedef struct {
    char     access_token[MAX_TOKEN_SIZE];
    char     refresh_token[MAX_TOKEN_SIZE];
    time_t   expires_at;
    int      is_valid;
} oipes_auth_token_t;

static oipes_auth_token_t auth_token;
static volatile sig_atomic_t running = 1;

static int load_token(void)
{
    FILE *f = fopen(TOKEN_PATH, "r");
    if (!f) {
        fprintf(stdout, "[OIPES] No stored token found\n");
        return -1;
    }

    int n = fscanf(f, "%4095s%4095s%ld",
                   auth_token.access_token, auth_token.refresh_token,
                   &auth_token.expires_at);
    fclose(f);

    if (n != 3) {
        fprintf(stdout, "[OIPES] Stored token corrupt, ignoring\n");
        memset(&auth_token, 0, sizeof(auth_token));
        return -1;
    }

    auth_token.is_valid = (auth_token.expires_at > time(NULL));
    fprintf(stdout, "[OIPES] Token loaded (expires: %s)",
            auth_token.is_valid ? "valid" : "expired");
    return auth_token.is_valid ? 0 : -1;
}

static int save_token(void)
{
    /* 首次写入前确保目录存在，否则 fopen("w") 会静默失败 */
    char dir[256];
    snprintf(dir, sizeof(dir), "%s", TOKEN_PATH);
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = 0;
        mkdir(dir, 0700);
    }

    FILE *f = fopen(TOKEN_PATH, "w");
    if (!f) return -1;

    /* 先收紧权限再写入，避免 token 以默认 umask 短暂落入可读状态 */
    fchmod(fileno(f), 0600);

    fprintf(f, "%s\n", auth_token.access_token);
    fprintf(f, "%s\n", auth_token.refresh_token);
    fprintf(f, "%ld\n", auth_token.expires_at);
    fclose(f);
    return 0;
}

static int http_request(const char *method, const char *path,
                         const char *body, char *response, size_t resp_size)
{
    char request[4096];
    char host[256] = "api.oipes.io";
    int port = 443;

    snprintf(request, sizeof(request),
             "%s /v1%s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Authorization: Bearer %s\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             method, path, host,
             auth_token.is_valid ? auth_token.access_token : "",
             body ? strlen(body) : 0,
             body ? body : "");

    struct hostent *server = gethostbyname(host);
    if (!server) {
        fprintf(stderr, "[OIPES] DNS resolution failed: %s\n", host);
        return -1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, server->h_addr, server->h_length);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "[OIPES] Connection failed to %s:%d\n", host, port);
        close(sock);
        return -1;
    }

    write(sock, request, strlen(request));

    ssize_t n = read(sock, response, resp_size - 1);
    if (n > 0)
        response[n] = 0;

    close(sock);
    return 0;
}

static int authenticate(void)
{
    fprintf(stdout, "[OIPES] Authenticating...\n");

    char response[8192];
    int ret = http_request("POST", "/auth/token",
                            "{\"grant_type\":\"client_credentials\","
                            "\"client_id\":\"servecosys\","
                            "\"device_fingerprint\":\"pending\"}",
                            response, sizeof(response));

    if (ret != 0) {
        fprintf(stderr, "[OIPES] Authentication request failed\n");
        return -1;
    }

    char *token_start = strstr(response, "\"access_token\":\"");
    if (token_start) {
        token_start += 16;
        char *token_end = strchr(token_start, '"');
        if (token_end) {
            size_t len = token_end - token_start;
            if (len < MAX_TOKEN_SIZE) {
                memcpy(auth_token.access_token, token_start, len);
                auth_token.access_token[len] = 0;
                auth_token.expires_at = time(NULL) + 3600;
                auth_token.is_valid = 1;
                save_token();
                fprintf(stdout, "[OIPES] Authenticated successfully\n");
                return 0;
            }
        }
    }

    fprintf(stderr, "[OIPES] Authentication failed: malformed response\n");
    return -1;
}

static void json_escape(const char *src, char *dst, size_t dst_size)
{
    size_t i = 0, o = 0;
    for (; src[i] && o + 6 < dst_size; i++) {
        switch (src[i]) {
            case '"':  dst[o++] = '\\'; dst[o++] = '"';  break;
            case '\\': dst[o++] = '\\'; dst[o++] = '\\'; break;
            case '\n': dst[o++] = '\\'; dst[o++] = 'n';  break;
            case '\r': dst[o++] = '\\'; dst[o++] = 'r';  break;
            case '\t': dst[o++] = '\\'; dst[o++] = 't';  break;
            default:   dst[o++] = src[i];                 break;
        }
    }
    dst[o] = 0;
}

static int ai_inference(const char *model, const char *prompt, char *result, size_t result_size)
{
    if (!auth_token.is_valid) {
        fprintf(stderr, "[OIPES] Not authenticated\n");
        return -1;
    }

    char esc_model[256];
    char esc_prompt[2048];
    json_escape(model, esc_model, sizeof(esc_model));
    json_escape(prompt, esc_prompt, sizeof(esc_prompt));

    char body[4096];
    snprintf(body, sizeof(body),
             "{\"model\":\"%s\",\"prompt\":\"%s\",\"max_tokens\":512}",
             esc_model, esc_prompt);

    return http_request("POST", "/ai/inference", body, result, result_size);
}

static int register_device(void)
{
    fprintf(stdout, "[OIPES] Registering device...\n");

    char response[4096];
    int ret = http_request("POST", "/devices/register",
                            "{\"device_type\":\"servecosys\","
                            "\"version\":\"0.1.0\"}",
                            response, sizeof(response));

    if (ret == 0)
        fprintf(stdout, "[OIPES] Device registration: %s\n",
                strstr(response, "200 OK") ? "OK" : "pending");

    return ret;
}

int main(int argc, char *argv[])
{
    fprintf(stdout, "ServEcosys OIPES Client v%s\n", OIPES_VERSION);
    fprintf(stdout, "Open Internet Public Ecosystem Services\n");
    fprintf(stdout, "Running in sys_dom_t security domain\n\n");

    load_token();

    if (!auth_token.is_valid) {
        authenticate();
    }

    register_device();

    FILE *pid_fd = fopen(PID_FILE, "w");
    if (pid_fd) {
        fprintf(pid_fd, "%d\n", getpid());
        fclose(pid_fd);
    }

    fprintf(stdout, "[OIPES] Running (PID: %d)\n", getpid());

    while (running) {
        sleep(30);

        if (auth_token.is_valid && auth_token.expires_at - time(NULL) < 60) {
            fprintf(stdout, "[OIPES] Token expiring, re-authenticating...\n");
            authenticate();
        }
    }

    unlink(PID_FILE);
    return 0;
}
