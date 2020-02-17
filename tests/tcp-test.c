#include <string.h>
#include <stdlib.h>
#include <unistd.h>

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

void on_peer_add(tcp_peer_t *peer)
{
    char *buf = malloc(128);
    strcpy(buf, "Hello World!\n");

    tcp_rq_t *rq = tcp_rq_new(TRQ_WRITE, peer, 1);
    rq->trq_iov[0].iov_base = buf;
    rq->trq_iov[0].iov_len = 128;

    rq->trq_cb = hello_sent;

    tcp_rq_submit(rq);
}

int main()
{
    tcp_init(on_peer_add);

    log("waiting for connection..");
    while (alive)
    {
        if (tcp_poll() == 0)
            usleep(100);
    }
}