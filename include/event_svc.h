#ifndef _EVENT_SVC_H
#define _EVENT_SVC_H 1

typedef int (*es_poll_fn_t)();

void event_svc_add(int fd, es_poll_fn_t cb);
void event_svc_del(int fd);
void event_svc_init();
void event_svc_wait();

#endif