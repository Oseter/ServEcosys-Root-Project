#include "ipc.h"
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>

ipc_error_t ipc_server_create(ipc_channel_t *ch, const char *path) {
    if (!ch || !path) return IPC_ERR_INVAL;

    struct sockaddr_un addr;
    socklen_t addr_len;

    unlink(path);
    ch->fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (ch->fd < 0) return IPC_ERR_CREATE;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    addr_len = sizeof(addr);

    if (bind(ch->fd, (struct sockaddr *)&addr, addr_len) < 0) {
        close(ch->fd);
        return IPC_ERR_BIND;
    }

    chmod(path, 0660);
    strncpy(ch->path, path, IPC_PATH_MAX - 1);
    ch->is_server = 1;

    if (listen(ch->fd, 5) < 0) {
        close(ch->fd);
        return IPC_ERR_CREATE;
    }

    return IPC_OK;
}

ipc_error_t ipc_client_connect(ipc_channel_t *ch, const char *path) {
    if (!ch || !path) return IPC_ERR_INVAL;

    struct sockaddr_un addr;

    ch->fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (ch->fd < 0) return IPC_ERR_CREATE;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (connect(ch->fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(ch->fd);
        return IPC_ERR_CONNECT;
    }

    strncpy(ch->path, path, IPC_PATH_MAX - 1);
    ch->is_server = 0;

    return IPC_OK;
}

ipc_error_t ipc_send(ipc_channel_t *ch, const ipc_message_t *msg) {
    if (!ch || !msg || ch->fd < 0) return IPC_ERR_INVAL;

    if (write(ch->fd, msg, sizeof(*msg)) < 0)
        return IPC_ERR_SEND;

    return IPC_OK;
}

ipc_error_t ipc_recv(ipc_channel_t *ch, ipc_message_t *msg) {
    if (!ch || !msg || ch->fd < 0) return IPC_ERR_INVAL;

    struct sockaddr_un client;
    socklen_t addr_len = sizeof(client);

    int client_fd = accept(ch->fd, (struct sockaddr *)&client, &addr_len);
    if (client_fd < 0) return IPC_ERR_RECV;

    ssize_t n = read(client_fd, msg, sizeof(*msg));
    close(client_fd);

    if (n <= 0) return IPC_ERR_RECV;

    return IPC_OK;
}

ipc_error_t ipc_send_recv(ipc_channel_t *ch, const ipc_message_t *req, ipc_message_t *resp) {
    ipc_error_t err;

    err = ipc_send(ch, req);
    if (err != IPC_OK) return err;

    err = ipc_recv(ch, resp);
    return err;
}

void ipc_close(ipc_channel_t *ch) {
    if (!ch) return;
    if (ch->fd >= 0) {
        close(ch->fd);
        if (ch->is_server)
            unlink(ch->path);
    }
    memset(ch, 0, sizeof(*ch));
    ch->fd = -1;
}

void ipc_msg_init(ipc_message_t *msg, uint32_t type) {
    memset(msg, 0, sizeof(*msg));
    msg->magic = IPC_MAGIC;
    msg->type = type;
    msg->sender_pid = getpid();
}
