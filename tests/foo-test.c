#include <assert.h>

#include "log.h"
#include "task.h"
#include "resource.h"

void task0_loop(task_t *task)
{
    static int run_count = 1;
    foo_t *foo = task_rd_get_data(task, 0);

    log("running task0 [%d], got %c", run_count, *foo);

    if (run_count++ < 10)
    {
        // re-submit task
        task_submit(task);
    }
    else
    {
        task_rd_done(task);
    }
}

void task0_fill(task_t *task)
{
    foo_t *foo = task_rd_get_data(task, 0);

    log("filling task0");

    // fill foo request
    *foo = 'a';
    task->task_cb = task0_loop;

    // submit task
    task_submit(task);
}

// foo pusher
void task0_submit(task_t *task)
{
    int rc = task_rd_new(task, 1);
    assert(!rc);

    log("task0_submit");

    task_rd_set_type(task, 0, RT_FOO);
    task->task_cb = task0_fill;

    task_submit(task);
}

void task_create_cb(res_desc_t *desc)
{
    log("tasks created [%p,%p,%p], submitting task0", desc->rd_data_list[0], desc->rd_data_list[1], desc->rd_data_list[2]);

    task0_submit(desc->rd_data_list[0]);
    //task1_submit(desc->rd_data_list[1]);
    //task2_submit(desc->rd_data_list[2]);

    log("running task_start");
    task_start();
}

int main()
{
    resource_pool_init();

    res_desc_t *desc = resource_desc_new(3);
    desc->rd_type_list[0] = desc->rd_type_list[1] = desc->rd_type_list[2] = RT_TASK;
    desc->rd_cb = task_create_cb;

    resource_desc_submit(desc);

    resource_desc_done(desc);

    resource_pool_fini();
}
