#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "log.h"
#include "tcp.h"
#include "task.h"

int alive = 1;

struct chat_peer
{
    tcp_peer_t *cp_tcp_peer;
    res_desc_t *cp_task_desc;

    char cp_name[256];
};

char *rtrim(char *s)
{
    char *back = s + strlen(s) - 1;
    while (isspace(*back))
    {
        back--;
    }
    *(back + 1) = 0;

    return s;
}

struct chat_peer *chat_peer_new()
{
    struct chat_peer *peer = malloc(sizeof(struct chat_peer));

    return peer;
}

void chat_peer_get_name2(task_t *task)
{
    struct chat_peer *peer = task->task_cb_data;
    tcp_rq_t *prompt = task_rd_get_data(task, 0);
    tcp_rq_t *resp = task_rd_get_data(task, 1);

    free(prompt->trq_iov[0].iov_base);

    peer->cp_name[resp->trq_res] = 0;
    rtrim(peer->cp_name);

    log("%s joined the chat.", peer->cp_name);

    alive = 0;
}

void chat_peer_get_name1(task_t *task)
{
    struct chat_peer *peer = task->task_cb_data;

    tcp_rq_t *prompt_rq = task_rd_get_data(task, 0);
    tcp_rq_t *resp_rq = task_rd_get_data(task, 1);

    tcp_rq_peer_init(prompt_rq, TRQ_WRITE, peer->cp_tcp_peer, 1);
    tcp_rq_peer_init(resp_rq, TRQ_READ, peer->cp_tcp_peer, 1);

    char *name_prompt = malloc(16);
    sprintf(name_prompt, "Name: ");

    prompt_rq->trq_iov[0].iov_base = name_prompt;
    prompt_rq->trq_iov[0].iov_len = 16;

    resp_rq->trq_iov[0].iov_base = peer->cp_name;
    resp_rq->trq_iov[0].iov_len = 256;

    task_submit(task, chat_peer_get_name2);
}

void chat_peer_get_name0(res_desc_t *desc)
{
    task_t *task = desc->rd_data_list[0];
    struct chat_peer *peer = desc->rd_cb_data;

    peer->cp_task_desc = desc;
    task->task_cb_data = peer;

    task_rd_new(task, 2);
    task_rd_set_type(task, 0, RT_TCP);
    task_rd_set_type(task, 1, RT_TCP);

    task_submit(task, chat_peer_get_name1);
}

void on_peer_add(tcp_peer_t *peer)
{
    struct chat_peer *chat_peer = chat_peer_new();

    chat_peer->cp_tcp_peer = peer;

    task_get_one(chat_peer_get_name0, chat_peer);
}

int main()
{
    resource_pool_init();

    tcp_cm_set_on_peer_add(on_peer_add);

    task_loop_watch(&alive);

    resource_pool_fini();
}