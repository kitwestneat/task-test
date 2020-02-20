#ifndef _BITMAP_H
#define _BITMAP_H 1

#include <unistd.h>
#include <stdlib.h>

static inline unsigned char *bitmap_new(size_t count)
{
    return (unsigned char *)calloc((count + 7) / 8, 1);
}

static inline int bitmap_get(unsigned char byte, unsigned bit)
{
    return byte & (1 << bit);
}

static inline void bitmap_set(unsigned char *byte, unsigned bit)
{
    *byte = *byte | (1 << bit);
}

static inline void bitmap_clr(unsigned char *byte, unsigned bit)
{
    *byte = *byte & ~(1 << bit);
}

static inline void bitmap_dealloc(unsigned char *bitmap, unsigned bitmap_index)
{
    int i = bitmap_index / 8;
    bitmap_clr(&bitmap[i], bitmap_index % 8);
}

static inline int bitmap_alloc(unsigned char *bitmap, size_t bitmap_size)
{
    int i;
    int j;
    unsigned char byte;

    for (i = 0; i < (bitmap_size + 7) / 8; i++)
    {
        if (bitmap[i] != 0xff)
        {
            goto found;
        }
    }

    return -1;

found:
    byte = bitmap[i];

    for (j = 0; j < 8; j++)
    {
        if (bitmap_get(byte, j) == 0)
        {
            bitmap_set(&bitmap[i], j);

            return i * 8 + j;
        }
    }

    return -1;
}

#endif