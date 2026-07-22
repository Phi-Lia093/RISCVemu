#include <extension/zicsr_extension.h>

void
ins_sret(void)
{
}

void
ins_mret(void)
{
}

void
ins_mnret(void)
{
}

void
ins_wfi(void)
{
}

void
ins_sctrclr(void)
{
}

void
ins_sfence_vma(void)
{
}

void
ins_zicsr_csrrw(uint32_t rs1, uint32_t rd, uint32_t csr)
{
}
void
ins_zicsr_csrrs(uint32_t rs1, uint32_t rd, uint32_t csr)
{
}
void
ins_zicsr_csrrc(uint32_t rs1, uint32_t rd, uint32_t csr)
{
}

void
ins_zicsr_csrrwi(uint32_t uimm, uint32_t rd, uint32_t csr)
{
}
void
ins_zicsr_csrrsi(uint32_t uimm, uint32_t rd, uint32_t csr)
{
}
void
ins_zicsr_csrrci(uint32_t uimm, uint32_t rd, uint32_t csr)
{
}
