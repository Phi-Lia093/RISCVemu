#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>

#include <debugger.h>
#include <emu.h>
#include <exec.h>
#include <fmap.h>
#include <logger.h>
#include <mem.h>

#define PROGRAM_BASE 0
#define PROGRAM_NAME "test.bin"

struct machine_state g_state;

void
load_program(void)
{
    size_t len;
    uint8_t *addr
        = (uint8_t *)map_file(PROGRAM_NAME, &len, PROT_READ | PROT_WRITE);
    if (!addr)
    {
        error("failed to load program: %s", PROGRAM_NAME);
        return;
    }

    if (len > MEM_SIZE - PROGRAM_BASE)
    {
        error("program too large");
        munmap(addr, len);
        return;
    }

    memcpy(g_main_mem + PROGRAM_BASE, addr, len);
    info("loaded %zu bytes to 0x%x", len, PROGRAM_BASE);
    munmap(addr, len);
}

int
main(void)
{
    init_logger(INFO, NULL);
    info("RISC-V Emulator starting...");

    init_mem();
    load_program();

    init_debugger();

    memset(&g_state, 0, sizeof(g_state));
    g_state.pc = PROGRAM_BASE;
    g_state.terminated = 0;
    g_state.single_step = 1;
    g_state.breakpoint = 0xDEADBEEF;
    g_state.breakpoint_enabled = 0;

    info("starting execution at PC=0x%x", g_state.pc);

    while (!g_state.terminated)
    {
        if (g_state.pc >= MEM_SIZE)
        {
            error("PC out of bounds: 0x%x", g_state.pc);
            break;
        }
        if (g_state.breakpoint_enabled && g_state.pc == g_state.breakpoint)
        {
            info("breakpoint hit at PC=0x%x", g_state.pc);
            g_state.single_step = 1;
            g_state.breakpoint_enabled = 0;
        }

        if (g_state.single_step)
        {
            tick_debugger();
        }
        if (g_state.terminated)
        {
            break;
        }
        uint32_t ins = mem_read32_unsigned(g_state.pc);

        exec(ins);

        g_state.pc += 4;
    }

    terminate_logger();
    return 0;
}
