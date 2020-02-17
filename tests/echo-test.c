#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "resource.h"
#include "tcp.h"
#include "log.h"

int alive = 1;

void on_resp(tcp_rq_t *rq)
{
    char *buf = rq->trq_iov[0].iov_base;
    buf[rq->trq_res - 1] = 0;
    log("read from client rc=%d, len=%zu %s", rq->trq_res, rq->trq_iov[0].iov_len, buf);
    alive = 0;
}

void hello_sent(tcp_rq_t *rq)
{
    log("hello_sent");

    rq->trq_type = TRQ_READ;
    rq->trq_cb = on_resp;

    tcp_rq_submit(rq);
}

void got_msg(tcp_rq_t *rq);

void post_read(tcp_rq_t *rq)
{
    log("post_read, res=%d", rq->trq_res);
    rq->trq_type = TRQ_READ;
    rq->trq_cb = got_msg;

    tcp_rq_submit(rq);
}

void got_msg(tcp_rq_t *rq)
{
    static int txn_id = 0;
    if (txn_id > 10)
    {
        alive = 0;
        return;
    }

    log("got_msg, rq %p res=%d", rq, rq->trq_res);

    char *buf = rq->trq_iov[0].iov_base;
    size_t buf_len = rq->trq_iov[0].iov_len;

    buf[rq->trq_res] = 0;

    char *new_buf = malloc(buf_len);

    log("got[%d]: %s", txn_id, buf);

    snprintf(new_buf, buf_len, "echo[%d]: %s\n", txn_id, buf);

    rq->trq_iov[0].iov_base = new_buf;
    rq->trq_iov[0].iov_len = buf_len;

    free(buf);

    txn_id++;

    rq->trq_type = TRQ_WRITE;
    rq->trq_cb = post_read;
    tcp_rq_submit(rq);
}

void submit_peer_buf(res_desc_t *desc)
{
    tcp_rq_t *rq = desc->rd_data_list[0];
    tcp_peer_t *peer = desc->rd_cb_data;

    tcp_rq_peer_init(rq, TRQ_READ, peer, 1);

    char *buf = malloc(128);
    rq->trq_iov[0].iov_base = buf;
    rq->trq_iov[0].iov_len = 128;

    post_read(rq);
}

void on_peer_add(tcp_peer_t *peer)
{
    res_desc_t *desc = resource_desc_new(1);

    desc->rd_type_list[0] = RT_TCP;
    desc->rd_cb = submit_peer_buf;
    desc->rd_cb_data = peer;

    resource_desc_submit(desc);
}

int main()
{
    resource_pool_init();
    tcp_init(on_peer_add);

    log("waiting for connection...");
    while (alive)
    {
        if (resource_poll() == 0)
            usleep(100);
    }
}