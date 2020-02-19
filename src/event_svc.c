#include <errno.h>
#include <sys/epoll.h>

#include "event_svc.h"
#include "log.h"

int event_svc_fd = 0;

void event_svc_add(int fd, es_poll_fn_t cb)
{
    log("event_svc_add: adding %d, cb %p", fd, cb);
    struct epoll_event ev = {
        .events = EPOLLIN | EPOLLOUT | EPOLLET,
        .data.ptr = cb,
    };

    epoll_ctl(event_svc_fd, EPOLL_CTL_ADD, fd, &ev);
}

void event_svc_del(int fd)
{
    epoll_ctl(event_svc_fd, EPOLL_CTL_DEL, fd, NULL);
}

void event_svc_init()
{
    event_svc_fd = epoll_create1(0);
    if (event_svc_fd == -1)
    {
        log("epoll_create1: %d, %m", errno);

        return;
    }
}

void event_svc_wait()
{
    struct epoll_event ev = {0};

    int rc = epoll_wait(event_svc_fd, &ev, 1, -1);
    if (rc < 0)
    {
        log("epoll_wait: error %d %m", errno);
    }

    es_poll_fn_t cb = ev.data.ptr;
    log("event_svc_wait: epoll wait rc = %d, ev.events %d cb %p", rc, ev.events, cb);
    if (cb)
    {
        cb();
    }
}