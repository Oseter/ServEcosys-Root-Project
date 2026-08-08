#ifndef _SERVECOSYS_FRONTEND_IPC_H_
#define _SERVECOSYS_FRONTEND_IPC_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define IPC_PATH_MAX 108
#define IPC_MAX_MSG  4096

typedef enum {
    IPC_OK          = 0,
    IPC_ERR_CREATE  = -1,
    IPC_ERR_BIND    = -2,
    IPC_ERR_CONNECT = -3,
    IPC_ERR_SEND    = -4,
    IPC_ERR_RECV    = -5,
} ipc_error_t;

typedef struct {
    uint32_t type;
    uint32_t seq;
    pid_t    sender_pid;
    size_t   payload_len;
    uint8_t  payload[IPC_MAX_MSG];
} ipc_message_t;

typedef struct {
    int  fd;
    char path[IPC_PATH_MAX];
    int  is_server;
} ipc_channel_t;

ipc_error_t ipc_server_create(ipc_channel_t *ch, const char *path);
ipc_error_t ipc_client_connect(ipc_channel_t *ch, const char *path);
ipc_error_t ipc_send(ipc_channel_t *ch, const ipc_message_t *msg);
ipc_error_t ipc_recv(ipc_channel_t *ch, ipc_message_t *msg);
void        ipc_close(ipc_channel_t *ch);

#endif
