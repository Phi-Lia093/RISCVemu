#include <stdint.h>

#include <emu.h>
#include <exec.h>
#include <logger.h>
#include <ops.h>

static void (*r_ins_optable[8][128])(uint32_t, uint32_t, uint32_t) = {
    [0] = {
        [0b0000000] = &insi_r_add,
        [0b0100000] = &insi_r_sub,
        [0b0000001] = &insm_r_mul,
    },
    [1] = {
        [0b0000000] = &insi_r_sll,
        [0b0000001] = &insm_r_mulh,
    },
    [2] = {
        [0b0000000] = &insi_r_slt,
        [0b0000001] = &insm_r_mulsu,
    },
    [3] = {
        [0b0000000] = &insi_r_sltu,
        [0b0000001] = &insm_r_mulu,
    },
    [4] = {
        [0b0000000] = &insi_r_xor,
        [0b0000001] = &insm_r_div,
    },
    [5] = {
        [0b0000000] = &insi_r_srl,
        [0b0100000] = &insi_r_sra,
        [0b0000001] = &insm_r_divu,
    },
    [6] = {
        [0b0000000] = &insi_r_or,
        [0b0000001] = &insm_r_rem,
    },
    [7] = {
        [0b0000000] = &insi_r_and,
        [0b0000001] = &insm_r_remu,
    },
};

static void (*i_alu_ins_optable[8][128])(uint32_t, uint32_t, uint32_t) = {
    [0] = { [0b0000000] = &insi_i_addi },
    [1] = { [0b0000000] = &insi_i_slli },
    [2] = { [0b0000000] = &insi_i_slti },
    [3] = { [0b0000000] = &insi_i_sltiu },
    [4] = { [0b0000000] = &insi_i_xori },
    [5] = {
        [0b0000000] = &insi_i_srli,
        [0b0100000] = &insi_i_srai,
    },
    [6] = { [0b0000000] = &insi_i_ori },
    [7] = { [0b0000000] = &insi_i_andi },
};

static void (*i_mem_ins_optable[8])(uint32_t, uint32_t, uint32_t) = {
    [0] = &insi_i_lb,  [1] = &insi_i_lh,  [2] = &insi_i_lw,
    [4] = &insi_i_lbu, [5] = &insi_i_lhu,
};

static void (*s_mem_ins_optable[8])(uint32_t, uint32_t, uint32_t) = {
    [0] = &insi_s_sb,
    [1] = &insi_s_sh,
    [2] = &insi_s_sw,
};

static void (*b_ins_optable[8])(uint32_t, uint32_t, uint32_t) = {
    [0] = &insi_b_beq, [1] = &insi_b_bne,  [4] = &insi_b_blt,
    [5] = &insi_b_bge, [6] = &insi_b_bltu, [7] = &insi_b_bgeu,
};

static inline uint32_t
sign_extend_12(uint32_t imm)
{
    if (imm & 0x800) return imm | 0xFFFFF000;
    return imm;
}

static inline uint32_t
sign_extend_13(uint32_t imm)
{
    if (imm & 0x1000) return imm | 0xFFFFE000;
    return imm;
}

static inline uint32_t
sign_extend_21(uint32_t imm)
{
    if (imm & 0x100000) return imm | 0xFFE00000;
    return imm;
}

void
exec(uint32_t ins)
{
    uint32_t opcode = get_opcode(ins);
    uint32_t funct3 = get_funct3(ins);
    uint32_t funct7 = get_funct7(ins);
    uint32_t rs1 = get_rs1(ins);
    uint32_t rs2 = get_rs2(ins);
    uint32_t rd = get_rd(ins);

    if ((opcode & 3) != 3) error("unsupported COMPACT extension");

    switch (opcode)
    {
    case 0b0110011:
        r_ins_optable[funct3][funct7](rs2, rs1, rd);
        break;

    case 0b0010011:
    {
        uint32_t imm = sign_extend_12((ins >> 20) & 0xFFF);
        uint32_t shamt = imm & 0x1F;

        if (funct3 == 1)
        {
            if ((imm & 0xFE0) != 0) error("invalid SLLI shamt");
            insi_i_slli(shamt, rs1, rd);
        }
        else if (funct3 == 5)
        {
            if ((imm & 0xFE0) != 0) error("invalid shift shamt");
            if (funct7 == 0b0000000)
                insi_i_srli(shamt, rs1, rd);
            else if (funct7 == 0b0100000)
                insi_i_srai(shamt, rs1, rd);
            else
                error("invalid SRLI/SRAI funct7");
        }
        else
        {
            i_alu_ins_optable[funct3][0](imm, rs1, rd);
        }
        break;
    }

    case 0b0000011:
    {
        uint32_t imm = sign_extend_12((ins >> 20) & 0xFFF);
        i_mem_ins_optable[funct3](imm, rs1, rd);
        break;
    }

    case 0b0100011:
    {
        uint32_t imm = ((ins >> 25) & 0x7F) << 5;
        imm |= ((ins >> 7) & 0x1F);
        imm = sign_extend_12(imm);
        s_mem_ins_optable[funct3](imm, rs2, rs1);
        break;
    }

    case 0b1100011:
    {
        uint32_t imm = ((ins >> 31) & 0x1) << 12;
        imm |= ((ins >> 25) & 0x3F) << 5;
        imm |= ((ins >> 8) & 0xF) << 1;
        imm |= ((ins >> 7) & 0x1) << 11;
        imm = sign_extend_13(imm);
        b_ins_optable[funct3](imm, rs2, rs1);
        break;
    }

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

    case 0b0110111:
    {
        uint32_t imm = ins & 0xFFFFF000;
        insi_u_lui(imm, rd);
        break;
    }

    case 0b0010111:
    {
        uint32_t imm = ins & 0xFFFFF000;
        insi_u_auipc(imm, rd);
        break;
    }

    case 0b1100111:
    {
        uint32_t imm = sign_extend_12((ins >> 20) & 0xFFF);
        insi_i_jalr(imm, rs1, rd);
        break;
    }

    case 0b1110011:
    {
        uint32_t imm = sign_extend_12((ins >> 20) & 0xFFF);

        if (funct3 == 0)
        {
            if (imm == 0)
                insi_i_ecall();
            else if (imm == 1)
                insi_i_ebreak();
            else
                error("illegal system instruction");
        }
        else
        {
            error("unsupported CSR instruction");
        }
        break;
    }

    default:
        error("illegal opcode");
    }
}
