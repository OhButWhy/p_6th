#!/bin/bash
# Добавьте в самое начало скрипта
echo "Benchmark started at $(date)" > progress.log

SIZES_ONE=(1024)
SIZES_MULTI=(128 256 512 1024)
RUNS=10
ONE_CORE="./smth_cpu_one"
MULTI_CORE="./smth_cpu_multi"

run_bench() {
    local prog=$1
    local size=$2
    echo "Running $prog with size $size" | tee -a progress.log
    local times=()
    for ((i=1; i<=RUNS; i++)); do
        echo "  Starting run $i at $(date)" >> progress.log
        output=$($prog --size $size --eps 1e-6 --iters 1000000)
        time_val=$(echo "$output" | grep -oP 'time:\s*\K[\d.]+')
        iter_val=$(echo "$output" | grep -oP 'iterations:\s*\K\d+')
        error_val=$(echo "$output" | grep -oP 'error:\s*\K[\d.e+-]+')
        echo "  Run $i: time=$time_val, iter=$iter_val, error=$error_val" | tee -a progress.log
        times+=($time_val)
    done
    mean=$(printf '%s\n' "${times[@]}" | awk '{sum+=$1} END {print sum/NR}')
    stddev=$(printf '%s\n' "${times[@]}" | awk -v mean=$mean '{sumsq+=($1-mean)^2} END {print sqrt(sumsq/(NR-1))}')
    echo "  Mean time: $mean, stddev: $stddev" | tee -a progress.log
    echo "---" | tee -a progress.log
}

echo "===== CPU onecore =====" | tee -a progress.log
for size in "${SIZES_ONE[@]}"; do
    run_bench "$ONE_CORE" $size
done

echo "===== CPU multicore =====" | tee -a progress.log
for size in "${SIZES_MULTI[@]}"; do
    run_bench "$MULTI_CORE" $size
done