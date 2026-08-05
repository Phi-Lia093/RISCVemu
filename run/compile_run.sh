#!/bin/bash
# SPDX-FileCopyrightText: 2026 PhiLia093 phi_lia093@126.com
# SPDX-License-Identifier: GPL-3.0-or-later

# Build the CLINT timer self-test (bare-metal, at 0x1000).
riscv32-unknown-elf-gcc -march=rv32imafd_zfh_zicond_zicsr_zifencei -mabi=ilp32 -nostdlib -ffreestanding self_test.c -T link.ld -o run.elf
riscv32-unknown-elf-objcopy -O binary run.elf run.bin

# Build the fake next-stage kernel (S-mode, at 0x80400000).
riscv32-unknown-elf-gcc -march=rv32imafd_zfh_zicond_zicsr_zifencei -mabi=ilp32 -nostdlib -ffreestanding run.c -T link_kernel.ld -o kernel.elf
riscv32-unknown-elf-objcopy -O binary kernel.elf kernel.bin
