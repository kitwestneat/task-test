#ifndef _TCP_H
#define _TCP_H 1

#include <liburing.h>
#include <netinet/ip.h>
#include <sys/queue.h>

#include "task.h"

#define PEER_MAX_RQ 8
#define TCP_LISTEN_BACKLOG 8

typedef struct tcp_peer
{
    int tp_sockfd;

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
    TRQ_BROADCAST,
};

struct tcp_rq;
typedef void (*trq_cb_t)(struct tcp_rq *rq);

typedef struct tcp_rq
{
    enum trq_type trq_type;
    union {
        tcp_peer_t *trq_peer;
        uintptr_t trq_bcast_cqs_left;
    };

    int trq_res;

    trq_cb_t trq_cb;
    void *trq_cb_data;

    size_t trq_iov_count;
    struct iovec *trq_iov;
} tcp_rq_t;

void tcp_init();
void tcp_fini();
int tcp_listen();

void tcp_cm_set_on_peer_add(peer_event_cb_t on_peer_add);
tcp_peer_t *tcp_connect(char *addr_str, int port);
void tcp_peer_free(tcp_peer_t *peer);

int tcp_poll();

tcp_rq_t *tcp_rq_new(enum trq_type type, tcp_peer_t *peer, size_t iov_count);
void tcp_rq_peer_init(tcp_rq_t *rq, enum trq_type type, tcp_peer_t *peer, size_t iov_count);
void tcp_rq_submit(tcp_rq_t *rq);
void tcp_rq_iov_alloc(tcp_rq_t *rq, size_t count);

void tcp_peer_post_read_buf(tcp_peer_t *peer, size_t bufsz, task_cb_t cb, void *cb_data);

#endif