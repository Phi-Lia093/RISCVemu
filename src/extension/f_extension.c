/*
 * SPDX-FileCopyrightText: 2026 PhiLia093 phi_lia093@126.com
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of RISCVemu.
 * RISCVemu is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * RISCVemu is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <config.h>

#ifdef CONFIG_ENABLE_F_EXTENSION
#include <stdint.h>

#include <emu.h>
#include <exec.h>
#include <extension/f_extension.h>
#include <logger.h>
#include <mem.h>
#include <mmu.h>
#include <softfloat.h>

#define RISCV_CANONICAL_NAN_H 0x7E00u
#define RISCV_CANONICAL_NAN_S 0x7FC00000u
#define RISCV_CANONICAL_NAN_D 0x7FF8000000000000ULL
#define RISCV_CANONICAL_NAN_Q_LO 0x0000000000000000ULL
#define RISCV_CANONICAL_NAN_Q_HI 0x7FFF800000000000ULL

uint64_t fpr[sizeof(__float128) * 32 / sizeof(uint64_t)];

uint32_t fcsr;

uint32_t
get_fcsr()
{
    return fcsr;
}

uint32_t
get_fflags()
{
    return fcsr & 0b11111;
}

uint32_t
get_frm()
{
    return (fcsr >> 5) & 0b111;
}

void
set_fflags(uint32_t flags)
{
    fcsr = (fcsr & ~0b11111) | (flags & 0b11111);
}

void
set_frm(uint32_t frm)
{
    if (frm > 4) return;
    fcsr = (fcsr & ~0b11100000) | (frm << 5);
}

void
set_fcsr(uint32_t csr)
{
    uint32_t new_frm = (csr >> 5) & 0b111;
    uint32_t new_fflags = csr & 0b11111;
    if (new_frm <= 4)
    {
        fcsr = (new_fflags & 0b11111) | (new_frm << 5);
    }
    else
    {
        fcsr = (new_fflags & 0b11111) | (fcsr & 0b11100000);
    }
}

/* ------------------------------------------------------------------ */
/* SoftFloat glue: map RISC-V rounding modes / collect exception flags */
/* ------------------------------------------------------------------ */

static inline uint_fast8_t
sf_rm(uint32_t rm)
{
    switch (get_rm(rm))
    {
    case RNE:
        return softfloat_round_near_even;
    case RTZ:
        return softfloat_round_minMag;
    case RDN:
        return softfloat_round_min;
    case RUP:
        return softfloat_round_max;
    case RMM:
        return softfloat_round_near_maxMag;
    default:
        return softfloat_round_near_even;
    }
}

static inline void
sf_begin(uint32_t rm)
{
    softfloat_roundingMode = sf_rm(rm);
    softfloat_exceptionFlags = 0;
}

static inline void
sf_end(void)
{
    fcsr |= softfloat_exceptionFlags;
}

/* -------------------------------------------------------------- */
/* FPR accessors.  The lane is 128 bit wide (to hold Q values);   */
/* single/double results are NaN-boxed per the RISC-V spec.        */
/* -------------------------------------------------------------- */

uint32_t
fpr_read_s(uint32_t r)
{
    return (uint32_t)fpr[2 * r];
}

uint64_t
fpr_read_d(uint32_t r)
{
    return fpr[2 * r];
}

uint16_t
fpr_read_h(uint32_t r)
{
    return (uint16_t)fpr[2 * r];
}

void
fpr_read_q(uint32_t r, float128_t *out)
{
    out->v[0] = fpr[2 * r];
    out->v[1] = fpr[2 * r + 1];
}

void
fpr_write_s(uint32_t r, uint32_t v)
{
    fpr[2 * r] = 0xFFFFFFFF00000000ULL | (uint64_t)v;
    fpr[2 * r + 1] = 0xFFFFFFFFFFFFFFFFULL;
}

void
fpr_write_d(uint32_t r, uint64_t v)
{
    fpr[2 * r] = v;
    fpr[2 * r + 1] = 0xFFFFFFFFFFFFFFFFULL;
}

void
fpr_write_h(uint32_t r, uint16_t v)
{
    fpr[2 * r] = 0xFFFFFFFFFFFF0000ULL | (uint64_t)v;
    fpr[2 * r + 1] = 0xFFFFFFFFFFFFFFFFULL;
}

void
fpr_write_q(uint32_t r, const float128_t *in)
{
    fpr[2 * r] = in->v[0];
    fpr[2 * r + 1] = in->v[1];
}

/* ------------------------------- */
/* NaN / class / min-max helpers   */
/* ------------------------------- */

static inline int
is_nan_s(uint32_t x)
{
    return ((x & 0x7F800000u) == 0x7F800000u) && (x & 0x007FFFFFu) != 0;
}

static inline int
is_signaling_nan_s(uint32_t x)
{
    return ((x & 0x7F800000u) == 0x7F800000u) && (x & 0x007FFFFFu)
           && !(x & 0x00400000u);
}

static inline int
is_nan_d(uint64_t x)
{
    return ((x & 0x7FF0000000000000ULL) == 0x7FF0000000000000ULL)
           && (x & 0x000FFFFFFFFFFFFFULL) != 0;
}

static inline int
is_signaling_nan_d(uint64_t x)
{
    return ((x & 0x7FF0000000000000ULL) == 0x7FF0000000000000ULL)
           && (x & 0x000FFFFFFFFFFFFFULL)
           && !(x & 0x0008000000000000ULL); // bit 51
}

static inline int
is_nan_h(uint16_t x)
{
    return ((x & 0x7C00u) == 0x7C00u) && (x & 0x03FFu) != 0;
}

static inline int
is_signaling_nan_h(uint16_t x)
{
    return ((x & 0x7C00u) == 0x7C00u) && (x & 0x03FFu) != 0
           && !(x & 0x0200u); // bit 9
}

static inline int
is_nan_q(const float128_t *x)
{
    uint64_t hi = x->v[1];
    uint64_t exp = (hi >> 48) & 0x7FFF;
    if (exp != 0x7FFF) return 0;
    uint64_t mant_hi = hi & 0x0000FFFFFFFFFFFFULL;
    uint64_t mant_lo = x->v[0];
    return (mant_hi != 0) || (mant_lo != 0);
}

static inline int
is_signaling_nan_q(const float128_t *p)
{
    uint64_t hi = p->v[1];
    uint64_t lo = p->v[0];
    uint64_t exp = (hi >> 48) & 0x7FFF;
    if (exp != 0x7FFF) return 0;
    uint64_t mant_hi = hi & 0x0000FFFFFFFFFFFFULL;
    uint64_t mant_lo = lo;
    if ((mant_hi == 0) && (mant_lo == 0)) return 0;
    return !(mant_hi & (1ULL << 47));
}

static uint32_t
fclass_s(uint32_t x)
{
    uint32_t sign = (x >> 31) & 1;
    uint32_t exp = (x >> 23) & 0xFF;
    uint32_t mant = x & 0x7FFFFF;

    if (exp == 0xFF)
    {
        if (mant == 0) return sign ? (1u << 0) : (1u << 7); /* +/- inf */
        if (mant & 0x400000u) return (1u << 9);             /* qNaN */
        return (1u << 8);                                   /* sNaN */
    }
    if (exp == 0)
    {
        if (mant == 0) return sign ? (1u << 3) : (1u << 4); /* +/- 0 */
        return sign ? (1u << 2) : (1u << 5);                /* subnormal */
    }
    return sign ? (1u << 1) : (1u << 6); /* normal */
}

static uint32_t
fclass_d(uint64_t x)
{
    uint64_t sign = (x >> 63) & 1;
    uint64_t exp = (x >> 52) & 0x7FF;
    uint64_t mant = x & 0xFFFFFFFFFFFFFULL;

    if (exp == 0x7FF)
    {
        if (mant == 0) return sign ? (1u << 0) : (1u << 7);
        if (mant & 0x8000000000000ULL) return (1u << 9);
        return (1u << 8);
    }
    if (exp == 0)
    {
        if (mant == 0) return sign ? (1u << 3) : (1u << 4);
        return sign ? (1u << 2) : (1u << 5);
    }
    return sign ? (1u << 1) : (1u << 6);
}

#ifdef CONFIG_ENABLE_ZFH_EXTENSION
static uint32_t
fclass_h(uint16_t x)
{
    uint32_t sign = (x >> 15) & 1;
    uint32_t exp = (x >> 10) & 0x1F;
    uint32_t mant = x & 0x3FFu;

    if (exp == 0x1F)
    {
        if (mant == 0) return sign ? (1u << 0) : (1u << 7); /* +/- inf */
        if (mant & 0x200u) return (1u << 9);                /* qNaN */
        return (1u << 8);                                   /* sNaN */
    }
    if (exp == 0)
    {
        if (mant == 0) return sign ? (1u << 3) : (1u << 4); /* +/- 0 */
        return sign ? (1u << 2) : (1u << 5);                /* subnormal */
    }
    return sign ? (1u << 1) : (1u << 6); /* normal */
}
#endif // CONFIG_ENABLE_ZFH_EXTENSION

static uint32_t
fclass_q(const float128_t *x)
{
    uint64_t hi = x->v[1], lo = x->v[0];
    uint64_t sign = (hi >> 63) & 1;
    uint64_t exp = (hi >> 48) & 0x7FFF;
    uint64_t mhi = hi & 0x0000FFFFFFFFFFFFULL;

    if (exp == 0x7FFF)
    {
        if (mhi == 0 && lo == 0) return sign ? (1u << 0) : (1u << 7);
        if (mhi & (1ULL << 47)) return (1u << 9); /* qNaN */
        return (1u << 8);                         /* sNaN */
    }
    if (exp == 0)
    {
        if (mhi == 0 && lo == 0) return sign ? (1u << 3) : (1u << 4);
        return sign ? (1u << 2) : (1u << 5);
    }
    return sign ? (1u << 1) : (1u << 6);
}

static uint32_t
rv_fmin_s(uint32_t a, uint32_t b)
{
    int a_snan = is_signaling_nan_s(a);
    int b_snan = is_signaling_nan_s(b);
    int a_qnan = is_nan_s(a) && !a_snan;
    int b_qnan = is_nan_s(b) && !b_snan;
    if (a_snan || b_snan)
    {
        fcsr |= NV;
    }
    if ((a_snan || a_qnan) && (b_snan || b_qnan))
    {
        return RISCV_CANONICAL_NAN_S;
    }
    if (a_snan || a_qnan)
    {
        return b;
    }
    if (b_snan || b_qnan)
    {
        return a;
    }
    float32_t fa = { a }, fb = { b };
    if (f32_eq(fa, fb))
    {
        return ((a >> 31) & 1) ? a : b;
    }
    return f32_lt(fa, fb) ? a : b;
}

static uint32_t
rv_fmax_s(uint32_t a, uint32_t b)
{
    int a_snan = is_signaling_nan_s(a);
    int b_snan = is_signaling_nan_s(b);
    int a_qnan = is_nan_s(a) && !a_snan;
    int b_qnan = is_nan_s(b) && !b_snan;
    if (a_snan || b_snan)
    {
        fcsr |= NV;
    }
    if ((a_snan || a_qnan) && (b_snan || b_qnan))
    {
        return RISCV_CANONICAL_NAN_S;
    }
    if (a_snan || a_qnan)
    {
        return b;
    }
    if (b_snan || b_qnan)
    {
        return a;
    }
    float32_t fa = { a }, fb = { b };
    if (f32_eq(fa, fb))
    {
        return ((a >> 31) & 1) ? b : a;
    }
    return f32_lt(fa, fb) ? b : a;
}

static uint64_t
rv_fmin_d(uint64_t a, uint64_t b)
{
    int a_snan = is_signaling_nan_d(a);
    int b_snan = is_signaling_nan_d(b);
    int a_qnan = is_nan_d(a) && !a_snan;
    int b_qnan = is_nan_d(b) && !b_snan;
    if (a_snan || b_snan)
    {
        fcsr |= NV;
    }
    if ((a_snan || a_qnan) && (b_snan || b_qnan))
    {
        return RISCV_CANONICAL_NAN_D;
    }
    if (a_snan || a_qnan)
    {
        return b;
    }
    if (b_snan || b_qnan)
    {
        return a;
    }
    float64_t fa = { a }, fb = { b };
    if (f64_eq(fa, fb))
    {
        return ((a >> 63) & 1) ? a : b;
    }
    return f64_lt(fa, fb) ? a : b;
}

static uint64_t
rv_fmax_d(uint64_t a, uint64_t b)
{
    int a_snan = is_signaling_nan_d(a);
    int b_snan = is_signaling_nan_d(b);
    int a_qnan = is_nan_d(a) && !a_snan;
    int b_qnan = is_nan_d(b) && !b_snan;
    if (a_snan || b_snan)
    {
        fcsr |= NV;
    }
    if ((a_snan || a_qnan) && (b_snan || b_qnan))
    {
        return RISCV_CANONICAL_NAN_D;
    }
    if (a_snan || a_qnan)
    {
        return b;
    }
    if (b_snan || b_qnan)
    {
        return a;
    }
    float64_t fa = { a }, fb = { b };
    if (f64_eq(fa, fb))
    {
        return ((a >> 63) & 1) ? b : a;
    }
    return f64_lt(fa, fb) ? b : a;
}

#ifdef CONFIG_ENABLE_ZFH_EXTENSION

static uint16_t
rv_fmin_h(uint16_t a, uint16_t b)
{
    int a_snan = is_signaling_nan_h(a);
    int b_snan = is_signaling_nan_h(b);
    int a_qnan = is_nan_h(a) && !a_snan;
    int b_qnan = is_nan_h(b) && !b_snan;
    if (a_snan || b_snan)
    {
        fcsr |= NV;
    }
    if ((a_snan || a_qnan) && (b_snan || b_qnan))
    {
        return RISCV_CANONICAL_NAN_H;
    }
    if (a_snan || a_qnan)
    {
        return b;
    }
    if (b_snan || b_qnan)
    {
        return a;
    }
    float16_t fa = { a }, fb = { b };
    if (f16_eq(fa, fb))
    {
        return ((a >> 15) & 1) ? a : b;
    }
    return f16_lt(fa, fb) ? a : b;
}

static uint16_t
rv_fmax_h(uint16_t a, uint16_t b)
{
    int a_snan = is_signaling_nan_h(a);
    int b_snan = is_signaling_nan_h(b);
    int a_qnan = is_nan_h(a) && !a_snan;
    int b_qnan = is_nan_h(b) && !b_snan;
    if (a_snan || b_snan)
    {
        fcsr |= NV;
    }
    if ((a_snan || a_qnan) && (b_snan || b_qnan))
    {
        return RISCV_CANONICAL_NAN_H;
    }
    if (a_snan || a_qnan)
    {
        return b;
    }
    if (b_snan || b_qnan)
    {
        return a;
    }
    float16_t fa = { a }, fb = { b };
    if (f16_eq(fa, fb))
    {
        return ((a >> 15) & 1) ? b : a;
    }
    return f16_lt(fa, fb) ? b : a;
}
#endif // CONFIG_ENABLE_ZFH_EXTENSION

static void
rv_fmin_q(const float128_t *a, const float128_t *b, float128_t *r)
{
    int a_snan = is_signaling_nan_q(a);
    int b_snan = is_signaling_nan_q(b);
    int a_qnan = is_nan_q(a) && !a_snan;
    int b_qnan = is_nan_q(b) && !b_snan;
    if (a_snan || b_snan)
    {
        fcsr |= NV;
    }
    if ((a_snan || a_qnan) && (b_snan || b_qnan))
    {
        r->v[0] = RISCV_CANONICAL_NAN_Q_LO;
        r->v[1] = RISCV_CANONICAL_NAN_Q_HI;
        return;
    }
    if (a_snan || a_qnan)
    {
        *r = *b;
        return;
    }
    if (b_snan || b_qnan)
    {
        *r = *a;
        return;
    }
    if (f128M_eq(a, b))
    {
        *r = ((a->v[1] >> 63) & 1) ? *a : *b;
        return;
    }
    *r = f128M_lt(a, b) ? *a : *b;
}

static void
rv_fmax_q(const float128_t *a, const float128_t *b, float128_t *r)
{
    int a_snan = is_signaling_nan_q(a);
    int b_snan = is_signaling_nan_q(b);
    int a_qnan = is_nan_q(a) && !a_snan;
    int b_qnan = is_nan_q(b) && !b_snan;
    if (a_snan || b_snan)
    {
        fcsr |= NV;
    }
    if ((a_snan || a_qnan) && (b_snan || b_qnan))
    {
        r->v[0] = RISCV_CANONICAL_NAN_Q_LO;
        r->v[1] = RISCV_CANONICAL_NAN_Q_HI;
        return;
    }
    if (a_snan || a_qnan)
    {
        *r = *b;
        return;
    }
    if (b_snan || b_qnan)
    {
        *r = *a;
        return;
    }
    if (f128M_eq(a, b))
    {
        *r = ((a->v[1] >> 63) & 1) ? *b : *a;
        return;
    }
    *r = f128M_lt(a, b) ? *b : *a;
}

/* -------------------------------------------------------------- */
/* RISC-V canonical NaN normalization                              */
/* -------------------------------------------------------------- */

static inline uint32_t
normalize_nan_s(uint32_t x)
{
    if (is_nan_s(x)) return RISCV_CANONICAL_NAN_S;
    return x;
}

static inline uint64_t
normalize_nan_d(uint64_t x)
{
    if (is_nan_d(x)) return RISCV_CANONICAL_NAN_D;
    return x;
}

static inline uint16_t
normalize_nan_h(uint16_t x)
{
    if (is_nan_h(x)) return RISCV_CANONICAL_NAN_H;
    return x;
}

static inline void
normalize_nan_q(float128_t *x)
{
    if (is_nan_q(x))
    {
        x->v[0] = RISCV_CANONICAL_NAN_Q_LO;
        x->v[1] = RISCV_CANONICAL_NAN_Q_HI;
    }
}

/* ---------------------------- */
/* Single precision (F)         */
/* ---------------------------- */

static void
s_add(uint32_t rs2, uint32_t rs1, uint32_t rm, uint32_t rd)
{
    float32_t a = { fpr_read_s(rs1) }, b = { fpr_read_s(rs2) };
    sf_begin(rm);
    float32_t r = f32_add(a, b);
    sf_end();
    r.v = normalize_nan_s(r.v);
    fpr_write_s(rd, r.v);
}

static void
s_sub(uint32_t rs2, uint32_t rs1, uint32_t rm, uint32_t rd)
{
    float32_t a = { fpr_read_s(rs1) }, b = { fpr_read_s(rs2) };
    sf_begin(rm);
    float32_t r = f32_sub(a, b);
    sf_end();
    r.v = normalize_nan_s(r.v);
    fpr_write_s(rd, r.v);
}

static void
s_mul(uint32_t rs2, uint32_t rs1, uint32_t rm, uint32_t rd)
{
    float32_t a = { fpr_read_s(rs1) }, b = { fpr_read_s(rs2) };
    sf_begin(rm);
    float32_t r = f32_mul(a, b);
    sf_end();
    r.v = normalize_nan_s(r.v);
    fpr_write_s(rd, r.v);
}

static void
s_div(uint32_t rs2, uint32_t rs1, uint32_t rm, uint32_t rd)
{
    float32_t a = { fpr_read_s(rs1) }, b = { fpr_read_s(rs2) };
    sf_begin(rm);
    float32_t r = f32_div(a, b);
    sf_end();
    r.v = normalize_nan_s(r.v);
    fpr_write_s(rd, r.v);
}

static void
s_sqrt(uint32_t rs1, uint32_t rm, uint32_t rd)
{
    float32_t a = { fpr_read_s(rs1) };
    sf_begin(rm);
    float32_t r = f32_sqrt(a);
    sf_end();
    r.v = normalize_nan_s(r.v);
    fpr_write_s(rd, r.v);
}

static void
s_fsgnj(uint32_t rs2, uint32_t rs1, uint32_t op, uint32_t rd)
{
    uint32_t a = fpr_read_s(rs1), b = fpr_read_s(rs2), r;
    switch (op)
    {
    case 0:
        r = (a & 0x7FFFFFFFu) | (b & 0x80000000u);
        break;
    case 1:
        r = (a & 0x7FFFFFFFu) | ((b ^ 0x80000000u) & 0x80000000u);
        break;
    default:
        r = (a & 0x7FFFFFFFu) | ((a ^ b) & 0x80000000u);
        break;
    }
    fpr_write_s(rd, r);
}

static void
s_fmin(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    fpr_write_s(rd, rv_fmin_s(fpr_read_s(rs1), fpr_read_s(rs2)));
}

static void
s_fmax(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    fpr_write_s(rd, rv_fmax_s(fpr_read_s(rs1), fpr_read_s(rs2)));
}

static void
s_fclass(uint32_t rs1, uint32_t rd)
{
    reg_write(rd, fclass_s(fpr_read_s(rs1)));
}

static void
s_fcmp(uint32_t op, uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    float32_t a = { fpr_read_s(rs1) }, b = { fpr_read_s(rs2) };
    uint32_t r = 0;

    bool a_is_nan = is_nan_s(a.v);
    bool b_is_nan = is_nan_s(b.v);
    bool a_is_snan = is_signaling_nan_s(a.v);
    bool b_is_snan = is_signaling_nan_s(b.v);

    if (a_is_nan || b_is_nan)
    {
        if (op == 2)
        {
            if (a_is_snan || b_is_snan) fcsr |= NV;
        }
        else if (op == 0 || op == 1)
        {
            fcsr |= NV;
        }
        r = 0;
    }
    else
    {
        if (op == 2)
            r = f32_eq(a, b) ? 1 : 0;
        else if (op == 1)
            r = f32_lt(a, b) ? 1 : 0;
        else if (op == 0)
            r = f32_le(a, b) ? 1 : 0;
        else
            r = 0;
    }

    reg_write(rd, r);
}

static void
s_fma(uint32_t subop, uint32_t rs3, uint32_t rs2, uint32_t rs1, uint32_t rm,
      uint32_t rd)
{
    float32_t a = { fpr_read_s(rs1) }, b = { fpr_read_s(rs2) },
              c = { fpr_read_s(rs3) };
    if (subop & 1) c.v ^= 0x80000000u;
    if (subop & 2) a.v ^= 0x80000000u;
    sf_begin(rm);
    float32_t r = f32_mulAdd(a, b, c);
    sf_end();
    r.v = normalize_nan_s(r.v);
    fpr_write_s(rd, r.v);
}

/* ---------------------------- */
/* Half precision (Zfh)         */
/* ---------------------------- */

#ifdef CONFIG_ENABLE_ZFH_EXTENSION
static void
h_add(uint32_t rs2, uint32_t rs1, uint32_t rm, uint32_t rd)
{
    float16_t a = { fpr_read_h(rs1) }, b = { fpr_read_h(rs2) };
    sf_begin(rm);
    float16_t r = f16_add(a, b);
    sf_end();
    r.v = normalize_nan_h(r.v);
    fpr_write_h(rd, r.v);
}

static void
h_sub(uint32_t rs2, uint32_t rs1, uint32_t rm, uint32_t rd)
{
    float16_t a = { fpr_read_h(rs1) }, b = { fpr_read_h(rs2) };
    sf_begin(rm);
    float16_t r = f16_sub(a, b);
    sf_end();
    r.v = normalize_nan_h(r.v);
    fpr_write_h(rd, r.v);
}

static void
h_mul(uint32_t rs2, uint32_t rs1, uint32_t rm, uint32_t rd)
{
    float16_t a = { fpr_read_h(rs1) }, b = { fpr_read_h(rs2) };
    sf_begin(rm);
    float16_t r = f16_mul(a, b);
    sf_end();
    r.v = normalize_nan_h(r.v);
    fpr_write_h(rd, r.v);
}

static void
h_div(uint32_t rs2, uint32_t rs1, uint32_t rm, uint32_t rd)
{
    float16_t a = { fpr_read_h(rs1) }, b = { fpr_read_h(rs2) };
    sf_begin(rm);
    float16_t r = f16_div(a, b);
    sf_end();
    r.v = normalize_nan_h(r.v);
    fpr_write_h(rd, r.v);
}

static void
h_sqrt(uint32_t rs1, uint32_t rm, uint32_t rd)
{
    float16_t a = { fpr_read_h(rs1) };
    sf_begin(rm);
    float16_t r = f16_sqrt(a);
    sf_end();
    r.v = normalize_nan_h(r.v);
    fpr_write_h(rd, r.v);
}

static void
h_fsgnj(uint32_t rs2, uint32_t rs1, uint32_t op, uint32_t rd)
{
    uint16_t a = fpr_read_h(rs1), b = fpr_read_h(rs2), r;
    switch (op)
    {
    case 0:
        r = (a & 0x7FFFu) | (b & 0x8000u);
        break;
    case 1:
        r = (a & 0x7FFFu) | ((b ^ 0x8000u) & 0x8000u);
        break;
    default:
        r = (a & 0x7FFFu) | ((a ^ b) & 0x8000u);
        break;
    }
    fpr_write_h(rd, r);
}

static void
h_fmin(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    fpr_write_h(rd, rv_fmin_h(fpr_read_h(rs1), fpr_read_h(rs2)));
}

static void
h_fmax(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    fpr_write_h(rd, rv_fmax_h(fpr_read_h(rs1), fpr_read_h(rs2)));
}

static void
h_fclass(uint32_t rs1, uint32_t rd)
{
    reg_write(rd, fclass_h(fpr_read_h(rs1)));
}

static void
h_fcmp(uint32_t op, uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    float16_t a = { fpr_read_h(rs1) }, b = { fpr_read_h(rs2) };
    uint32_t r = 0;

    bool a_is_nan = is_nan_h(a.v);
    bool b_is_nan = is_nan_h(b.v);
    bool a_is_snan = is_signaling_nan_h(a.v);
    bool b_is_snan = is_signaling_nan_h(b.v);

    if (a_is_nan || b_is_nan)
    {
        if (op == 2)
        {
            if (a_is_snan || b_is_snan) fcsr |= NV;
        }
        else if (op == 0 || op == 1)
        {
            fcsr |= NV;
        }
        r = 0;
    }
    else
    {
        if (op == 2)
            r = f16_eq(a, b) ? 1 : 0;
        else if (op == 1)
            r = f16_lt(a, b) ? 1 : 0;
        else if (op == 0)
            r = f16_le(a, b) ? 1 : 0;
        else
            r = 0;
    }

    reg_write(rd, r);
}

static void
h_fma(uint32_t subop, uint32_t rs3, uint32_t rs2, uint32_t rs1, uint32_t rm,
      uint32_t rd)
{
    float16_t a = { fpr_read_h(rs1) }, b = { fpr_read_h(rs2) },
              c = { fpr_read_h(rs3) };
    if (subop & 1) c.v ^= 0x8000u;
    if (subop & 2) a.v ^= 0x8000u;
    sf_begin(rm);
    float16_t r = f16_mulAdd(a, b, c);
    sf_end();
    r.v = normalize_nan_h(r.v);
    fpr_write_h(rd, r.v);
}
#endif // CONFIG_ENABLE_ZFH_EXTENSION

/* ---------------------------- */

#ifdef CONFIG_ENABLE_D_EXTENSION
static void
d_add(uint32_t rs2, uint32_t rs1, uint32_t rm, uint32_t rd)
{
    float64_t a = { fpr_read_d(rs1) }, b = { fpr_read_d(rs2) };
    sf_begin(rm);
    float64_t r = f64_add(a, b);
    sf_end();
    r.v = normalize_nan_d(r.v);
    fpr_write_d(rd, r.v);
}

static void
d_sub(uint32_t rs2, uint32_t rs1, uint32_t rm, uint32_t rd)
{
    float64_t a = { fpr_read_d(rs1) }, b = { fpr_read_d(rs2) };
    sf_begin(rm);
    float64_t r = f64_sub(a, b);
    sf_end();
    r.v = normalize_nan_d(r.v);
    fpr_write_d(rd, r.v);
}

static void
d_mul(uint32_t rs2, uint32_t rs1, uint32_t rm, uint32_t rd)
{
    float64_t a = { fpr_read_d(rs1) }, b = { fpr_read_d(rs2) };
    sf_begin(rm);
    float64_t r = f64_mul(a, b);
    sf_end();
    r.v = normalize_nan_d(r.v);
    fpr_write_d(rd, r.v);
}

static void
d_div(uint32_t rs2, uint32_t rs1, uint32_t rm, uint32_t rd)
{
    float64_t a = { fpr_read_d(rs1) }, b = { fpr_read_d(rs2) };
    sf_begin(rm);
    float64_t r = f64_div(a, b);
    sf_end();
    r.v = normalize_nan_d(r.v);
    fpr_write_d(rd, r.v);
}

static void
d_sqrt(uint32_t rs1, uint32_t rm, uint32_t rd)
{
    float64_t a = { fpr_read_d(rs1) };
    sf_begin(rm);
    float64_t r = f64_sqrt(a);
    sf_end();
    r.v = normalize_nan_d(r.v);
    fpr_write_d(rd, r.v);
}

static void
d_fsgnj(uint32_t rs2, uint32_t rs1, uint32_t op, uint32_t rd)
{
    uint64_t a = fpr_read_d(rs1), b = fpr_read_d(rs2), r;
    switch (op)
    {
    case 0:
        r = (a & 0x7FFFFFFFFFFFFFFFULL) | (b & 0x8000000000000000ULL);
        break;
    case 1:
        r = (a & 0x7FFFFFFFFFFFFFFFULL)
            | ((b ^ 0x8000000000000000ULL) & 0x8000000000000000ULL);
        break;
    default:
        r = (a & 0x7FFFFFFFFFFFFFFFULL) | ((a ^ b) & 0x8000000000000000ULL);
        break;
    }
    fpr_write_d(rd, r);
}

static void
d_fmin(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    fpr_write_d(rd, rv_fmin_d(fpr_read_d(rs1), fpr_read_d(rs2)));
}

static void
d_fmax(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    fpr_write_d(rd, rv_fmax_d(fpr_read_d(rs1), fpr_read_d(rs2)));
}

static void
d_fclass(uint32_t rs1, uint32_t rd)
{
    reg_write(rd, fclass_d(fpr_read_d(rs1)));
}

static void
d_fcmp(uint32_t op, uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    float64_t a = { fpr_read_d(rs1) }, b = { fpr_read_d(rs2) };
    uint32_t r = 0;

    bool a_is_nan = is_nan_d(a.v);
    bool b_is_nan = is_nan_d(b.v);
    bool a_is_snan = is_signaling_nan_d(a.v);
    bool b_is_snan = is_signaling_nan_d(b.v);
    if (a_is_nan || b_is_nan)
    {
        if (op == 2)
        {
            if (a_is_snan || b_is_snan) fcsr |= NV;
        }
        else if (op == 0 || op == 1)
        {
            fcsr |= NV;
        }
        r = 0;
    }
    else
    {
        if (op == 2)
            r = f64_eq(a, b) ? 1 : 0;
        else if (op == 1)
            r = f64_lt(a, b) ? 1 : 0;
        else if (op == 0)
            r = f64_le(a, b) ? 1 : 0;
        else
            r = 0;
    }

    reg_write(rd, r);
}

static void
d_fma(uint32_t subop, uint32_t rs3, uint32_t rs2, uint32_t rs1, uint32_t rm,
      uint32_t rd)
{
    float64_t a = { fpr_read_d(rs1) }, b = { fpr_read_d(rs2) },
              c = { fpr_read_d(rs3) };
    if (subop & 1) c.v ^= 0x8000000000000000ULL;
    if (subop & 2) a.v ^= 0x8000000000000000ULL;
    sf_begin(rm);
    float64_t r = f64_mulAdd(a, b, c);
    sf_end();
    r.v = normalize_nan_d(r.v);
    fpr_write_d(rd, r.v);
}
#endif // CONFIG_ENABLE_D_EXTENSION

/* --------------------------- */
/* Quad precision (Q)          */
/* --------------------------- */

#ifdef CONFIG_ENABLE_Q_EXTENSION
static void
q_binop(void (*fn)(const float128_t *, const float128_t *, float128_t *),
        uint32_t rs2, uint32_t rs1, uint32_t rm, uint32_t rd)
{
    float128_t a, b, r;
    fpr_read_q(rs1, &a);
    fpr_read_q(rs2, &b);
    sf_begin(rm);
    fn(&a, &b, &r);
    sf_end();
    normalize_nan_q(&r);
    fpr_write_q(rd, &r);
}

static void
q_sqrt(uint32_t rs1, uint32_t rm, uint32_t rd)
{
    float128_t a, r;
    fpr_read_q(rs1, &a);
    sf_begin(rm);
    f128M_sqrt(&a, &r);
    sf_end();
    normalize_nan_q(&r);
    fpr_write_q(rd, &r);
}

static void
q_fsgnj(uint32_t rs2, uint32_t rs1, uint32_t op, uint32_t rd)
{
    float128_t a, b, r;
    fpr_read_q(rs1, &a);
    fpr_read_q(rs2, &b);
    uint64_t sign_a = a.v[1] & 0x8000000000000000ULL;
    uint64_t sign_b = b.v[1] & 0x8000000000000000ULL;
    r = a;
    switch (op)
    {
    case 0:
        r.v[1] = (a.v[1] & 0x7FFFFFFFFFFFFFFFULL) | sign_b;
        break;
    case 1:
        r.v[1] = (a.v[1] & 0x7FFFFFFFFFFFFFFFULL)
                 | (sign_b ^ 0x8000000000000000ULL);
        break;
    default:
        r.v[1] = (a.v[1] & 0x7FFFFFFFFFFFFFFFULL) | (sign_a ^ sign_b);
        break;
    }
    fpr_write_q(rd, &r);
}

static void
q_fmin(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    float128_t a, b, r;
    fpr_read_q(rs1, &a);
    fpr_read_q(rs2, &b);
    rv_fmin_q(&a, &b, &r);
    fpr_write_q(rd, &r);
}

static void
q_fmax(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    float128_t a, b, r;
    fpr_read_q(rs1, &a);
    fpr_read_q(rs2, &b);
    rv_fmax_q(&a, &b, &r);
    fpr_write_q(rd, &r);
}

static void
q_fclass(uint32_t rs1, uint32_t rd)
{
    float128_t a;
    fpr_read_q(rs1, &a);
    reg_write(rd, fclass_q(&a));
}

static void
q_fcmp(uint32_t op, uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    float128_t a, b;
    fpr_read_q(rs1, &a);
    fpr_read_q(rs2, &b);
    uint32_t r = 0;

    bool a_is_nan = is_nan_q(&a);
    bool b_is_nan = is_nan_q(&b);
    bool a_is_snan = is_signaling_nan_q(&a);
    bool b_is_snan = is_signaling_nan_q(&b);

    if (a_is_nan || b_is_nan)
    {
        if (op == 2)
        {
            if (a_is_snan || b_is_snan) fcsr |= NV;
        }
        else if (op == 0 || op == 1)
        {
            fcsr |= NV;
        }
        r = 0;
    }
    else
    {
        if (op == 2)
            r = f128M_eq(&a, &b) ? 1 : 0;
        else if (op == 1)
            r = f128M_lt(&a, &b) ? 1 : 0;
        else if (op == 0)
            r = f128M_le(&a, &b) ? 1 : 0;
        else
            r = 0;
    }

    reg_write(rd, r);
}

static void
q_fma(uint32_t subop, uint32_t rs3, uint32_t rs2, uint32_t rs1, uint32_t rm,
      uint32_t rd)
{
    float128_t a, b, c, r;
    fpr_read_q(rs1, &a);
    fpr_read_q(rs2, &b);
    fpr_read_q(rs3, &c);
    if (subop & 1) c.v[1] ^= 0x8000000000000000ULL;
    if (subop & 2) a.v[1] ^= 0x8000000000000000ULL;
    sf_begin(rm);
    f128M_mulAdd(&a, &b, &c, &r);
    sf_end();
    normalize_nan_q(&r);
    fpr_write_q(rd, &r);
}
#endif // CONFIG_ENABLE_Q_EXTENSION

/* ----------------------------- */
/* Load / store                  */
/* ----------------------------- */

#ifdef CONFIG_SUPPORT_MISALIGN
static uint64_t
load64(uint32_t addr)
{
    if (unlikely(!is_aligned(addr, 8)))
    {
        return mmu_misaligned_load64(addr);
    }
    return mmu_read64_unsigned(addr);
}

static void
store64(uint32_t addr, uint64_t val)
{
    if (unlikely(!is_aligned(addr, 8)))
    {
        mmu_misaligned_store64(addr, val);
        return;
    }
    mmu_write64(addr, val);
}
#endif

void
insf_flw(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t addr = reg_read(rs1) + imm;
    uint32_t val;
#ifdef CONFIG_SUPPORT_MISALIGN
    if (unlikely(!is_aligned(addr, 4)))
        val = mmu_misaligned_load32(addr);
    else
#endif
        val = mmu_read32_unsigned(addr);
    fpr_write_s(rd, val);
}

void
insf_fsw(uint32_t imm, uint32_t rs1, uint32_t rs2)
{
    uint32_t addr = reg_read(rs1) + imm;
    uint32_t val = fpr_read_s(rs2);
#ifdef CONFIG_SUPPORT_MISALIGN
    if (unlikely(!is_aligned(addr, 4)))
        mmu_misaligned_store32(addr, val);
    else
#endif
        mmu_write32(addr, val);
}

void
insf_fld(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t addr = reg_read(rs1) + imm;
    uint64_t val;
#ifdef CONFIG_SUPPORT_MISALIGN
    val = load64(addr);
#else
    val = mmu_read64_unsigned(addr);
#endif
    fpr_write_d(rd, val);
}

void
insf_fsd(uint32_t imm, uint32_t rs1, uint32_t rs2)
{
    uint32_t addr = reg_read(rs1) + imm;
    uint64_t val = fpr_read_d(rs2);
#ifdef CONFIG_SUPPORT_MISALIGN
    store64(addr, val);
#else
    mmu_write64(addr, val);
#endif
}

void
insf_flh(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t addr = reg_read(rs1) + imm;
    uint16_t val;
#ifdef CONFIG_SUPPORT_MISALIGN
    if (unlikely(!is_aligned(addr, 2)))
        val = mmu_misaligned_load16(addr);
    else
#endif
        val = mmu_read16_unsigned(addr);
    fpr_write_h(rd, val);
}

void
insf_fsh(uint32_t imm, uint32_t rs1, uint32_t rs2)
{
    uint32_t addr = reg_read(rs1) + imm;
    uint16_t val = fpr_read_h(rs2);
#ifdef CONFIG_SUPPORT_MISALIGN
    if (unlikely(!is_aligned(addr, 2)))
        mmu_misaligned_store16(addr, val);
    else
#endif
        mmu_write16(addr, val);
}

void
insf_flq(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t addr = reg_read(rs1) + imm;
    float128_t v;
#ifdef CONFIG_SUPPORT_MISALIGN
    if (unlikely(!is_aligned(addr, 16)))
    {
        v.v[0] = 0;
        v.v[1] = 0;
        for (int i = 0; i < 8; i++)
            v.v[0] |= (uint64_t)mem_read8_unsigned(addr + i) << (8 * i);
        for (int i = 0; i < 8; i++)
            v.v[1] |= (uint64_t)mem_read8_unsigned(addr + 8 + i) << (8 * i);
    }
    else
#endif
    {
        v.v[0] = mmu_read64_unsigned(addr);
        v.v[1] = mmu_read64_unsigned(addr + 8);
    }
    fpr_write_q(rd, &v);
}

void
insf_fsq(uint32_t imm, uint32_t rs1, uint32_t rs2)
{
    uint32_t addr = reg_read(rs1) + imm;
    float128_t v;
    fpr_read_q(rs2, &v);
#ifdef CONFIG_SUPPORT_MISALIGN
    if (unlikely(!is_aligned(addr, 16)))
    {
        for (int i = 0; i < 8; i++)
            mem_write8(addr + i, (v.v[0] >> (8 * i)) & 0xFF);
        for (int i = 0; i < 8; i++)
            mem_write8(addr + 8 + i, (v.v[1] >> (8 * i)) & 0xFF);
    }
    else
#endif
    {
        mmu_write64(addr, v.v[0]);
        mmu_write64(addr + 8, v.v[1]);
    }
}

/* ----------------------------- */
/* FCVT helpers                  */
/* ----------------------------- */

/* --------------------------------------------------------------------- */
/* RISC-V fp -> int conversion.                                          */
/*                                                                       */
/* The bundled softfloat library returns non-RISC-V saturation values on */
/* invalid conversions (e.g. 0x80000000 for a *positive* overflow, and   */
/* 0xffffffff for a *negative* value converted to unsigned, and a broken */
/* f64_to_ui32 of +inf / -inf / NaN).  We therefore detect the special   */
/* cases (NaN / infinity / out-of-range) ourselves and override the      */
/* returned value with the RISC-V-mandated saturated result, while still */
/* using softfloat for the (correct) rounding of in-range values.        */
/*                                                                       */
/* RISC-V result rules (kind 0=w 1=wu 2=l 3=lu):                         */
/*   NaN         -> INT_MAX (signed) / UINT_MAX (unsigned), set NV       */
/*   +inf        -> INT_MAX (signed) / UINT_MAX (unsigned), set NV       */
/*   -inf        -> INT_MIN (signed) / 0         (unsigned), set NV      */
/*   + overflow  -> INT_MAX (signed) / UINT_MAX (unsigned), set NV       */
/*   - overflow  -> INT_MIN (signed) / 0         (unsigned), set NV      */
/* --------------------------------------------------------------------- */

static uint32_t
fcvt_int_sat(uint32_t kind, int sign)
{
    if (kind == 1 || kind == 3) /* unsigned: wu / lu */
        return sign ? 0u : 0xFFFFFFFFu;
    return sign ? 0x80000000u : 0x7FFFFFFFu; /* signed: w / l */
}

/* fp -> int : kind 0=w 1=wu 2=l 3=lu ; reads frs1 as fmt precision */
static uint32_t
fcvt_fp_int(uint32_t fmt, uint32_t kind, uint32_t rs1, uint32_t rm)
{
    uint32_t r = 0;
    uint_fast8_t mode;
    sf_begin(rm);
    mode = softfloat_roundingMode;
    switch (fmt)
    {
    case S:
    {
        uint32_t a = fpr_read_s(rs1);
        int sign = (a >> 31) & 1;
        int nan = is_nan_s(a);
        int inf
            = ((a & 0x7F800000u) == 0x7F800000u) && ((a & 0x007FFFFFu) == 0);
        float32_t fa = { a };
        if (nan || inf)
        {
            softfloat_exceptionFlags |= NV;
            r = fcvt_int_sat(kind, nan ? 0 : sign);
            break;
        }
        if (kind == 0)
            r = (uint32_t)f32_to_i32(fa, mode, true);
        else if (kind == 1)
            r = (uint32_t)f32_to_ui32(fa, mode, true);
        else if (kind == 2)
            r = (uint32_t)f32_to_i64(fa, mode, true);
        else
            r = (uint32_t)f32_to_ui64(fa, mode, true);
        if (softfloat_exceptionFlags & NV) /* finite value out of range */
            r = fcvt_int_sat(kind, sign);
        break;
    }
#ifdef CONFIG_ENABLE_ZFH_EXTENSION
    case H:
    {
        uint16_t a = fpr_read_h(rs1);
        int sign = (a >> 15) & 1;
        int nan = is_nan_h(a);
        int inf = ((a & 0x7C00u) == 0x7C00u) && ((a & 0x03FFu) == 0);
        float16_t fa = { a };
        if (nan || inf)
        {
            softfloat_exceptionFlags |= NV;
            r = fcvt_int_sat(kind, nan ? 0 : sign);
            break;
        }
        if (kind == 0)
            r = (uint32_t)f16_to_i32(fa, mode, true);
        else if (kind == 1)
            r = (uint32_t)f16_to_ui32(fa, mode, true);
        else if (kind == 2)
            r = (uint32_t)f16_to_i64(fa, mode, true);
        else
            r = (uint32_t)f16_to_ui64(fa, mode, true);
        if (softfloat_exceptionFlags & NV) /* finite value out of range */
            r = fcvt_int_sat(kind, sign);
        break;
    }
#endif
#ifdef CONFIG_ENABLE_D_EXTENSION
    case D:
    {
        uint64_t a = fpr_read_d(rs1);
        int sign = (a >> 63) & 1;
        int nan = is_nan_d(a);
        int inf = ((a & 0x7FF0000000000000ULL) == 0x7FF0000000000000ULL)
                  && ((a & 0x000FFFFFFFFFFFFFULL) == 0);
        float64_t fa = { a };
        if (nan || inf)
        {
            softfloat_exceptionFlags |= NV;
            r = fcvt_int_sat(kind, nan ? 0 : sign);
            break;
        }
        if (kind == 0)
            r = (uint32_t)f64_to_i32(fa, mode, true);
        else if (kind == 1)
            r = (uint32_t)f64_to_ui32(fa, mode, true);
        else if (kind == 2)
            r = (uint32_t)f64_to_i64(fa, mode, true);
        else
            r = (uint32_t)f64_to_ui64(fa, mode, true);
        if (softfloat_exceptionFlags & NV) /* finite value out of range */
            r = fcvt_int_sat(kind, sign);
        break;
    }
#endif
#ifdef CONFIG_ENABLE_Q_EXTENSION
    case Q:
    {
        float128_t a;
        fpr_read_q(rs1, &a);
        int sign = (a.v[1] >> 63) & 1;
        int nan = is_nan_q(&a);
        int inf = ((a.v[1] & 0x7FFF000000000000ULL) == 0x7FFF000000000000ULL)
                  && ((a.v[1] & 0x0000FFFFFFFFFFFFULL) == 0) && (a.v[0] == 0);
        if (nan || inf)
        {
            softfloat_exceptionFlags |= NV;
            r = fcvt_int_sat(kind, nan ? 0 : sign);
            break;
        }
        if (kind == 0)
            r = (uint32_t)f128M_to_i32(&a, mode, true);
        else if (kind == 1)
            r = (uint32_t)f128M_to_ui32(&a, mode, true);
        else if (kind == 2)
            r = (uint32_t)f128M_to_i64(&a, mode, true);
        else
            r = (uint32_t)f128M_to_ui64(&a, mode, true);
        if (softfloat_exceptionFlags & NV) /* finite value out of range */
            r = fcvt_int_sat(kind, sign);
        break;
    }
#endif
    default:
        fatal("fcvt.fp->int: unsupported fmt=%u", fmt);
    }
    sf_end();
    return r;
}

/* int -> fp : kind 0=w 1=wu 2=l 3=lu ; writes to fpr as fmt precision */
static void
fcvt_int_fp(uint32_t fmt, uint32_t kind, uint32_t rs1, uint32_t rm, uint32_t rd)
{
    int32_t v = (int32_t)reg_read(rs1);
    uint32_t uv = reg_read(rs1);
    sf_begin(rm);
    switch (fmt)
    {
    case S:
    {
        float32_t r;
        if (kind == 0)
            r = i32_to_f32(v);
        else if (kind == 1)
            r = ui32_to_f32(uv);
        else if (kind == 2)
            r = i64_to_f32((int64_t)v);
        else
            r = ui64_to_f32((uint64_t)uv);
        sf_end();
        fpr_write_s(rd, r.v);
        return;
    }
#ifdef CONFIG_ENABLE_ZFH_EXTENSION
    case H:
    {
        float16_t r;
        if (kind == 0)
            r = i32_to_f16(v);
        else if (kind == 1)
            r = ui32_to_f16(uv);
        else if (kind == 2)
            r = i64_to_f16((int64_t)v);
        else
            r = ui64_to_f16((uint64_t)uv);
        sf_end();
        fpr_write_h(rd, r.v);
        return;
    }
#endif
#ifdef CONFIG_ENABLE_D_EXTENSION
    case D:
    {
        float64_t r;
        if (kind == 0)
            r = i32_to_f64(v);
        else if (kind == 1)
            r = ui32_to_f64(uv);
        else if (kind == 2)
            r = i64_to_f64((int64_t)v);
        else
            r = ui64_to_f64((uint64_t)uv);
        sf_end();
        fpr_write_d(rd, r.v);
        return;
    }
#endif
#ifdef CONFIG_ENABLE_Q_EXTENSION
    case Q:
    {
        float128_t r;
        if (kind == 0)
            i32_to_f128M(v, &r);
        else if (kind == 1)
            ui32_to_f128M(uv, &r);
        else if (kind == 2)
            i64_to_f128M((int64_t)v, &r);
        else
            ui64_to_f128M((uint64_t)uv, &r);
        sf_end();
        fpr_write_q(rd, &r);
        return;
    }
#endif
    default:
        sf_end();
        fatal("fcvt.int->fp: unsupported fmt=%u", fmt);
    }
}

/* fp -> fp : dfmt = dest precision, sfmt = source precision */
static void
fcvt_fp_fp(uint32_t dfmt, uint32_t sfmt, uint32_t rs1, uint32_t rm, uint32_t rd)
{
    if (dfmt == S)
    {
#if defined(CONFIG_ENABLE_ZFH_EXTENSION)
        if (sfmt == H)
        {
            float16_t a = { fpr_read_h(rs1) };
            sf_begin(rm);
            float32_t r = f16_to_f32(a);
            if (is_nan_s(r.v)) r.v = (r.v & 0x80000000u) | 0x7FC00000u;
            sf_end();
            fpr_write_s(rd, r.v);
            return;
        }
#endif
#if defined(CONFIG_ENABLE_D_EXTENSION)
        if (sfmt == D)
        {
            float64_t a = { fpr_read_d(rs1) };
            sf_begin(rm);
            float32_t r = f64_to_f32(a);
            if (is_nan_s(r.v)) r.v = (r.v & 0x80000000u) | 0x7FC00000u;
            sf_end();
            fpr_write_s(rd, r.v);
            return;
        }
#endif
#if defined(CONFIG_ENABLE_Q_EXTENSION)
        if (sfmt == Q)
        {
            float128_t a;
            fpr_read_q(rs1, &a);
            sf_begin(rm);
            float32_t r = f128M_to_f32(&a);
            if (is_nan_s(r.v)) r.v = (r.v & 0x80000000u) | 0x7FC00000u;
            sf_end();
            fpr_write_s(rd, r.v);
            return;
        }
#endif
        fatal("fcvt S<-%u: unsupported", sfmt);
    }
    else if (dfmt == D)
    {
#if defined(CONFIG_ENABLE_D_EXTENSION)
        if (sfmt == S)
        {
            float32_t a = { fpr_read_s(rs1) };
            sf_begin(rm);
            float64_t r = f32_to_f64(a);
            if (is_nan_d(r.v))
                r.v = (r.v & 0x8000000000000000ULL) | 0x7FF8000000000000ULL;
            sf_end();
            fpr_write_d(rd, r.v);
            return;
        }
#if defined(CONFIG_ENABLE_Q_EXTENSION)
        if (sfmt == Q)
        {
            float128_t a;
            fpr_read_q(rs1, &a);
            sf_begin(rm);
            float64_t r = f128M_to_f64(&a);
            if (is_nan_d(r.v))
                r.v = (r.v & 0x8000000000000000ULL) | 0x7FF8000000000000ULL;
            sf_end();
            fpr_write_d(rd, r.v);
            return;
        }
#endif
#endif
#if defined(CONFIG_ENABLE_ZFH_EXTENSION)
        if (sfmt == H)
        {
            float16_t a = { fpr_read_h(rs1) };
            sf_begin(rm);
            float64_t r = f16_to_f64(a);
            if (is_nan_d(r.v))
                r.v = (r.v & 0x8000000000000000ULL) | 0x7FF8000000000000ULL;
            sf_end();
            fpr_write_d(rd, r.v);
            return;
        }
#endif
        fatal("fcvt D<-%u: unsupported", sfmt);
    }
    else if (dfmt == Q)
    {
#if defined(CONFIG_ENABLE_Q_EXTENSION)
        if (sfmt == S)
        {
            float32_t a = { fpr_read_s(rs1) };
            float128_t r;
            f32_to_f128M(a, &r);
            fpr_write_q(rd, &r);
            return;
        }
#if defined(CONFIG_ENABLE_D_EXTENSION)
        if (sfmt == D)
        {
            float64_t a = { fpr_read_d(rs1) };
            float128_t r;
            f64_to_f128M(a, &r);
            fpr_write_q(rd, &r);
            return;
        }
#endif
#if defined(CONFIG_ENABLE_ZFH_EXTENSION)
        if (sfmt == H)
        {
            float16_t a = { fpr_read_h(rs1) };
            float128_t r;
            f16_to_f128M(a, &r);
            fpr_write_q(rd, &r);
            return;
        }
#endif
#endif
        fatal("fcvt Q<-%u: unsupported", sfmt);
    }
    else if (dfmt == H)
    {
#if defined(CONFIG_ENABLE_ZFH_EXTENSION)
        if (sfmt == S)
        {
            float32_t a = { fpr_read_s(rs1) };
            sf_begin(rm);
            float16_t r = f32_to_f16(a);
            if (is_nan_h(r.v)) r.v = (r.v & 0x8000u) | 0x7E00u;
            sf_end();
            fpr_write_h(rd, r.v);
            return;
        }
#if defined(CONFIG_ENABLE_D_EXTENSION)
        if (sfmt == D)
        {
            float64_t a = { fpr_read_d(rs1) };
            sf_begin(rm);
            float16_t r = f64_to_f16(a);
            if (is_nan_h(r.v)) r.v = (r.v & 0x8000u) | 0x7E00u;
            sf_end();
            fpr_write_h(rd, r.v);
            return;
        }
#endif
#if defined(CONFIG_ENABLE_Q_EXTENSION)
        if (sfmt == Q)
        {
            float128_t a;
            fpr_read_q(rs1, &a);
            sf_begin(rm);
            float16_t r = f128M_to_f16(&a);
            if (is_nan_h(r.v)) r.v = (r.v & 0x8000u) | 0x7E00u;
            sf_end();
            fpr_write_h(rd, r.v);
            return;
        }
#endif
#endif
        fatal("fcvt H<-%u: unsupported", sfmt);
    }
    else
    {
        fatal("fcvt.fp->fp: unsupported dest fmt=%u", dfmt);
    }
}

/* ----------------------------- */
/* Public dispatchers            */
/* ----------------------------- */

static void
do_fadd(uint32_t fmt, uint32_t rs2, uint32_t rs1, uint32_t rm, uint32_t rd)
{
    switch (fmt)
    {
    case S:
        s_add(rs2, rs1, rm, rd);
        break;
#ifdef CONFIG_ENABLE_ZFH_EXTENSION
    case H:
        h_add(rs2, rs1, rm, rd);
        break;
#endif
#ifdef CONFIG_ENABLE_D_EXTENSION
    case D:
        d_add(rs2, rs1, rm, rd);
        break;
#endif
#ifdef CONFIG_ENABLE_Q_EXTENSION
    case Q:
        q_binop(&f128M_add, rs2, rs1, rm, rd);
        break;
#endif
    default:
        fatal("fadd: unsupported fmt=%u", fmt);
    }
}

static void
do_fsub(uint32_t fmt, uint32_t rs2, uint32_t rs1, uint32_t rm, uint32_t rd)
{
    switch (fmt)
    {
    case S:
        s_sub(rs2, rs1, rm, rd);
        break;
#ifdef CONFIG_ENABLE_ZFH_EXTENSION
    case H:
        h_sub(rs2, rs1, rm, rd);
        break;
#endif
#ifdef CONFIG_ENABLE_D_EXTENSION
    case D:
        d_sub(rs2, rs1, rm, rd);
        break;
#endif
#ifdef CONFIG_ENABLE_Q_EXTENSION
    case Q:
        q_binop(&f128M_sub, rs2, rs1, rm, rd);
        break;
#endif
    default:
        fatal("fsub: unsupported fmt=%u", fmt);
    }
}

static void
do_fmul(uint32_t fmt, uint32_t rs2, uint32_t rs1, uint32_t rm, uint32_t rd)
{
    switch (fmt)
    {
    case S:
        s_mul(rs2, rs1, rm, rd);
        break;
#ifdef CONFIG_ENABLE_ZFH_EXTENSION
    case H:
        h_mul(rs2, rs1, rm, rd);
        break;
#endif
#ifdef CONFIG_ENABLE_D_EXTENSION
    case D:
        d_mul(rs2, rs1, rm, rd);
        break;
#endif
#ifdef CONFIG_ENABLE_Q_EXTENSION
    case Q:
        q_binop(&f128M_mul, rs2, rs1, rm, rd);
        break;
#endif
    default:
        fatal("fmul: unsupported fmt=%u", fmt);
    }
}

static void
do_fdiv(uint32_t fmt, uint32_t rs2, uint32_t rs1, uint32_t rm, uint32_t rd)
{
    switch (fmt)
    {
    case S:
        s_div(rs2, rs1, rm, rd);
        break;
#ifdef CONFIG_ENABLE_ZFH_EXTENSION
    case H:
        h_div(rs2, rs1, rm, rd);
        break;
#endif
#ifdef CONFIG_ENABLE_D_EXTENSION
    case D:
        d_div(rs2, rs1, rm, rd);
        break;
#endif
#ifdef CONFIG_ENABLE_Q_EXTENSION
    case Q:
        q_binop(&f128M_div, rs2, rs1, rm, rd);
        break;
#endif
    default:
        fatal("fdiv: unsupported fmt=%u", fmt);
    }
}

static void
do_fsqrt(uint32_t fmt, uint32_t rs1, uint32_t rm, uint32_t rd)
{
    switch (fmt)
    {
    case S:
        s_sqrt(rs1, rm, rd);
        break;
#ifdef CONFIG_ENABLE_ZFH_EXTENSION
    case H:
        h_sqrt(rs1, rm, rd);
        break;
#endif
#ifdef CONFIG_ENABLE_D_EXTENSION
    case D:
        d_sqrt(rs1, rm, rd);
        break;
#endif
#ifdef CONFIG_ENABLE_Q_EXTENSION
    case Q:
        q_sqrt(rs1, rm, rd);
        break;
#endif
    default:
        fatal("fsqrt: unsupported fmt=%u", fmt);
    }
}

static void
do_fsgnj(uint32_t fmt, uint32_t rs2, uint32_t rs1, uint32_t op, uint32_t rd)
{
    switch (fmt)
    {
    case S:
        s_fsgnj(rs2, rs1, op, rd);
        break;
#ifdef CONFIG_ENABLE_ZFH_EXTENSION
    case H:
        h_fsgnj(rs2, rs1, op, rd);
        break;
#endif
#ifdef CONFIG_ENABLE_D_EXTENSION
    case D:
        d_fsgnj(rs2, rs1, op, rd);
        break;
#endif
#ifdef CONFIG_ENABLE_Q_EXTENSION
    case Q:
        q_fsgnj(rs2, rs1, op, rd);
        break;
#endif
    default:
        fatal("fsgnj: unsupported fmt=%u", fmt);
    }
}

static void
do_fminmax(uint32_t fmt, uint32_t rs2, uint32_t rs1, uint32_t op, uint32_t rd)
{
    switch (fmt)
    {
    case S:
        if (op == 0)
            s_fmin(rs2, rs1, rd);
        else
            s_fmax(rs2, rs1, rd);
        break;
#ifdef CONFIG_ENABLE_ZFH_EXTENSION
    case H:
        if (op == 0)
            h_fmin(rs2, rs1, rd);
        else
            h_fmax(rs2, rs1, rd);
        break;
#endif
#ifdef CONFIG_ENABLE_D_EXTENSION
    case D:
        if (op == 0)
            d_fmin(rs2, rs1, rd);
        else
            d_fmax(rs2, rs1, rd);
        break;
#endif
#ifdef CONFIG_ENABLE_Q_EXTENSION
    case Q:
        if (op == 0)
            q_fmin(rs2, rs1, rd);
        else
            q_fmax(rs2, rs1, rd);
        break;
#endif
    default:
        fatal("fmin/fmax: unsupported fmt=%u", fmt);
    }
}

static void
do_fcmp(uint32_t fmt, uint32_t rs2, uint32_t rs1, uint32_t op, uint32_t rd)
{
    switch (fmt)
    {
    case S:
        s_fcmp(op, rs2, rs1, rd);
        break;
#ifdef CONFIG_ENABLE_ZFH_EXTENSION
    case H:
        h_fcmp(op, rs2, rs1, rd);
        break;
#endif
#ifdef CONFIG_ENABLE_D_EXTENSION
    case D:
        d_fcmp(op, rs2, rs1, rd);
        break;
#endif
#ifdef CONFIG_ENABLE_Q_EXTENSION
    case Q:
        q_fcmp(op, rs2, rs1, rd);
        break;
#endif
    default:
        fatal("fcmp: unsupported fmt=%u", fmt);
    }
}

static void
do_fclass(uint32_t fmt, uint32_t rs1, uint32_t rd)
{
    switch (fmt)
    {
    case S:
        s_fclass(rs1, rd);
        break;
#ifdef CONFIG_ENABLE_ZFH_EXTENSION
    case H:
        h_fclass(rs1, rd);
        break;
#endif
#ifdef CONFIG_ENABLE_D_EXTENSION
    case D:
        d_fclass(rs1, rd);
        break;
#endif
#ifdef CONFIG_ENABLE_Q_EXTENSION
    case Q:
        q_fclass(rs1, rd);
        break;
#endif
    default:
        fatal("fclass: unsupported fmt=%u", fmt);
    }
}

void
insf_r_fma(uint32_t ins, uint32_t subop)
{
    uint32_t fmt = (ins >> 25) & 3;
    uint32_t rs3 = (ins >> 27) & 0x1F;
    uint32_t rs2 = (ins >> 20) & 0x1F;
    uint32_t rs1 = (ins >> 15) & 0x1F;
    uint32_t rm = (ins >> 12) & 7;
    uint32_t rd = (ins >> 7) & 0x1F;

    switch (fmt)
    {
    case S:
        s_fma(subop, rs3, rs2, rs1, rm, rd);
        break;
#ifdef CONFIG_ENABLE_ZFH_EXTENSION
    case H:
        h_fma(subop, rs3, rs2, rs1, rm, rd);
        break;
#endif
#ifdef CONFIG_ENABLE_D_EXTENSION
    case D:
        d_fma(subop, rs3, rs2, rs1, rm, rd);
        break;
#endif
#ifdef CONFIG_ENABLE_Q_EXTENSION
    case Q:
        q_fma(subop, rs3, rs2, rs1, rm, rd);
        break;
#endif
    default:
        fatal("FMA: unsupported fmt=%u", fmt);
    }
}

void
insf_r_fpop(uint32_t ins)
{
    uint32_t op = (ins >> 27) & 0x1F;
    uint32_t fmt = (ins >> 25) & 3;
    uint32_t rs2 = (ins >> 20) & 0x1F;
    uint32_t rs1 = (ins >> 15) & 0x1F;
    uint32_t f3 = (ins >> 12) & 7;
    uint32_t rd = (ins >> 7) & 0x1F;

    switch (op)
    {
    case 0:
        do_fadd(fmt, rs2, rs1, f3, rd);
        break;
    case 1:
        do_fsub(fmt, rs2, rs1, f3, rd);
        break;
    case 2:
        do_fmul(fmt, rs2, rs1, f3, rd);
        break;
    case 3:
        do_fdiv(fmt, rs2, rs1, f3, rd);
        break;
    case 4:
        do_fsgnj(fmt, rs2, rs1, f3, rd);
        break;
    case 5:
        do_fminmax(fmt, rs2, rs1, f3, rd);
        break;
    case 11:
        do_fsqrt(fmt, rs1, f3, rd);
        break;
    case 20:
        do_fcmp(fmt, rs2, rs1, f3, rd);
        break;
    case 28: /* fmv.x.* (funct3=0) or fclass (funct3=1) */
        if (f3 == 0)
        {
            /* fmv.x.* : fp -> integer, raw bits copied (RV32 keeps low 32) */
            switch (fmt)
            {
            case S:
                reg_write(rd, fpr_read_s(rs1));
                break;
#ifdef CONFIG_ENABLE_ZFH_EXTENSION
            case H:
                /* FMV.X.H sign-extends bit 15 into the upper XLEN-16 bits */
                reg_write(rd, (uint32_t)(int16_t)fpr_read_h(rs1));
                break;
#endif
#ifdef CONFIG_ENABLE_D_EXTENSION
            case D:
                reg_write(rd, (uint32_t)fpr_read_d(rs1));
                break;
#endif
#ifdef CONFIG_ENABLE_Q_EXTENSION
            case Q:
                reg_write(rd, (uint32_t)fpr[2 * rs1]);
                break;
#endif
            default:
                fatal("fmv.x.*: unsupported fmt=%u", fmt);
            }
        }
        else if (f3 == 1)
        {
            do_fclass(fmt, rs1, rd);
        }
        else
        {
            fatal("FP-OP op=28: invalid funct3=%u", f3);
        }
        break;
    case 30: /* fmv.*.x : integer -> fp */
        if (f3 != 0 || rs2 != 0)
            fatal("fmv.*.x: invalid f3=%u rs2=%u", f3, rs2);
        switch (fmt)
        {
        case S:
            fpr_write_s(rd, reg_read(rs1));
            break;
#ifdef CONFIG_ENABLE_ZFH_EXTENSION
        case H:
            fpr_write_h(rd, (uint16_t)reg_read(rs1));
            break;
#endif
#ifdef CONFIG_ENABLE_D_EXTENSION
        case D:
            fpr_write_d(rd, (uint64_t)reg_read(rs1));
            break;
#endif
#ifdef CONFIG_ENABLE_Q_EXTENSION
        case Q:
        {
            float128_t r;
            r.v[0] = reg_read(rs1);
            r.v[1] = 0;
            fpr_write_q(rd, &r);
            break;
        }
#endif
        default:
            fatal("fmv.*.x: unsupported fmt=%u", fmt);
        }
        break;
    /* FCVT: top-5 bits distinguish direction */
    case 24: /* fp -> int */
        reg_write(rd, fcvt_fp_int(fmt, rs2, rs1, f3));
        break;
    case 26: /* int -> fp */
        fcvt_int_fp(fmt, rs2, rs1, f3, rd);
        break;
    case 8: /* fp -> fp : rs2 = source precision */
        fcvt_fp_fp(fmt, rs2, rs1, f3, rd);
        break;
    default:
        fatal("unsupported FP-OP op=%u ins=0x%08x", op, ins);
    }
}

#endif // CONFIG_ENABLE_F_EXTENSION
