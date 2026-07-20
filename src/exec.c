#include <stdint.h>

#include <config.h>

#include <basic_instructions.h>
#include <emu.h>
#include <exec.h>
#include <logger.h>

#ifdef CONFIG_ENABLE_M_EXTENSION
#include <extension/m_extension.h>
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

    // this is C extension, no supported
    if (unlikely((opcode & 3) != 3))
    {
        error("unsupported COMPACT extension");
        return;
    }

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
            error("unsupported M extension instruction");
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
                    error("invalid R-type funct7");
                break;

            case 1: // SLL
                if (likely(funct7 == 0b0000000))
                    insi_r_sll(rs2, rs1, rd);
                else
                    error("invalid R-type funct7");
                break;

            case 2: // SLT
                if (likely(funct7 == 0b0000000))
                    insi_r_slt(rs2, rs1, rd);
                else
                    error("invalid R-type funct7");
                break;

            case 3: // SLTU
                if (likely(funct7 == 0b0000000))
                    insi_r_sltu(rs2, rs1, rd);
                else
                    error("invalid R-type funct7");
                break;

            case 4: // XOR
                if (likely(funct7 == 0b0000000))
                    insi_r_xor(rs2, rs1, rd);
                else
                    error("invalid R-type funct7");
                break;

            case 5: // SRL / SRA
                if (likely(funct7 == 0b0000000))
                    insi_r_srl(rs2, rs1, rd);
                else if (funct7 == 0b0100000)
                    insi_r_sra(rs2, rs1, rd);
                else
                    error("invalid R-type funct7");
                break;

            case 6: // OR
                if (likely(funct7 == 0b0000000))
                    insi_r_or(rs2, rs1, rd);
                else
                    error("invalid R-type funct7");
                break;

            case 7: // AND
                if (likely(funct7 == 0b0000000))
                    insi_r_and(rs2, rs1, rd);
                else
                    error("invalid R-type funct7");
                break;

            default:
                error("invalid R-type funct3");
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
            // if (unlikely((imm & 0xFE0) != 0)) error("invalid SLLI shamt");
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
            // if (unlikely((imm & 0xFE0) != 0)) error("invalid shift shamt");
            if (likely(funct7 == 0b0000000))
                insi_i_srli(shamt, rs1, rd);
            else if (funct7 == 0b0100000)
                insi_i_srai(shamt, rs1, rd);
            else
                error("invalid SRLI/SRAI funct7");
            break;

        case 6: // ORI
            insi_i_ori(imm, rs1, rd);
            break;

        case 7: // ANDI
            insi_i_andi(imm, rs1, rd);
            break;

        default:
            error("invalid I-type funct3");
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
            error("invalid load instruction");
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
            error("invalid store instruction");
        }
        break;
    }
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
            error("invalid branch instruction");
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
        uint32_t imm = sign_extend_12((ins >> 20) & 0xFFF);

        if (likely(funct3 == 0))
        {
            if (likely(imm == 0))
                insi_i_ecall();
            else if (imm == 1)
                insi_i_ebreak();
            else
                error("illegal system instruction");
        }
#ifndef CONFIG_ENABLE_ZICSR_EXTENSION
        else
        {
            error("unsupported CSR instruction");
        }
#endif
#ifdef CONFIG_ENABLE_ZICSR_EXTENSION

#endif
        break;
    }

    // fence fence.i
    case 0b0001111:
    {
        switch (funct3)
        {
        case 0:
            // FENCE
            break;
#ifdef CONFIG_ENABLE_ZIFENCEI_EXTENSION
        case 1:
            // FENCE.I
            break;
#endif
        default:
            error("illegal fence instruction");
        }
        break;
    }

    default:
        error("illegal opcode");
    }
}
