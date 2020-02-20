#include <errno.h>
#include <unistd.h>
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
#include "event_svc.h"

#define TCP_PORT 1701
#define URING_ENTRIES 32

LIST_HEAD(peer_head_t, tcp_peer)
peer_list_head = LIST_HEAD_INITIALIZER(peer_list_head);

/**
 * XXX
 * 
 * One uring to rule them all or one uring per connection? 
 * - the memory needed for 1000s of connections is very high for uring per connection
 * - uring per connection gives a lot more flexibility in terms of limiting completions
 *   per connection (aka preventing certain connections from spawning tasks)
 * - could possibly put all task generating reads into a separate uring, then you could
 *   control task creation rate by limiting completion polling on that uring.
 * - task creation rate will also be limited by availablity of read buffers to post to socket
 * 
 * Also how to deal with buffer sizing?
 * - essentially the peers need to match buffer sizes
 * - if a peer sends a PUT cmd to another peer, it needs to post a bulk receive buffer, but
 *   if the client sends its own cmd simultaneously, the bulk buffer could be filled with a
 *   small cmd message.
 * - perhaps bulk buffers could be proceeded with headers that are decoded, and then the buffer
 *   would be immediately read into to ensure that bulk buffers are only used with bulk sends / recvs
 * - alternatively, could create a second channel that would only post / receive bulk messages, tho
 *   presumably the bulk buffers would all need to be a fixed size.
 */

struct tcp_cm cm;
int tcp_cm_poll();

int tcp_listen()
{
    int rc = socket(AF_INET, SOCK_STREAM, 0);
    if (rc < 0)
    {
        log("tcp_listen: socket() = %m");

        return -errno;
    }

    cm.tcm_sockfd = rc;

    event_svc_add(cm.tcm_sockfd, tcp_cm_poll);

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

void tcp_cm_set_on_peer_add(peer_event_cb_t on_peer_add)
{
    cm.tcm_on_peer_add = on_peer_add;
}

int tcp_init()
{
    LIST_INIT(&peer_list_head);

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

    event_svc_add(fd, tcp_poll);

    log("got connection: %s:%d", inet_ntoa(addr->sin_addr), addr->sin_port);

    if (cm.tcm_on_peer_add)
    {
        cm.tcm_on_peer_add(peer);
    }

    return 0;
}

void tcp_peer_free(tcp_peer_t *peer)
{
    event_svc_del(peer->tp_sockfd);

    close(peer->tp_sockfd);
    LIST_REMOVE(peer, tp_list_entry);
    io_uring_queue_exit(&peer->tp_ring);
    free(peer);
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

    if (rq->trq_type == TRQ_BROADCAST)
    {
        rq->trq_bcast_cqs_left--;
        log("bcast cqe seen, %d left", rq->trq_bcast_cqs_left);
        if (rq->trq_bcast_cqs_left > 0)
        {
            return;
        }
    }

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

    int cqs_found = 0;
    LIST_FOREACH(peer, &peer_list_head, tp_list_entry)
    {
        rc = io_uring_peek_cqe(&peer->tp_ring, &cqe);
        if (rc == 0)
        {
            peer_process_cqe(peer, cqe);
            cqs_found++;

            // don't return here because that could starve peers at end of list
        }
    }

    return cqs_found;
}

void tcp_rq_rw(tcp_rq_t *rq)
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

void tcp_rq_broadcast(tcp_rq_t *rq)
{
    tcp_peer_t *peer;
    rq->trq_bcast_cqs_left = 0;

    LIST_FOREACH(peer, &peer_list_head, tp_list_entry)
    {
        struct io_uring *ring = &peer->tp_ring;
        struct io_uring_sqe *sqe = io_uring_get_sqe(ring);

        io_uring_prep_writev(sqe, peer->tp_sockfd, rq->trq_iov, rq->trq_iov_count, 0);

        io_uring_sqe_set_data(sqe, rq);
        io_uring_submit(ring);

        rq->trq_bcast_cqs_left++;
    }
}

void tcp_rq_submit(tcp_rq_t *rq)
{
    if (rq->trq_type == TRQ_BROADCAST)
    {
        tcp_rq_broadcast(rq);
    }
    else
    {
        tcp_rq_rw(rq);
    }
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