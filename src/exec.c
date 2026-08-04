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

#include <stdint.h>

#include <config.h>

#include <basic_instructions.h>
#include <emu.h>
#include <exec.h>
#include <logger.h>

#ifdef CONFIG_ENABLE_C_EXTENSION
#include <extension/c_extension.h>
#endif

#ifdef CONFIG_ENABLE_M_EXTENSION
#include <extension/m_extension.h>
#endif

#ifdef CONFIG_ENABLE_A_EXTENSION
#include <extension/a_extension.h>
#endif

#ifdef CONFIG_ENABLE_ZIFENCEI_EXTENSION
#include <extension/zifencei_extension.h>
#endif

#ifdef CONFIG_ENABLE_ZICSR_EXTENSION
#include <extension/system.h>
#include <extension/zicsr_extension.h>
#endif

#ifdef CONFIG_ENABLE_ZICOND_EXTENSION
#include <extension/zicond_extension.h>
#endif

#ifdef CONFIG_ENABLE_F_EXTENSION
#include <extension/f_extension.h>
#endif

#ifdef CONFIG_ENABLE_M_EXTENSION
static void (*m_ins_optable[8][128])(uint32_t, uint32_t, uint32_t) = {
    [0] = {
        [0b0000001] = &insm_r_mul,
    },
    [1] = {
        [0b0000001] = &insm_r_mulh,
    },
    [2] = {
        [0b0000001] = &insm_r_mulsu,
    },
    [3] = {
        [0b0000001] = &insm_r_mulu,
    },
    [4] = {
        [0b0000001] = &insm_r_div,
    },
    [5] = {
        [0b0000001] = &insm_r_divu,
    },
    [6] = {
        [0b0000001] = &insm_r_rem,
    },
    [7] = {
        [0b0000001] = &insm_r_remu,
    },
};
#endif

uint32_t g_prev_ins_pc = 0;

void
exec(uint32_t ins)
{
    uint32_t opcode = get_opcode(ins);
    uint32_t funct3 = get_funct3(ins);
    uint32_t funct7 = get_funct7(ins);
    uint32_t rs1 = get_rs1(ins);
    uint32_t rs2 = get_rs2(ins);
    uint32_t rd = get_rd(ins);

#ifdef CONFIG_ENABLE_ZICSR_EXTENSION
    // Instruction-fetch alignment (IALIGN): 2 bytes if misa.C is set (RVC),
    // otherwise 4 bytes. A misaligned fetch raises CAUSE_MISALIGNED_FETCH.
    if ((g_state.pc & (misa_c_enabled() ? 1U : 3U)) != 0)
    {
        // mtval = the misaligned fetch address; MEPC = the control-transfer
        // instruction whose target was misaligned (spike-compatible).
        raise_exception_pc(CAUSE_MISALIGNED_FETCH, g_state.pc, g_prev_ins_pc);
        return;
    }
#endif

// Compressed (RVC) instructions: only legal when misa.C is set and the
// address is 2-byte aligned. The riscv-tests M-mode programs enable C only
// inside explicit `.option rvc` regions (and toggle misa.C), so we gate the
// 16-bit decode path on the runtime misa.C bit rather than the build option.
// exec_c_insn() takes ownership of PC advancing for this 16-bit instruction
// (it compensates for the main loop's unconditional `pc += 4`), so we must not
// apply PC_BACKWARD here.
#ifdef CONFIG_ENABLE_C_EXTENSION
    if (likely(misa_c_enabled() && (opcode & 3) != 3))
    {
        exec_c_insn((uint16_t)(ins & 0xFFFF));
        return;
    }
#endif

    switch (opcode) // sort by hotness
    {
    // R format
    case 0b0110011:
    {
        if (unlikely(funct7 == 0b0000001))
        {
#ifdef CONFIG_ENABLE_M_EXTENSION
            m_ins_optable[funct3][funct7](rs2, rs1, rd);
#else
            fatal("unsupported M extension instruction");
#endif
        }
#ifdef CONFIG_ENABLE_ZICOND_EXTENSION
        else if (unlikely(funct7 == 0b0000111))
        {
            if (unlikely(funct3 == 0b101))
                ins_zicond_czero_eqz(rs1, rs2, rd);
            else if (unlikely(funct3 == 0b111))
                ins_zicond_czero_nez(rs1, rs2, rd);
            else
                fatal("invalid ZICOND funct3");
        }
#endif
        else
        {
            switch (funct3)
            {
            case 0: // ADD / SUB
                if (likely(funct7 == 0b0000000))
                    insi_r_add(rs2, rs1, rd);
                else if (funct7 == 0b0100000)
                    insi_r_sub(rs2, rs1, rd);
                else
                    fatal("invalid R-type funct7");
                break;

            case 1: // SLL
                if (likely(funct7 == 0b0000000))
                    insi_r_sll(rs2, rs1, rd);
                else
                    fatal("invalid R-type funct7");
                break;

            case 2: // SLT
                if (likely(funct7 == 0b0000000))
                    insi_r_slt(rs2, rs1, rd);
                else
                    fatal("invalid R-type funct7");
                break;

            case 3: // SLTU
                if (likely(funct7 == 0b0000000))
                    insi_r_sltu(rs2, rs1, rd);
                else
                    fatal("invalid R-type funct7");
                break;

            case 4: // XOR
                if (likely(funct7 == 0b0000000))
                    insi_r_xor(rs2, rs1, rd);
                else
                    fatal("invalid R-type funct7");
                break;

            case 5: // SRL / SRA
                if (likely(funct7 == 0b0000000))
                    insi_r_srl(rs2, rs1, rd);
                else if (funct7 == 0b0100000)
                    insi_r_sra(rs2, rs1, rd);
                else
                    fatal("invalid R-type funct7");
                break;

            case 6: // OR
                if (likely(funct7 == 0b0000000))
                    insi_r_or(rs2, rs1, rd);
                else
                    fatal("invalid R-type funct7");
                break;

            case 7: // AND
                if (likely(funct7 == 0b0000000))
                    insi_r_and(rs2, rs1, rd);
                else
                    fatal("invalid R-type funct7");
                break;

            default:
                fatal("invalid R-type funct3");
            }
        }
        break;
    }
    // I format
    case 0b0010011:
    {
        uint32_t imm = sign_extend_12((ins >> 20) & 0xFFF);
        uint32_t shamt = imm & 0x1F;

        switch (funct3)
        {
        case 0: // ADDI
            insi_i_addi(imm, rs1, rd);
            break;

        case 1: // SLLI
            // In RV32, SLLI requires funct7 == 0. A nonzero funct7 (e.g. the
            // illegal shamt[5] set, as in rv32mi/shamt.S) is an illegal
            // instruction.
            if (unlikely(funct7 != 0b0000000))
            {
                raise_exception(CAUSE_ILLEGAL_INSTRUCTION, ins);
                break;
            }
            insi_i_slli(shamt, rs1, rd);
            break;

        case 2: // SLTI
            insi_i_slti(imm, rs1, rd);
            break;

        case 3: // SLTIU
            insi_i_sltiu(imm, rs1, rd);
            break;

        case 4: // XORI
            insi_i_xori(imm, rs1, rd);
            break;

        case 5: // SRLI / SRAI
            if (likely(funct7 == 0b0000000))
                insi_i_srli(shamt, rs1, rd);
            else if (funct7 == 0b0100000)
                insi_i_srai(shamt, rs1, rd);
            else
                fatal("invalid SRLI/SRAI funct7");
            break;

        case 6: // ORI
            insi_i_ori(imm, rs1, rd);
            break;

        case 7: // ANDI
            insi_i_andi(imm, rs1, rd);
            break;

        default:
            fatal("invalid I-type funct3");
        }
        break;
    }
    // I format, LOAD instructions
    case 0b0000011:
    {
        uint32_t imm = sign_extend_12((ins >> 20) & 0xFFF);

        switch (funct3)
        {
        case 0: // LB
            insi_i_lb(imm, rs1, rd);
            break;

        case 1: // LH
            insi_i_lh(imm, rs1, rd);
            break;

        case 2: // LW
            insi_i_lw(imm, rs1, rd);
            break;

        case 4: // LBU
            insi_i_lbu(imm, rs1, rd);
            break;

        case 5: // LHU
            insi_i_lhu(imm, rs1, rd);
            break;

        default:
            fatal("invalid load instruction");
        }
        break;
    }
    // S format, STORE instructions
    case 0b0100011:
    {
        uint32_t imm = ((ins >> 25) & 0x7F) << 5;
        imm |= ((ins >> 7) & 0x1F);
        imm = sign_extend_12(imm);

        switch (funct3)
        {
        case 0: // SB
            insi_s_sb(imm, rs2, rs1);
            break;

        case 1: // SH
            insi_s_sh(imm, rs2, rs1);
            break;

        case 2: // SW
            insi_s_sw(imm, rs2, rs1);
            break;

        default:
            fatal("invalid store instruction");
        }
        break;
    }
#ifdef CONFIG_ENABLE_F_EXTENSION
    // FP load (FLW / FLD / FLQ / FLH)
    case 0b0000111:
    {
        if (fpu_fs_off())
        {
            raise_exception(CAUSE_ILLEGAL_INSTRUCTION, ins);
            break;
        }
        uint32_t imm = sign_extend_12((ins >> 20) & 0xFFF);
        switch (funct3)
        {
        case 1:
            insf_flh(imm, rs1, rd);
            break;
        case 2:
            insf_flw(imm, rs1, rd);
            break;
        case 3:
            insf_fld(imm, rs1, rd);
            break;
        case 4:
            insf_flq(imm, rs1, rd);
            break;
        default:
            fatal("invalid FP load instruction");
            break;
        }
        break;
    }
    // FP store (FSW / FSD / FSQ / FSH)
    case 0b0100111:
    {
        if (fpu_fs_off())
        {
            raise_exception(CAUSE_ILLEGAL_INSTRUCTION, ins);
            break;
        }
        uint32_t imm = ((ins >> 25) & 0x7F) << 5;
        imm |= ((ins >> 7) & 0x1F);
        imm = sign_extend_12(imm);
        switch (funct3)
        {
        case 1:
            insf_fsh(imm, rs1, rs2);
            break;
        case 2:
            insf_fsw(imm, rs1, rs2);
            break;
        case 3:
            insf_fsd(imm, rs1, rs2);
            break;
        case 4:
            insf_fsq(imm, rs1, rs2);
            break;
        default:
            fatal("invalid FP store instruction");
            break;
        }
        break;
    }
    // FMA: fmadd / fmsub / fnmsub / fnmadd
    case 0b1000011:
        if (fpu_fs_off()) { raise_exception(CAUSE_ILLEGAL_INSTRUCTION, ins); break; }
        insf_r_fma(ins, 0);
        break; // fmadd
    case 0b1000111:
        if (fpu_fs_off()) { raise_exception(CAUSE_ILLEGAL_INSTRUCTION, ins); break; }
        insf_r_fma(ins, 1);
        break; // fmsub
    case 0b1001011:
        if (fpu_fs_off()) { raise_exception(CAUSE_ILLEGAL_INSTRUCTION, ins); break; }
        insf_r_fma(ins, 2);
        break; // fnmsub
    case 0b1001111:
        if (fpu_fs_off()) { raise_exception(CAUSE_ILLEGAL_INSTRUCTION, ins); break; }
        insf_r_fma(ins, 3);
        break; // fnmadd
    // FP-OP
    case 0b1010011:
        if (fpu_fs_off()) { raise_exception(CAUSE_ILLEGAL_INSTRUCTION, ins); break; }
        insf_r_fpop(ins);
        break;
#endif
#ifdef CONFIG_ENABLE_A_EXTENSION
    case 0b0101111:
    {
        if (likely(funct3 == 0x2))
        {
            uint32_t funct5 = (ins >> 27) & 0x1F;
            switch (funct5)
            {
            case 0x00:
            {
                insa_r_amoadd_w(rs1, rs2, rd);
                break;
            }
            case 0x01:
            {
                insa_r_amoswap_w(rs1, rs2, rd);
                break;
            }
            case 0x02:
            {
                insa_r_lr_w(rs1, rd);
                break;
            }
            case 0x03:
            {
                insa_r_sc_w(rs1, rs2, rd);
                break;
            }
            case 0x04:
            {
                insa_r_amoxor_w(rs1, rs2, rd);
                break;
            }
            case 0x08:
            {
                insa_r_amoor_w(rs1, rs2, rd);
                break;
            }
            case 0x0C:
            {
                insa_r_amoand_w(rs1, rs2, rd);
                break;
            }
            case 0x10:
            {
                insa_r_amomin_w(rs1, rs2, rd);
                break;
            }
            case 0x14:
            {
                insa_r_amomax_w(rs1, rs2, rd);
                break;
            }
            case 0x18:
            {
                insa_r_amominu_w(rs1, rs2, rd);
                break;
            }
            case 0x1C:
            {
                insa_r_amomaxu_w(rs1, rs2, rd);
                break;
            }
            default:
                info("Invalid atomic: funct5=0x%02x, ins=0x%08x\n", funct5,
                     ins);
                fatal("invalid atomic instruction");
            }
        }
        else
        {
            fatal("invalid atomic instruction");
        }
        break;
    }
#endif
    // B format, BRANCH instructions
    case 0b1100011:
    {
        uint32_t imm = ((ins >> 31) & 0x1) << 12;
        imm |= ((ins >> 25) & 0x3F) << 5;
        imm |= ((ins >> 8) & 0xF) << 1;
        imm |= ((ins >> 7) & 0x1) << 11;
        imm = sign_extend_13(imm);

        switch (funct3)
        {
        case 0: // BEQ
            insi_b_beq(imm, rs2, rs1);
            break;

        case 1: // BNE
            insi_b_bne(imm, rs2, rs1);
            break;

        case 4: // BLT
            insi_b_blt(imm, rs2, rs1);
            break;

        case 5: // BGE
            insi_b_bge(imm, rs2, rs1);
            break;

        case 6: // BLTU
            insi_b_bltu(imm, rs2, rs1);
            break;

        case 7: // BGEU
            insi_b_bgeu(imm, rs2, rs1);
            break;

        default:
            fatal("invalid branch instruction");
        }
        break;
    }

    // JAL
    case 0b1101111:
    {
        uint32_t imm = ((ins >> 31) & 0x1) << 20;
        imm |= ((ins >> 21) & 0x3FF) << 1;
        imm |= ((ins >> 20) & 0x1) << 11;
        imm |= ((ins >> 12) & 0xFF) << 12;
        imm = sign_extend_21(imm);
        insi_j_jal(imm, rd);
        break;
    }

    // LUI
    case 0b0110111:
    {
        uint32_t imm = ins & 0xFFFFF000;
        insi_u_lui(imm, rd);
        break;
    }

    // AUIPC
    case 0b0010111:
    {
        uint32_t imm = ins & 0xFFFFF000;
        insi_u_auipc(imm, rd);
        break;
    }

    // JALR
    case 0b1100111:
    {
        uint32_t imm = sign_extend_12((ins >> 20) & 0xFFF);
        insi_i_jalr(imm, rs1, rd);
        break;
    }

    // SYSTEM
    case 0b1110011:
    {
        uint32_t imm = (ins >> 20) & 0xFFF;
#ifdef CONFIG_ENABLE_ZICSR_EXTENSION
        uint32_t csr = (ins >> 20) & 0xFFF;
#endif
        uint32_t rs1 = (ins >> 15) & 0x1F;
        uint32_t rd = (ins >> 7) & 0x1F;

        switch (funct3)
        {
        case 0:
        {
            if (rs1 == 0 && rd == 0)
            {
                switch (imm)
                {
                case 0x000: // ecall
                    insi_i_ecall();
                    break;
                case 0x001: // ebreak
                    insi_i_ebreak();
                    break;
#ifdef CONFIG_ENABLE_ZICSR_EXTENSION
                case 0x102: // sret
                    ins_sret();
                    break;
                case 0x302: // mret
                    ins_mret();
                    break;
                case 0x702: // mnret
                    ins_mnret();
                    break;
                case 0x104: // sctrclr
                    ins_sctrclr();
                    break;
                case 0x105: // wfi
                    ins_wfi();
                    break;
#endif
                default:
#ifdef CONFIG_ENABLE_ZICSR_EXTENSION
                    // sfence.vma has funct7 == 0x09 in bits[31:25]; its imm
                    // field is (0x09 << 5) | rs2, i.e. 0x120 when rs2 == 0.
                    if (funct7 == 0b0001001)
                    {
                        ins_sfence_vma();
                        break;
                    }
#endif
                    fatal("illegal system instruction imm=0x%x", imm);
                }
            }
#ifdef CONFIG_ENABLE_ZICSR_EXTENSION
            else if (funct7 == 0b0001001)
            {
                // sfence.vma with a nonzero rs1 operand (rd must still be 0).
                if (rd != 0)
                {
                    fatal("sfence.vma: rd must be 0, got rd=%d", rd);
                }
                ins_sfence_vma();
            }
#endif
            else
            {
                fatal("illegal system instruction imm=0x%x rs1=%d rd=%d", imm,
                      rs1, rd);
            }

            break;
        }

#ifdef CONFIG_ENABLE_ZICSR_EXTENSION
        case 0b001: // CSRRW
        {
            ins_zicsr_csrrw(rs1, rd, csr, ins);
            break;
        }
        case 0b010: // CSRRS
        {
            ins_zicsr_csrrs(rs1, rd, csr, ins);
            break;
        }
        case 0b011: // CSRRC
        {
            ins_zicsr_csrrc(rs1, rd, csr, ins);
            break;
        }
        case 0b101: // CSRRWI
        {
            ins_zicsr_csrrwi(rs1, rd, csr, ins);
            break;
        }
        case 0b110: // CSRRSI
        {
            ins_zicsr_csrrsi(rs1, rd, csr, ins);
            break;
        }
        case 0b111: // CSRRCI
        {
            ins_zicsr_csrrci(rs1, rd, csr, ins);
            break;
        }
#endif
        default:
        {
            fatal("unsupported CSR instruction funct3=%d", funct3);
        }
        }
        break;
    }

    // fence fence.i
    case 0b0001111:
    {
        switch (funct3)
        {
        case 0:
            // FENCE
            ins_fence();
            break;
#ifdef CONFIG_ENABLE_ZIFENCEI_EXTENSION
        case 1:
            // FENCE.I
            ins_zifencei_zifencei();
            break;
#endif
        default:
            fatal("illegal fence instruction");
        }
        break;
    }

    default:
#ifdef CONFIG_ENABLE_ZICSR_EXTENSION
        raise_illegal(ins);
#else
        fatal("illegal opcode");
#endif
    }
}
