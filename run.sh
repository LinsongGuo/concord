#!/bin/bash

mode=$1

quantum=(200 100 50 30 20 15 10 5 3)
# quantum=(100000000)

if [ $mode == "uintr" ] || [ $mode == "concord" ] || [ $mode == "safepoint" ] || [ $mode == "signal" ]; then
    echo "Argument is $mode"
else
    echo "Argument is not 'uintr' or 'concord' or 'safepoint' or 'signal'"
    exit 1  
fi

# Build cocnord/safepoint pass.
if [ "$mode" == "concord" ]; then
    pushd src/cache-line-pass
    ./setup-pass.sh
    popd
elif [ "$mode" == "safepoint" ]; then
    pushd src/cache-line-pass
    ./setup-pass.sh safepoint
    popd
fi


mkdir -p benchmarks/overhead/parsec-benchmark/pkgs/results 
mkdir -p benchmarks/overhead/phoenix/phoenix-2.0/results
mkdir -p benchmarks/overhead/splash2/codes/results


for q in "${quantum[@]}"
do
    echo "======== Quantum: $q us ========="
    
    pushd src/lib
    ns=$((q * 1000))
    sed -i "189s/.*/#define quantum ${ns}/" concord.c
    if [ "$mode" == "safepoint" ]; then
        make concord
    else 
        make "$mode"
    fi
    popd

    pushd benchmarks/overhead
    
    rm -rf parsec-benchmark/pkgs/parsec_stats
    rm -rf parsec-benchmark/pkgs/results/parsec_stats-$mode-$q
    rm -rf phoenix/phoenix-2.0/phoenix_stats
    rm -rf phoenix/phoenix-2.0/results/phoenix_stats-$mode-$q
    rm -rf splash2/codes/splash2_stats
    rm -rf splash2/codes/results/splash2_stats-$mode-$q

    python3 run.py overhead_results-$mode.txt

    mv parsec-benchmark/pkgs/parsec_stats parsec-benchmark/pkgs/results/parsec_stats-$mode-$q
    mv phoenix/phoenix-2.0/phoenix_stats phoenix/phoenix-2.0/results/phoenix_stats-$mode-$q
    mv splash2/codes/splash2_stats splash2/codes/results/splash2_stats-$mode-$q

    popd

done