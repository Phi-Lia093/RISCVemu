#ifndef EMU_H
#define EMU_H

#include <stdint.h>

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
