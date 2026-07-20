void
_start(void) __attribute__((section(".text._start")));

int
_main();

void
_start(void)
{
    __asm__ volatile("la sp, _stack_top\n");
    _main();
    __asm__ volatile("ecall\n");
    while (1);
}

int
_main()
{
    return 0x325;
}
