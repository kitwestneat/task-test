#ifndef _MISC_H
#define _MISC_H 1

#include <stdbool.h>

static inline bool is_valid_uring_rq_nr(int nr)
{
  return nr && (!(nr & (nr - 1)));
}

#endif