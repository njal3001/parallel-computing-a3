#!/bin/bash

if [[ $# -ne 2 ]]; then
    echo "./local_check_diff.sh <input> <n>"
    exit 2
fi

in=$1
n=$2

diff <(mpirun -n $n ./troons $in) <(./troons_seq $in)
