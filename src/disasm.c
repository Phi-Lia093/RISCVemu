#include <disasm.h>
#include <emu.h>
#include <stdio.h>
#include <string.h>

static char disasm_buf[128];

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

typedef void (*format_func_t)(char *buf, const char *mnemonic, uint32_t a,
                              uint32_t b, uint32_t c);

static void
format_r(char *buf, const char *mnemonic, uint32_t rd, uint32_t rs1,
         uint32_t rs2)
{
    sprintf(buf, "%s %s, %s, %s", mnemonic, reg_name(rd), reg_name(rs1),
            reg_name(rs2));
}

static void
format_i(char *buf, const char *mnemonic, uint32_t rd, uint32_t rs1,
         uint32_t imm)
{
    sprintf(buf, "%s %s, %s, %d", mnemonic, reg_name(rd), reg_name(rs1),
            (int32_t)imm);
}

static void
format_s(char *buf, const char *mnemonic, uint32_t rs2, uint32_t rs1,
         uint32_t imm)
{
    sprintf(buf, "%s %s, %d(%s)", mnemonic, reg_name(rs2), (int32_t)imm,
            reg_name(rs1));
}

static void
format_b(char *buf, const char *mnemonic, uint32_t rs1, uint32_t rs2,
         uint32_t imm)
{
    uint32_t pc = 0;
#ifdef EMU_H
    pc = g_state.pc;
#endif
    sprintf(buf, "%s %s, %s, 0x%x", mnemonic, reg_name(rs1), reg_name(rs2),
            pc + (int32_t)imm);
}

static void
format_u(char *buf, const char *mnemonic, uint32_t rd, uint32_t imm,
         __attribute__((unused)) uint32_t unused)
{
    sprintf(buf, "%s %s, 0x%x", mnemonic, reg_name(rd), imm);
}

static void
format_j(char *buf, const char *mnemonic, uint32_t rd, uint32_t imm,
         __attribute__((unused)) uint32_t unused)
{
    uint32_t pc = 0;
#ifdef EMU_H
    pc = g_state.pc;
#endif
    sprintf(buf, "%s %s, 0x%x", mnemonic, reg_name(rd), pc + (int32_t)imm);
}

static void
format_system(char *buf, const char *mnemonic,
              __attribute__((unused)) uint32_t unused1,
              __attribute__((unused)) uint32_t unused2,
              __attribute__((unused)) uint32_t unused3)
{
    sprintf(buf, "%s", mnemonic);
}

typedef struct
{
    uint32_t opcode;
    uint32_t funct3;
    uint32_t funct7;
    const char *mnemonic;
    format_func_t format;
    uint32_t type; // 0=uses imm, 1=R-type, 2=store, 3=branch, 4=system,
                   // 5=U-type, 6=J-type
} ins_def_t;

static const ins_def_t ins_table[] = {
    // R-type instructions
    { 0b0110011, 0b0000000, 0b0000000, "add", format_r, 1 },
    { 0b0110011, 0b0100000, 0b0000000, "sub", format_r, 1 },
    { 0b0110011, 0b0000001, 0b0000000, "mul", format_r, 1 },
    { 0b0110011, 0b0000000, 0b0000001, "sll", format_r, 1 },
    { 0b0110011, 0b0000001, 0b0000001, "mulh", format_r, 1 },
    { 0b0110011, 0b0000000, 0b0000010, "slt", format_r, 1 },
    { 0b0110011, 0b0000001, 0b0000010, "mulsu", format_r, 1 },
    { 0b0110011, 0b0000000, 0b0000011, "sltu", format_r, 1 },
    { 0b0110011, 0b0000001, 0b0000011, "mulu", format_r, 1 },
    { 0b0110011, 0b0000000, 0b0000100, "xor", format_r, 1 },
    { 0b0110011, 0b0000001, 0b0000100, "div", format_r, 1 },
    { 0b0110011, 0b0000000, 0b0000101, "srl", format_r, 1 },
    { 0b0110011, 0b0100000, 0b0000101, "sra", format_r, 1 },
    { 0b0110011, 0b0000001, 0b0000101, "divu", format_r, 1 },
    { 0b0110011, 0b0000000, 0b0000110, "or", format_r, 1 },
    { 0b0110011, 0b0000001, 0b0000110, "rem", format_r, 1 },
    { 0b0110011, 0b0000000, 0b0000111, "and", format_r, 1 },
    { 0b0110011, 0b0000001, 0b0000111, "remu", format_r, 1 },

    // I-type ALU
    { 0b0010011, 0b0000000, 0x7F, "addi", format_i, 0 },
    { 0b0010011, 0b0000010, 0x7F, "slti", format_i, 0 },
    { 0b0010011, 0b0000011, 0x7F, "sltiu", format_i, 0 },
    { 0b0010011, 0b0000100, 0x7F, "xori", format_i, 0 },
    { 0b0010011, 0b0000110, 0x7F, "ori", format_i, 0 },
    { 0b0010011, 0b0000111, 0x7F, "andi", format_i, 0 },

    // I-type shifts
    { 0b0010011, 0b0000001, 0b0000000, "slli", format_i, 0 },
    { 0b0010011, 0b0000101, 0b0000000, "srli", format_i, 0 },
    { 0b0010011, 0b0000101, 0b0100000, "srai", format_i, 0 },

    // Loads
    { 0b0000011, 0b0000000, 0x7F, "lb", format_i, 0 },
    { 0b0000011, 0b0000001, 0x7F, "lh", format_i, 0 },
    { 0b0000011, 0b0000010, 0x7F, "lw", format_i, 0 },
    { 0b0000011, 0b0000100, 0x7F, "lbu", format_i, 0 },
    { 0b0000011, 0b0000101, 0x7F, "lhu", format_i, 0 },

    // Stores
    { 0b0100011, 0b0000000, 0x7F, "sb", format_s, 2 },
    { 0b0100011, 0b0000001, 0x7F, "sh", format_s, 2 },
    { 0b0100011, 0b0000010, 0x7F, "sw", format_s, 2 },

    // Branches
    { 0b1100011, 0b0000000, 0x7F, "beq", format_b, 3 },
    { 0b1100011, 0b0000001, 0x7F, "bne", format_b, 3 },
    { 0b1100011, 0b0000100, 0x7F, "blt", format_b, 3 },
    { 0b1100011, 0b0000101, 0x7F, "bge", format_b, 3 },
    { 0b1100011, 0b0000110, 0x7F, "bltu", format_b, 3 },
    { 0b1100011, 0b0000111, 0x7F, "bgeu", format_b, 3 },

    // JAL
    { 0b1101111, 0x7F, 0x7F, "jal", format_j, 6 },

    // U-type
    { 0b0110111, 0x7F, 0x7F, "lui", format_u, 5 },
    { 0b0010111, 0x7F, 0x7F, "auipc", format_u, 5 },

    // JALR
    { 0b1100111, 0x7F, 0x7F, "jalr", format_i, 0 },

    // System
    { 0b1110011, 0b0000000, 0x7F, "ecall", format_system, 4 },
};

char *
disasm(uint32_t ins)
{
    uint32_t opcode = get_opcode(ins);
    uint32_t funct3 = get_funct3(ins);
    uint32_t funct7 = get_funct7(ins);
    uint32_t rs1 = get_rs1(ins);
    uint32_t rs2 = get_rs2(ins);
    uint32_t rd = get_rd(ins);
    uint32_t imm = 0;

    switch (opcode)
    {
    case 0b0010011:
    case 0b0000011:
    case 0b1100111:
    case 0b1110011:
        imm = sign_extend_12((ins >> 20) & 0xFFF);
        break;
    case 0b0100011:
        imm = ((ins >> 25) & 0x7F) << 5;
        imm |= ((ins >> 7) & 0x1F);
        imm = sign_extend_12(imm);
        break;
    case 0b1100011:
        imm = ((ins >> 31) & 0x1) << 12;
        imm |= ((ins >> 25) & 0x3F) << 5;
        imm |= ((ins >> 8) & 0xF) << 1;
        imm |= ((ins >> 7) & 0x1) << 11;
        imm = sign_extend_13(imm);
        break;
    case 0b1101111:
        imm = ((ins >> 31) & 0x1) << 20;
        imm |= ((ins >> 21) & 0x3FF) << 1;
        imm |= ((ins >> 20) & 0x1) << 11;
        imm |= ((ins >> 12) & 0xFF) << 12;
        imm = sign_extend_21(imm);
        break;
    case 0b0110111:
    case 0b0010111:
        imm = ins & 0xFFFFF000;
        break;
    }

    uint32_t shamt = (ins >> 20) & 0x1F;

    for (size_t i = 0; i < sizeof(ins_table) / sizeof(ins_table[0]); i++)
    {
        const ins_def_t *def = &ins_table[i];

        if (opcode == def->opcode)
        {
            if (def->funct3 != 0x7F && funct3 != def->funct3) continue;
            if (def->funct7 != 0x7F && funct7 != def->funct7) continue;

            switch (def->type)
            {
            case 1: // R-type
                def->format(disasm_buf, def->mnemonic, rd, rs1, rs2);
                break;
            case 2: // Store
                def->format(disasm_buf, def->mnemonic, rs2, rs1, imm);
                break;
            case 3: // Branch
                def->format(disasm_buf, def->mnemonic, rs1, rs2, imm);
                break;
            case 4: // System
                def->format(disasm_buf, def->mnemonic, 0, 0, 0);
                break;
            case 5: // U-type
                def->format(disasm_buf, def->mnemonic, rd, imm, 0);
                break;
            case 6: // J-type
                def->format(disasm_buf, def->mnemonic, rd, imm, 0);
                break;
            default: // I-type
                if (def->funct3 == 0b0000001 || def->funct3 == 0b0000101)
                {
                    def->format(disasm_buf, def->mnemonic, rd, rs1, shamt);
                }
                else
                {
                    def->format(disasm_buf, def->mnemonic, rd, rs1, imm);
                }
                break;
            }
            return disasm_buf;
        }
    }

    sprintf(disasm_buf, "unknown (0x%08x)", ins);
    return disasm_buf;
}
