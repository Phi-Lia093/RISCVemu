#!/bin/bash

BUILD_DIR="$1"
SOURCE_DIR="$2"

# Create tmp directory
mkdir -p "$BUILD_DIR/tests/tmp"

# Build tests
cd "$SOURCE_DIR/tests" && make XLEN=32

# Copy files
cp "$SOURCE_DIR/tests/isa/"rv32ui*-p-* "$BUILD_DIR/tests/tmp/"
cp "$SOURCE_DIR/tests/isa/"rv32um*-p-* "$BUILD_DIR/tests/tmp/"
cp "$SOURCE_DIR/tests/isa/"rv32ua*-p-* "$BUILD_DIR/tests/tmp/"

# Convert all non-bin files to bin
for file in "$BUILD_DIR/tests/tmp/"*; do
    if [[ -f "$file" ]] && [[ ! "$file" =~ \.bin$ ]] && [[ ! "$file" =~ \.(elf|o|S|s|dump|out|log)$ ]]; then
        riscv32-unknown-elf-objcopy -O binary "$file" "$file.bin"
    fi
done
