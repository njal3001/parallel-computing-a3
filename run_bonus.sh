#!/bin/bash
#	CS3210 SBATCH Runner v1.0
#	
#	You will primarily be changing the settings in GENERAL SETTINGS and SLURM SETTINGS.
#	
#	USAGE: 		./run_sbatch.sh
#			Change the settings below this comment section (GENERAL and SLURM settings). 

#	DESCRIPTION: 	Runs programs with sbatch while addressing a few usability issues.
#				- Lets you work in any directory you want, no need to work in the home directory / copy files to homedir
#				- Automatically copies specified code folders to NFS using rsync
#				- Changes to home before running to avoid Slurm crashing
#				- Executes the job: the job copies the code folder from NFS to a local working dir without using sbcast 
#					- sbcast only works with single files, so we use srun
#				- The job then copies the output files (one each for slurm and the actual executable) back to NFS
#				- The runner then creates symlinks to the NFS logfiles in the user's submit directory
#				- It does some more utility things like handle some errors and print squeue/sacct output

# Exit when any command fails: remove this if necessary
set -e

if [[ $# -ne 1 ]]; then
    echo "Usage: ./run_bonus.sh <testcase>"
    echo "Example: ./run_bonus.sh testcases/sample1.in"
    exit 2
fi

##########################
# 1. GENERAL SETTINGS 	 #
##########################

# A meaningful name for this job
export SBATCH_JOB_NAME="bonus_job"

# You can change this to your username, but not necessary.
export USERNAME=$USER

# Change this to the directory with everything your job needs, will be copied to entirely to NFS!
export CODE_DIR=slurm/

# Change this to point at the main executable or shell script you want to run
# This is relative to CODE_DIR (the executable must be within CODE_DIR, or be accessible universally like `hostname`)
export EXECUTABLE=troons
# Change this to change the arguments passed to the executable, comment out this line if there are no args
export EXECUTABLE_ARGS=$1

# Destination directory in NFS that your code directory is copied to, not necessary to change
export NFS_DIR=/nfs/home/$USERNAME/$SBATCH_JOB_NAME/

# Change this to your job file, we have provided one example.
# This job file must be inside the CODE_DIR!
export SBATCH_FILE="troons.sh"


##########################
# 2. SLURM SETTINGS 	 #
##########################

# Uncomment this line to run your job on a specific partition
export SBATCH_PARTITION="i7-7700"

# Uncomment this line to run your job on a specific node (takes priority over partition)
# export SBATCH_WHICHNODE="soctf-pdc-006"

# How many nodes to run this job on
export SBATCH_NODES=1

# How many tasks the slurm controller should allocate resources for (leave at 1 for now)
export SBATCH_TASKS=1

# Memory required for each node
export SBATCH_MEM_PER_NODE="1G"

# Job time limit (00:10:00 --> 10 minutes)
export SBATCH_TIME_LIMIT="00:20:00"

# Job output and error names - these are relative to the local home directory
# Do NOT change this unless you change the copy step of the job script and the symlink step of this script
export SBATCH_OUTPUT="$SBATCH_JOB_NAME-%j.slurmlog"
export SBATCH_ERROR="$SBATCH_JOB_NAME-%j.slurmlog"
export PROGRAM_OUTPUT="$SBATCH_JOB_NAME-%j.slurmlog"

##################
# 3. EXECUTE 	 #
##################

# Copy all required code to the NFS directory (this overwrites the existing files if they are there)
[[ -e $CODE_DIR ]] || { echo "!!! Runner: CODE_DIR $CODE_DIR does not exist, quitting..."; exit 1; }
echo -e "\n>>> Runner: Changing directory to $CODE_DIR\n"
INITIAL_DIR=$PWD
cd $CODE_DIR
echo -e "\n>>> Runner: Synchronizing files between local directory (./) and remote directory ($NFS_DIR)\n"
mkdir -p $NFS_DIR/
rsync -av --progress . "$NFS_DIR/"

# Change to local home dir to execute batch script
cd /home/$USERNAME/

# Prepare to execute the job file
SBATCH_FILE_FULL="$NFS_DIR/$SBATCH_FILE"
[[ -f $SBATCH_FILE_FULL ]] || { echo "!!! Runner: sbatch file $SBATCH_FILE_FULL does not exist, quitting..."; exit 1; }
echo -e "\n>>> Runner: Executing $SBATCH_FILE_FULL with Slurm\n"

# Execute the sbatch command with all arguments
if [[ ! -z ${SBATCH_WHICHNODE} ]]; then
	set -x
	jobid=$(sbatch \
		--nodes=$SBATCH_NODES \
		--ntasks=$SBATCH_TASKS \
		--mem=$SBATCH_MEM_PER_NODE \
		--time=$SBATCH_TIME_LIMIT \
		--output=$SBATCH_OUTPUT \
		--error=$SBATCH_ERROR \
		-w $SBATCH_WHICHNODE \
		--parsable \
		$SBATCH_FILE_FULL)
	set +x
else
	set -x
	jobid=$(sbatch \
		--nodes=$SBATCH_NODES \
		--ntasks=$SBATCH_TASKS \
		--mem=$SBATCH_MEM_PER_NODE \
		--time=$SBATCH_TIME_LIMIT \
		--output=$SBATCH_OUTPUT \
		--error=$SBATCH_ERROR \
		--parsable \
			$SBATCH_FILE_FULL)
	set +x
fi
echo -e "\n>>> Runner: Submitted slurm job with ID $jobid"

##########################
# 4. CLEANUP AND UTIL 	 #
##########################

# Change to job submission directory
cd $INITIAL_DIR > /dev/null

# Create symlinks in current directory to the nfs logfiles
echo -e "\n>>> Runner: creating symlinks to NFS working directory and job logs"
# ln -nsf $NFS_DIR ./nfs_dir
ln -nsf $NFS_DIR/$SBATCH_JOB_NAME-$jobid.slurmlog ./bonus_latest_slurm_log.slurmlog
ln -nsf $NFS_DIR/$SBATCH_JOB_NAME-$jobid.out ./bonus_latest_program_log.out

echo -e "\n>>> Runner: printing queue and account status"

# Show queue just to confirm job status
echo -e "\n>>> Runner: Queue status: ...\n"
squeue -u $USERNAME

# Sleep to let the job hit the accounting system
sleep 1

# Display a snapshot of accounting
echo -e "\n>>> Runner: sacct status (last 5 rows): ...\n"
sacct -j $jobid --format=JobID,Start,End,Elapsed,NCPUS,NodeList,NTasks
echo -e "\nRun the command:\nsacct -j $jobid --format=JobID,Start,End,Elapsed,NCPUS,NodeList,NTasks\nto see job status"

# Finish
echo -e "\n>>> Runner: Finished. Please wait for job to end and then look at the logfile symlink in this folder."