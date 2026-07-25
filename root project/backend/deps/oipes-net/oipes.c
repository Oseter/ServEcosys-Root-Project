#include "oipes.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

int oipes_init(oipes_client_t *client, const char *host, int port) {
    if (!client || !host) return -1;
    memset(client, 0, sizeof(*client));
    strncpy(client->host, host, OIPES_URL_MAX - 1);
    client->port = port ? port : 443;
    client->token.is_valid = 0;
    snprintf(client->device_id, sizeof(client->device_id),
             "servecosys-%d", getpid());
    return 0;
}

static int http_raw_request(const char *host, int port,
                            const char *request, char *response, size_t resp_size)
{
    struct hostent *server = gethostbyname(host);
    if (!server) return -1;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, server->h_addr, server->h_length);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }

    write(sock, request, strlen(request));

    ssize_t n = read(sock, response, resp_size - 1);
    close(sock);

    if (n > 0) {
        response[n] = 0;
        return 0;
    }

    return -1;
}

int oipes_request(oipes_client_t *client, const char *method,
                  const char *path, const char *body,
                  char *response, size_t resp_size)
{
    if (!client || !method || !path) return -1;

    char request[8192];
    int n = snprintf(request, sizeof(request),
        "%s /v1%s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Authorization: Bearer %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "User-Agent: ServEcosys-OIPES/0.1.0\r\n"
        "\r\n"
        "%s",
        method, path, client->host,
        client->token.is_valid ? client->token.access_token : "",
        body ? strlen(body) : 0,
        body ? body : "");

    return http_raw_request(client->host, client->port, request, response, resp_size);
}

int oipes_auth(oipes_client_t *client, const char *client_id) {
    if (!client) return -1;

    char body[512];
    snprintf(body, sizeof(body),
             "{\"grant_type\":\"client_credentials\","
             "\"client_id\":\"%s\"}", client_id ? client_id : "servecosys");

    char response[OIPES_RESPONSE_MAX];
    if (oipes_request(client, "POST", "/auth/token", body,
                      response, sizeof(response)) != 0)
        return -1;

    char *token_start = strstr(response, "\"access_token\":\"");
    if (!token_start) return -1;

    token_start += 16;
    char *token_end = strchr(token_start, '"');
    if (!token_end) return -1;

    size_t len = token_end - token_start;
    if (len >= OIPES_TOKEN_MAX) return -1;

    memcpy(client->token.access_token, token_start, len);
    client->token.access_token[len] = 0;
    client->token.expires_at = time(NULL) + 3600;
    client->token.is_valid = 1;

    return 0;
}

int oipes_ai_infer(oipes_client_t *client, const char *model,
                   const char *prompt, char *result, size_t result_size)
{
    if (!client || !client->token.is_valid) return -1;

    char body[4096];
    snprintf(body, sizeof(body),
             "{\"model\":\"%s\",\"prompt\":\"%s\",\"max_tokens\":512}",
             model ? model : "default", prompt ? prompt : "");

    return oipes_request(client, "POST", "/ai/inference", body, result, result_size);
}

int oipes_device_register(oipes_client_t *client) {
    if (!client) return -1;

    char body[512];
    snprintf(body, sizeof(body),
             "{\"device_id\":\"%s\",\"device_type\":\"servecosys\","
             "\"version\":\"0.1.0\"}", client->device_id);

    char response[4096];
    return oipes_request(client, "POST", "/devices/register", body,
                         response, sizeof(response));
}

int oipes_token_load(oipes_client_t *client, const char *path) {
    if (!client || !path) return -1;
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    if (fscanf(f, "%s\n%s\n%ld\n",
               client->token.access_token,
               client->token.refresh_token,
               &client->token.expires_at) == 3) {
        client->token.is_valid = (client->token.expires_at > time(NULL));
    }

    fclose(f);
    return client->token.is_valid ? 0 : -1;
}

int oipes_token_save(oipes_client_t *client, const char *path) {
    if (!client || !path) return -1;
    FILE *f = fopen(path, "w");
    if (!f) return -1;

    fprintf(f, "%s\n%s\n%ld\n",
            client->token.access_token,
            client->token.refresh_token,
            client->token.expires_at);
    fclose(f);
    return 0;
}

int oipes_token_refresh(oipes_client_t *client) {
    if (!client) return -1;
    return oipes_auth(client, "servecosys");
}
