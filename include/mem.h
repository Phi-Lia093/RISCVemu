#ifndef MEM_H
#define MEM_H

#include <stdint.h>

#define MEM_SIZE 1 * 1024 * 1024 * 1024L

void init_mem(void);

uint32_t
mem_read8_unsigned(uint32_t addr);

uint32_t
mem_read16_unsigned(uint32_t addr);

uint32_t
mem_read32_unsigned(uint32_t addr);

int32_t
mem_read8_signed(uint32_t addr);

int32_t
mem_read16_signed(uint32_t addr);

int32_t
mem_read32_signed(uint32_t addr);

void
mem_write8(uint32_t addr, uint8_t val);

void
mem_write16(uint32_t addr, uint16_t val);

void
mem_write32(uint32_t addr, uint32_t val);

bool
is_aligned(uint32_t addr, uint32_t size);

extern uint8_t *g_main_mem;

#endif