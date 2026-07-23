<!--
SPDX-FileCopyrightText: 2026 2026 PhiLia093 phi_lia093@126.com

SPDX-License-Identifier: GPL-3.0-or-later
-->

# RISCVemu

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![C23](https://img.shields.io/badge/C-23-blue.svg)](https://en.cppreference.com/w/c/23)
[![CMake](https://img.shields.io/badge/CMake-3.10+-green.svg)](https://cmake.org/)

一个教学用 RV32IMA(U) 指令集模拟器，支持基础整数指令集（I）、乘除扩展（M）、原子操作扩展（A），以及用户态模式（U）的指令解码。使用 C23 编写，通过 CMake 构建，专为学习和探索 RISC-V 架构设计。

## ✨ 特性

- **完整的 RV32I 基础指令集**：所有整数指令
- **M 扩展**：乘除指令支持
- **A 扩展**：原子操作指令（LR/SC, AMO*）
- **U 模式支持**：识别并解码用户态指令，特权指令和 Zicsr/Zifencei 扩展作为 NOP 处理
- **4GB 内存空间**：采用虚拟页框分配，写时分配策略，高效利用内存
- **交互式调试器**：单步执行、寄存器查看/修改、内存读写、反汇编、断点
- **UART 模拟**：写入 `0x10000000` 的数据会作为 ASCII 字符直接输出到 stdout
- **Flat Binary 加载**：支持直接加载二进制文件到指定基址
- **通过 riscv-tests 测试**：除特权级相关测试外 100% PASS

## 📋 依赖

### 构建依赖
- C23 兼容编译器（GCC 13+ / Clang 16+）
- CMake 3.10+
- GNU Make

### 运行测试依赖
- [RISC-V GNU Toolchain](https://github.com/riscv-collab/riscv-gnu-toolchain) (`riscv32-unknown-elf-gcc`, `riscv32-unknown-elf-objcopy`)

## 🚀 快速开始

### 构建

```bash
# 克隆仓库（包含 submodule）
git clone --recursive <repository-url>
cd RISCVemu

# 构建模拟器
mkdir build && cd build
cmake ..
make
```

### 运行一个程序

```bash
# 基本用法
./RISCVemu <program.bin> <base_address>

# 示例：加载 run.bin 到 0x1000
./RISCVemu run.bin 0x1000

# 带调试器启动
./RISCVemu run.bin 0x1000 -d

# 设置断点
./RISCVemu run.bin 0x1000 -d -b 0x1200
```

### 运行测试套件

```bash
# 首次使用需要初始化 riscv-tests submodule
git submodule update --init
cd tests && git submodule update --init && cd ..

# 构建测试程序并运行
make build_tests   # 编译 riscv-tests 并转换为 bin 格式
make run_tests     # 运行所有测试
```

## 🔧 调试器命令

启动时使用 `-d` 参数进入调试模式，支持以下交互式命令：

| 命令 | 说明 |
|------|------|
| `s` / `n` | 单步执行 |
| `c` | 继续执行 |
| `q` | 退出模拟器 |
| `r` | 显示所有寄存器 |
| `p <addr>` | 设置 PC 指针 |
| `R <reg> <val>` | 设置寄存器值（支持数字或名称，如 `a0`, `sp`） |
| `m <addr>` | 显示指定地址的 32-bit 值 |
| `d <start> <end>` | 显示内存范围 |
| `B <addr> [count]` | 显示字节（默认 16 字节） |
| `S <addr>` | 从地址读取并显示字符串 |
| `w <addr> <val>` | 写入 32-bit 值到内存 |
| `u <addr> [count]` | 反汇编指令（默认当前 PC） |
| `D` | 切换反汇编/原始十六进制显示 |
| `b <addr>` | 设置断点 |
| `C` | 清除断点 |
| `h` / `?` | 显示帮助 |
| `<Enter>` | 重复上一条命令 |

## 📁 项目结构

```
RISCVemu/
├── CMakeLists.txt          # CMake 构建配置
├── include/                # 公共头文件
│   ├── debugger.h          # 调试器接口
│   ├── disasm.h            # 反汇编器
│   ├── emu.h               # 模拟器核心状态
│   ├── exec.h              # 指令执行引擎
│   ├── fmap.h              # 内存映射工具
│   ├── logger.h            # 日志系统
│   ├── mem.h               # 内存管理
│   └── extension/          # 扩展指令头文件
├── src/                    # 源代码
│   ├── debugger.c          # 调试器实现
│   ├── disasm.c            # 反汇编器实现
│   ├── emu.c               # 主程序入口
│   ├── exec.c              # 指令执行核心
│   ├── fmap.c              # 文件映射
│   ├── logger.c            # 日志实现
│   ├── mem.c               # 内存管理
│   └── extension/          # 扩展指令实现（M/A）
├── run/                    # 示例程序
│   ├── compile_run.sh      # 编译示例程序脚本
│   ├── link.ld             # 链接脚本
│   ├── run.c               # 示例 C 程序
│   └── run.bin             # 预编译二进制
├── tests/                  # riscv-tests (submodule)
│   ├── isa/                # 指令集测试
│   ├── benchmarks/         # 性能测试
│   └── ...
└── LICENSE                 # GPLv3 许可证
```

## ⚙️ CMake Targets

| Target | 说明 |
|--------|------|
| `make` / `make all` | 构建模拟器可执行文件 |
| `make run` | 构建并运行 `run.bin` 示例 |
| `make debug` | 以调试模式运行示例 |
| `make build_tests` | 构建 riscv-tests |
| `make run_tests` | 运行所有测试 |
| `make tidy` | 使用 clang-format 格式化代码 |
| `make rebuild` | 清理并重新构建 |

## 📊 测试状态

通过 `make run_tests` 运行测试，所有 `rv32ui-p-*`、`rv32um-p-*`、`rv32ua-p-*` 测试中：

- ✅ **全部通过**（`rv32ui-p-ma_data` 除外，因该测试涉及异常/中断处理，当前版本仅模拟用户态指令行为）
- ⏭️ `rv32ui-p-ma_data`：预期跳过（特权级/中断未实现）

预期测试输出示例：
```
[rv32ui-p-add] ✅ PASS
[rv32ui-p-addi] ✅ PASS
[rv32um-p-mul] ✅ PASS
[rv32ua-p-amoadd] ✅ PASS
...
```

## 🎯 已知限制

- 不支持特权级和中断/异常处理
- 不支持 Zicsr 和 Zifencei 扩展（解码为 NOP）
- 仅支持 Flat Binary 格式输入（不支持 ELF 直接加载）
- 程序基址需与编译时链接脚本匹配

## 📝 许可证

本项目使用 [GPLv3](https://www.gnu.org/licenses/gpl-3.0.html) 许可证。

---

*仅供教学和个人项目使用。欢迎贡献和反馈！*
