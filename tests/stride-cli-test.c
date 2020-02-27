#include <unistd.h>
#include <stdlib.h>
#include "disk.h"
#include "task.h"
#include "tcp.h"
#include "log.h"

#define BUF_COUNT 4
#define BUF_SIZE 4096

struct stio_cmd
{
    enum drq_type stc_type;
    off_t stc_offset;
    uint32_t stc_buf_count;
};

void send_bulk0(task_t *task);

void send_bulk1(task_t *task)
{
    static int buf_sends_left = BUF_COUNT;
    tcp_rq_t *rq = task_rd_get_data(task, 0);

    buf_sends_left--;

    if (buf_sends_left == 0)
    {
        free(rq->trq_iov[0].iov_base);
        tcp_peer_free(task->task_cb_data);
        task_rd_done(task);

        return;
    }

    task_submit(task, send_bulk0);
}

void send_bulk0(task_t *task)
{
    static char *buf = NULL;
    if (!buf)
    {
        buf = malloc(BUF_SIZE);
    }

    tcp_peer_t *peer = task->task_cb_data;

    tcp_rq_t *rq = task_rd_get_data(task, 0);
    tcp_rq_peer_init(rq, TRQ_WRITE, peer, 1);

    rq->trq_iov[0].iov_base = buf;
    rq->trq_iov[0].iov_len = BUF_SIZE;

    for (int i = 0; i < BUF_SIZE; i++)
    {
        buf[i] = i & 0xff;
    }

    task_submit(task, send_bulk1);
}

void send_cmd1(task_t *task)
{
    tcp_rq_t *rq = task_rd_get_data(task, 0);
    struct stio_cmd *cmd_buf = rq->trq_iov[0].iov_base;

    free(cmd_buf);
    task_submit(task, send_bulk0);
}

void send_cmd0(task_t *task)
{
    tcp_rq_t *rq = task_rd_get_data(task, 0);

    tcp_peer_t *peer = tcp_connect("127.0.0.1", 1701);
    if (!peer)
    {
        task_rd_done(task);
        return;
    }

    task->task_cb_data = peer;

    tcp_rq_peer_init(rq, TRQ_WRITE, peer, 1);

    struct stio_cmd *cmd_buf = malloc(sizeof(struct stio_cmd));

    cmd_buf->stc_type = DRQ_WRITE;
    cmd_buf->stc_offset = 0;
    cmd_buf->stc_buf_count = BUF_COUNT;

    rq->trq_iov[0].iov_base = cmd_buf;
    rq->trq_iov[0].iov_len = sizeof(*cmd_buf);

    task_submit(task, send_cmd1);
}

void get_rq(res_desc_t *desc)
{
    task_t *task = desc->rd_data_list[0];
    task_rd_new(task, 1);
    task_rd_set_type(task, 0, RT_TCP);
    task_submit(task, send_cmd0);
}

int main()
{
    task_init(1, get_rq);
}