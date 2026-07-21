#ifndef EMU_H
#define EMU_H

#include <stdint.h>

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

// mmu flags
// bit 0: reserve (in "A" extension)

struct machine_state
{
    uint32_t gpr[32];
    uint32_t pc;
    uint8_t *main_memory;
    uint32_t *mmu_flags;
    int single_step;
    int terminated;
    uint32_t breakpoint;
    uint32_t breakpoint_enabled;
};

extern struct machine_state g_state;

#endif
