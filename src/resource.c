#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>

#include "log.h"
#include "resource.h"
#include "task.h"

struct resource_pool *task_pool;
struct resource_pool *foo_pool;
struct resource_pool *bar_pool;

res_desc_t *queue_head;
res_desc_t *queue_tail;

unsigned char *bitmap_new(size_t count)
{
    return calloc(count / 8, 1);
}

int bitmap_get(unsigned char byte, unsigned bit)
{
    return byte & (1 << bit);
}

void bitmap_set(unsigned char *byte, unsigned bit)
{
    *byte = *byte | (1 << bit);
}

void bitmap_clr(unsigned char *byte, unsigned bit)
{
    *byte = *byte & ~(1 << bit);
}

void bitmap_dealloc(unsigned char *bitmap, unsigned bitmap_index)
{
    int i = bitmap_index / 8;
    bitmap_clr(&bitmap[i], bitmap_index % 8);
}

int bitmap_alloc(unsigned char *bitmap, size_t bitmap_size)
{
    int i;
    int j;
    unsigned char byte;

    for (i = 0; i < bitmap_size / 8; i++)
    {
        if (bitmap[i] != 0xff)
        {
            goto found;
        }
    }

    return -1;

found:
    byte = bitmap[i];

    for (j = 0; j < 8; j++)
    {
        if (bitmap_get(byte, j) == 0)
        {
            bitmap_set(&bitmap[i], j);

            return i * 8 + j;
        }
    }

    return -1;
}

size_t resource_pool_get_obj_size(enum resource_type type)
{
    size_t obj_size;

    switch (type)
    {
    case RT_TASK:
        obj_size = sizeof(struct task);
        break;
    case RT_FOO:
        obj_size = sizeof(foo_t);
        break;
    case RT_BAR:
        obj_size = sizeof(bar_t);
        break;
    default:
        break;
    }

    return obj_size;
}

unsigned resource_pool_get_index(struct resource_pool *rp, void *obj)
{
    size_t obj_size = resource_pool_get_obj_size(rp->rp_type);

    void *rp_obj_base = &rp->rp_data[0];

    assert(obj > rp_obj_base);

    return (obj - rp_obj_base) / obj_size;
}

struct resource_pool *resource_pool_new(enum resource_type type, size_t count)
{
    size_t obj_size = resource_pool_get_obj_size(type);

    struct resource_pool *rp = malloc(sizeof(struct resource_pool) + (obj_size * count));

    rp->rp_type = type;
    rp->rp_count = count;
    rp->rp_free_count = count;
    rp->rp_free_bitmap = bitmap_new(count);

    return rp;
}

void resource_pool_init()
{
    task_pool = resource_pool_new(RT_TASK, TASK_COUNT);
    foo_pool = resource_pool_new(RT_FOO, FOO_COUNT);
    bar_pool = resource_pool_new(RT_BAR, BAR_COUNT);

    queue_head = NULL;
    queue_tail = NULL;
}

struct resource_pool *resource_pool_get_by_type(enum resource_type type)
{
    struct resource_pool *rp;

    switch (type)
    {
    case RT_TASK:
        rp = task_pool;
        break;
    case RT_FOO:
        rp = foo_pool;
        break;
    case RT_BAR:
        rp = bar_pool;
        break;
    default:
        log("unknown task type %d", type);
        return NULL;
    }
    assert(rp);
}

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

        int rp_index = bitmap_alloc(rp->rp_free_bitmap, rp->rp_count);
        rp->rp_free_count--;

        desc->rd_data_list[i] = rp->rp_data + rp_index * resource_pool_get_obj_size(rp->rp_type);
    }

    return 0;
}

void resource_desc_release_resources(res_desc_t *desc)
{
    for (int i = 0; i < desc->rd_count; i++)
    {
        struct resource_pool *rp = resource_pool_get_by_type(desc->rd_type_list[i]);

        unsigned rp_index = resource_pool_get_index(rp, desc->rd_data_list[i]);
        bitmap_dealloc(rp->rp_free_bitmap, rp_index);
        rp->rp_free_count++;
    }
}

int resource_submit(res_desc_t *res)
{
    int rc;

    if (!res->rd_cb)
    {
        log("resource allocation request missing callback, ignoring");
        return -EINVAL;
    }

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

void resource_poll()
{
    res_desc_t *desc = queue_head;
    while (desc)
    {
        int rc = resource_desc_fill(desc);
        if (rc == 0)
        {
            desc->rd_cb(desc);
            queue_head = queue_head->rd_next;

            break;
        }

        desc = desc->rd_next;
    }
}

res_desc_t *res_desc_new(size_t count)
{
    res_desc_t *desc = malloc(sizeof(res_desc_t) + sizeof(void *) * count);
    desc->rd_cb_data = NULL;
    desc->rd_type_list = malloc(sizeof(enum resource_type) * count);
    desc->rd_count = count;
    desc->rd_next = NULL;
    desc->rd_cb = NULL;

    return desc;
}

void res_desc_done(res_desc_t *desc)
{

    free(desc->rd_type_list);
    free(desc);
}
