#include <assert.h>
#include <errno.h>
#include <unistd.h>

#include "task.h"
#include "log.h"

int tasks_alive = 0;

int task_rd_new(task_t *task, size_t count)
{
    if (task->task_rd)
    {
        return -EINVAL;
    }

    task->task_rd = resource_desc_new(count);
    task->task_rd->rd_cb_data = task;

    return 0;
}

void task_rd_done(task_t *task)
{
    assert(task->task_rd);

    resource_desc_release(task->task_rd);

    resource_desc_done(task->task_rd);
}

void task_rd_set_type(task_t *task, int slot, enum resource_type type)
{
    assert(!task->task_rd->rd_allocated);
    assert(slot < task->task_rd->rd_count);

    task->task_rd->rd_type_list[slot] = type;
}

void *task_rd_get_data(task_t *task, int slot)
{
    assert(task->task_rd->rd_allocated);

    return task->task_rd->rd_data_list[slot];
}

static void task_run_cb(task_t *task)
{
    task->task_cb(task);
    tasks_alive--;
    log("post task_cb %p [alive=%d]", task, tasks_alive);
}

void task_resource_done(struct resource_descriptor *desc)
{
    task_t *task = desc->rd_cb_data;
    task->task_resource_done_count++;

    if (task->task_resource_done_count == desc->rd_count)
    {
        task->task_resource_done_count = 0;
        task_run_cb(task);
    }
}

void task_resource_allocated(struct resource_descriptor *desc)
{
    task_t *task = desc->rd_cb_data;
    task_run_cb(task);
}

void task_submit(task_t *task)
{
    log("task_submit %p [alive=%d]", task, tasks_alive);
    tasks_alive++;

    if (task->task_rd->rd_allocated)
    {
        task->task_rd->rd_cb = task_resource_done;
    }
    else
    {
        task->task_rd->rd_cb = task_resource_allocated;
    }

    resource_desc_submit(task->task_rd);
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