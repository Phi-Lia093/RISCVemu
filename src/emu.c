#include <emu.h>
#include <stdio.h>
#include <logger.h>

int main(void)
{
	init_logger(INFO, "latest.log");
    terminate_logger();
	return 0;
}