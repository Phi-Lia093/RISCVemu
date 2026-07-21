#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <logger.h>
#include <mem.h>

void
init_mem(void)
{
    g_state.main_memory = (uint8_t *)calloc(sizeof(uint8_t), MEM_SIZE);
    g_state.mmu_flags = (uint32_t *)calloc(sizeof(uint32_t), MEM_SIZE);
}
