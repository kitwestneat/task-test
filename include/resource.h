#ifndef _RESOURCE_H
#define _RESOURCE_H 1

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define TASK_COUNT 6
#define DISK_COUNT 50
#define TCP_COUNT 50

enum resource_type
{
    RT_INVAL = 0,
    RT_TASK = 1,
    RT_TCP,
    RT_DISK,
    RT_MAX,
};

struct resource_descriptor;
typedef void (*desc_cb_t)(struct resource_descriptor *desc);

typedef struct resource_descriptor
{
    struct resource_descriptor *rd_next;
    desc_cb_t rd_cb;
    void *rd_cb_data;
    size_t rd_count;

    bool rd_allocated;
    void **rd_data_list;

    enum resource_type rd_type_list[];
} res_desc_t;

typedef void (*resource_submit_fn_t)(void *resource, res_desc_t *desc);

struct resource_pool
{
    enum resource_type rp_type;
    size_t rp_obj_size;

    resource_submit_fn_t rp_submit;
    int (*rp_poll)();
    void (*rp_fini)(void *obj);

    size_t rp_count;
    size_t rp_free_count;
    unsigned char *rp_free_bitmap;

    void *rp_data;
};

void resource_pool_init();
struct resource_pool *resource_pool_get_by_type(enum resource_type type);
void *resource_pool_alloc_obj(struct resource_pool *rp);
void resource_pool_put_obj(struct resource_pool *rp, void *obj);
void resource_pool_fini();

res_desc_t *resource_desc_new(size_t count);
void resource_desc_done(res_desc_t *desc);

// try to allocate the resources in the descriptor
int resource_desc_submit(res_desc_t *res);
void resource_desc_release(res_desc_t *res);
int resource_poll();

void resource_desc_children_submit(res_desc_t *desc);

#endif
