#include <assert.h>

#include "log.h"
#include "task.h"
#include "resource.h"

#define TEST_FOO_COUNT 42

void task0_loop(task_t *task)
{
    static int run_count = 1;

    log("running task0 [%d], got: ", run_count);
    printf(" - ");
    for (int i = 0; i < TEST_FOO_COUNT; i++)
    {
        foo_t *foo = task_rd_get_data(task, i);
        printf("%c ", *foo);
    }
    printf("\n");

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

    log("filling task0");

    // fill foo request
    for (int i = 0; i < TEST_FOO_COUNT; i++)
    {
        foo_t *foo = task_rd_get_data(task, 0);
        *foo = 'A';
    }

    task->task_cb = task0_loop;

    // submit task
    task_submit(task);
}

// foo pusher
void task0_submit(task_t *task)
{
    int rc = task_rd_new(task, TEST_FOO_COUNT);
    assert(!rc);

    log("task0_submit");

    for (int i = 0; i < TEST_FOO_COUNT; i++)
    {
        task_rd_set_type(task, 0, RT_FOO);
    }

    task->task_cb = task0_fill;

    task_submit(task);
}

void task_create_cb(res_desc_t *desc)
{
    log("task created [%p], submitting task0", desc->rd_data_list[0]);

    task0_submit(desc->rd_data_list[0]);

    log("running task_start");
    task_start();
}

int main()
{
    resource_pool_init();

    res_desc_t *desc = resource_desc_new(1);
    desc->rd_type_list[0] = RT_TASK;
    desc->rd_cb = task_create_cb;

    resource_desc_submit(desc);

    resource_desc_done(desc);

    resource_pool_fini();
}
