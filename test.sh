#!/usr/bin/bash

times=20

hosts_lists="
pcunix137
pcunix137,pcunix139
pcunix137,pcunix139,pcunix140,pcunix141
pcunix137,pcunix139,pcunix140,pcunix141,pcunix142,pcunix143,pcunix144,pcunix145
"

node_counts="
2
4
8
16
32"

for hosts in $hosts_lists; do
    for node_count in $node_counts; do
        echo -n "$node_count,"
        for i in $(seq $times); do
            #time mpirun -n $node_count -hosts pcunix137,pcunix139,pcunix140,pcunix141 src/hpc_project ../04_2025_short.csv
            execution_time=$((time (mpirun -n $node_count -hosts "$hosts" src/hpc_project "$@" &>/dev/null)) 2>&1 | grep real | awk -F'[ms]' '{print $(NF - 1)}')
            echo -n "$execution_time,"
        done
        echo
    done
done
