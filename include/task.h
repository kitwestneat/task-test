#ifndef _TASK_H
#define _TASK_H 1

#include <stdint.h>

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

  task_cb_t task_cb;
  void *task_cb_data;
  void *task_result;

} task_t;

typedef uint8_t foo_t;
typedef uint64_t bar_t;

void task_new(task_cb_t cb, void *task_cb_data);

int task_rad_new(task_t *task, size_t count);
void task_rad_done(task_t *task);

void task_submit(task_t *task);

#endif