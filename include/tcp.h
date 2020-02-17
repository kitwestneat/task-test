#ifndef _TCP_H
#define _TCP_H 1

#include <liburing.h>
#include <netinet/ip.h>
#include <sys/queue.h>

#define PEER_MAX_RQ 8
#define TCP_LISTEN_BACKLOG 8

typedef struct tcp_peer
{
    int tp_sockfd;
    struct io_uring tp_ring;
    uint8_t tp_in_flight;
    struct sockaddr_in tp_addr;
    LIST_ENTRY(tcp_peer)
    tp_list_entry;
} tcp_peer_t;

typedef void (*peer_event_cb_t)(tcp_peer_t *peer);

struct tcp_cm
{
    int tcm_sockfd;
    peer_event_cb_t tcm_on_peer_add;
};

enum trq_type
{
    TRQ_READ = 0,
    TRQ_WRITE,
};

struct tcp_rq;
typedef void (*trq_cb_fn_t)(struct tcp_rq *rq);

typedef struct tcp_rq
{
    enum trq_type trq_type;
    tcp_peer_t *trq_peer;
    int trq_res;

    trq_cb_fn_t trq_cb;
    void *trq_cb_data;

    size_t trq_iov_count;
    struct iovec *trq_iov;
} tcp_rq_t;

int tcp_init(peer_event_cb_t on_peer_add);
int tcp_poll();
tcp_rq_t *tcp_rq_new(enum trq_type type, tcp_peer_t *peer, size_t iov_count);
void tcp_rq_peer_init(tcp_rq_t *rq, enum trq_type type, tcp_peer_t *peer, size_t iov_count);
void tcp_rq_submit(tcp_rq_t *rq);
#endif