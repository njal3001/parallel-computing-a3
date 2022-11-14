#!/bin/bash

if [[ $# -ne 3 ]]; then
    echo "./run.sh <input> <nodes> <ntasks>"
    exit 2
fi

in=$1
nodes=$2
ntasks=$3

nfs_dir="/nfs/home/$USER"

cp troons $nfs_dir
cd ~
srun --nodes $nodes --ntasks-per-node $ntasks --time=00:60:00 $nfs_dir/troons $nfs_dir/$in
