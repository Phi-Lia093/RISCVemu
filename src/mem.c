#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <mem.h>

uint8_t *g_main_mem;

void
init_mem(void)
{
    g_main_mem = (uint8_t *)calloc(1, MEM_SIZE);
    // activate page
    memset(g_main_mem, 0x5A, MEM_SIZE);
    memset(g_main_mem, 0, MEM_SIZE);
}

// TODO: add ALIGN and OVERFLOW exception

uint32_t
mem_read8_unsigned(uint32_t addr)
{
    if (addr >= MEM_SIZE)
    {
        // E
        return 0;
    }
    return (uint32_t)g_main_mem[addr];
}

uint32_t
mem_read16_unsigned(uint32_t addr)
{
    if (addr >= MEM_SIZE - 1)
    {
        // E
        return 0;
    }
    if (addr & 0x1)
    {
        // ALIGN
    }
    return (uint32_t)g_main_mem[addr] | ((uint32_t)g_main_mem[addr + 1] << 8);
}

uint32_t
mem_read32_unsigned(uint32_t addr)
{
    if (addr >= MEM_SIZE - 3)
    {
        // E
        return 0;
    }
    if (addr & 0x3)
    {
        // ALIGN
    }
    return (uint32_t)g_main_mem[addr] | ((uint32_t)g_main_mem[addr + 1] << 8)
           | ((uint32_t)g_main_mem[addr + 2] << 16)
           | ((uint32_t)g_main_mem[addr + 3] << 24);
}

int32_t
mem_read8_signed(uint32_t addr)
{
    if (addr >= MEM_SIZE)
    {
        // E
        return 0;
    }
    return (int32_t)(int8_t)g_main_mem[addr];
}

int32_t
mem_read16_signed(uint32_t addr)
{
    if (addr >= MEM_SIZE - 1)
    {
        // E
        return 0;
    }
    if (addr & 0x1)
    {
        // ALIGN
    }
    uint16_t val = (uint16_t)(g_main_mem[addr] | (g_main_mem[addr + 1] << 8));
    return (int32_t)(int16_t)val;
}

uint32_t
mem_read32_signed(uint32_t addr)
{ // the same as mem_read_32_unsinged as 32bit has no extend problem
    if (addr >= MEM_SIZE - 3)
    {
        // E
        return 0;
    }
    if (addr & 0x3)
    {
        // ALIGN
    }
    return (uint32_t)g_main_mem[addr] | ((uint32_t)g_main_mem[addr + 1] << 8)
           | ((uint32_t)g_main_mem[addr + 2] << 16)
           | ((uint32_t)g_main_mem[addr + 3] << 24);
}

void
mem_write8(uint32_t addr, uint8_t val)
{
    if (addr >= MEM_SIZE)
    {
        // E
        return;
    }
    g_main_mem[addr] = val;
}

void
mem_write16(uint32_t addr, uint16_t val)
{
    if (addr >= MEM_SIZE - 1)
    {
        // E
        return;
    }
    g_main_mem[addr] = val & 0xFF;
    g_main_mem[addr + 1] = (val >> 8) & 0xFF;
}

void
mem_write32(uint32_t addr, uint32_t val)
{
    if (addr >= MEM_SIZE - 3)
    {
        // E
        return;
    }
    g_main_mem[addr] = val & 0xFF;
    g_main_mem[addr + 1] = (val >> 8) & 0xFF;
    g_main_mem[addr + 2] = (val >> 16) & 0xFF;
    g_main_mem[addr + 3] = (val >> 24) & 0xFF;
}

bool
is_aligned(uint32_t addr, uint32_t size)
{
    return (addr & (size - 1)) == 0;
}