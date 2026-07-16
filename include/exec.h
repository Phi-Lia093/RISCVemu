#ifndef EXEC_H
#define EXEC_H

#include <stdint.h>

static inline uint32_t
get_opcode(uint32_t ins)
{
    return ins & 0x7F;
}

static inline uint32_t
get_rd(uint32_t ins)
{
    return (ins >> 7) & 0x1F;
}

static inline uint32_t
get_funct3(uint32_t ins)
{
    return (ins >> 12) & 0x7;
}

static inline uint32_t
get_rs1(uint32_t ins)
{
    return (ins >> 15) & 0x1F;
}

static inline uint32_t
get_rs2(uint32_t ins)
{
    return (ins >> 20) & 0x1F;
}

static inline uint32_t
get_funct7(uint32_t ins)
{
    return (ins >> 25) & 0x7F;
}

uint32_t reg_read(uint32_t id);

void reg_write(uint32_t id, uint32_t d);

void exec(uint32_t ins);

#endif