#!/bin/bash

set -x
set -e

if [[ $# -ne 2 ]]; then
    echo "Usage: ./run_troons.sh <testcase> <partition>"
    echo "Example: ./run_troons.sh sample1 i7-7700"
    exit 2
fi

INPUT=$1
PARTITION=$2

rsync -av /home/$USER/cs3210-a3-a3-a0200705x_a0260770h/testcases /nfs/home/$USER
cp /home/$USER/cs3210-a3-a3-a0200705x_a0260770h/troons /nfs/home/$USER/troons

cd /home/$USER
srun -n 4 --time=00:30:00 /nfs/home/$USER/troons /nfs/home/$USER/testcases/$INPUT.in > /home/$USER/cs3210-a3-a3-a0200705x_a0260770h/$INPUT.out

cd /home/$USER/cs3210-a3-a3-a0200705x_a0260770h
./troons_seq testcases/$INPUT.in > $INPUT-correct.out
diff $INPUT.out $INPUT-correct.out > diff.out