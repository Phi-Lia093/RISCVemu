#ifndef MEM_H
#define MEM_H

#include <stdbool.h>
#include <stdint.h>

#include <emu.h>

#define MEM_SIZE (4 * 1024 * 1024 * 1024L - 4096L)

void init_mem(void);

static inline uint32_t
mem_read8_unsigned(uint32_t addr)
{
    if (unlikely(addr >= MEM_SIZE)) return 0;
    return g_state.main_memory[addr];
}

static inline int32_t
mem_read8_signed(uint32_t addr)
{
    if (unlikely(addr >= MEM_SIZE)) return 0;
    return (int32_t)(int8_t)g_state.main_memory[addr];
}

static inline void
mem_write8(uint32_t addr, uint8_t val)
{
    if (unlikely(addr >= MEM_SIZE)) return;
    g_state.main_memory[addr] = val;
}

static inline uint32_t
mem_read16_unsigned(uint32_t addr)
{
    if (unlikely(addr >= MEM_SIZE - 1)) return 0;
    return *(uint16_t *)(g_state.main_memory + addr);
}

static inline int32_t
mem_read16_signed(uint32_t addr)
{
    if (unlikely(addr >= MEM_SIZE - 1)) return 0;
    return (int32_t)(int16_t)(*(uint16_t *)(g_state.main_memory + addr));
}

static inline void
mem_write16(uint32_t addr, uint16_t val)
{
    if (unlikely(addr >= MEM_SIZE - 1)) return;
    *(uint16_t *)(g_state.main_memory + addr) = val;
}

static inline uint32_t
mem_read32_unsigned(uint32_t addr)
{
    if (unlikely(addr >= MEM_SIZE - 3)) return 0;
    return *(uint32_t *)(g_state.main_memory + addr);
}

static inline int32_t
mem_read32_signed(uint32_t addr)
{
    if (unlikely(addr >= MEM_SIZE - 3)) return 0;
    return *(int32_t *)(g_state.main_memory + addr);
}

static inline void
mem_write32(uint32_t addr, uint32_t val)
{
    if (unlikely(addr >= MEM_SIZE - 3)) return;
    *(uint32_t *)(g_state.main_memory + addr) = val;
}

static inline bool
is_aligned(uint32_t addr, uint32_t size)
{
    return (addr & (size - 1)) == 0;
}

#endif
