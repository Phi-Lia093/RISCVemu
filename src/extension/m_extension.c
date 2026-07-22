#include <emu.h>
#include <exec.h>
#include <extension/m_extension.h>
#include <limits.h>
#include <logger.h>
#include <mem.h>
#include <stdint.h>

void
insm_r_mul(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    int32_t v1 = (int32_t)reg_read(rs1);
    int32_t v2 = (int32_t)reg_read(rs2);
    reg_write(rd, (uint32_t)(v1 * v2));
}

void
insm_r_mulh(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    int32_t v1 = (int32_t)reg_read(rs1);
    int32_t v2 = (int32_t)reg_read(rs2);
    int64_t result = (int64_t)v1 * (int64_t)v2;
    reg_write(rd, (uint32_t)(result >> 32));
}

void
insm_r_mulsu(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    int32_t v1 = (int32_t)reg_read(rs1);
    uint32_t v2 = reg_read(rs2);
    int64_t result = (int64_t)v1 * (uint64_t)v2;
    reg_write(rd, (uint32_t)(result >> 32));
}

void
insm_r_mulu(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2);
    uint64_t result = (uint64_t)v1 * (uint64_t)v2;
    reg_write(rd, (uint32_t)(result >> 32));
}

void
insm_r_div(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    int32_t v1 = (int32_t)reg_read(rs1);
    int32_t v2 = (int32_t)reg_read(rs2);

    if (unlikely(v2 == 0))
    {
        reg_write(rd, 0xFFFFFFFF);
    }
    else if (unlikely(v1 == INT32_MIN && v2 == -1))
    {
        reg_write(rd, (uint32_t)INT32_MIN);
    }
    else
    {
        reg_write(rd, (uint32_t)(v1 / v2));
    }
}

void
insm_r_divu(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2);

    if (unlikely(v2 == 0))
    {
        reg_write(rd, 0xFFFFFFFF);
    }
    else
    {
        reg_write(rd, v1 / v2);
    }
}

void
insm_r_rem(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    int32_t v1 = (int32_t)reg_read(rs1);
    int32_t v2 = (int32_t)reg_read(rs2);

    if (unlikely(v2 == 0))
    {
        reg_write(rd, v1);
    }
    else if (unlikely(v1 == INT32_MIN && v2 == -1))
    {
        reg_write(rd, 0);
    }
    else
    {
        reg_write(rd, (uint32_t)(v1 % v2));
    }
}

void
insm_r_remu(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2);

    if (unlikely(v2 == 0))
    {
        reg_write(rd, v1);
    }
    else
    {
        reg_write(rd, v1 % v2);
    }
}
