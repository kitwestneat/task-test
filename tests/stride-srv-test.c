#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "log.h"
#include "tcp.h"
#include "disk.h"
#include "task.h"

#define BUF_SIZE 4096

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

struct stio_client_cmd
{
    struct stio_cmd *scc_cmd;
    struct stio_client *scc_client;
    void *scc_buf;
};

void stio_client_done(struct stio_client *client)
{
    free(client);
}

void stio_cc_free(struct stio_client_cmd *scc)
{
    if (scc->scc_buf)
    {
        free(scc->scc_buf);
    }

    free(scc->scc_cmd);
    free(scc);
}

void do_client_read(task_t *task)
{
    log("do_client_read");
}

void client_write_fini(task_t *task)
{
    task_rd_done(task);

    struct stio_client_cmd *data = task->task_cb_data;
    stio_client_done(data->scc_client);
    stio_cc_free(data);
}

void do_client_write0(task_t *task);

void do_client_write3(task_t *task)
{
    disk_rq_t *rq = task_rd_get_data(task, 0);
    if (rq->drq_res < 0)
    {
        log("error writing: %d", rq->drq_res);
        client_write_fini(task);

        return;
    }

    log("-- allocating net read rq");
    task_rd_release(task);
    task_rd_set_type(task, 0, RT_TCP);
    task_submit(task, do_client_write0);
}

void do_client_write2(task_t *task)
{
    struct stio_client_cmd *data = task->task_cb_data;
    disk_rq_t *rq = task_rd_get_data(task, 0);
    disk_rq_init(rq, DRQ_WRITE, disk, 1);

    rq->drq_iov[0].iov_base = data->scc_buf;
    rq->drq_iov[0].iov_len = BUF_SIZE;

    rq->drq_offset = data->scc_cmd->stc_offset;
    data->scc_cmd->stc_offset += BUF_SIZE;

    log("-- posting disk bulk write %d", *(int *)data->scc_buf);
    task_submit(task, do_client_write3);
}

void do_client_write1(task_t *task)
{
    tcp_rq_t *rq = task_rd_get_data(task, 0);
    if (rq->trq_res < 0)
    {
        log("error posting read buffer: %d", rq->trq_res);
        client_write_fini(task);

        return;
    }

    log("-- allocating write rq");
    task_rd_release(task);
    task_rd_set_type(task, 0, RT_DISK);
    task_submit(task, do_client_write2);
}

void do_client_write0(task_t *task)
{
    struct stio_client_cmd *data = task->task_cb_data;
    if (data->scc_cmd->stc_buf_count == 0)
    {
        client_write_fini(task);

        return;
    }
    data->scc_cmd->stc_buf_count--;

    tcp_rq_t *rq = task_rd_get_data(task, 0);
    tcp_rq_peer_init(rq, TRQ_READ, data->scc_client->stcli_peer, 1);

    if (data->scc_buf == 0)
    {
        data->scc_buf = malloc(BUF_SIZE);
    }

    rq->trq_iov[0].iov_base = data->scc_buf;
    rq->trq_iov[0].iov_len = BUF_SIZE;

    log("-- posting network bulk read");
    task_submit(task, do_client_write1);
}

void on_client_cmd(task_t *task)
{
    struct stio_client *client = task->task_cb_data;

    tcp_rq_t *rq = task_rd_get_data(task, 0);
    struct stio_cmd *cmd = rq->trq_iov[0].iov_base;

    log("-- Received cmd from client, type %d, offset %d, buf count %d",
        cmd->stc_type, cmd->stc_offset, cmd->stc_buf_count);

    struct stio_client_cmd *data = malloc(sizeof(struct stio_client_cmd));
    data->scc_client = client;
    data->scc_cmd = cmd;
    data->scc_buf = 0;

    task->task_cb_data = data;

    if (cmd->stc_type == DRQ_WRITE)
    {
        task_submit(task, do_client_write0);
    }
    else
    {
        task_submit(task, do_client_read);
    }
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