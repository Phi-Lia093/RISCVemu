#!/bin/bash
# SPDX-FileCopyrightText: 2026 PhiLia093 phi_lia093@126.com
# SPDX-License-Identifier: GPL-3.0-or-later

riscv32-unknown-elf-gcc -march=rv32ima_zicsr_zifencei -mabi=ilp32 -nostdlib -ffreestanding run.c -T link.ld -o run.elf
riscv32-unknown-elf-objcopy -O binary run.elf run.bin
