#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>

#include "log.h"
#include "resource.h"
#include "foobar.h"
#include "task.h"

res_desc_t *queue_head = NULL;
res_desc_t *queue_tail = NULL;

void print_resource_queue();

bool resource_desc_count_check(res_desc_t *desc, bool check_free)
{
    int type_count[RT_MAX] = {0};

    for (int i = 0; i < desc->rd_count; i++)
    {
        enum resource_type type = desc->rd_type_list[i];
        struct resource_pool *rp = resource_pool_get_by_type(type);
        if (!rp)
        {
            return -EINVAL;
        }

        type_count[type]++;

        int tgt_count = check_free ? rp->rp_free_count : rp->rp_count;

        if (type_count[type] > tgt_count)
        {
            return false;
        }
    }

    return true;
}

int resource_desc_is_available(res_desc_t *desc)
{
    return resource_desc_count_check(desc, true);
}

int resource_desc_is_valid(res_desc_t *desc)
{
    return resource_desc_count_check(desc, false);
}

int resource_desc_fill(res_desc_t *desc)
{
    if (!resource_desc_is_available(desc))
    {
        return -EAGAIN;
    }

    for (int i = 0; i < desc->rd_count; i++)
    {
        struct resource_pool *rp = resource_pool_get_by_type(desc->rd_type_list[i]);

        desc->rd_data_list[i] = resource_pool_alloc_obj(rp);
    }

    desc->rd_allocated = true;

    return 0;
}

void resource_desc_release(res_desc_t *desc)
{
    for (int i = 0; i < desc->rd_count; i++)
    {
        struct resource_pool *rp = resource_pool_get_by_type(desc->rd_type_list[i]);

        resource_pool_put_obj(rp, desc->rd_data_list[i]);
    }

    desc->rd_allocated = false;
}

static void push_resource(res_desc_t *res)
{
    res->rd_next = NULL;

    if (!queue_tail)
    {
        log("push_resource - %p (task %p) q head | head %p tail %p", res, res->rd_cb_data, queue_head, queue_tail);
        queue_head = queue_tail = res;
    }
    else
    {
        log("push_resource - %p (task %p) q tail | head %p tail %p", res, res->rd_cb_data, queue_head, queue_tail);
        queue_tail->rd_next = res;
        queue_tail = res;
    }

    print_resource_queue();
}

int resource_desc_alloc_submit(res_desc_t *res)
{
    int rc;

    if (!resource_desc_is_valid(res))
    {
        log("attempting to allocate too many resources, aborting");
        return -E2BIG;
    }

    push_resource(res);

    return 0;
}

int resource_desc_submit(res_desc_t *res)
{
    if (!res->rd_cb)
    {
        log("resource allocation request missing callback, ignoring");
        return -EINVAL;
    }

    if (res->rd_allocated)
    {
        resource_desc_children_submit(res);

        return 0;
    }
    else
    {
        return resource_desc_alloc_submit(res);
    }
}

void print_res_desc(res_desc_t *desc)
{
    fprintf(stderr, "print_res_desc: [%p] cb_data %p next %p ", desc, desc->rd_cb_data, desc->rd_next);
    int type_count[RT_MAX] = {0};
    for (int i = 0; i < desc->rd_count; i++)
    {
        enum resource_type type = desc->rd_type_list[i];
        struct resource_pool *rp = resource_pool_get_by_type(type);
        if (!rp)
        {
            log("error getting rp");
            return;
        }

        type_count[type]++;

        fprintf(stderr, "rp_free %zu / rp_count %zu rq %d | ", rp->rp_free_count, rp->rp_count, type_count[type]);
    }

    fprintf(stderr, "\n");
}

void dequeue_next_resource(res_desc_t *prev)
{
    res_desc_t *node;

    if (prev)
    {
        node = prev->rd_next;
        prev->rd_next = node->rd_next;
    }
    else
    {
        node = queue_head;
        queue_head = node->rd_next;
    }

    if (queue_tail == node)
    {
        queue_tail = prev;
    }

    print_resource_queue();
}

int poll_count = 0;
int resource_desc_alloc_poll()
{
    res_desc_t *next = queue_head;
    res_desc_t *prev = NULL;

    poll_count++;

    int rc;
    int q_count = 0;

    while (next && (rc = resource_desc_fill(next)) == -EAGAIN)
    {
        q_count++;

        prev = next;
        next = next->rd_next;
    }
    if (poll_count == 100)
    {
        log("resource_desc_alloc_poll - resource allocation stalled, %d requests in queue | head %p tail %p", q_count, queue_head, queue_tail);
        print_resource_queue();
    }

    if (!next)
    {
        return 0;
    }

    if (rc != 0)
    {
        return rc;
    }

    log("resource_desc_alloc_poll - %p (data %p) running cb", next, next->rd_cb_data);
    dequeue_next_resource(prev);

    next->rd_cb(next);

    poll_count = 0;

    return 1;
}

void print_resource_queue()
{
    res_desc_t *node = queue_head;
    log("resource queue | head %p tail %p --", queue_head, queue_tail);
    while (node)
    {
        print_res_desc(node);
        node = node->rd_next;
    }
}

int resource_poll()
{
    int rc = 0;
    for (enum resource_type type = 1; type < RT_MAX; type++)
    {
        struct resource_pool *rp = resource_pool_get_by_type(type);
        ASSERT(rp);

        if (rp->rp_poll)
        {
            rc += rp->rp_poll();
        }
    }

    rc += resource_desc_alloc_poll();

    return rc;
}

res_desc_t *resource_desc_new(size_t count)
{
    res_desc_t *desc = malloc(sizeof(res_desc_t) + sizeof(enum resource_type) * count);
    desc->rd_cb_data = NULL;
    desc->rd_data_list = malloc(sizeof(void *) * count);
    desc->rd_count = count;
    desc->rd_next = NULL;
    desc->rd_cb = NULL;
    desc->rd_allocated = false;

    return desc;
}

void resource_desc_done(res_desc_t *desc)
{

    free(desc->rd_data_list);
    free(desc);
}

void resource_desc_children_submit(res_desc_t *desc)
{
    for (int i = 0; i < desc->rd_count; i++)
    {
        struct resource_pool *rp = resource_pool_get_by_type(desc->rd_type_list[i]);

        if (rp->rp_submit)
        {
            log("resource_desc_children_submit - desc %p task %p res %p type %d", desc, desc->rd_cb_data, desc->rd_data_list[i], desc->rd_type_list[i]);
            rp->rp_submit(desc->rd_data_list[i], desc);
        }
        else
        {
            desc->rd_cb(desc);
        }
    }
}