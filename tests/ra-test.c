#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include "resource.h"
#include "log.h"

void allocate_too_many_tasks();
void allocate_one_task(struct resource_descriptor *desc);
void allocate_one_of_each(struct resource_descriptor *desc);
void finished(struct resource_descriptor *desc);

int main()
{
  resource_pool_init();

  allocate_too_many_tasks();
}

void allocate_too_many_tasks()
{
  log("allocate_too_many_tasks");
  res_desc_t *desc = res_desc_new(TASK_COUNT + 1);
  for (int i = 0; i < TASK_COUNT + 1; i++)
  {
    desc->rd_type_list[i] = RT_TASK;
  }
  desc->rd_cb = allocate_one_task;

  int rc = resource_submit(desc);
  assert(rc == -E2BIG);

  allocate_one_task(desc);
}

void allocate_one_task(struct resource_descriptor *desc)
{
  log("allocate_one_task");
  res_desc_done(desc);

  desc = res_desc_new(1);
  desc->rd_type_list[0] = RT_TASK;
  desc->rd_cb = allocate_one_of_each;

  int rc = resource_submit(desc);
  assert(rc == 0);
}

void allocate_one_of_each(struct resource_descriptor *desc)
{
  log("allocate_one_of_each");
  res_desc_done(desc);

  desc = res_desc_new(3);
  desc->rd_type_list[0] = RT_TASK;
  desc->rd_type_list[1] = RT_FOO;
  desc->rd_type_list[2] = RT_BAR;
  desc->rd_cb = finished;

  int rc = resource_submit(desc);
  assert(rc == 0);
}

void finished(struct resource_descriptor *desc)
{
  res_desc_done(desc);
  log("done!");
}