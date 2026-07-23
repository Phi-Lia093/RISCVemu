<!--
SPDX-FileCopyrightText: 2026 2026 PhiLia093 phi_lia093@126.com

SPDX-License-Identifier: GPL-3.0-or-later
-->

# RISCVemu

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![C23](https://img.shields.io/badge/C-23-blue.svg)](https://en.cppreference.com/w/c/23)
[![CMake](https://img.shields.io/badge/CMake-3.10+-green.svg)](https://cmake.org/)
[![RISC-V](https://img.shields.io/badge/RISC--V-IMA(U)-red.svg)](https://riscv.org/)

A pedagogical RV32IMA(U) instruction set simulator written in C23. Supports the base integer instruction set (I), multiply/divide extension (M), atomic operations extension (A), and user-mode (U) instruction decoding. Built for learning and exploring RISC-V architecture.

[中文文档](./README_CN.MD) | [English](./README.MD)

## ✨ Features

- **Complete RV32I Base Integer ISA**: All integer instructions implemented
- **M Extension**: Multiply/divide instructions
- **A Extension**: Atomic operations (LR/SC, AMO\*)
- **U-Mode Support**: User-mode instruction decoding; privileged instructions and Zicsr/Zifencei extensions are treated as NOPs
- **4GB Memory Space**: Virtual page frame allocation with copy-on-write strategy for efficient memory usage
- **Interactive Debugger**: Single-step, register view/modify, memory read/write, disassembly, breakpoints
- **UART Emulation**: Writes to `0x10000000` are stripped to chars and output directly to stdout
- **Flat Binary Loading**: Load binary files to any specified base address
- **riscv-tests Suite**: 100% PASS rate (excluding privilege-level dependent tests)

## 📋 Dependencies

### Build Dependencies
- C23-compatible compiler (GCC 13+ / Clang 16+)
- CMake 3.10+
- GNU Make

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

### Running a Program

```bash
# Basic usage
./RISCVemu <program.bin> <base_address>

# Example: Load run.bin at 0x1000
./RISCVemu run.bin 0x1000

# Start with debugger enabled
./RISCVemu run.bin 0x1000 -d

# Set a breakpoint
./RISCVemu run.bin 0x1000 -d -b 0x1200
```

### Running the Test Suite

```bash
# First-time setup: initialize riscv-tests submodule
git submodule update --init
cd tests && git submodule update --init && cd ..

# Build test programs and run
make build_tests   # Compile riscv-tests and convert to bin format
make run_tests     # Run all tests
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
| `R <reg> <val>` | Set register value (supports number or name, e.g., `a0`, `sp`) |
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
├── CMakeLists.txt          # CMake build configuration
├── include/                # Public headers
│   ├── debugger.h          # Debugger interface
│   ├── disasm.h            # Disassembler
│   ├── emu.h               # Core emulator state
│   ├── exec.h              # Instruction execution engine
│   ├── fmap.h              # Memory mapping utilities
│   ├── logger.h            # Logging system
│   ├── mem.h               # Memory management
│   └── extension/          # Extension instruction headers
├── src/                    # Source code
│   ├── debugger.c          # Debugger implementation
│   ├── disasm.c            # Disassembler implementation
│   ├── emu.c               # Main program entry
│   ├── exec.c              # Instruction execution core
│   ├── fmap.c              # File mapping
│   ├── logger.c            # Logging implementation
│   ├── mem.c               # Memory management
│   └── extension/          # Extension instruction implementations (M/A)
├── run/                    # Example programs
│   ├── compile_run.sh      # Build example script
│   ├── link.ld             # Linker script
│   ├── run.c               # Example C program
│   └── run.bin             # Pre-built binary
├── tests/                  # riscv-tests (submodule)
│   ├── isa/                # ISA tests
│   ├── benchmarks/         # Benchmark tests
│   └── ...
└── LICENSE                 # GPLv3 License
```

## ⚙️ CMake Targets

| Target | Description |
|--------|-------------|
| `make` / `make all` | Build emulator executable |
| `make run` | Build and run `run.bin` example |
| `make debug` | Run example in debug mode |
| `make build_tests` | Build riscv-tests |
| `make run_tests` | Run all tests |
| `make tidy` | Format code with clang-format |
| `make rebuild` | Clean and rebuild |

## 📊 Test Status

Run tests with `make run_tests`. All `rv32ui-p-*`, `rv32um-p-*`, `rv32ua-p-*` tests:

- ✅ **All passing** (except `rv32ui-p-ma_data`, which requires exception/interrupt handling not yet implemented in user-mode simulation)
- ⏭️ `rv32ui-p-ma_data`: Skipped (privilege/interrupt mechanism not implemented)

Expected test output:
```
[rv32ui-p-add] ✅ PASS
[rv32ui-p-addi] ✅ PASS
[rv32um-p-mul] ✅ PASS
[rv32ua-p-amoadd] ✅ PASS
...
```

## 🎯 Known Limitations

- No privilege-level or interrupt/exception handling
- No Zicsr or Zifencei extension support (decoded as NOPs)
- Flat binary format only (no direct ELF loading)
- Program base address must match linker script configuration

## 🤝 Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

## 📝 License

This project is licensed under the [GPLv3](https://www.gnu.org/licenses/gpl-3.0.html) License.

---

*For educational and personal use only. Feedback and contributions are appreciated!*
