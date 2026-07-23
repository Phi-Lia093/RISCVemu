#include <disasm.h>
#include <emu.h>
#include <stdio.h>
#include <string.h>

static char disasm_buf[1024];

static inline uint32_t
get_opcode(uint32_t ins)
{
    return ins & 0x7F;
}
static inline uint32_t
get_funct3(uint32_t ins)
{
    return (ins >> 12) & 0x7;
}
static inline uint32_t
get_funct7(uint32_t ins)
{
    return (ins >> 25) & 0x7F;
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
get_rd(uint32_t ins)
{
    return (ins >> 7) & 0x1F;
}
static inline uint32_t
get_funct5(uint32_t ins)
{
    return (ins >> 27) & 0x1F;
}

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

static const char *
reg_name(uint32_t id)
{
    static const char *regs[]
        = { "zero", "ra", "sp",  "gp",  "tp", "t0", "t1", "t2",
            "s0",   "s1", "a0",  "a1",  "a2", "a3", "a4", "a5",
            "a6",   "a7", "s2",  "s3",  "s4", "s5", "s6", "s7",
            "s8",   "s9", "s10", "s11", "t3", "t4", "t5", "t6" };
    return (id < 32) ? regs[id] : "?";
}

char *
disasm(uint32_t ins)
{
    uint32_t opcode = get_opcode(ins);
    uint32_t funct3 = get_funct3(ins);
    uint32_t funct7 = get_funct7(ins);
    uint32_t rs1 = get_rs1(ins);
    uint32_t rs2 = get_rs2(ins);
    uint32_t rd = get_rd(ins);
    uint32_t funct5 = get_funct5(ins);

    // COMPACT extension not supported in disasm
    if ((opcode & 3) != 3)
    {
        sprintf(disasm_buf, "c.<unknown> (0x%04x)", ins & 0xFFFF);
        return disasm_buf;
    }

    switch (opcode) // keep consistent with exec.c
    {
    // R format
    case 0b0110011:
    {
        // M extension
        if (funct7 == 0b0000001)
        {
            const char *mnemonic = "?";
            switch (funct3)
            {
            case 0:
                mnemonic = "mul";
                break;
            case 1:
                mnemonic = "mulh";
                break;
            case 2:
                mnemonic = "mulsu";
                break;
            case 3:
                mnemonic = "mulu";
                break;
            case 4:
                mnemonic = "div";
                break;
            case 5:
                mnemonic = "divu";
                break;
            case 6:
                mnemonic = "rem";
                break;
            case 7:
                mnemonic = "remu";
                break;
            default:
                goto unknown;
            }
            sprintf(disasm_buf, "%s %s, %s, %s", mnemonic, reg_name(rd),
                    reg_name(rs1), reg_name(rs2));
            break;
        }
        else
        {
            switch (funct3)
            {
            case 0: // ADD / SUB
                if (funct7 == 0b0000000)
                    sprintf(disasm_buf, "add %s, %s, %s", reg_name(rd),
                            reg_name(rs1), reg_name(rs2));
                else if (funct7 == 0b0100000)
                    sprintf(disasm_buf, "sub %s, %s, %s", reg_name(rd),
                            reg_name(rs1), reg_name(rs2));
                else
                    goto unknown;
                break;
            case 1: // SLL
                if (funct7 == 0b0000000)
                    sprintf(disasm_buf, "sll %s, %s, %s", reg_name(rd),
                            reg_name(rs1), reg_name(rs2));
                else
                    goto unknown;
                break;
            case 2: // SLT
                if (funct7 == 0b0000000)
                    sprintf(disasm_buf, "slt %s, %s, %s", reg_name(rd),
                            reg_name(rs1), reg_name(rs2));
                else
                    goto unknown;
                break;
            case 3: // SLTU
                if (funct7 == 0b0000000)
                    sprintf(disasm_buf, "sltu %s, %s, %s", reg_name(rd),
                            reg_name(rs1), reg_name(rs2));
                else
                    goto unknown;
                break;
            case 4: // XOR
                if (funct7 == 0b0000000)
                    sprintf(disasm_buf, "xor %s, %s, %s", reg_name(rd),
                            reg_name(rs1), reg_name(rs2));
                else
                    goto unknown;
                break;
            case 5: // SRL / SRA
                if (funct7 == 0b0000000)
                    sprintf(disasm_buf, "srl %s, %s, %s", reg_name(rd),
                            reg_name(rs1), reg_name(rs2));
                else if (funct7 == 0b0100000)
                    sprintf(disasm_buf, "sra %s, %s, %s", reg_name(rd),
                            reg_name(rs1), reg_name(rs2));
                else
                    goto unknown;
                break;
            case 6: // OR
                if (funct7 == 0b0000000)
                    sprintf(disasm_buf, "or %s, %s, %s", reg_name(rd),
                            reg_name(rs1), reg_name(rs2));
                else
                    goto unknown;
                break;
            case 7: // AND
                if (funct7 == 0b0000000)
                    sprintf(disasm_buf, "and %s, %s, %s", reg_name(rd),
                            reg_name(rs1), reg_name(rs2));
                else
                    goto unknown;
                break;
            default:
                goto unknown;
            }
        }
        break;
    }

    // I format ALU
    case 0b0010011:
    {
        uint32_t imm = sign_extend_12((ins >> 20) & 0xFFF);
        uint32_t shamt = imm & 0x1F;

        switch (funct3)
        {
        case 0: // ADDI
            sprintf(disasm_buf, "addi %s, %s, %d", reg_name(rd), reg_name(rs1),
                    (int32_t)imm);
            break;
        case 1: // SLLI
            sprintf(disasm_buf, "slli %s, %s, %d", reg_name(rd), reg_name(rs1),
                    shamt);
            break;
        case 2: // SLTI
            sprintf(disasm_buf, "slti %s, %s, %d", reg_name(rd), reg_name(rs1),
                    (int32_t)imm);
            break;
        case 3: // SLTIU
            sprintf(disasm_buf, "sltiu %s, %s, %d", reg_name(rd), reg_name(rs1),
                    (int32_t)imm);
            break;
        case 4: // XORI
            sprintf(disasm_buf, "xori %s, %s, %d", reg_name(rd), reg_name(rs1),
                    (int32_t)imm);
            break;
        case 5: // SRLI / SRAI
            if (funct7 == 0b0000000)
                sprintf(disasm_buf, "srli %s, %s, %d", reg_name(rd),
                        reg_name(rs1), shamt);
            else if (funct7 == 0b0100000)
                sprintf(disasm_buf, "srai %s, %s, %d", reg_name(rd),
                        reg_name(rs1), shamt);
            else
                goto unknown;
            break;
        case 6: // ORI
            sprintf(disasm_buf, "ori %s, %s, %d", reg_name(rd), reg_name(rs1),
                    (int32_t)imm);
            break;
        case 7: // ANDI
            sprintf(disasm_buf, "andi %s, %s, %d", reg_name(rd), reg_name(rs1),
                    (int32_t)imm);
            break;
        default:
            goto unknown;
        }
        break;
    }

    // LOAD
    case 0b0000011:
    {
        uint32_t imm = sign_extend_12((ins >> 20) & 0xFFF);

        switch (funct3)
        {
        case 0: // LB
            sprintf(disasm_buf, "lb %s, %d(%s)", reg_name(rd), (int32_t)imm,
                    reg_name(rs1));
            break;
        case 1: // LH
            sprintf(disasm_buf, "lh %s, %d(%s)", reg_name(rd), (int32_t)imm,
                    reg_name(rs1));
            break;
        case 2: // LW
            sprintf(disasm_buf, "lw %s, %d(%s)", reg_name(rd), (int32_t)imm,
                    reg_name(rs1));
            break;
        case 4: // LBU
            sprintf(disasm_buf, "lbu %s, %d(%s)", reg_name(rd), (int32_t)imm,
                    reg_name(rs1));
            break;
        case 5: // LHU
            sprintf(disasm_buf, "lhu %s, %d(%s)", reg_name(rd), (int32_t)imm,
                    reg_name(rs1));
            break;
        default:
            goto unknown;
        }
        break;
    }

    // STORE
    case 0b0100011:
    {
        uint32_t imm = ((ins >> 25) & 0x7F) << 5;
        imm |= ((ins >> 7) & 0x1F);
        imm = sign_extend_12(imm);

        switch (funct3)
        {
        case 0: // SB
            sprintf(disasm_buf, "sb %s, %d(%s)", reg_name(rs2), (int32_t)imm,
                    reg_name(rs1));
            break;
        case 1: // SH
            sprintf(disasm_buf, "sh %s, %d(%s)", reg_name(rs2), (int32_t)imm,
                    reg_name(rs1));
            break;
        case 2: // SW
            sprintf(disasm_buf, "sw %s, %d(%s)", reg_name(rs2), (int32_t)imm,
                    reg_name(rs1));
            break;
        default:
            goto unknown;
        }
        break;
    }

    // BRANCH
    case 0b1100011:
    {
        uint32_t imm = ((ins >> 31) & 0x1) << 12;
        imm |= ((ins >> 25) & 0x3F) << 5;
        imm |= ((ins >> 8) & 0xF) << 1;
        imm |= ((ins >> 7) & 0x1) << 11;
        imm = sign_extend_13(imm);
        uint32_t pc = 0;
#ifdef EMU_H
        pc = g_state.pc;
#endif

        switch (funct3)
        {
        case 0: // BEQ
            sprintf(disasm_buf, "beq %s, %s, 0x%x", reg_name(rs1),
                    reg_name(rs2), pc + (int32_t)imm);
            break;
        case 1: // BNE
            sprintf(disasm_buf, "bne %s, %s, 0x%x", reg_name(rs1),
                    reg_name(rs2), pc + (int32_t)imm);
            break;
        case 4: // BLT
            sprintf(disasm_buf, "blt %s, %s, 0x%x", reg_name(rs1),
                    reg_name(rs2), pc + (int32_t)imm);
            break;
        case 5: // BGE
            sprintf(disasm_buf, "bge %s, %s, 0x%x", reg_name(rs1),
                    reg_name(rs2), pc + (int32_t)imm);
            break;
        case 6: // BLTU
            sprintf(disasm_buf, "bltu %s, %s, 0x%x", reg_name(rs1),
                    reg_name(rs2), pc + (int32_t)imm);
            break;
        case 7: // BGEU
            sprintf(disasm_buf, "bgeu %s, %s, 0x%x", reg_name(rs1),
                    reg_name(rs2), pc + (int32_t)imm);
            break;
        default:
            goto unknown;
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
        uint32_t pc = 0;
#ifdef EMU_H
        pc = g_state.pc;
#endif
        sprintf(disasm_buf, "jal %s, 0x%x", reg_name(rd), pc + (int32_t)imm);
        break;
    }

    // LUI
    case 0b0110111:
    {
        uint32_t imm = ins & 0xFFFFF000;
        sprintf(disasm_buf, "lui %s, 0x%x", reg_name(rd), imm);
        break;
    }

    // AUIPC
    case 0b0010111:
    {
        uint32_t imm = ins & 0xFFFFF000;
        sprintf(disasm_buf, "auipc %s, 0x%x", reg_name(rd), imm);
        break;
    }

    // JALR
    case 0b1100111:
    {
        uint32_t imm = sign_extend_12((ins >> 20) & 0xFFF);
        sprintf(disasm_buf, "jalr %s, %s, %d", reg_name(rd), reg_name(rs1),
                (int32_t)imm);
        break;
    }

    // ATOMIC (A extension)
    case 0b0101111:
    {
        if (funct3 == 0x2)
        {
            const char *mnemonic = "?";
            switch (funct5)
            {
            case 0x00:
                mnemonic = "amoadd.w";
                break;
            case 0x01:
                mnemonic = "amoswap.w";
                break;
            case 0x02:
                mnemonic = "lr.w";
                break;
            case 0x03:
                mnemonic = "sc.w";
                break;
            case 0x04:
                mnemonic = "amoxor.w";
                break;
            case 0x08:
                mnemonic = "amoor.w";
                break;
            case 0x0C:
                mnemonic = "amoand.w";
                break;
            case 0x10:
                mnemonic = "amomin.w";
                break;
            case 0x14:
                mnemonic = "amomax.w";
                break;
            default:
                goto unknown;
            }

            if (funct5 == 0x02) // lr.w: rd, rs1
            {
                sprintf(disasm_buf, "%s %s, (%s)", mnemonic, reg_name(rd),
                        reg_name(rs1));
            }
            else if (funct5 == 0x03) // sc.w: rd, rs2, (rs1)
            {
                sprintf(disasm_buf, "%s %s, %s, (%s)", mnemonic, reg_name(rd),
                        reg_name(rs2), reg_name(rs1));
            }
            else // am*: rd, rs2, (rs1)
            {
                sprintf(disasm_buf, "%s %s, %s, (%s)", mnemonic, reg_name(rd),
                        reg_name(rs2), reg_name(rs1));
            }
        }
        else
        {
            goto unknown;
        }
        break;
    }

    // SYSTEM
    case 0b1110011:
    {
        uint32_t imm = (ins >> 20) & 0xFFF;
        uint32_t csr = (ins >> 20) & 0xFFF;

        switch (funct3)
        {
        case 0:
        {
            if (rs1 == 0 && rd == 0)
            {
                switch (imm)
                {
                case 0x000:
                    sprintf(disasm_buf, "ecall");
                    break;
                case 0x001:
                    sprintf(disasm_buf, "ebreak");
                    break;
                case 0x102:
                    sprintf(disasm_buf, "sret");
                    break;
                case 0x302:
                    sprintf(disasm_buf, "mret");
                    break;
                case 0x702:
                    sprintf(disasm_buf, "mnret");
                    break;
                case 0x104:
                    sprintf(disasm_buf, "sctrclr");
                    break;
                case 0x105:
                    sprintf(disasm_buf, "wfi");
                    break;
                case 0x009:
                    sprintf(disasm_buf, "sfence.vma");
                    break;
                default:
                    goto unknown;
                }
            }
            else if (imm == 0x009)
            {
                if (rs1 == 0 && rd == 0)
                {
                    sprintf(disasm_buf, "sfence.vma");
                }
                else if (rs1 != 0 && rd == 0)
                {
                    sprintf(disasm_buf, "sfence.vma %s", reg_name(rs1));
                }
                else if (rs1 == 0 && rd != 0)
                {
                    sprintf(disasm_buf, "sfence.vma , %s", reg_name(rd));
                }
                else
                {
                    sprintf(disasm_buf, "sfence.vma %s, %s", reg_name(rs1),
                            reg_name(rd));
                }
            }
            else
            {
                goto unknown;
            }
            break;
        }

        case 0b001: // CSRRW
            sprintf(disasm_buf, "csrrw %s, %s, 0x%03x", reg_name(rd),
                    reg_name(rs1), csr);
            break;
        case 0b010: // CSRRS
            sprintf(disasm_buf, "csrrs %s, %s, 0x%03x", reg_name(rd),
                    reg_name(rs1), csr);
            break;
        case 0b011: // CSRRC
            sprintf(disasm_buf, "csrrc %s, %s, 0x%03x", reg_name(rd),
                    reg_name(rs1), csr);
            break;
        case 0b101: // CSRRWI
            sprintf(disasm_buf, "csrrwi %s, %d, 0x%03x", reg_name(rd), rs1,
                    csr);
            break;
        case 0b110: // CSRRSI
            sprintf(disasm_buf, "csrrsi %s, %d, 0x%03x", reg_name(rd), rs1,
                    csr);
            break;
        case 0b111: // CSRRCI
            sprintf(disasm_buf, "csrrci %s, %d, 0x%03x", reg_name(rd), rs1,
                    csr);
            break;
        default:
            goto unknown;
        }
        break;
    }

    // FENCE
    case 0b0001111:
    {
        switch (funct3)
        {
        case 0: // FENCE
            sprintf(disasm_buf, "fence");
            break;
        case 1: // FENCE.I
            sprintf(disasm_buf, "fence.i");
            break;
        default:
            goto unknown;
        }
        break;
    }

    default:
    unknown:
        sprintf(disasm_buf, "unknown (0x%08x)", ins);
        break;
    }

    return disasm_buf;
}
