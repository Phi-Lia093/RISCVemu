#ifndef EMU_H
#define EMU_H

#include <stdint.h>

struct machine_state
{
    uint32_t gpr[32];
    uint32_t pc;
    int halted;
};

extern struct machine_state g_state;

#endif