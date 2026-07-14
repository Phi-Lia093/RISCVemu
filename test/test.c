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
    while (1)
    {
        i++;
    }
}