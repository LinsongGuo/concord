#!/bin/bash
LLVM_VERSION=11


# Default value for safepoint option
ENABLE_SAFEPOINT=OFF

# Check if the argument is "safepoint"
if [ "$#" -gt 0 ]; then
    if [ "$1" == "safepoint" ]; then
        ENABLE_SAFEPOINT=ON
    else
        echo "Usage: $0 [safepoint]"
        echo "  safepoint - Enable safepoint"
        exit 1
    fi
fi

rm -rf build
mkdir -p build
cd build
cmake -DLLVM_DIR=/usr/lib/llvm-${LLVM_VERSION}/cmake/ -DENABLE_SAFEPOINT=${ENABLE_SAFEPOINT} ..
make
cd ..

