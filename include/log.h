#ifndef _LOG_H
#define _LOG_H 1

#include <stdio.h>
#include <assert.h>

#define log(message, ...)                         \
  {                                               \
    fprintf(stderr, message "\n", ##__VA_ARGS__); \
  }

#define ASSERT(val) assert(val)

#endif