#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "task.h"
#include "foobar.h"
#include "resource.h"
#include "tcp.h"

struct resource_pool *task_pool;
struct resource_pool *tcp_pool;

unsigned char *bitmap_new(size_t count)
{
  return calloc((count + 7) / 8, 1);
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

  for (i = 0; i < (bitmap_size + 7) / 8; i++)
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
  case RT_TCP:
    obj_size = sizeof(tcp_rq_t);
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

  assert(obj >= rp_obj_base);

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

  switch (type)
  {
  case RT_TASK:
    rp->rp_submit = NULL;
    rp->rp_poll = NULL;
    break;
  case RT_TCP:
    rp->rp_submit = (resource_submit_fn_t)tcp_rq_submit;
    rp->rp_poll = tcp_poll;
    break;
  default:
    log("unknown resource type %d", type);
    assert(false);
  }

  return rp;
}

void resource_pool_done(struct resource_pool *rp)
{
  free(rp->rp_free_bitmap);
  free(rp);
}

struct resource_pool *resource_pool_get_by_type(enum resource_type type)
{
  struct resource_pool *rp;

  switch (type)
  {
  case RT_TASK:
    rp = task_pool;
    break;
  case RT_TCP:
    rp = tcp_pool;
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
  task_pool = resource_pool_new(RT_TASK, TASK_COUNT);
  tcp_pool = resource_pool_new(RT_TCP, TCP_COUNT);

  log("resource pools created: %p %p", task_pool, tcp_pool);
}

void resource_pool_fini()
{
  resource_pool_done(task_pool);
  resource_pool_done(tcp_pool);
}

void *resource_pool_alloc_obj(struct resource_pool *rp)
{

  int rp_index = bitmap_alloc(rp->rp_free_bitmap, rp->rp_count);
  assert(rp_index != -1);

  rp->rp_free_count--;

  size_t obj_size = resource_pool_get_obj_size(rp->rp_type);
  void *obj = rp->rp_data + rp_index * obj_size;
  memset(obj, 0, obj_size);

  log("allocating resource type %d, index %d, rp_base %p obj addr %p", rp->rp_type, rp_index, rp->rp_data, obj);

  return obj;
}

void resource_pool_put_obj(struct resource_pool *rp, void *obj)
{
  unsigned rp_index = resource_pool_get_index(rp, obj);
  bitmap_dealloc(rp->rp_free_bitmap, rp_index);
  rp->rp_free_count++;
}