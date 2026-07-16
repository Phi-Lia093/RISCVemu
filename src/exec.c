#include <stdint.h>

#include <emu.h>
#include <exec.h>
#include <logger.h>
#include <ops.h>

// R-type instructions (opcode=0110011)
// funct7[6:0] + funct3[2:0]
static void (*r_ins_optable[8][128])(uint32_t, uint32_t, uint32_t) = {
    // funct3=000
    [0] = {
        [0b0000000] = &insi_r_add,    // ADD
        [0b0100000] = &insi_r_sub,    // SUB
        [0b0000001] = &insm_r_mul,    // MUL (M extension)
    },
    // funct3=001
    [1] = {
        [0b0000000] = &insi_r_sll,    // SLL
        [0b0000001] = &insm_r_mulh,   // MULH (M extension)
    },
    // funct3=010
    [2] = {
        [0b0000000] = &insi_r_slt,    // SLT
        [0b0000001] = &insm_r_mulsu,  // MULSU (M extension)
    },
    // funct3=011
    [3] = {
        [0b0000000] = &insi_r_sltu,   // SLTU
        [0b0000001] = &insm_r_mulu,   // MULU (M extension)
    },
    // funct3=100
    [4] = {
        [0b0000000] = &insi_r_xor,    // XOR
        [0b0000001] = &insm_r_div,    // DIV (M extension)
    },
    // funct3=101
    [5] = {
        [0b0000000] = &insi_r_srl,    // SRL
        [0b0100000] = &insi_r_sra,    // SRA
        [0b0000001] = &insm_r_divu,   // DIVU (M extension)
    },
    // funct3=110
    [6] = {
        [0b0000000] = &insi_r_or,     // OR
        [0b0000001] = &insm_r_rem,    // REM (M extension)
    },
    // funct3=111
    [7] = {
        [0b0000000] = &insi_r_and,    // AND
        [0b0000001] = &insm_r_remu,   // REMU (M extension)
    },
};

// I-type ALU instructions (opcode=0010011)
// funct7[6:0] + funct3[2:0]
static void (*i_alu_ins_optable[8][128])(uint32_t, uint32_t, uint32_t) = {
    // funct3=000 - ADDI
    [0] = {
        [0b0000000] = &insi_i_addi,   // ADDI
    },
    // funct3=001 - SLLI
    [1] = {
        [0b0000000] = &insi_i_slli,   // SLLI
    },
    // funct3=010 - SLTI
    [2] = {
        [0b0000000] = &insi_i_slti,   // SLTI
    },
    // funct3=011 - SLTIU
    [3] = {
        [0b0000000] = &insi_i_sltiu,  // SLTIU
    },
    // funct3=100 - XORI
    [4] = {
        [0b0000000] = &insi_i_xori,   // XORI
    },
    // funct3=101 - SRLI / SRAI
    [5] = {
        [0b0000000] = &insi_i_srli,   // SRLI
        [0b0100000] = &insi_i_srai,   // SRAI
    },
    // funct3=110 - ORI
    [6] = {
        [0b0000000] = &insi_i_ori,    // ORI
    },
    // funct3=111 - ANDI
    [7] = {
        [0b0000000] = &insi_i_andi,   // ANDI
    },
};

// I-type Memory instructions (opcode=0000011)
static void (*i_mem_ins_optable[8])(uint32_t, uint32_t, uint32_t) = {
    [0] = &insi_i_lb,  // LB
    [1] = &insi_i_lh,  // LH
    [2] = &insi_i_lw,  // LW
    [4] = &insi_i_lbu, // LBU
    [5] = &insi_i_lhu, // LHU
};

// S-type Store instructions (opcode=0100011)
static void (*s_mem_ins_optable[8])(uint32_t, uint32_t, uint32_t) = {
    [0] = &insi_s_sb, // SB
    [1] = &insi_s_sh, // SH
    [2] = &insi_s_sw, // SW
};

// B-type Branch instructions (opcode=1100011)
static void (*b_ins_optable[8])(uint32_t, uint32_t, uint32_t) = {
    [0] = &insi_b_beq,  // BEQ
    [1] = &insi_b_bne,  // BNE
    [4] = &insi_b_blt,  // BLT
    [5] = &insi_b_bge,  // BGE
    [6] = &insi_b_bltu, // BLTU
    [7] = &insi_b_bgeu, // BGEU
};

uint32_t
reg_read(uint32_t id)
{
    if (id == 0) return 0;
    if (id > 31)
    {
        error("register index out of bound READ");
    }
    return g_state.gpr[id];
}

void
reg_write(uint32_t id, uint32_t d)
{
    if (id == 0)
    {
        warn("access to ZERO register WRITE");
        return;
    }
    if (id > 31)
    {
        error("register index out of bound WRITE");
    }
    g_state.gpr[id] = d;
}

static inline uint32_t
sign_extend_12(uint32_t imm)
{
    if (imm & 0x800)
    {
        return imm | 0xFFFFF000;
    }
    return imm;
}

static inline uint32_t
sign_extend_13(uint32_t imm)
{
    if (imm & 0x1000)
    {
        return imm | 0xFFFFE000;
    }
    return imm;
}

static inline uint32_t
sign_extend_21(uint32_t imm)
{
    if (imm & 0x100000)
    {
        return imm | 0xFFE00000;
    }
    return imm;
}

void
exec(uint32_t ins)
{
    uint32_t opcode = get_opcode(ins);
    if ((opcode & 3) != 3)
    {
        error("unsupported COMPACT extension");
    }

    switch (opcode)
    {
    case 0b0110011: // R-type
        r_ins_optable[get_funct3(ins)][get_funct7(ins)](
            get_rs2(ins), get_rs1(ins), get_rd(ins));
        break;

    case 0b0010011: // I-type ALU
    {
        uint32_t imm = (ins >> 20) & 0xFFF;
        uint32_t funct3 = get_funct3(ins);
        uint32_t funct7 = get_funct7(ins);

        if (funct3 == 1 || funct3 == 5)
        {
            if (funct3 == 1 && (imm & 0xFE0) != 0)
            {
                error("invalid SLLI shamt");
            }
            if (funct3 == 5 && funct7 == 0b0000000 && (imm & 0xFE0) != 0)
            {
                error("invalid SRLI shamt");
            }
            if (funct3 == 5 && funct7 == 0b0100000 && (imm & 0xFE0) != 0)
            {
                error("invalid SRAI shamt");
            }
        }

        i_alu_ins_optable[funct3][funct7](imm, get_rs1(ins), get_rd(ins));
        break;
    }

    case 0b0000011: // I-type Load
    {
        uint32_t imm = sign_extend_12((ins >> 20) & 0xFFF);
        i_mem_ins_optable[get_funct3(ins)](imm, get_rs1(ins), get_rd(ins));
        break;
    }

    case 0b0100011: // S-type Store
    {
        uint32_t imm = ((ins >> 25) & 0x7F) << 5; // imm[11:5]
        imm |= ((ins >> 7) & 0x1F);               // imm[4:0]
        imm = sign_extend_12(imm);
        s_mem_ins_optable[get_funct3(ins)](imm, get_rs2(ins), get_rs1(ins));
        break;
    }

    case 0b1100011: // B-type Branch
    {
        uint32_t imm = ((ins >> 31) & 0x1) << 12; // imm[12]
        imm |= ((ins >> 25) & 0x3F) << 5;         // imm[10:5]
        imm |= ((ins >> 8) & 0xF) << 1;           // imm[4:1]
        imm |= ((ins >> 7) & 0x1) << 11;          // imm[11]
        imm = sign_extend_13(imm);
        b_ins_optable[get_funct3(ins)](imm, get_rs2(ins), get_rs1(ins));
        break;
    }

    case 0b1101111: // JAL
    {
        uint32_t imm = ((ins >> 31) & 0x1) << 20; // imm[20]
        imm |= ((ins >> 21) & 0x3FF) << 1;        // imm[10:1]
        imm |= ((ins >> 20) & 0x1) << 11;         // imm[11]
        imm |= ((ins >> 12) & 0xFF) << 12;        // imm[19:12]
        imm = sign_extend_21(imm);
        insi_j_jal(imm, get_rd(ins));
        break;
    }

    case 0b0110111: // LUI
    {
        uint32_t imm = ins & 0xFFFFF000;
        insi_u_lui(imm, get_rd(ins));
        break;
    }

    case 0b0010111: // AUIPC
    {
        uint32_t imm = ins & 0xFFFFF000;
        insi_u_auipc(imm, get_rd(ins));
        break;
    }

    case 0b1100111: // JALR
    {
        uint32_t imm = sign_extend_12((ins >> 20) & 0xFFF);
        insi_i_jalr(imm, get_rs1(ins), get_rd(ins));
        break;
    }

    case 0b1110011: // System
    {
        uint32_t imm = (ins >> 20) & 0xFFF;
        uint32_t funct3 = get_funct3(ins);

        if (funct3 == 0)
        {
            if (imm == 0)
            {
                insi_i_ecall();
            }
            else if (imm == 1)
            {
                insi_i_ebreak();
            }
            else
            {
                error("illegal system instruction");
            }
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