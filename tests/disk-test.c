#include <stdlib.h>
#include <unistd.h>

#include "disk.h"
#include "log.h"

int alive = 1;

void on_read(disk_rq_t *rq)
{
    char *buf = rq->drq_iov[0].iov_base;

    log("on_read: got (res=%d) %s", rq->drq_res, buf);

    alive = 0;
}

void submit_drq(disk_t *disk)
{
    disk_rq_t *rq = malloc(sizeof(disk_rq_t));
    char *buf = malloc(128);

    rq->drq_disk = disk;
    rq->drq_offset = 0;
    rq->drq_res = 0;
    rq->drq_type = DRQ_READ;
    rq->drq_iov_count = 1;
    rq->drq_iov = malloc(sizeof(struct iovec));
    rq->drq_iov[0].iov_base = buf;
    rq->drq_iov[0].iov_len = 128;

    rq->drq_cb = on_read;

    disk_rq_submit(rq);
}

int main()
{
    disk_t *disk;

    disk_init();

    disk_open("./tests/disk-test.txt", &disk);

    submit_drq(disk);

    while (alive)
    {
        if (disk_poll() == 0)
            usleep(100);
    }

    disk_close(disk);

    return 0;
}
