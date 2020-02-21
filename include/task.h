#ifndef _TASK_H
#define _TASK_H 1

#include <stdint.h>
#include <stddef.h>

#include "resource.h"

enum task_state
{
  TS_NEW = 0,
  TS_FILLING,
  TS_SUBMITTED,
};

struct task;
typedef void (*task_cb_t)(struct task *task);

typedef struct task
{
  enum task_state task_state;
  res_desc_t *task_rd;
  res_desc_t *task_parent_desc;
  size_t task_resource_done_count;

  task_cb_t task_cb;
  void *task_cb_data;
  void *task_result;
} task_t;

void task_new(task_cb_t cb, void *task_cb_data);

int task_rd_new(task_t *task, size_t count);
void task_rd_done(task_t *task);
void task_rd_release(task_t *task);
void task_rd_set_type(task_t *task, int slot, enum resource_type type);
void *task_rd_get_data(task_t *task, int slot);

void task_submit(task_t *task, task_cb_t next);

void task_init(size_t count, desc_cb_t cb);
void task_get_one(desc_cb_t cb, void *cb_data);
void task_loop_watch(int *tasks_alive_ptr);
void task_loop();

#endif