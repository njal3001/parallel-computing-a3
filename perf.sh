#!/bin/bash
#SBATCH --job-name=performance
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --mem=1gb
#SBATCH --time=00:30:00
#SBATCH --output=troons_%j.log
#SBATCH --error=troons_%j.log

echo "Running Performance Measurement!"
echo "Running on $(hostname)"
echo "Executing $EXEC with input $INPUT"

rm -rf /home/$USER/$EXEC
rm -rf /home/$USER/$INPUT
sbcast /nfs/home/$USER/$EXEC /home/$USER/$EXEC
sbcast /nfs/home/$USER/$INPUT /home/$USER/$INPUT

echo ""
echo "Output:"

perf stat -r 5 -e cache-references,cache-misses,cycles,instructions,branches,faults,migrations,duration_time -- /home/$USER/$EXEC $INPUT > /dev/null
# perf record -F 99 -- /home/$USER/$EXEC $INPUT > /dev/null
# cp perf.data /nfs/home/$USER/

cp "troons_$SLURM_JOB_ID.log" "/nfs/home/$USER/$EXEC-$SLURM_JOB_ID.log"