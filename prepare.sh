#!/bin/bash

rsync -av --delete ./testcases ./slurm
cp ./troons ./slurm/troons
cp ./troons_seq ./slurm/troons_seq