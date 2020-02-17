#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include <sys/queue.h>
#include <stdlib.h>
#include <string.h>

#include "tcp.h"
#include "log.h"
#include "misc.h"

#define TCP_PORT 1701
#define URING_ENTRIES 32

LIST_HEAD(peer_head_t, tcp_peer)
peer_list_head = LIST_HEAD_INITIALIZER(peer_list_head);

/**
 * XXX One uring to rule them all or one uring per connection? Probably need to do one per connection in order to be able to slow them down.
 */

struct tcp_cm cm;

int tcp_listen()
{
    int rc = socket(AF_INET, SOCK_STREAM, 0);
    if (rc < 0)
    {
        log("tcp_listen: socket() = %m");

        return -errno;
    }

    cm.tcm_sockfd = rc;

    int enable = 1;
    rc = setsockopt(cm.tcm_sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    if (rc < 0)
    {
        log("tcp_listen: setsockopt() = %m");

        return -errno;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TCP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    rc = bind(cm.tcm_sockfd, (struct sockaddr *)&addr, sizeof(addr));
    if (rc < 0)
    {
        log("tcp_listen: bind() = %m");

        return -errno;
    }

    rc = fcntl(cm.tcm_sockfd, F_SETFL, O_NONBLOCK);
    if (rc < 0)
    {
        log("tcp_listen: fcntl() = %m");

        return -errno;
    }

    rc = listen(cm.tcm_sockfd, TCP_LISTEN_BACKLOG);
    if (rc < 0)
    {
        log("tcp_listen: listen() = %m");

        return -errno;
    }

    return 0;
}

int tcp_init(peer_event_cb_t on_peer_add)
{
    LIST_INIT(&peer_list_head);

    cm.tcm_on_peer_add = on_peer_add;

    return tcp_listen();
}

int tcp_peer_add(int fd, struct sockaddr_in *addr)
{
    tcp_peer_t *peer = malloc(sizeof(tcp_peer_t));

    memcpy(&peer->tp_addr, addr, sizeof(struct sockaddr_in));
    io_uring_queue_init(URING_ENTRIES, &peer->tp_ring, 0);
    peer->tp_sockfd = fd;
    peer->tp_in_flight = 0;
    LIST_INSERT_HEAD(&peer_list_head, peer, tp_list_entry);

    log("got connection: %s:%d", inet_ntoa(addr->sin_addr), addr->sin_port);

    if (cm.tcm_on_peer_add)
    {
        cm.tcm_on_peer_add(peer);
    }

    return 0;
}

int tcp_cm_poll()
{
    struct sockaddr_in peer_addr;
    socklen_t len = sizeof(peer_addr);

    int fd = accept(cm.tcm_sockfd, (struct sockaddr *)&peer_addr, &len);
    if (fd < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return 0;
        }

        log("tcp_cm_poll: accept() = %m, rc=%d", fd);

        return -errno;
    }

    tcp_peer_add(fd, &peer_addr);
    return 1;
}

void peer_process_cqe(tcp_peer_t *peer, struct io_uring_cqe *cqe)
{
    tcp_rq_t *rq = io_uring_cqe_get_data(cqe);
    rq->trq_res = cqe->res;
    io_uring_cqe_seen(&peer->tp_ring, cqe);

    log("cqe seen, calling cb");
    rq->trq_cb(rq);
}

int tcp_poll()
{
    int rc = tcp_cm_poll();
    if (rc < 0)
    {
        log("tcp_poll: tcp_cm_poll() = %d", rc);
    }

    tcp_peer_t *peer;
    struct io_uring_cqe *cqe;

    LIST_FOREACH(peer, &peer_list_head, tp_list_entry)
    {
        rc = io_uring_peek_cqe(&peer->tp_ring, &cqe);
        if (rc == 0)
        {
            peer_process_cqe(peer, cqe);

            return 1;
        }
    }

    return 0;
}

void tcp_rq_submit(tcp_rq_t *rq)
{
    struct io_uring *ring = &rq->trq_peer->tp_ring;

    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    switch (rq->trq_type)
    {
    case TRQ_WRITE:
        log("rq_submit: write");
        io_uring_prep_writev(sqe, rq->trq_peer->tp_sockfd, rq->trq_iov, rq->trq_iov_count, 0);
        break;
    case TRQ_READ:
        log("rq_submit: read");
        io_uring_prep_readv(sqe, rq->trq_peer->tp_sockfd, rq->trq_iov, rq->trq_iov_count, 0);
        break;
    }

    io_uring_sqe_set_data(sqe, rq);

    io_uring_submit(ring);
}

tcp_rq_t *tcp_rq_new(enum trq_type type, tcp_peer_t *peer, size_t iov_count)
{
    tcp_rq_t *rq = calloc(1, sizeof(tcp_rq_t));
    tcp_rq_peer_init(rq, type, peer, iov_count);

    return rq;
}

void tcp_rq_peer_init(tcp_rq_t *rq, enum trq_type type, tcp_peer_t *peer, size_t iov_count)
{
    rq->trq_peer = peer;
    rq->trq_iov_count = iov_count;
    rq->trq_iov = calloc(iov_count, sizeof(struct iovec));

    rq->trq_type = type;
}