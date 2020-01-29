#include "task.h"
#include "resource.h"

void task_create_cb(res_desc_t *desc)
{
    log("tasks created!");
}

void task0_fill(task_t *task)
{
}

// foo pusher
void task0_init(task_t *task)
{
    int rc = task_rad_new(task, 1);
    assert(!rc);

    task->task_rd->rd_type_list[0] = RT_FOO;

    task->task_cb = task0_fill;
    task_submit(task);
}

int main()
{
    resource_pool_init();

    res_desc_t *desc = res_desc_new(6);
    desc->rd_type_list[0] = desc->rd_type_list[1] = desc->rd_type_list[2] = RT_TASK;
    desc->rd_type_list[3] = desc->rd_type_list[4] = desc->rd_type_list[5] = RT_TASK;
    desc->rd_cb = task_create_cb;

    resource_submit(desc);

    res_desc_done(desc);
}
