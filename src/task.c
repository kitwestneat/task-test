#include <errno.h>
#include <unistd.h>

#include "task.h"
#include "log.h"
#include "event_svc.h"

int tasks_alive = 0;

int task_rd_new(task_t *task, size_t count)
{
    task_rd_set(task, resource_desc_new(count));

    return 0;
}

void task_rd_set(task_t *task, res_desc_t *desc)
{
    ASSERT(!task->task_rd);

    task->task_rd = desc;
    task->task_rd->rd_cb_data = task;
}

void task_rd_release(task_t *task)
{
    ASSERT(task->task_rd);

    resource_desc_release(task->task_rd);
}

void task_rd_done(task_t *task)
{
    task_rd_release(task);
    resource_desc_done(task->task_rd);
    task->task_rd = NULL;
}

void task_rd_set_type(task_t *task, int slot, enum resource_type type)
{
    ASSERT(!task->task_rd->rd_allocated);
    ASSERT(slot < task->task_rd->rd_count);

    task->task_rd->rd_type_list[slot] = type;
}

void *task_rd_get_data(task_t *task, int slot)
{
    ASSERT(task->task_rd->rd_allocated);
    ASSERT(slot < task->task_rd->rd_count);

    return task->task_rd->rd_data_list[slot];
}

static void task_run_cb(task_t *task)
{
    assert(task->task_cb);
    task->task_cb(task);
    tasks_alive--;
    log("task_run_cb %p: after cb [alive=%d]", task, tasks_alive);
}

void task_resource_done(struct resource_descriptor *desc)
{
    task_t *task = desc->rd_cb_data;
    task->task_resource_done_count++;

    log("task_resource_done %p: done %zu total %zu | %s", task, task->task_resource_done_count, desc->rd_count,
        (task->task_resource_done_count == desc->rd_count) ? "running cb" : "waiting");

    if (task->task_resource_done_count == desc->rd_count)
    {
        task->task_resource_done_count = 0;
        task_run_cb(task);
    }
}

void task_resource_allocated(struct resource_descriptor *desc)
{
    task_t *task = desc->rd_cb_data;
    log("task_resource_allocated %p: running cb", task);
    task_run_cb(task);
}

static void task_rd_submit(task_t *task)
{
    ASSERT(task->task_rd);
    if (task->task_state == TS_FILLING)
    {
        ASSERT(task->task_rd->rd_allocated);
        task->task_rd->rd_cb = task_resource_done;
        task->task_state = TS_SUBMITTED;
        log("task_rd_submit %p -> SUBMITTED", task);
    }
    else
    {
        if (task->task_rd->rd_allocated)
        {
            log("task_rd_submit %p releasing resources", task);
            resource_desc_release(task->task_rd);
        }

        task->task_rd->rd_cb = task_resource_allocated;
        task->task_state = TS_FILLING;
        log("task_rd_submit %p -> FILLING", task);
    }

    resource_desc_submit(task->task_rd);
}

void task_submit(task_t *task, task_cb_t next)
{
    log("task_submit %p [alive=%d]", task, tasks_alive);

    tasks_alive++;

    task->task_cb = next;

    if (task->task_rd)
    {
        task_rd_submit(task);
    }
    else
    {
        log("Not sure what to do with task with no resources");
        ASSERT(false);
    }
}

static void task_sleep()
{
    static uint8_t i = 0;

    i++;
    if (i & 7)
    {
        usleep(200);
    }
    else
    {
        event_svc_wait();
    }
}

void task_loop_watch(int *tasks_alive_ptr)
{
    while (*tasks_alive_ptr)
    {
        if (!resource_poll())
        {
            task_sleep();
        }
    }
}

void task_loop()
{
    task_loop_watch(&tasks_alive);
}

void task_get_one(desc_cb_t cb, void *cb_data)
{
    resource_get_one(RT_TASK, cb, cb_data);
}

void task_init(size_t count, desc_cb_t cb)
{
    resource_pool_init();

    res_desc_t *desc = resource_desc_new(count);

    for (int i = 0; i < count; i++)
    {
        desc->rd_type_list[i] = RT_TASK;
    }
    desc->rd_cb = cb;

    resource_desc_submit(desc);

    resource_poll();

    log("running task_loop");
    task_loop();

    resource_desc_done(desc);

    resource_pool_fini();
}