#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stdio.h>
#define UART_OUT 0x10000000

static inline void
uart_putc(uint32_t c)
{
    putc(c & 0xff, stdout);
}

#endif
