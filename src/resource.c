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
}

int resource_desc_alloc_submit(res_desc_t *res)
{
    int rc;

    if (!resource_desc_is_valid(res))
    {
        log("attempting to allocate too many resources, aborting");
        return -E2BIG;
    }

    res->rd_next = NULL;

    if (!queue_tail)
    {
        log("calling resource_desc_fill");
        rc = resource_desc_fill(res);
        if (rc == -EAGAIN)
        {
            log("queuing resource fill req to head");
            queue_head = queue_tail = res;
        }
        else
        {
            log("completing resource fill req");
            res->rd_cb(res);
        }
    }
    else
    {
        log("queuing resource fill req to tail");
        queue_tail->rd_next = res;
    }

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

int resource_desc_alloc_poll()
{
    if (!queue_head)
    {
        return 0;
    }

    int rc = resource_desc_fill(queue_head);
    if (rc != 0)
    {
        return 0;
    }

    queue_head->rd_cb(queue_head);
    queue_head = queue_head->rd_next;

    return 1;
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
    res_desc_t *desc = malloc(sizeof(res_desc_t) + sizeof(void *) * count);
    desc->rd_cb_data = NULL;
    desc->rd_type_list = malloc(sizeof(enum resource_type) * count);
    desc->rd_count = count;
    desc->rd_next = NULL;
    desc->rd_cb = NULL;
    desc->rd_allocated = false;

    return desc;
}

void resource_desc_done(res_desc_t *desc)
{

    free(desc->rd_type_list);
    free(desc);
}

void resource_desc_children_submit(res_desc_t *desc)
{
    for (int i = 0; i < desc->rd_count; i++)
    {
        struct resource_pool *rp = resource_pool_get_by_type(desc->rd_type_list[i]);

        if (rp->rp_submit)
        {
            log("submitting resource %p type %d", desc->rd_data_list[i], desc->rd_type_list[i]);
            rp->rp_submit(desc->rd_data_list[i], desc);
        }
        else
        {
            desc->rd_cb(desc);
        }
    }
}