#include "ipc.h"
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>

ipc_error_t ipc_server_create(ipc_channel_t *ch, const char *path) {
    if (!ch || !path) return IPC_ERR_CREATE;

    struct sockaddr_un addr;
    unlink(path);

    ch->fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (ch->fd < 0) return IPC_ERR_CREATE;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    addr.sun_path[sizeof(addr.sun_path) - 1] = 0;

    if (bind(ch->fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(ch->fd);
        return IPC_ERR_BIND;
    }

    chmod(path, 0660);
    strncpy(ch->path, path, IPC_PATH_MAX - 1);
    ch->path[IPC_PATH_MAX - 1] = 0;
    ch->is_server = 1;
    listen(ch->fd, 5);

    return IPC_OK;
}

ipc_error_t ipc_client_connect(ipc_channel_t *ch, const char *path) {
    if (!ch || !path) return IPC_ERR_CREATE;

    struct sockaddr_un addr;
    ch->fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (ch->fd < 0) return IPC_ERR_CREATE;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    addr.sun_path[sizeof(addr.sun_path) - 1] = 0;

    if (connect(ch->fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(ch->fd);
        return IPC_ERR_CONNECT;
    }

    strncpy(ch->path, path, IPC_PATH_MAX - 1);
    ch->path[IPC_PATH_MAX - 1] = 0;
    ch->is_server = 0;
    return IPC_OK;
}

ipc_error_t ipc_send(ipc_channel_t *ch, const ipc_message_t *msg) {
    if (!ch || !msg || ch->fd < 0) return IPC_ERR_SEND;
    if (write(ch->fd, msg, sizeof(*msg)) < 0) return IPC_ERR_SEND;
    return IPC_OK;
}

ipc_error_t ipc_recv(ipc_channel_t *ch, ipc_message_t *msg) {
    if (!ch || !msg || ch->fd < 0) return IPC_ERR_RECV;

    struct sockaddr_un client;
    socklen_t addr_len = sizeof(client);

    int client_fd = accept(ch->fd, (struct sockaddr *)&client, &addr_len);
    if (client_fd < 0) return IPC_ERR_RECV;

    ssize_t n = read(client_fd, msg, sizeof(*msg));
    close(client_fd);
    if (n <= 0) return IPC_ERR_RECV;

    return IPC_OK;
}

void ipc_close(ipc_channel_t *ch) {
    if (!ch) return;
    if (ch->fd >= 0) {
        close(ch->fd);
        if (ch->is_server) unlink(ch->path);
    }
    memset(ch, 0, sizeof(*ch));
    ch->fd = -1;
}
