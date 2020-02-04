#include <assert.h>
#include <stdlib.h>

#include "log.h"
#include "task.h"
#include "resource.h"

void task0_fill(task_t *task);
void task1_fill(task_t *task);
void task2_fill(task_t *task);

struct foobar_data
{
    foo_t foo;
    bar_t bar;
};

void task0_done(task_t *task)
{
    static int run_count = 1;
    foo_t *foo = task_rd_get_data(task, 0);

    log("running task0(%p) [%d], got %d", task, run_count, *foo);

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

    log("filling task0(%p)", task);

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

    log("task0_submit(%p)", task);

    task_rd_set_type(task, 0, RT_FOO);

    task_submit(task, task0_fill);
}

void task1_done(task_t *task)
{
    static int run_count = 1;
    bar_t *bar = task_rd_get_data(task, 0);
    bar_t *bar1 = task_rd_get_data(task, 1);

    log("running task1(%p) [%d], got %d", task, run_count, *bar, *bar1);

    if (run_count++ < 10)
    {
        // re-submit task
        task_submit(task, task1_fill);
    }
    else
    {
        task_rd_done(task);
    }
}

void task1_fill(task_t *task)
{
    bar_t *bar = task_rd_get_data(task, 0);
    bar_t *bar1 = task_rd_get_data(task, 1);

    log("filling task1(%p)", task);

    // fill foo request
    *bar = 654;
    *bar1 = 321;

    // submit task
    task_submit(task, task1_done);
}

// bar pusher
void task1_submit(task_t *task)
{
    int rc = task_rd_new(task, 2);
    assert(!rc);

    log("task1_submit(%p)", task);

    task_rd_set_type(task, 0, RT_BAR);
    task_rd_set_type(task, 1, RT_BAR);

    task_submit(task, task1_fill);
}

void task2_done(task_t *task)
{
    static int run_count = 1;
    foo_t *foo = task_rd_get_data(task, 0);
    bar_t *bar = task_rd_get_data(task, 1);

    log("running task2(%p) [%d], got %c %d", task, run_count, *foo, *bar);

    struct foobar_data *fbd = task->task_cb_data;
    fbd->foo = *foo;
    fbd->bar = *bar;

    if (run_count++ < 10)
    {
        // re-submit task
        task_submit(task, task2_fill);
    }
    else
    {
        free(fbd);
        task_rd_done(task);
    }
}

void task2_fill(task_t *task)
{
    foo_t *foo = task_rd_get_data(task, 0);
    bar_t *bar = task_rd_get_data(task, 1);

    log("filling task2(%p)", task);
    struct foobar_data *fbd = task->task_cb_data;
    *foo = fbd->foo;
    *bar = fbd->bar;

    // submit task
    task_submit(task, task2_done);
}

// bar pusher
void task2_submit(task_t *task)
{
    int rc = task_rd_new(task, 2);
    assert(!rc);

    log("task2_submit(%p)", task);

    struct foobar_data *fbd = malloc(sizeof(struct foobar_data));
    fbd->foo = 'a';
    fbd->bar = 321;

    task->task_cb_data = fbd;

    task_rd_set_type(task, 0, RT_FOO);
    task_rd_set_type(task, 1, RT_BAR);

    task_submit(task, task2_fill);
}

void task_create_cb(res_desc_t *desc)
{
    log("tasks created [%p,%p,%p], submitting", desc->rd_data_list[0], desc->rd_data_list[1], desc->rd_data_list[2]);

    task0_submit(desc->rd_data_list[0]);
    task1_submit(desc->rd_data_list[1]);
    task2_submit(desc->rd_data_list[2]);
}

int main()
{
    task_init(3, task_create_cb);
}
