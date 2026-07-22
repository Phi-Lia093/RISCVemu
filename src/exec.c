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
#include <extension/zicsr_extension.h>
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

void
exec(uint32_t ins)
{
    uint32_t opcode = get_opcode(ins);
    uint32_t funct3 = get_funct3(ins);
    uint32_t funct7 = get_funct7(ins);
    uint32_t rs1 = get_rs1(ins);
    uint32_t rs2 = get_rs2(ins);
    uint32_t rd = get_rd(ins);

// we now support COMPACT extension ┌(┌^o^)┐
#ifdef CONFIG_ENABLE_C_EXTENSION
    if (likely((opcode & 3) != 3))
    {
        uint16_t c_ins = (uint16_t)(ins & 0xFFFF);
        g_state.pc -= 2;
        return;
    }
#endif
#ifndef CONFIG_ENABLE_C_EXTENSION
    if (unlikely((opcode & 3) != 3))
    {
        fatal("unsupported COMPACT extension");
        return;
    }
#endif

    switch (opcode) // sort by hotness
    {
    // R format
    case 0b0110011:
    {
#ifdef CONFIG_ENABLE_M_EXTENSION
        // ALL M extension instructions
        if (unlikely(funct7 == 0b0000001))
        {
            m_ins_optable[funct3][funct7](rs2, rs1, rd);
        }
#endif
#ifndef CONFIG_ENABLE_M_EXTENSION
        // ALL M extension instructions
        if (unlikely(funct7 == 0b0000001))
        {
            fatal("unsupported M extension instruction");
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
            case 0x0A:
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
            default:
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
        uint32_t imm
            = (ins >> 20)
              & 0xFFF; // 不需要 sign_extend，对于 SYSTEM 指令是无符号的
        uint32_t csr = (ins >> 20) & 0xFFF;
        uint32_t rs1 = (ins >> 15) & 0x1F;
        uint32_t rd = (ins >> 7) & 0x1F;

        switch (funct3)
        {
        case 0: // 特权指令 (ecall, ebreak, sret, mret, wfi, sfence.vma 等)
        {
            // 先提取关键字段判断
            if (rs1 == 0 && rd == 0)
            { // 注意：对于 SYSTEM 指令，rd 字段也是 0
                // rs1=0, rd=0 的情况：ecall, ebreak, sret, mret, mnret, wfi,
                // sctrclr
                switch (imm)
                {
                case 0x000: // ecall
                    insi_i_ecall();
                    break;
                case 0x001: // ebreak
                    insi_i_ebreak();
                    break;
                case 0x100: // sret (正确的编码)
                    ins_sret();
                    break;
                case 0x300: // mret (正确的编码)
                    ins_mret();
                    break;
                case 0x700: // mnret (正确的编码)
                    ins_mnret();
                    break;
                case 0x104: // sctrclr (正确的编码)
                    ins_sctrclr();
                    break;
                case 0x105: // wfi (正确的编码，0x105 不是 0x102)
                    ins_wfi();
                    break;
                default:
                    fatal("illegal system instruction imm=0x%x", imm);
                }
            }
            else if (imm == 0)
            {
                // sfence.vma: imm=0, rs1/rd 可以是非零值
                // rs1 是地址寄存器，rd 应该是 0 (但规范中 rd 字段保留)
                ins_sfence_vma();
            }
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
            ins_zicsr_csrrw(rs1, rd, csr);
            break;
        }
        case 0b010: // CSRRS
        {
            ins_zicsr_csrrs(rs1, rd, csr);
            break;
        }
        case 0b011: // CSRRC
        {
            ins_zicsr_csrrc(rs1, rd, csr);
            break;
        }
        case 0b101: // CSRRWI (rs1 是立即数 uimm)
        {
            ins_zicsr_csrrwi(rs1, rd, csr);
            break;
        }
        case 0b110: // CSRRSI
        {
            ins_zicsr_csrrsi(rs1, rd, csr);
            break;
        }
        case 0b111: // CSRRCI
        {
            ins_zicsr_csrrci(rs1, rd, csr);
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
        fatal("illegal opcode");
    }
}
