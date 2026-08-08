<!--
SPDX-FileCopyrightText: 2026 2026 PhiLia093 phi_lia093@126.com

SPDX-License-Identifier: GPL-3.0-or-later
-->

# RISCVemu

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![C23](https://img.shields.io/badge/C-23-blue.svg)](https://en.cppreference.com/w/c/23)
[![CMake](https://img.shields.io/badge/CMake-3.10+-green.svg)](https://cmake.org/)
[![RISC-V](https://img.shields.io/badge/RISC--V-IMA(FDQ)C%20&%20more-red.svg)](https://riscv.org/)
[![Linux](https://img.shields.io/badge/Linux-6.x-blue.svg)](https://kernel.org/)
[![OpenSBI](https://img.shields.io/badge/OpenSBI-v1.9-green.svg)](https://github.com/riscv-software-src/opensbi)
[![Version](https://img.shields.io/badge/version-1.0-blue.svg)]()

A pedagogical RISC-V instruction set simulator written in C23. Supports base integer (I), multiply/divide (M), atomic (A), compressed (C), single/double/quad/half-precision floating-point (F/D/Q/Zfh), and a growing set of standard extensions. Built for learning and exploring RISC-V architecture, now with **Linux kernel boot support** (via OpenSBI + Device Tree).

[中文文档](./README_CN.MD) | [English](./README.MD)

## ✨ Features

### ISA Support
- **RV32I Base Integer ISA**: All integer instructions implemented
- **M Extension**: Integer multiplication/division
- **A Extension**: Atomic operations (LR/SC, AMO*)
- **C Extension**: 16-bit compressed instructions
- **F/D/Q Extensions**: Single/Double/Quad-precision floating-point (requires Zicsr)
- **Zfh Extension**: Half-precision floating-point
- **Zicsr/Zifencei**: CSR instructions and FENCE.I
- **Zicntr**: Counter instructions (cycle, instret, time)
- **Zicond**: Conditional operations

### System & Boot Support
- **Machine/Supervisor/User Privilege Modes**: Full privilege-level support
- **SV32 Page Tables**: Virtual memory with MMU
- **CLINT Timer**: Core Local Interruptor with mtime/mtimecmp
- **PLIC Interrupt Controller**: Platform-Level Interrupt Controller at `0x0C000000`
- **UART Emulation**: 16550-compatible at `0x10000000`
- **Device Tree (FDT)**: Auto-generated Flattened Device Tree for OpenSBI/OS boot
- **OpenSBI Integration**: Boot `fw_jump.bin` at `0x80000000` with next-stage kernel staging
- **Multi-stage Binary Loading**: `--load FILE@HEX` for loading additional binaries (e.g., kernel, initrd)
- **Separate Initrd Support**: `--initrd FILE[@HEX]` loads a cpio archive into RAM and hands it to Linux via the FDT's `/chosen/linux,initrd-*`, so the initramfs need not be baked into the kernel Image

### Linux Support 🐧
- **Full Boot Flow**: Boots Linux 6.x kernels via OpenSBI + FDT
- **UART Console**: Dedicated host terminal (raw mode), instant key response, Ctrl-] x to exit
- **Initrd Support**: Standalone cpio archive loading without kernel recompilation
- **SBI Extensions**: Supports standard SBI extensions including TIME, IPI, RFENCE, DBCN, FWFT
- **PLIC Interrupts**: UART provides interrupts via PLIC, `/dev/ttyS0` works properly
- **Tested Kernel Version**: Linux 6.18.43 (rv32)

### Runtime & Debug
- **4GB Memory Space**: Virtual page frame allocation with copy-on-write
- **Interactive Debugger**: Single-step, register view/modify, memory read/write, disassembly, breakpoints
- **Sdtrig Hardware Breakpoints**: Trigger module support for fetch breakpoints
- **riscv-tests Suite**: Full test coverage for all supported extensions

## 📋 Dependencies

### Build Dependencies
- C23-compatible compiler (GCC 13+ / Clang 16+)
- CMake 3.10+
- GNU Make
- libsoftfloat (included as submodule or system package)
- OpenSBI (optional, for SBI-based boot)
- Linux (optional, experimental)

### Test Dependencies
- [RISC-V GNU Toolchain](https://github.com/riscv-collab/riscv-gnu-toolchain) (`riscv32-unknown-elf-gcc`, `riscv32-unknown-elf-objcopy`)

## 🚀 Quick Start

### Building

```bash
# Clone repository (including submodules)
git clone --recursive <repository-url>
cd RISCVemu

# Build emulator
mkdir build && cd build
cmake ..
make
```

### Running Bare-Metal Programs

```bash
# Basic usage
./RISCVemu <program.bin> <base_address>

# Example: Load run.bin at 0x1000 (CLINT self-test)
./RISCVemu run.bin 0x1000

# Start with the built-in debugger (requires rebuild with -DCONFIG_ENABLE_DEBUGGER=ON)
./RISCVemu run.bin 0x1000 -d
```

### Booting Linux Kernel

In `--console` mode the guest UART owns the host terminal (stdin/stdout) and all emulator log messages are dumped to a log file, so the terminal shows exactly what the guest console prints -- no interleaved host log lines.

The host terminal is switched to **raw mode**: every keystroke is delivered to the guest immediately (no waiting for Enter, no canonical line buffering), the host does not echo anything, and control characters (e.g. Ctrl+C) are passed straight through to the guest -- the guest controls the screen entirely. To leave console mode and quit the emulator, press **Ctrl-] then x** (the telnet escape convention). The original terminal settings are restored automatically on exit.

```bash
# Boot OpenSBI fw_jump with a kernel staged at 0x80400000 (UART console mode)
./RISCVemu fw_jump.bin 0x80000000 --fdt --load kernel.bin@0x80400000 \
    --console --log=riscvemu.log

# Full boot with separate initrd
./RISCVemu fw_jump.bin 0x80000000 --fdt --load Image@0x80400000 \
    --initrd initrd.cpio.gz --console --log=riscvemu.log
```

### CMake Convenience Targets

```bash
make run            # Run OpenSBI + kernel boot demo
make run_sbi_debug  # Same with debugger enabled
make run_kernel     # Boot with real Linux kernel Image + initrd
make run_selftest   # Run CLINT timer self-test
make debug          # Run self-test in debug mode
make build_tests    # Build riscv-tests
make run_tests      # Run all tests
```

## 🔧 CLI Options

```
Usage: ./RISCVemu <program_file> <program_base> [options]
Options:
  -L, --load FILE@HEX   Load an extra binary into RAM at the given address
                        (repeatable; e.g., OpenSBI next-stage kernel).
  -R, --initrd FILE[@HEX]
                        Load a separate initrd cpio archive into RAM
                        (default address 0x82000000) and pass it to Linux via
                        /chosen/linux,initrd-* in the FDT (requires --fdt).
                        Use this instead of baking the initramfs into the
                        kernel Image.
  -C, --console         UART console mode: the guest console owns the host
                        terminal (stdin/stdout), logger output is dumped to
                        --log (default riscvemu.log), and the built-in
                        debugger is disabled.  The host terminal switches to
                        raw mode: keystrokes reach the guest immediately and
                        are not echoed (the guest controls the screen).
                        Press Ctrl-] then x to quit console mode.
  -l, --log FILE        Write logger output to FILE (truncated on open).
  -d, --debug           Enable single-step debug mode (needs a build with
                        CONFIG_ENABLE_DEBUGGER=ON).
  -b, --breakpoint      Set breakpoint address (hex format)
  -F, --fdt[=addr]      Build and hand a device tree to firmware in a1
                        (a0=hartid 0); addr defaults to 0x87f00000
  -h, --help            Show this help message

Examples:
  ./RISCVemu test.bin 0x1000 -d -b 0x1200
  ./RISCVemu fw_jump.bin 0x80000000 --fdt --load kernel.bin@0x80400000 \
      --console --log=riscvemu.log
  ./RISCVemu fw_jump.bin 0x80000000 --fdt --load Image@0x80400000 \
      --initrd initrd.cpio.gz --console
```

## 🔧 Debugger Commands

Launch with `-d` flag to enter interactive debug mode:

| Command | Description |
|---------|-------------|
| `s` / `n` | Single step |
| `c` | Continue execution |
| `q` | Quit emulator |
| `r` | Display all registers |
| `p <addr>` | Set Program Counter |
| `R <reg> <val>` | Set register value (supports number or name) |
| `m <addr>` | Display 32-bit value at address |
| `d <start> <end>` | Dump memory range |
| `B <addr> [count]` | Dump bytes (default 16) |
| `S <addr>` | Read and display string from address |
| `w <addr> <val>` | Write 32-bit value to memory |
| `u <addr> [count]` | Disassemble instructions (defaults to current PC) |
| `D` | Toggle disassembly/raw hex display |
| `b <addr>` | Set breakpoint |
| `C` | Clear breakpoint |
| `h` / `?` | Show help |
| `<Enter>` | Repeat last command |

## 📁 Project Structure

```
RISCVemu/
├── CMakeLists.txt          # CMake build with configurable extensions
├── config.h.in             # Build-time configuration template
├── include/                # Public headers
│   ├── debugger.h          # Debugger interface
│   ├── disasm.h            # Disassembler
│   ├── emu.h               # Core emulator state
│   ├── exec.h              # Instruction execution engine
│   ├── fmap.h              # File mapping utilities
│   ├── logger.h            # Logging system
│   ├── mem.h               # Memory management
│   ├── mmu.h               # SV32 MMU implementation
│   ├── device/             # Device emulation
│   │   ├── clint.h         # CLINT timer
│   │   ├── fdt.h           # Flattened Device Tree generator
│   │   ├── plic.h          # PLIC interrupt controller
│   │   └── uart.h          # 16550 UART
│   └── extension/          # Extension instruction headers
│       ├── a_extension.h
│       ├── c_extension.h
│       ├── f_extension.h
│       ├── m_extension.h
│       ├── sdtrig_extension.h
│       ├── system.h
│       ├── zicntr_extension.h
│       ├── zicond_extension.h
│       ├── zicsr_extension.h
│       └── zifencei_extension.h
├── src/                    # Source code
│   ├── debugger.c
│   ├── disasm.c
│   ├── emu.c
│   ├── exec.c
│   ├── fmap.c
│   ├── logger.c
│   ├── mem.c
│   ├── device/
│   │   ├── clint.c
│   │   ├── fdt.c
│   │   └── plic.c          # PLIC implementation
│   └── extension/
│       ├── c_extension.c
│       ├── f_extension.c
│       ├── m_extension.c
│       ├── sdtrig_extension.c
│       ├── system.c
│       ├── zicntr_extension.c
│       ├── zicsr_extension.c
│       └── zifencei_extension.c
├── run/                    # Example programs and scripts
│   ├── compile_run.sh      # Build examples
│   ├── run_tests.sh        # Test runner
│   ├── build_tests.sh      # Test builder
│   ├── self_test.c         # CLINT timer self-test
│   ├── run.c               # Fake kernel example
│   ├── link.ld             # Bare-metal linker script
│   └── link_kernel.ld      # Kernel linker script
├── tests/                  # riscv-tests (submodule)
└── LICENSE                 # GPLv3 License
```

## ⚙️ CMake Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `CONFIG_ENABLE_DEBUGGER` | OFF | Built-in debugger (`-d` flag) |
| `CONFIG_SUPPORT_MISALIGN` | OFF | Support misaligned memory access (slower) |
| `CONFIG_ENABLE_M_EXTENSION` | ON | M: Integer Multiplication/Division |
| `CONFIG_ENABLE_A_EXTENSION` | ON | A: Atomic Operations |
| `CONFIG_ENABLE_C_EXTENSION` | ON | C: 16-bit Compressed Instructions |
| `CONFIG_ENABLE_F_EXTENSION` | ON | F: Single-Precision FPU (requires Zicsr) |
| `CONFIG_ENABLE_D_EXTENSION` | ON | D: Double-Precision FPU (requires F) |
| `CONFIG_ENABLE_Q_EXTENSION` | ON | Q: Quad-Precision FPU (requires D) |
| `CONFIG_ENABLE_ZFH_EXTENSION` | ON | Zfh: Half-Precision FPU (requires F) |
| `CONFIG_ENABLE_ZICSR_EXTENSION` | ON | Zicsr: CSR Instructions |
| `CONFIG_ENABLE_ZICNTR_EXTENSION` | ON | Zicntr: Counter Instructions |
| `CONFIG_ENABLE_ZIFENCEI_EXTENSION` | ON | Zifencei: FENCE.I |
| `CONFIG_ENABLE_ZICOND_EXTENSION` | ON | Zicond: Conditional Instructions |
| `CONFIG_ENABLE_UART_DEVICE` | ON | 16550 UART at 0x10000000 |
| `CONFIG_ENABLE_PLIC_DEVICE` | ON | PLIC interrupt controller at 0x0C000000 |
| `CONFIG_ENABLE_FDT` | ON | Device Tree Blob generator |
| `OPTIMIZE_FOR_SIZE` | OFF | Optimize for binary size (`-Os`) |
| `ENABLE_LTO` | OFF | Link Time Optimization |

### Building with custom extensions

```bash
cmake .. -DCONFIG_ENABLE_D_EXTENSION=OFF -DCONFIG_ENABLE_FDT=ON
make
```

## 📊 Test Status

Run tests with `make run_tests`. Supported test suites:

| Suite | Tests | Status |
|-------|-------|--------|
| `rv32ui-p-*` | Base integer | ✅ All passing |
| `rv32um-p-*` | M extension | ✅ All passing |
| `rv32ua-p-*` | A extension | ✅ All passing |
| `rv32uc-p-*` | C extension | ✅ All passing |
| `rv32uf-p-*` | F extension | ✅ All passing |
| `rv32ud-p-*` | D extension | ✅ All passing |
| `rv32uzfh-p-*` | Zfh extension | ✅ All passing |
| `rv32uzicond-p-*` | Zicond | ✅ All passing |
| `rv32mi-p-*` | Machine-mode | ✅ All passing |
| `rv32si-p-*` | Supervisor-mode | ✅ All passing |

### Linux Boot Status

| Component | Status |
|-----------|--------|
| OpenSBI Boot | ✅ Working (v1.9-16) |
| Linux 6.18.43 Kernel | ✅ Boots successfully |
| UART Console | ✅ Working |
| Initrd Loading | ✅ Working (cpio) |
| PLIC Interrupts | ✅ Working |
| SBI Extensions (TIME/IPI/RFENCE/DBCN/FWFT) | ✅ Supported |

Expected output (Linux boot log):
```
OpenSBI v1.9-16-g337c23dd
...
[    0.000000] Linux version 6.18.43 ...
[    0.000000] Machine model: RISCVemu RV32 virt
...
BusyBox v1.38.0 built-in shell (ash)
# ls
bin   dev   etc   init  proc  root  sbin  sys   tmp   usr   var
```

## 🎯 Known Limitations

- Flat binary format only (no direct ELF loading; use `objcopy -O binary`) (ELF support coming soon!)
- RV32 only (RV64 support in development)
- Portability: may not work on big-endian systems (DO NOT TOUCH THE MAGICAL DECODER! I've f**ked it and reverted thousands of times)
- Performance: very slow if all extensions are enabled (maybe only 130MIPS) vs 270 MIPS if only "I" is enabled (maybe JIT later?)

## 🤝 Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

## 📝 License

This project is licensed under the [GPLv3](https://www.gnu.org/licenses/gpl-3.0.html) License.

---

*For educational and personal use only. Feedback and contributions are appreciated!*
