#!/bin/bash

TOOLCHAIN_BIN=$(dirname $(find ~ -name "aarch64-none-linux-gnu-gcc" 2>/dev/null | head -n 1))
export CROSS_COMPILE=${TOOLCHAIN_BIN}/aarch64-none-linux-gnu-

make clean
make CROSS_COMPILE=${CROSS_COMPILE} all



