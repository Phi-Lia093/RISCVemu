void
_start(void) __attribute__((naked)) __attribute__((section(".text._start")));
void
_main();

void
_start(void)
{
    __asm__ volatile("la sp, _stack_top\n");
    _main();
    while (1);
}

void
_main()
{
    unsigned int i = 1;
    for(int i=0;i<10000;i++);
    int *p = (int*)0x1000;
    p[0] = 0xABCDEFEF;
    __asm__ volatile("ebreak\n");
}