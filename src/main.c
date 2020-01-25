#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <liburing.h>

       #include <sys/types.h>
       #include <sys/stat.h>
       #include <fcntl.h>

#define FILENAME "/home/kwestneat/tmp/test.dat"
#define OP_COUNT 100
#define BUF_SIZE 4096
#define BUF_ALIGN 4096
#define RING_SIZE 32

#define log(message, ...)                             \
{                                                                       \
    fprintf(stderr, message "\n", ##__VA_ARGS__); \
}


int main() {
    int fd, rc;
    void *buf;
    struct io_uring ring;
    struct io_uring_sqe *sqe;
    struct iovec *iov;
    size_t offset = 0;

    fd = open(FILENAME, O_CREAT | O_RDWR, S_IRWXU);
    if (fd < 0) {
        log("error opening " FILENAME ": errno = %d", errno);
        return errno;
    }

    rc = io_uring_queue_init(RING_SIZE, &ring, 0);
    if (rc < 0) {
        log("io_uring_queue_init: rc = %d", rc);

        return rc;
    }

    for (int i = 0; i < OP_COUNT; i++) {
        rc = posix_memalign(&buf, BUF_ALIGN, BUF_SIZE);
        if (rc) {
            log("unable to alloc buffer");
            return -ENOMEM;
        }
        iov = malloc(sizeof(struct iovec));
        if (!iov) {
            log("unable to alloc iov");
            return -ENOMEM;
        }

        sqe = io_uring_get_sqe(&ring);
        if (!sqe)
        {
            log("unable to get sqe, ring full?");
            return -1;
        }

        iov->iov_base = buf;
        iov->iov_len = BUF_SIZE;
        io_uring_prep_writev(sqe, fd, iov, 1, offset);
        log("io_uring_prep_write: buf %p len %llu offset %llu", buf, BUF_SIZE, offset);

        sqe->user_data = i;

        rc = io_uring_submit(&ring);
        log("io_submit: rc = %d", rc);
        if (rc < 0) {
            return rc;
        }

        offset += BUF_SIZE;
    }

    struct io_uring_cqe *cqe;
    int found = 0;
    int slept = 0;
    while (1) {
        rc = io_uring_peek_cqe(&ring, &cqe);
        if (rc == -EAGAIN) {
            if (slept) {
                break;
            }

            sleep(1);
            slept = 1;
            continue;
        } else if (rc < 0) {
            log("io_uring_peek_cqe: rc = %d", rc);
            return rc;
        }

        slept = 0;

        log("io_uring_peek_cqe: data %d res %d", cqe->user_data, cqe->res);
        io_uring_cqe_seen(&ring, cqe);
        found++;
    }

    log("saw %d cqes", found);

    unsigned head = 2;
    unsigned tail = head - 5;

    log("h - t %u", head - tail);

    return 0;
}
