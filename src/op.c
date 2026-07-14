#include <stdint.h>

#include <emu.h>
#include <op.h>

static uint32_t
reg_read(uint32_t id)
{
    if (id == 0) return 0;
    if (id > 31)
        ; // ERR
    return g_state.gpr[id];
}

static void
reg_write(uint32_t id, uint32_t d)
{
    if (id == 0) return;
    if (id > 31)
        ; // ERR
    g_state.gpr[id] = d;
}

void
exec(uint32_t ins)
{
}