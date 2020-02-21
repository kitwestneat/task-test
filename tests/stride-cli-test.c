#include <unistd.h>
#include <stdlib.h>
#include "disk.h"
#include "task.h"
#include "tcp.h"
#include "log.h"

struct stio_cmd
{
    enum drq_type stc_type;
    off_t stc_offset;
    uint32_t stc_buf_count;
};

void send_done(task_t *task)
{
    tcp_rq_t *rq = task_rd_get_data(task, 0);
    struct stio_cmd *cmd_buf = rq->trq_iov[0].iov_base;

    log("sent");

    free(cmd_buf);
    tcp_peer_free(rq->trq_peer);
    task_rd_done(task);
}

void send_cmd(task_t *task)
{
    tcp_rq_t *rq = task_rd_get_data(task, 0);

    tcp_peer_t *peer = tcp_connect("127.0.0.1", 1701);
    if (!peer)
    {
        task_rd_done(task);
        return;
    }

    tcp_rq_peer_init(rq, TRQ_WRITE, peer, 1);

    struct stio_cmd *cmd_buf = malloc(sizeof(struct stio_cmd));

    cmd_buf->stc_type = DRQ_WRITE;
    cmd_buf->stc_offset = 420;
    cmd_buf->stc_buf_count = 69;

    rq->trq_iov[0].iov_base = cmd_buf;
    rq->trq_iov[0].iov_len = sizeof(*cmd_buf);

    log("sending cmd");
    task_submit(task, send_done);
}

void get_rq(res_desc_t *desc)
{
    task_t *task = desc->rd_data_list[0];
    task_rd_new(task, 1);
    task_rd_set_type(task, 0, RT_TCP);
    task_submit(task, send_cmd);
}

int main()
{
    task_init(1, get_rq);
}