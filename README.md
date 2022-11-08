# A03: Troons Reloaded
`Makefile` has been updated to use mpiCC.

`main.cc` has been updated to include the MPI headers.

## Getting Started

### Provided files

* `troons_seq`: an executable of a correct sequential implementation for you to test your program
* `Makefile`: makefile with basic recipes to build, build with debug symbols and clean
* `main.cc`: skeleton code with input logic
* `testcases/`: example testcases from the assignment brief to get you started
* `gen_test.py`: a sample testcase generator for you test extensively against the reference executable
* `.gitignore`: a sensible gitignore template for C++

### Building the project

This starter code comes with a Makefile and a few recipes that can be run from the command line.

```
make        # builds the project using mpiCC and C++17
make debug  # builds the project with debug symbols and a symbol `DEBUG` defined
make clean  # cleans your project directory
```

### Testing your code

After building, run the code with `srun -n <ntasks> ./troons <testcase_file>`. Your program should output to `stdout` (the command line
usually), and you can then pipe the output to a file like so:
`srun -n 4 ./troons testcases/sample1.in > sample1.out`.

One way to test is as follows:

1. Run a testcase and save the output of your program to a file. E.g. `srun -n 4 ./troons testcases/sample1.in > sample1.out`
2. Run the correct implementation executable provided with the same test case and save the output of that to another
   file. E.g. `./troons_seq testcases/sample1.in > sample1-correct.out`
3. Diff the outputs of the two files. E.g. `diff sample1.out sample1-correct.out` (note: we will use `diff -ZB` flags but just to be safe you should check strictly)

## Submitting your code

Submit your solution to this assignment by [creating a tagged release on GitHub](https://help.github.com/en/github/administering-a-repository/creating-releases) and providing a link to it on Canvas.

IMPORTANT: You can use C or C++ (C++ recommended), and you can modify any file in the starter code. **We only require
that when your code is cloned on a lab machine and `make submission` is run, an executable named `troons` is produced in
the same directory**. We will autograde your submissions, and reserve the right to deduct any number of marks if your
project does not follow this requirement.
