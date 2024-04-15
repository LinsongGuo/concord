#!/bin/bash

mode=$1
quantum=(100 50 20 15 10 5 2)
#quantum=(5)

if [ $mode == "uintr" ] || [ $mode == "concord" ]; then
    echo "Argument is 'uintr' or 'concord'"
else
    echo "Argument is not 'uintr' or 'concord'"
    exit 1  
fi


for q in "${quantum[@]}"
do
    echo "======== Quantum: $num========="
    
    pushd src/lib
    ns=$((q * 1000))
    sed -i "176s/.*/#define quantum ${ns}/" concord.c 
    make $mode
    popd


    pushd benchmarks/overhead
    
    rm -rf parsec-benchmark/pkgs/parsec_stats
    rm -rf parsec-benchmark/pkgs/parsec_stats-$mode-$q
    rm -rf phoenix/phoenix-2.0/phoenix_stats
    rm -rf phoenix/phoenix-2.0/phoenix_stats-$mode-$q
    rm -rf splash2/codes/splash2_stats
    rm -rf splash2/codes/splash2_stats-$mode-$q

    python3 run.py

    mv parsec-benchmark/pkgs/parsec_stats parsec-benchmark/pkgs/parsec_stats-$mode-$q
    mv phoenix/phoenix-2.0/phoenix_stats phoenix/phoenix-2.0/phoenix_stats-$mode-$q
    mv splash2/codes/splash2_stats splash2/codes/splash2_stats-$mode-$q

    popd

done
