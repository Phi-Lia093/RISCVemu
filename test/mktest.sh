riscv64-unknown-elf-gcc -march=rv32ima_zicsr_zifencei -mabi=ilp32 -nostdlib -ffreestanding test.c -T link.ld -o test.elf
riscv64-unknown-elf-objcopy -O binary test.elf test.bin
