#!/bin/bash

rsync -av ./testcases ./slurm
cp ./troons ./slurm/troons
cp ./troons_seq ./slurm/troons_seq