#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "log.h"
#include "tcp.h"
#include "disk.h"
#include "task.h"

#define BUF_SIZE 4 * 1024

disk_t *disk;

int alive = 1;

struct stio_cmd
{
    enum drq_type stc_type;
    off_t stc_offset;
    uint32_t stc_buf_count;
};

struct stio_client
{
    tcp_peer_t *stcli_peer;
};

void on_client_cmd(task_t *task)
{
    tcp_rq_t *rq = task_rd_get_data(task, 0);
    struct stio_cmd *cmd = rq->trq_iov[0].iov_base;

    log("Received cmd from client, type %d, offset %d, buf count %d",
        cmd->stc_type, cmd->stc_offset, cmd->stc_buf_count);
}

struct stio_client *stio_client_new()
{
    return malloc(sizeof(struct stio_client));
}

void on_peer_add(tcp_peer_t *peer)
{
    struct stio_client *client = stio_client_new();

    client->stcli_peer = peer;

    log("on_peer_add: %p", peer);

    tcp_peer_post_read_buf(peer, sizeof(struct stio_cmd), on_client_cmd, client);
}

int main()
{
    resource_pool_init();
    tcp_listen();

    tcp_cm_set_on_peer_add(on_peer_add);
    int rc = disk_open("./stio.bin", &disk);
    if (rc < 0)
    {
        return rc;
    }

    task_loop_watch(&alive);

    resource_pool_fini();
}