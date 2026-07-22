void
_start(void) __attribute__((section(".text._start")));

int
_main();

void debug_print(char *str)
{
    while (*str) {
        *(volatile unsigned int *)0x10000000 = *str++;
    }
}

void
_start(void)
{
    __asm__ volatile("la sp, _stack_top\n");
    __asm__ volatile("wfi\n");
    _main();
    __asm__ volatile("ecall\n");
    while (1);
}

int
_main()
{
    debug_print("Hello, World!\n");
    return 0x325;
}
