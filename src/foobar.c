#define _POSIX_C_SOURCE 199309L
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>
#include "foobar.h"
#include "resource.h"
#include "log.h"

struct foobar_queue *foo_queue_head = NULL;
struct foobar_queue *bar_queue_head = NULL;

#define MS_TO_NS 1000000
#define S_TO_NS 10000000000

void deadline_add(struct timespec *ts, long millisecs)
{
  ts->tv_nsec += millisecs * MS_TO_NS;

  while (ts->tv_nsec >= S_TO_NS)
  {
    ts->tv_sec++;
    ts->tv_nsec -= S_TO_NS;
  }

  while (ts->tv_nsec < 0)
  {
    ts->tv_sec--;
    ts->tv_nsec += S_TO_NS;
  }
}

bool deadline_passed(struct timespec *ts)
{
  struct timespec now;

  clock_gettime(CLOCK_MONOTONIC, &now);

  return now.tv_sec > ts->tv_sec || (now.tv_sec == ts->tv_sec && now.tv_nsec >= ts->tv_nsec);
}

static void foobar_queue_push_resource(struct foobar_queue **head_ptr, long wait_for, void *resource, res_desc_t *desc)
{
  struct foobar_queue *new_resc = malloc(sizeof(struct foobar_queue));

  ASSERT(resource);
  ASSERT(desc);

  new_resc->fbq_resource = resource;
  new_resc->fbq_desc = desc;

  clock_gettime(CLOCK_MONOTONIC, &new_resc->fbq_deadline);
  deadline_add(&new_resc->fbq_deadline, wait_for);

  new_resc->fbq_next = NULL;

  if (*head_ptr)
  {
    (*head_ptr)->fbq_next = new_resc;
  }
  else
  {
    *head_ptr = new_resc;
  }
}

static struct foobar_queue *foobar_queue_poll(struct foobar_queue **head_ptr)
{
  struct foobar_queue *node = *head_ptr;

  if (!node)
  {
    return NULL;
  }

  bool node_ready = deadline_passed(&node->fbq_deadline);

  if (!node_ready)
  {
    log("foobar not ready %p", node);

    return NULL;
  }

  *head_ptr = node->fbq_next;

  return node;
}

void foo_submit(void *resource, res_desc_t *desc)
{
  foobar_queue_push_resource(&foo_queue_head, FOO_LATENCY, resource, desc);
}

int foo_poll()
{
  struct foobar_queue *node = foobar_queue_poll(&foo_queue_head);
  if (!node)
  {
    return 0;
  }

  ASSERT(node->fbq_resource);

  foo_t *foo_ptr = node->fbq_resource;

  // do the fake work
  *foo_ptr += FOO_VAL;

  struct resource_descriptor *desc = node->fbq_desc;
  free(node);
  desc->rd_cb(desc);

  return 1;
}

void bar_submit(void *resource, res_desc_t *desc)
{
  foobar_queue_push_resource(&bar_queue_head, BAR_LATENCY, resource, desc);
}

int bar_poll()
{
  struct foobar_queue *node = foobar_queue_poll(&bar_queue_head);
  if (!node)
  {
    return 0;
  }

  // do the fake work
  *((bar_t *)node->fbq_resource) += BAR_VAL;

  struct resource_descriptor *desc = node->fbq_desc;
  free(node);
  desc->rd_cb(desc);

  return 1;
}
