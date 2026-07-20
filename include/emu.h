#ifndef EMU_H
#define EMU_H

#include <stdint.h>

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

struct machine_state
{
    uint32_t gpr[32];
    uint32_t pc;
    int single_step;
    int terminated;
    uint32_t breakpoint;
    uint32_t breakpoint_enabled;
};

extern struct machine_state g_state;

#endif
