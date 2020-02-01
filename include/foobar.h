#ifndef _FOOBAR_H
#define _FOOBAR_H 1

#include "resource.h"

// XXX should 2nd arg just be a callback pointer?
void foo_submit(void *resource, res_desc_t *desc);
int foo_poll();
void bar_submit(void *resource, res_desc_t *desc);
int bar_poll();

#endif
