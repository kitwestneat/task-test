#include <assert.h>

#include "log.h"
#include "task.h"
#include "resource.h"

void task0_fill(task_t *task);

void task0_done(task_t *task)
{
    static int run_count = 1;
    foo_t *foo = task_rd_get_data(task, 0);

    log("running task0 [%d], got %d", run_count, *foo);

    task->task_cb_data = (void *)(uintptr_t)*foo;

    if (run_count++ < 10)
    {
        // re-submit task
        task_submit(task, task0_fill);
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
    *foo = (foo_t)(uintptr_t)task->task_cb_data;

    // submit task
    task_submit(task, task0_done);
}

// foo pusher
void task0_submit(task_t *task)
{
    int rc = task_rd_new(task, 1);
    assert(!rc);

    task->task_cb_data = (void *)(uintptr_t)'A';

    log("task0_submit");

    task_rd_set_type(task, 0, RT_FOO);

    task_submit(task, task0_fill);
}

void task_create_cb(res_desc_t *desc)
{
    log("task created [%p], submitting task0", desc->rd_data_list[0]);

    task0_submit(desc->rd_data_list[0]);
}

int main()
{
    task_init(1, task_create_cb);
}
