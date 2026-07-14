#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>

#include <fmap.h>
#include <logger.h>
#include <mem.h>

#define PROGRAM_BASE 0
#define PROGRAM_NAME "test.bin"

void
load_program(void)
{
    size_t len;
    uint8_t *addr
        = (uint8_t *)map_file(PROGRAM_NAME, &len, PROT_READ | PROT_WRITE);
    memcpy(main_mem + PROGRAM_BASE, addr, len);
}

int
main(void)
{
    init_logger(INFO, NULL);
    info("loading program...");

    init_mem();
    load_program();
    terminate_logger();
    return 0;
}