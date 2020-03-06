#include <stdlib.h>
#include <string.h>

#include "bitmap.h"
#include "event_svc.h"
#include "log.h"
#include "task.h"
#include "disk.h"
#include "resource.h"
#include "tcp.h"

void disk_pool_cb(disk_rq_t *rq)
{
  res_desc_t *desc = rq->drq_cb_data;

  desc->rd_cb(desc);
}

void disk_pool_submit(void *data, res_desc_t *desc)
{
  disk_rq_t *rq = data;
  rq->drq_cb = disk_pool_cb;
  rq->drq_cb_data = desc;

  disk_rq_submit(rq);
}

void disk_pool_fini(void *data)
{
  disk_rq_t *rq = data;
  if (rq->drq_iov)
  {
    free(rq->drq_iov);
  }
}

void tcp_pool_cb(tcp_rq_t *rq)
{
  res_desc_t *desc = rq->trq_cb_data;

  desc->rd_cb(desc);
}

void tcp_pool_submit(void *data, res_desc_t *desc)
{
  tcp_rq_t *rq = data;
  rq->trq_cb = tcp_pool_cb;
  rq->trq_cb_data = desc;

  tcp_rq_submit(rq);
}

void tcp_pool_fini(void *data)
{
  tcp_rq_t *rq = data;
  if (rq->trq_iov)
  {
    free(rq->trq_iov);
  }
}

struct resource_pool task_pool = {
    .rp_type = RT_TASK,
    .rp_obj_size = sizeof(task_t),
    .rp_submit = NULL,
    .rp_fini = NULL,
    .rp_poll = NULL,
    .rp_count = TASK_COUNT,
};

struct resource_pool tcp_pool = {
    .rp_type = RT_TCP,
    .rp_obj_size = sizeof(tcp_rq_t),
    .rp_submit = tcp_pool_submit,
    .rp_fini = tcp_pool_fini,
    .rp_poll = tcp_poll,
    .rp_count = TCP_COUNT,
};

struct resource_pool disk_pool = {
    .rp_type = RT_DISK,
    .rp_obj_size = sizeof(disk_rq_t),
    .rp_submit = disk_pool_submit,
    .rp_fini = disk_pool_fini,
    .rp_poll = disk_poll,
    .rp_count = DISK_COUNT,
};

unsigned resource_pool_get_index(struct resource_pool *rp, void *obj)
{
  size_t obj_size = rp->rp_obj_size;
  void *rp_obj_base = rp->rp_data;

  assert(obj >= rp_obj_base);

  return (obj - rp_obj_base) / obj_size;
}

int resource_pool_alloc(struct resource_pool *rp)
{
  rp->rp_data = malloc(rp->rp_obj_size * rp->rp_count);
  rp->rp_free_count = rp->rp_count;
  rp->rp_free_bitmap = bitmap_new(rp->rp_count);
}

void resource_pool_done(struct resource_pool *rp)
{
  free(rp->rp_free_bitmap);
  free(rp->rp_data);
}

struct resource_pool *resource_pool_get_by_type(enum resource_type type)
{
  struct resource_pool *rp;

  switch (type)
  {
  case RT_TASK:
    rp = &task_pool;
    break;
  case RT_TCP:
    rp = &tcp_pool;
    break;
  case RT_DISK:
    rp = &disk_pool;
    break;
  default:
    log("unknown resource type %d", type);
    return NULL;
  }
  assert(rp);

  return rp;
}

void resource_pool_init()
{
  resource_pool_alloc(&task_pool);
  resource_pool_alloc(&tcp_pool);
  resource_pool_alloc(&disk_pool);

  event_svc_init();
  disk_init();
  tcp_init();

  log("resource pools created: %p %p", task_pool, tcp_pool);
}

void resource_pool_fini()
{
  tcp_fini();

  resource_pool_done(&task_pool);
  resource_pool_done(&tcp_pool);
  resource_pool_done(&disk_pool);
}

void *resource_pool_alloc_obj(struct resource_pool *rp)
{

  int rp_index = bitmap_alloc(rp->rp_free_bitmap, rp->rp_count);
  assert(rp_index != -1);

  rp->rp_free_count--;

  size_t obj_size = rp->rp_obj_size;
  void *obj = rp->rp_data + rp_index * obj_size;
  memset(obj, 0, obj_size);

  log("allocating resource type %d, index %d, rp_base %p obj addr %p", rp->rp_type, rp_index, rp->rp_data, obj);

  return obj;
}

void resource_pool_put_obj(struct resource_pool *rp, void *obj)
{
  unsigned rp_index = resource_pool_get_index(rp, obj);
  bitmap_dealloc(rp->rp_free_bitmap, rp_index);

  if (rp->rp_fini)
  {
    rp->rp_fini(obj);
  }

  rp->rp_free_count++;
}