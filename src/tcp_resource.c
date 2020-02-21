#include <stdlib.h>
#include "tcp.h"
#include "resource.h"
#include "task.h"
#include "log.h"

struct tcp_peer_read_data
{
    union {
        tcp_peer_t *tprd_peer;
        res_desc_t *tprd_rq_desc;
    };
    task_cb_t tprd_cb;
    void *tprd_cb_data;
    size_t tprd_bufsz;
};

static void tcp_peer_read_task(res_desc_t *desc)
{
    task_t *task = desc->rd_data_list[0];

    struct tcp_peer_read_data *tprd = desc->rd_cb_data;
    task_cb_t cb = tprd->tprd_cb;
    task->task_cb_data = tprd->tprd_cb_data;
    task->task_rd = tprd->tprd_rq_desc;
    task->task_parent_desc = desc;

    free(tprd);

    cb(task);
}

static void tcp_peer_on_read(res_desc_t *desc)
{
    struct tcp_peer_read_data *tprd = desc->rd_cb_data;

    tprd->tprd_rq_desc = desc;

    task_get_one(tcp_peer_read_task, tprd);
}

static void tcp_peer_fill_rd_rq(res_desc_t *desc)
{
    struct tcp_peer_read_data *tprd = desc->rd_cb_data;
    tcp_rq_t *rq = desc->rd_data_list[0];

    tcp_rq_peer_init(rq, TRQ_READ, tprd->tprd_peer, 1);

    rq->trq_iov[0].iov_base = malloc(tprd->tprd_bufsz);
    rq->trq_iov[0].iov_len = tprd->tprd_bufsz;

    log("tcp_peer_fill_rd_rq: peer %p, tprd %p", rq->trq_peer, tprd);

    desc->rd_cb = tcp_peer_on_read;
    desc->rd_cb_data = tprd;

    resource_desc_submit(desc);
}

void tcp_peer_post_read_buf(tcp_peer_t *peer, size_t bufsz, task_cb_t cb, void *cb_data)
{
    struct tcp_peer_read_data *tprd = malloc(sizeof(struct tcp_peer_read_data));
    tprd->tprd_peer = peer;
    tprd->tprd_cb = cb;
    tprd->tprd_cb_data = cb_data;
    tprd->tprd_bufsz = bufsz;

    log("tcp_peer_post_read_buf: peer %p, tprd %p", tprd->tprd_peer, tprd);

    resource_get_one(RT_TCP, tcp_peer_fill_rd_rq, tprd);
}