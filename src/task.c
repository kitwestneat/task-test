#include <assert.h>
#include <errno.h>
#include <unistd.h>

#include "task.h"

int tasks_alive = 0;

int task_rd_new(task_t *task, size_t count)
{
    if (task->task_rd)
    {
        return -EINVAL;
    }

    task->task_rd = resource_desc_new(count);

    return 0;
}

void task_rd_done(task_t *task)
{
    assert(task->task_rd);

    resource_desc_release_resources(task->task_rd);

    resource_desc_done(task->task_rd);
}

void task_resource_done(struct resource_descriptor *desc)
{
    task_t *task = desc->rd_cb_data;
    task->task_resource_done_count++;

    if (task->task_resource_done_count == desc->rd_count)
    {
        task->task_cb(task);
        tasks_alive--;
    }
}

void task_submit(task_t *task)
{
    tasks_alive++;

    task->task_rd->rd_cb = task_resource_done;
    resource_desc_children_submit(task->task_rd);
}

static void task_sleep()
{
    usleep(200);
}

void task_start()
{
    while (tasks_alive)
    {
        if (!resource_poll())
        {
            task_sleep();
        }
    }
}