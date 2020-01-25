#ifndef _TASK_H
#define _TASK_H 1

#include <stdio.h>
#include <stdint.h>

enum resource_type
{
  RT_MIN = 0,
  RT_TASK = 0,
  RT_FOO,
  RT_BAR,
  RT_MAX,
};

struct resource_pool
{
  enum resource_type rp_type;
  size_t rp_count;
  unsigned char *rp_free_bitmap;
  uint8_t rp_data[];
};

enum task_state
{
  TS_NEW = 0,
  TS_FILLING,
  TS_SUBMITTED,
};

typedef struct task
{
  enum task_state task_state;
  res_desc_t *task_rd;

  void *task_cb_data;
  void *task_result;

} task_t;

typedef void (*task_cb_t)(task_t *task);

typedef uint8_t foo_t;
typedef uint64_t bar_t;

typedef struct resource_descriptor
{
  size_t rd_count;
  enum resource_type *rd_type_list;
  void **rd_data_list;
} res_desc_t;

void task_new(task_cb_t cb, void *task_cb_data);

void task_rad_new(task_t *task, size_t count);
void task_rad_done(task_t *task);

void task_submit(task_t *task);

#endif