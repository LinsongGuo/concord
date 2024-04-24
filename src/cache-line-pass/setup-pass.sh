#!/bin/bash
LLVM_VERSION=11

mkdir -p build
cd build
cmake -DLLVM_DIR=/usr/lib/llvm-${LLVM_VERSION}/cmake/ ..
make
cd ..

