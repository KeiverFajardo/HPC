#!/usr/bin/bash

salida="salida.csv"

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
    echo -n "\"$hosts\"," | tee "$salida"
    for node_count in $node_counts; do
        echo -n "$node_count," | tee "$salida"
        for i in $(seq $times); do
            #time (mpirun -n $node_count -hosts "$hosts" src/hpc_project "$@")
            execution_time=$((time (mpirun -n $node_count -hosts "$hosts" src/hpc_project "$@")) 2>&1 | grep real | awk -F'[ms]' '{print $(NF - 1)}')
            echo -n "$execution_time," | tee "$salida"
        done
        echo
    done
done
