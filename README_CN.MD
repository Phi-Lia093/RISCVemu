<!--
SPDX-FileCopyrightText: 2026 2026 PhiLia093 phi_lia093@126.com

SPDX-License-Identifier: GPL-3.0-or-later
-->

# RISCVemu

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![C23](https://img.shields.io/badge/C-23-blue.svg)](https://en.cppreference.com/w/c/23)
[![CMake](https://img.shields.io/badge/CMake-3.10+-green.svg)](https://cmake.org/)
[![RISC-V](https://img.shields.io/badge/RISC--V-IMA(FDQ)C%20%26%20more-red.svg)](https://riscv.org/)
[![Version](https://img.shields.io/badge/version-0.4-blue.svg)]()

一个使用 C23 编写的教学用 RISC-V 指令集模拟器。支持基础整数指令集 (I)、乘除扩展 (M)、原子操作扩展 (A)、压缩指令扩展 (C)、单/双/四/半精度浮点扩展 (F/D/Q/Zfh) 以及不断增加的更多标准扩展。专为学习和探索 RISC-V 架构而设计，现已支持 OpenSBI/操作系统引导和完整的设备树。

[中文文档](./README_CN.MD) | [English](./README.MD)

## ✨ 特性

### ISA 支持
- **RV32I 基础整数指令集**：实现了所有整数指令
- **M 扩展**：整数乘除法
- **A 扩展**：原子操作 (LR/SC, AMO*)
- **C 扩展**：16位压缩指令
- **F/D/Q 扩展**：单/双/四精度浮点数 (需要 Zicsr)
- **Zfh 扩展**：半精度浮点数
- **Zicsr/Zifencei**：CSR 指令和 FENCE.I
- **Zicntr**：计数器指令 (cycle, instret, time)
- **Zicond**：条件操作指令

### 系统与引导支持
- **机器/监管/用户模式**：完整的特权级支持
- **SV32 页表**：带 MMU 的虚拟内存
- **CLINT 定时器**：带有 mtime/mtimecmp 的核心本地中断器
- **UART 模拟**：在 `0x10000000` 处兼容 16550
- **设备树 (FDT)**：为 OpenSBI/操作系统引导自动生成扁平设备树
- **OpenSBI 集成**：在 `0x80000000` 引导 `fw_jump.bin`，并支持下一阶段内核加载
- **多阶段二进制加载**：`--load FILE@HEX` 用于加载额外的二进制文件（如内核、initrd）
- **独立 Initrd 支持**：`--initrd FILE[@HEX]` 将 cpio 归档加载到 RAM，并通过 FDT 的 `/chosen/linux,initrd-*` 传递给 Linux，无需将 initramfs 打包进内核 Image

### 运行时与调试
- **4GB 内存空间**：采用写时复制策略的虚拟页帧分配
- **交互式调试器**：单步执行、寄存器查看/修改、内存读写、反汇编、断点
- **Sdtrig 硬件断点**：支持取指断点的触发模块
- **riscv-tests 测试套件**：对所有支持的扩展进行完整测试覆盖

## 📋 依赖

### 构建依赖
- 支持 C23 的编译器 (GCC 13+ / Clang 16+)
- CMake 3.10+
- GNU Make
- libsoftfloat（作为子模块包含或系统包）
- OpenSBI（可选，用于基于 SBI 的引导）
- Linux（可选，实验性支持）

### 测试依赖
- [RISC-V GNU Toolchain](https://github.com/riscv-collab/riscv-gnu-toolchain) (`riscv32-unknown-elf-gcc`, `riscv32-unknown-elf-objcopy`)

## 🚀 快速开始

### 构建

```bash
# 克隆仓库（包含子模块）
git clone --recursive <repository-url>
cd RISCVemu

# 构建模拟器
mkdir build && cd build
cmake ..
make
```

### 运行裸机程序

```bash
# 基本用法
./RISCVemu <program.bin> <base_address>

# 示例：在 0x1000 加载 run.bin（CLINT 自测程序）
./RISCVemu run.bin 0x1000

# 启动内置调试器（当前默认暂时禁用；以 -DCONFIG_ENABLE_DEBUGGER=ON 重新构建
# 后 -d 才会可用）
./RISCVemu run.bin 0x1000 -d
```

### 运行 OpenSBI + 内核（操作系统引导）

在 `--console` 模式下，客户机 UART 独占宿主终端（stdin/stdout），所有模拟器
日志消息都转储到日志文件，因此终端上只会显示客户机控制台输出，不会再与宿主
日志行混杂。

宿主终端会切换为**原始模式（raw mode）**：每次按键都会立即送达客户机（无需
等待回车、没有行缓冲），宿主不会回显任何内容，控制字符（如 Ctrl+C）也会直接
透传给客户机——屏幕完全由客户机控制。按 **Ctrl-] 再按 x**（telnet 转义约定）
可退出控制台模式并结束模拟器。退出时终端设置会自动恢复。

```bash
# 引导 OpenSBI fw_jump，并将内核暂存到 0x80400000（UART 控制台模式）
./RISCVemu fw_jump.bin 0x80000000 --fdt --load kernel.bin@0x80400000 \
    --console --log=riscvemu.log
```

### CMake 便捷目标

```bash
make run            # 运行 OpenSBI + 内核引导演示
make run_sbi_debug  # 同上，启用调试器
make run_selftest   # 运行 CLINT 定时器自测
make debug          # 在调试模式下运行自测
make build_tests    # 构建 riscv-tests
make run_tests      # 运行所有测试
```

## 🔧 CLI 选项

```
Usage: ./RISCVemu <program_file> <program_base> [options]
Options:
  -L, --load FILE@HEX   将额外的二进制文件加载到 RAM 的指定地址
                        （可重复使用；例如 OpenSBI 的下一阶段内核）
  -R, --initrd FILE[@HEX]
                        加载独立的 initrd cpio 归档到 RAM（默认地址
                        0x82000000），并通过 FDT 中的 /chosen/linux,initrd-*
                        传递给 Linux（需要 --fdt）。用于代替把 initramfs
                        打包进内核 Image 的做法
  -C, --console         UART 控制台模式：客户机控制台独占宿主终端
                        （stdin/stdout），日志输出转储到 --log（默认
                        riscvemu.log），并禁用内置调试器。宿主终端会切换
                        为原始模式：按键立即送达客户机且不回显（屏幕由
                        客户机控制）。按 Ctrl-] 再按 x 退出控制台模式
  -l, --log FILE        将日志输出写入 FILE（打开时截断）
  -d, --debug           启用单步调试模式（需要以 CONFIG_ENABLE_DEBUGGER=ON
                        构建；当前暂时禁用）
  -b, --breakpoint      设置断点地址（十六进制格式）
  -F, --fdt[=addr]      构建设备树并通过 a1 传递给固件
                        （a0=hartid 0）；地址默认为 0x87f00000
  -h, --help            显示此帮助信息

示例：
  ./RISCVemu test.bin 0x1000 -d -b 0x1200
  ./RISCVemu fw_jump.bin 0x80000000 --fdt --load kernel.bin@0x80400000 \
      --console --log=riscvemu.log
```

## 🔧 调试器命令

使用 `-d` 标志启动进入交互式调试模式：

| 命令 | 描述 |
|---------|-------------|
| `s` / `n` | 单步执行 |
| `c` | 继续执行 |
| `q` | 退出模拟器 |
| `r` | 显示所有寄存器 |
| `p <addr>` | 设置程序计数器 |
| `R <reg> <val>` | 设置寄存器值（支持数字或名称） |
| `m <addr>` | 显示地址处的 32 位值 |
| `d <start> <end>` | 转储内存范围 |
| `B <addr> [count]` | 转储字节（默认 16 个） |
| `S <addr>` | 从地址读取并显示字符串 |
| `w <addr> <val>` | 向内存写入 32 位值 |
| `u <addr> [count]` | 反汇编指令（默认使用当前 PC） |
| `D` | 切换反汇编/原始十六进制显示 |
| `b <addr>` | 设置断点 |
| `C` | 清除断点 |
| `h` / `?` | 显示帮助 |
| `<Enter>` | 重复上一个命令 |

## 📁 项目结构

```
RISCVemu/
├── CMakeLists.txt          # 可配置扩展的 CMake 构建文件
├── config.h.in             # 构建时配置模板
├── include/                # 公共头文件
│   ├── debugger.h          # 调试器接口
│   ├── disasm.h            # 反汇编器
│   ├── emu.h               # 核心模拟器状态
│   ├── exec.h              # 指令执行引擎
│   ├── fmap.h              # 文件映射工具
│   ├── logger.h            # 日志系统
│   ├── mem.h               # 内存管理
│   ├── mmu.h               # SV32 MMU 实现
│   ├── device/             # 设备模拟
│   │   ├── clint.h         # CLINT 定时器
│   │   ├── fdt.h           # 扁平设备树生成器
│   │   └── uart.h          # 16550 UART
│   └── extension/          # 扩展指令头文件
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
├── src/                    # 源代码
│   ├── debugger.c
│   ├── disasm.c
│   ├── emu.c
│   ├── exec.c
│   ├── fmap.c
│   ├── logger.c
│   ├── mem.c
│   ├── device/
│   │   ├── clint.c
│   │   └── fdt.c
│   └── extension/
│       ├── c_extension.c
│       ├── f_extension.c
│       ├── m_extension.c
│       ├── sdtrig_extension.c
│       ├── system.c
│       ├── zicntr_extension.c
│       ├── zicsr_extension.c
│       └── zifencei_extension.c
├── run/                    # 示例程序和脚本
│   ├── compile_run.sh      # 构建示例
│   ├── run_tests.sh        # 测试运行器
│   ├── build_tests.sh      # 测试构建器
│   ├── self_test.c         # CLINT 定时器自测
│   ├── run.c               # 模拟内核示例
│   ├── link.ld             # 裸机链接脚本
│   └── link_kernel.ld      # 内核链接脚本
├── tests/                  # riscv-tests（子模块）
└── LICENSE                 # GPLv3 许可证
```

## ⚙️ CMake 配置选项

| 选项 | 默认值 | 描述 |
|--------|---------|-------------|
| `CONFIG_ENABLE_DEBUGGER` | OFF | 内置调试器（`-d` 标志）；当前暂时禁用 |
| `CONFIG_SUPPORT_MISALIGN` | OFF | 支持非对齐内存访问（较慢） |
| `CONFIG_ENABLE_M_EXTENSION` | ON | M：整数乘除法 |
| `CONFIG_ENABLE_A_EXTENSION` | ON | A：原子操作 |
| `CONFIG_ENABLE_C_EXTENSION` | ON | C：16 位压缩指令 |
| `CONFIG_ENABLE_F_EXTENSION` | ON | F：单精度 FPU（需要 Zicsr） |
| `CONFIG_ENABLE_D_EXTENSION` | ON | D：双精度 FPU（需要 F） |
| `CONFIG_ENABLE_Q_EXTENSION` | ON | Q：四精度 FPU（需要 D） |
| `CONFIG_ENABLE_ZFH_EXTENSION` | ON | Zfh：半精度 FPU（需要 F） |
| `CONFIG_ENABLE_ZICSR_EXTENSION` | ON | Zicsr：CSR 指令 |
| `CONFIG_ENABLE_ZICNTR_EXTENSION` | ON | Zicntr：计数器指令 |
| `CONFIG_ENABLE_ZIFENCEI_EXTENSION` | ON | Zifencei：FENCE.I |
| `CONFIG_ENABLE_ZICOND_EXTENSION` | ON | Zicond：条件操作指令 |
| `CONFIG_ENABLE_UART_DEVICE` | ON | 0x10000000 处的 16550 UART |
| `CONFIG_ENABLE_FDT` | ON | 设备树 Blob 生成器 |
| `OPTIMIZE_FOR_SIZE` | OFF | 针对二进制大小优化（`-Os`） |
| `ENABLE_LTO` | OFF | 链接时优化 |

### 使用自定义扩展构建

```bash
cmake .. -DCONFIG_ENABLE_D_EXTENSION=OFF -DCONFIG_ENABLE_FDT=ON
make
```

## 📊 测试状态

使用 `make run_tests` 运行测试。支持的测试套件：

| 套件 | 测试内容 | 状态 |
|-------|-------|--------|
| `rv32ui-p-*` | 基础整数 | ✅ 全部通过 |
| `rv32um-p-*` | M 扩展 | ✅ 全部通过 |
| `rv32ua-p-*` | A 扩展 | ✅ 全部通过 |
| `rv32uc-p-*` | C 扩展 | ✅ 全部通过 |
| `rv32uf-p-*` | F 扩展 | ✅ 全部通过 |
| `rv32ud-p-*` | D 扩展 | ✅ 全部通过 |
| `rv32uzfh-p-*` | Zfh 扩展 | ✅ 全部通过 |
| `rv32uzicond-p-*` | Zicond | ✅ 全部通过 |
| `rv32mi-p-*` | 机器模式 | ✅ 全部通过 |
| `rv32si-p-*` | 监管模式 | ✅ 全部通过 |

预期测试输出：
```
[rv32ui-p-add] ✅ PASS
[rv32ui-p-addi] ✅ PASS
[rv32uf-p-fadd] ✅ PASS
...
```

## 🎯 已知限制

- 仅支持扁平二进制格式（不支持直接加载 ELF；请使用 `objcopy -O binary`）（即将支持！）
- 可移植性：可能无法在大端系统上工作（别碰那个神奇的解码器！我搞砸过好几次，又回滚了无数次）
- 性能：启用所有扩展时非常慢（可能只有 130MIPS），而仅启用 "I" 时约为 270MIPS（也许以后会做 JIT？）

## 🤝 贡献

欢迎贡献！请随时提交 Issue 和 Pull Request。

## 📝 许可证

本项目基于 [GPLv3](https://www.gnu.org/licenses/gpl-3.0.html) 许可证。

---

*仅供教育和个人使用。欢迎反馈和贡献！*
