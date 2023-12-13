#ifndef HEXDUMP_H
#define HEXDUMP_H

#include <stddef.h>
#include <assert.h>
#include <stdio.h>

int hexdump(void const *data, size_t length, int linelen, int split);

#endif // HEXDUMP_H