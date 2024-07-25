#!/bin/bash

mode=$1

quantum=(200 100 50 30 20 15 10 5 3)

if [ $mode == "uintr" ] || [ $mode == "concord" ] || [ $mode == "signal" ]; then
    echo "Argument is 'uintr' or 'concord' or 'signal'"
else
    echo "Argument is not 'uintr' or 'concord' or 'signal'"
    exit 1  
fi


for q in "${quantum[@]}"
do
    echo "======== Quantum: $q us ========="
    
    pushd src/lib
    ns=$((q * 1000))
    sed -i "189s/.*/#define quantum ${ns}/" concord.c 
    make $mode
    popd


    pushd benchmarks/overhead
    
    rm -rf parsec-benchmark/pkgs/parsec_stats
    rm -rf parsec-benchmark/pkgs/results/parsec_stats-$mode-$q
    rm -rf phoenix/phoenix-2.0/phoenix_stats
    rm -rf phoenix/phoenix-2.0/results/phoenix_stats-$mode-$q
    rm -rf splash2/codes/splash2_stats
    rm -rf splash2/codes/results/splash2_stats-$mode-$q

    python3 run.py

    mv parsec-benchmark/pkgs/parsec_stats parsec-benchmark/pkgs/results/parsec_stats-$mode-$q
    mv phoenix/phoenix-2.0/phoenix_stats phoenix/phoenix-2.0/results/phoenix_stats-$mode-$q
    mv splash2/codes/splash2_stats splash2/codes/results/splash2_stats-$mode-$q

    popd

done
