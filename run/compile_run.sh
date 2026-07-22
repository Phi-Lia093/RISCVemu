riscv32-unknown-elf-gcc -march=rv32ima_zicsr_zifencei -mabi=ilp32 -nostdlib -ffreestanding run.c -T link.ld -o run.elf
riscv32-unknown-elf-objcopy -O binary run.elf run.bin
