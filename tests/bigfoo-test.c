#include <assert.h>
#include <stdlib.h>

#include "log.h"
#include "task.h"
#include "resource.h"

#define BIGFOO_COUNT 42

void task0_fill(task_t *task);

void task0_done(task_t *task)
{
    static int run_count = 1;
    foo_t *data = task->task_cb_data;

    for (int i = 0; i < BIGFOO_COUNT; i++)
    {
        foo_t *foo = task_rd_get_data(task, i);
        data[i] = ((*foo - 'A') % 26) + 'A';
    }

    log("running task0 [%d], got %.*s", run_count, BIGFOO_COUNT, data);

    if (run_count++ < 10)
    {
        // re-submit task
        task_submit(task, task0_fill);
    }
    else
    {
        task_rd_done(task);
        free(data);
    }
}

void task0_fill(task_t *task)
{

    log("filling task0");

    foo_t *data = task->task_cb_data;

    // fill foo request
    for (int i = 0; i < BIGFOO_COUNT; i++)
    {
        foo_t *foo = task_rd_get_data(task, i);
        *foo = data[i];
    }

    // submit task
    task_submit(task, task0_done);
}

// foo pusher
void task0_submit(task_t *task)
{
    int rc = task_rd_new(task, BIGFOO_COUNT);
    assert(!rc);

    task->task_cb_data = malloc(sizeof(foo_t) * BIGFOO_COUNT);

    foo_t *data = task->task_cb_data;

    for (int i = 0; i < BIGFOO_COUNT; i++)
    {
        data[i] = 'A' + (i % 26);
        task_rd_set_type(task, i, RT_FOO);
    }

    log("task0_submit, data %.*s", BIGFOO_COUNT, data);
    task_submit(task, task0_fill);
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

    resource_poll();

    log("running task_start");
    task_start();

    resource_desc_done(desc);

    resource_pool_fini();
}
