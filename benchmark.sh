#!/bin/bash

SIZES_ONE=(128 256 512)
SIZES_MULTI=(128 256 512 1024)

RUNS=20

ONE_CORE="./smth_cpu_one"
MULTI_CORE="./smth_cpu_multi"

run_bench() {
    local prog=$1
    local size=$2
    echo "Running $prog with size $size"
    local times=()
    for ((i=1; i<=RUNS; i++)); do
        output=$($prog --size $size --eps 1e-6 --iters 1000000)
        time_val=$(echo "$output" | grep -oP 'time:\s*\K[\d.]+')
        iter_val=$(echo "$output" | grep -oP 'iterations:\s*\K\d+')
        error_val=$(echo "$output" | grep -oP 'error:\s*\K[\d.e+-]+')
        echo "  Run $i: time=$time_val, iter=$iter_val, error=$error_val"
        times+=($time_val)
    done
    mean=$(printf '%s\n' "${times[@]}" | awk '{sum+=$1} END {print sum/NR}')
    stddev=$(printf '%s\n' "${times[@]}" | awk -v mean=$mean '{sumsq+=($1-mean)^2} END {print sqrt(sumsq/(NR-1))}')
    echo "  Mean time: $mean, stddev: $stddev"
    echo "  Iterations: $iter_val (should be consistent), error: $error_val"
    echo "---"
}

echo "===== CPU onecore ====="
for size in "${SIZES_ONE[@]}"; do
    run_bench "$ONE_CORE" $size
done

echo "===== CPU multicore ====="
for size in "${SIZES_MULTI[@]}"; do
    run_bench "$MULTI_CORE" $size
done