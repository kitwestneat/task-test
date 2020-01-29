#include <assert.h>
#include <errno.h>

#include "task.h"

int task_rd_new(task_t *task, size_t count)
{
    if (task->task_rd)
    {
        return -EINVAL;
    }

    task->task_rd = res_desc_new(count);

    return 0;
}

void task_rd_done(task_t *task)
{
    assert(task->task_rd);

    resource_release(task->task_rd);

    res_desc_done(task->task_rd);
}
