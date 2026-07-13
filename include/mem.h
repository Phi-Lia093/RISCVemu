#ifndef MEM_H
#define MEM_H

#include <emu.h>

#define MEM_SIZE 1*1024*1024*1024L

void init_mem(void);

extern uint8_t* main_mem;

#endif