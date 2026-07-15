#include <stdint.h>

#include <emu.h>
#include <exec.h>
#include <logger.h>

uint32_t
reg_read(uint32_t id)
{
    if (id == 0) return 0;
    if (id > 31)
    {
        error("register index out of bound READ");
    }
    return g_state.gpr[id];
}

void
reg_write(uint32_t id, uint32_t d)
{
    if (id == 0)
    {
        warn("access to ZERO register WRITE");
    }
    if (id > 31)
    {
        error("register index out of bound WRITE");
    }
    g_state.gpr[id] = d;
}

static void
r_ins(uint32_t ins)
{
}

void
exec(uint32_t ins)
{
    uint32_t opcode = get_opcode(ins);
    if ((opcode & 3) != 3)
    {
        // C extension
        error("unsupported COMPACT extension");
    }
    switch (opcode)
    {
    case 0x33:
    { // R ins
        r_ins(ins);
        break;
    }
    }
}