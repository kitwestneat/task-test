
#include "log.h"
#include "task.h"
#include "resource.h"

void task_create_cb(res_desc_t *desc)
{
    log("tasks created!");

    task0_submit(desc->rd_data_list[0]);
    task1_submit(desc->rd_data_list[1]);
    task2_submit(desc->rd_data_list[2]);

    task_start();
}

void task0_fill(task_t *task)
{
    foo_t *foo = task->task_rd->rd_data_list[0];

    // fill foo request
    *foo = 123;

    // submit task
    task_submit(task);
}

// foo pusher
void task0_submit(task_t *task)
{
    int rc = task_rad_new(task, 1);
    assert(!rc);

    task->task_rd->rd_type_list[0] = RT_FOO;

    task->task_cb = task0_fill;
    task_submit(task);
}

int main()
{
    resource_pool_init();

    res_desc_t *desc = resource_desc_new(3);
    desc->rd_type_list[0] = desc->rd_type_list[1] = desc->rd_type_list[2] = RT_TASK;
    desc->rd_cb = task_create_cb;

    resource_desc_submit(desc);

    resource_desc_done(desc);
}
