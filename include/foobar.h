#ifndef _FOOBAR_H
#define _FOOBAR_H 1

#include <time.h>
#include "resource.h"

#define FOO_LATENCY 100
#define BAR_LATENCY 250

#define FOO_VAL 1
#define BAR_VAL -2

struct foobar_queue
{
  void *fbq_resource;
  res_desc_t *fbq_desc;
  struct timespec fbq_deadline;

  struct foobar_queue *fbq_next;
};

// XXX should 2nd arg just be a callback pointer?
void foo_submit(void *resource, res_desc_t *desc);
int foo_poll();
void bar_submit(void *resource, res_desc_t *desc);
int bar_poll();

#endif
