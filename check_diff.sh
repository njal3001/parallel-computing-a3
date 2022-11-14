#!/bin/bash

if [[ $# -ne 3 ]]; then
    echo "./check_diff.sh <input> <nodes> <ntasks>"
    exit 2
fi

in=$1
nodes=$2
ntasks=$3

diff <(./run.sh $in $nodes $ntasks) <(./troons_seq $in)
