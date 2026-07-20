#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <logger.h>
#include <mem.h>

uint8_t *g_main_mem;

void
init_mem(void)
{
    g_main_mem = (uint8_t *)calloc(1, MEM_SIZE);
    // activate page
    memset(g_main_mem, 0x5A, MEM_SIZE);
    memset(g_main_mem, 0, MEM_SIZE);
}
