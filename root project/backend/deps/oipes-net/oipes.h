#ifndef _SERVECOSYS_OIPES_H_
#define _SERVECOSYS_OIPES_H_

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define OIPES_TOKEN_MAX     4096
#define OIPES_RESPONSE_MAX  65536
#define OIPES_URL_MAX       256

typedef struct {
    char access_token[OIPES_TOKEN_MAX];
    char refresh_token[OIPES_TOKEN_MAX];
    time_t expires_at;
    int  is_valid;
} oipes_token_t;

typedef struct {
    oipes_token_t token;
    char host[OIPES_URL_MAX];
    int  port;
    char device_id[128];
} oipes_client_t;

int  oipes_init(oipes_client_t *client, const char *host, int port);
int  oipes_auth(oipes_client_t *client, const char *client_id);
int  oipes_request(oipes_client_t *client, const char *method,
                   const char *path, const char *body,
                   char *response, size_t resp_size);
int  oipes_ai_infer(oipes_client_t *client, const char *model,
                    const char *prompt, char *result, size_t result_size);
int  oipes_device_register(oipes_client_t *client);
int  oipes_token_load(oipes_client_t *client, const char *path);
int  oipes_token_save(oipes_client_t *client, const char *path);
int  oipes_token_refresh(oipes_client_t *client);

#endif
