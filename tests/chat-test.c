#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "log.h"
#include "tcp.h"
#include "disk.h"
#include "task.h"

#define MSG_SIZE 256

char NAME_PROMPT[] = "Name: ";
char MSG_PROMPT[] = "> ";

int alive = 1;

disk_t *log_disk;

struct chat_peer
{
    tcp_peer_t *cp_tcp_peer;
    res_desc_t *cp_task_desc;

    char cp_name[MSG_SIZE];
    char cp_msg_buf[MSG_SIZE];
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

void chat_peer_done(struct chat_peer *peer)
{
    tcp_peer_free(peer->cp_tcp_peer);
    resource_desc_release(peer->cp_task_desc);
    resource_desc_done(peer->cp_task_desc);
}

void log2disk_fill(disk_rq_t *rq, char *msg)
{
    static off_t offset = 0;
    assert(log_disk);

    size_t len = strlen(msg);

    log("writing %d to 0x%x, msg: %d", len, offset, msg);

    rq->drq_type = DRQ_WRITE;
    rq->drq_offset = offset;
    rq->drq_disk = log_disk;
    rq->drq_iov = calloc(1, sizeof(struct iovec));
    rq->drq_iov_count = 1;
    rq->drq_iov[0].iov_base = msg;
    rq->drq_iov[0].iov_len = len;

    offset += len;
}

void chat_peer_get_msg0(task_t *task);

void chat_peer_broadcast1(task_t *task)
{
    log("chat_peer_broadcast1");

    tcp_rq_t *bcast_rq = task_rd_get_data(task, 0);
    disk_rq_t *log_rq = task_rd_get_data(task, 1);

    free(bcast_rq->trq_iov[0].iov_base);

    if (bcast_rq->trq_res < 0 || log_rq->drq_res < 0)
    {
        log("chat_peer_broadcast1: error, rc = %d / %d", bcast_rq->trq_res, log_rq->drq_res);
        task_rd_done(task);

        struct chat_peer *peer = task->task_cb_data;
        chat_peer_done(peer);

        return;
    };

    task_rd_release(task);
    task_rd_set_type(task, 1, RT_TCP);

    task_submit(task, chat_peer_get_msg0);
}

void chat_peer_broadcast0(task_t *task)
{
    log("chat_peer_broadcast0");
    struct chat_peer *peer = task->task_cb_data;
    tcp_rq_t *bcast_rq = task_rd_get_data(task, 0);

    tcp_rq_peer_init(bcast_rq, TRQ_BROADCAST, 0, 1);
    size_t buflen = sizeof(peer->cp_msg_buf);
    char *buf = malloc(buflen);

    snprintf(buf, buflen, "[%s] %s\n", peer->cp_name, peer->cp_msg_buf);
    log("bcast buf: %s", buf);

    bcast_rq->trq_iov[0].iov_base = buf;
    bcast_rq->trq_iov[0].iov_len = strlen(buf);

    disk_rq_t *disk_rq = task_rd_get_data(task, 1);
    log2disk_fill(disk_rq, buf);

    task_submit(task, chat_peer_broadcast1);
}

void chat_peer_get_msg1(task_t *task)
{
    log("chat_peer_get_msg1");
    struct chat_peer *peer = task->task_cb_data;
    tcp_rq_t *prompt = task_rd_get_data(task, 0);
    tcp_rq_t *resp = task_rd_get_data(task, 1);

    if (prompt->trq_res < 0 || resp->trq_res < 0)
    {
        log("chat_peer_get_msg1: client network error, rc = %d / %d", prompt->trq_res, resp->trq_res);
        task_rd_done(task);
        chat_peer_done(peer);

        return;
    };

    if (resp->trq_res == 0)
    {
        log("chat_peer_get_msg1: got eof, closing connection");
        task_rd_done(task);
        chat_peer_done(peer);

        return;
    }

    peer->cp_msg_buf[resp->trq_res - 1] = 0;
    rtrim(peer->cp_msg_buf);

    log("[%s,rc=%d] %s", peer->cp_name, resp->trq_res, peer->cp_msg_buf);

    task_rd_release(task);
    task_rd_set_type(task, 1, RT_DISK);

    task_submit(task, chat_peer_broadcast0);
}

void chat_peer_get_msg0(task_t *task)
{
    log("chat_peer_get_msg0");
    struct chat_peer *peer = task->task_cb_data;
    tcp_rq_t *prompt_rq = task_rd_get_data(task, 0);
    tcp_rq_t *resp_rq = task_rd_get_data(task, 1);

    tcp_rq_peer_init(prompt_rq, TRQ_WRITE, peer->cp_tcp_peer, 1);
    tcp_rq_peer_init(resp_rq, TRQ_READ, peer->cp_tcp_peer, 1);

    prompt_rq->trq_iov[0].iov_base = MSG_PROMPT;
    prompt_rq->trq_iov[0].iov_len = sizeof(MSG_PROMPT);

    peer->cp_msg_buf[0] = 0;
    resp_rq->trq_iov[0].iov_base = peer->cp_msg_buf;
    resp_rq->trq_iov[0].iov_len = sizeof(peer->cp_msg_buf);

    task_submit(task, chat_peer_get_msg1);
}

void chat_peer_log_name1(task_t *task)
{
    struct chat_peer *peer = task->task_cb_data;
    disk_rq_t *disk_rq = task_rd_get_data(task, 0);

    free(disk_rq->drq_iov[0].iov_base);

    task_rd_done(task);
    task_rd_new(task, 2);
    task_rd_set_type(task, 0, RT_TCP);
    task_rd_set_type(task, 1, RT_TCP);

    task_submit(task, chat_peer_get_msg0);
}

void chat_peer_log_name0(task_t *task)
{
    struct chat_peer *peer = task->task_cb_data;
    disk_rq_t *disk_rq = task_rd_get_data(task, 0);

    char *buf = malloc(MSG_SIZE);
    snprintf(buf, MSG_SIZE, "%s joined the chat\n", peer->cp_name);

    log2disk_fill(disk_rq, buf);

    task_submit(task, chat_peer_log_name1);
}

void chat_peer_get_name2(task_t *task)
{
    struct chat_peer *peer = task->task_cb_data;
    tcp_rq_t *prompt = task_rd_get_data(task, 0);
    tcp_rq_t *resp = task_rd_get_data(task, 1);

    if (prompt->trq_res < 0 || resp->trq_res < 0)
    {
        log("chat_peer_get_name2: client network error, rc = %d / %d", prompt->trq_res, resp->trq_res);
        task_rd_done(task);
        chat_peer_done(peer);

        return;
    };

    peer->cp_name[resp->trq_res] = 0;
    rtrim(peer->cp_name);

    log("%s joined the chat.", peer->cp_name);

    task_rd_done(task);
    task_rd_new(task, 1);
    task_rd_set_type(task, 0, RT_DISK);

    task_submit(task, chat_peer_log_name0);
}

void chat_peer_get_name1(task_t *task)
{
    struct chat_peer *peer = task->task_cb_data;

    tcp_rq_t *prompt_rq = task_rd_get_data(task, 0);
    tcp_rq_t *resp_rq = task_rd_get_data(task, 1);

    tcp_rq_peer_init(prompt_rq, TRQ_WRITE, peer->cp_tcp_peer, 1);
    tcp_rq_peer_init(resp_rq, TRQ_READ, peer->cp_tcp_peer, 1);

    prompt_rq->trq_iov[0].iov_base = NAME_PROMPT;
    prompt_rq->trq_iov[0].iov_len = sizeof(NAME_PROMPT);

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
    int rc = disk_open("/tmp/chat.log", &log_disk);
    if (rc < 0)
    {
        return rc;
    }

    task_loop_watch(&alive);

    resource_pool_fini();
}