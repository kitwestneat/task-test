#include <assert.h>
#include <stdlib.h>

#include "log.h"
#include "task.h"
#include "resource.h"

void child_fill(task_t *task);
void task0_fill(task_t *task);

#define CHILD_TASK_COUNT 3

struct child_group
{
    task_t *cg_task;
    int cg_children_done_count;
};

struct child_task
{
    struct child_group *ct_group;
    foo_t ct_foo;
    int ct_actions_remaining_count;
};

void child_group_fini(struct child_group *cg)
{
    task_rd_done(cg->cg_task);
    free(cg);
}

void child_done(task_t *task)
{
    struct child_task *ct = task->task_cb_data;
    foo_t *foo = task_rd_get_data(task, 0);

    ct->ct_foo = *foo;
    ct->ct_actions_remaining_count--;

    log("child_done: %p foo %d remaining %d", task, *foo, ct->ct_actions_remaining_count);

    if (ct->ct_actions_remaining_count == 0)
    {
        struct child_group *cg = ct->ct_group;
        cg->cg_children_done_count++;

        free(ct);
        task_rd_done(task);

        if (cg->cg_children_done_count == CHILD_TASK_COUNT)
        {
            child_group_fini(cg);
        }
    }
    else
    {
        task_submit(task, child_fill);
    }
}

void child_fill(task_t *task)
{
    foo_t *foo = task_rd_get_data(task, 0);
    struct child_task *ct = task->task_cb_data;

    // fill foo request
    *foo = ct->ct_foo;

    // submit task
    task_submit(task, child_done);
}

void task0_spawn(task_t *task)
{
    struct child_group *cg = calloc(1, sizeof(struct child_group));
    cg->cg_task = task;

    for (int i = 0; i < CHILD_TASK_COUNT; i++)
    {
        task_t *child = task_rd_get_data(task, i);
        int rc = task_rd_new(child, 1);
        assert(!rc);

        struct child_task *ct = calloc(1, sizeof(struct child_task));
        ct->ct_actions_remaining_count = i + 1;
        ct->ct_group = cg;
        ct->ct_foo = 'A' + i * 5;

        child->task_cb_data = ct;

        log("task0_spawn: submitting child %p", child);

        task_rd_set_type(child, 0, RT_FOO);
        task_submit(child, child_fill);
    }

    /* XXX
    Tasks that spawn other tasks shouldn't submit because the task resources will be released

    bar_t *bar = task_rd_get_data(task, CHILD_TASK_COUNT);

    *bar = 123;

    // submit task
    task_submit(task, task0_done);
    */
}

void task0_submit(task_t *task)
{
    int rc = task_rd_new(task, CHILD_TASK_COUNT);
    assert(!rc);

    for (int i = 0; i < CHILD_TASK_COUNT; i++)
    {
        task_rd_set_type(task, i, RT_TASK);
    }

    task_submit(task, task0_spawn);
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
