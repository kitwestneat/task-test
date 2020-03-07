#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "event_svc.h"
#include "disk.h"
#include "log.h"
#include "misc.h"

#define URING_ENTRIES 32

LIST_HEAD(disk_head_t, disk)
disk_list_head = LIST_HEAD_INITIALIZER(disk_list_head);

struct io_uring ring;

int disk_init()
{
    log("disk_init");

    LIST_INIT(&disk_list_head);
    io_uring_queue_init(URING_ENTRIES, &ring, 0);

    return 0;
}

int disk_open(const char *fn, disk_t **disk)
{
    disk_t *disk_ptr;
    if (!disk)
    {
        log("disk_ptr null");
        disk = &disk_ptr;
    }

    log("disk_open: opening %s (%p)", fn, disk);
    int fd = open(fn, DISK_OPEN_FLAGS);
    if (fd < 0)
    {
        log("disk_open: error opening %s, %m (%d)", fn, errno);
        return -errno;
    }

    event_svc_add(fd, disk_poll);

    *disk = malloc(sizeof(disk_t));

    (*disk)->disk_fd = fd;

    LIST_INSERT_HEAD(&disk_list_head, (*disk), disk_list_entry);

    return 0;
}

void disk_close(disk_t *disk)
{
    LIST_REMOVE(disk, disk_list_entry);
    close(disk->disk_fd);
    free(disk);
}

int disk_poll()
{
    struct io_uring_cqe *cqe;
    int rc = io_uring_peek_cqe(&ring, &cqe);
    if (rc == 0)
    {
        disk_rq_t *drq = io_uring_cqe_get_data(cqe);
        drq->drq_res = cqe->res;
        io_uring_cqe_seen(&ring, cqe);

        log("disk cqe seen, calling cb");
        drq->drq_cb(drq);

        return 1;
    }

    return 0;
}

void disk_rq_submit(disk_rq_t *rq)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    switch (rq->drq_type)
    {
    case DRQ_WRITE:
        log("rq_submit: write, fd: %d, first iov size: %x offset: %x", rq->drq_disk->disk_fd, rq->drq_iov[0].iov_len, rq->drq_offset);
        io_uring_prep_writev(sqe, rq->drq_disk->disk_fd, rq->drq_iov, rq->drq_iov_count, rq->drq_offset);
        break;
    case DRQ_READ:
        log("rq_submit: read, fd: %d, first iov size: %x offset: %x", rq->drq_disk->disk_fd, rq->drq_iov[0].iov_len, rq->drq_offset);
        io_uring_prep_readv(sqe, rq->drq_disk->disk_fd, rq->drq_iov, rq->drq_iov_count, rq->drq_offset);
        break;
    }

    io_uring_sqe_set_data(sqe, rq);

    io_uring_submit(&ring);
}

void disk_rq_init(disk_rq_t *rq, enum drq_type type, disk_t *disk, size_t iov_count)
{
    rq->drq_disk = disk;
    rq->drq_iov_count = iov_count;
    rq->drq_iov = calloc(iov_count, sizeof(struct iovec));

    rq->drq_type = type;
}