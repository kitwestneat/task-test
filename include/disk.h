#ifndef _DISK_H
#define _DISK_H 1

#include <liburing.h>
#include <sys/queue.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define DISK_OPEN_FLAGS O_RDWR | O_EXCL

typedef struct disk
{
  int disk_fd;
  LIST_ENTRY(disk)
  disk_list_entry;
} disk_t;

enum drq_type
{
  DRQ_READ = 0,
  DRQ_WRITE,
};

struct disk_rq;
typedef void (*drq_cb_fn_t)(struct disk_rq *rq);

typedef struct disk_rq
{
  enum drq_type drq_type;
  struct disk *drq_disk;
  int drq_res;

  drq_cb_fn_t drq_cb;
  void *drq_cb_data;

  off_t drq_offset;
  size_t drq_iov_count;
  struct iovec *drq_iov;
} disk_rq_t;

int disk_open(const char *fn, disk_t **disk);
void disk_close(disk_t *disk);
int disk_init();
void disk_rq_submit(disk_rq_t *rq);
int disk_poll();

#endif