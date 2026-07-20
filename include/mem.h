#ifndef MEM_H
#define MEM_H

#include <stdint.h>

#include <emu.h>

extern uint8_t *g_main_mem;

#define MEM_SIZE 1 * 1024 * 1024 * 1024L

void init_mem(void);
// TODO: add ALIGN and OVERFLOW exception

static inline uint32_t
mem_read8_unsigned(uint32_t addr)
{
    if (unlikely(addr >= MEM_SIZE)) return 0;
    return g_main_mem[addr];
}

static inline uint32_t
mem_read16_unsigned(uint32_t addr)
{
    if (unlikely(addr >= MEM_SIZE - 1)) return 0;
    if (unlikely(addr & 0x1)) return 0;

    return *(uint16_t *)(g_main_mem + addr);
}

static inline uint32_t
mem_read32_unsigned(uint32_t addr)
{
    if (unlikely(addr >= MEM_SIZE - 3)) return 0;
    if (unlikely(addr & 0x3)) return 0;

    return *(uint32_t *)(g_main_mem + addr);
}

static inline int32_t
mem_read8_signed(uint32_t addr)
{
    if (unlikely(addr >= MEM_SIZE)) return 0;
    return (int32_t)(int8_t)g_main_mem[addr];
}

static inline int32_t
mem_read16_signed(uint32_t addr)
{
    if (unlikely(addr >= MEM_SIZE - 1)) return 0;
    if (unlikely(addr & 0x1)) return 0;
    return (int32_t)(int16_t)(*(uint16_t *)(g_main_mem + addr));
}

static inline int32_t
mem_read32_signed(uint32_t addr)
{
    if (unlikely(addr >= MEM_SIZE - 3)) return 0;
    if (unlikely(addr & 0x3)) return 0;
    return (int32_t)(*(int32_t *)(g_main_mem + addr));
}

static inline void
mem_write8(uint32_t addr, uint8_t val)
{
    if (unlikely(addr >= MEM_SIZE)) return;
    g_main_mem[addr] = val;
}

static inline void
mem_write16(uint32_t addr, uint16_t val)
{
    if (unlikely(addr >= MEM_SIZE)) return;
    g_main_mem[addr] = val & 0xFF;
    g_main_mem[addr + 1] = (val >> 8) & 0xFF;
}

static inline void
mem_write32(uint32_t addr, uint32_t val)
{
    if (unlikely(addr >= MEM_SIZE - 3)) return;
    if (unlikely(addr & 0x3)) return;

    *(uint32_t *)(g_main_mem + addr) = val;
}

static inline bool
is_aligned(uint32_t addr, uint32_t size)
{
    return (addr & (size - 1)) == 0;
}

#endif
