# Scripts for MSPSP experiments

## Pre-starting scripts

* `make_binary.sh`

    Compiles the code and generates the binary.

    **Environment variables:**

    * `W_SCHEDULING_BIN_DIR`: the root directory of the mspsp project.

    **Output:**

    * `bin/mspsp`: the compiled binary.

* `make_repository.sh`

    Creates a list of input files to be used in the experiments.

    **Environment variables:**

    * `W_DATA_DIRS`: the folders where the instances are located. Supports a list of paths separated by `;`.

    * `W_REPOSITORY_DIR`: parent folder to save the generated list (optional). Default: `pwd`.

    **Output:**

    * `repository.txt`: the list of input files (instances of mspsp) to be used in the experiments. The file is under the `W_REPOSITORY_DIR` path.

## Scripts to run the experiments

* `bootstrap.sh`

    This script takes the list of formalisms
![formula](https://latex.codecogs.com/svg.image?F), the corresponding list of
solvers ![formula](https://latex.codecogs.com/svg.image?S) and the meaningful
implied constraints ![formula](https://latex.codecogs.com/svg.image?I) and
produces all combinations of formalisms, solvers, and implied constraint sets.

    More formally:

    * ![formula](https://latex.codecogs.com/svg.image?F): the list of formalisms (e.g.,`"opb;sat"`).

    * ![formula](https://latex.codecogs.com/svg.image?S): the list of solver lists, one list per formalism (e.g.,`"sol_a,sol_b;sol_a',sol_b',sol_c'"`).

    * Let ![formula](https://latex.codecogs.com/svg.image?g) be the function that maps each formalism to its list of solvers ![formula](https://latex.codecogs.com/svg.image?\textstyle%20g(f)%20=%20B)

    * ![formula](https://latex.codecogs.com/svg.image?I): the set of meaningful implied constraints.

        |id|comb|
        |-|-|
        |"3"|3|
        |"1"          |1|
        |"2"          |2|
        |"1,2"        |1,2|
        |"empty" |$\emptyset$|

    *  Be ![formula](https://latex.codecogs.com/svg.image?W) the algorithm that send the jobs configured by ![formula](https://latex.codecogs.com/svg.image?(f,%20s,%20i)).

        ![formula](https://latex.codecogs.com/svg.image?\forall%20f%20\in%20F,\%20\forall%20s%20\in%20g(f),\%20\forall%20i%20\in%20I:\%20W(f,%20s,%20i))


    **Environment variables:**

    * `W_FORMALISM`: list of formalisms.

    * `W_SOLVERS`: list of solvers, separated by `;` per formalism.

    * `W_IMPLIED_CONSTRAINTS`: list of meaningful implied constraints.

    **Output:**

    * Executes ![formula](https://latex.codecogs.com/svg.image?\textstyle|F|%20\times%20\sum_{B%20\in%20S}|B|%20\times%20|I|) the algorithm ![formula](https://latex.codecogs.com/svg.image?W).

* `send_jobs.sh`

    This script generates the jobs to be executed in the cluster. The number of
jobs depends on the number of instances $-$ corresponding to the number of
tasks.

    The cluster supports a maximum number of tasks per job, so the script
generates as many jobs as needed to cover all instances.

    **Input:**

    * `$1`: the formalism to be used.

    * `$2`: the solver to be used.

    * `$3`: the implied constraints to be used.

    **Environment variables:**

    * `W_REPOSITORY_PARENT`: parent folder to get the generated list (optional). Default: `pwd`.

    * `W_RESULTS_FOLDER`: the folder where the results will be stored (optional). Default: `results`.

* `run_task.slurm`

    This script actually makes a task in the cluster.  It first gets the
instance to be solved by indexing over the list of instances `repository.txt`
under the path `W_REPOSITORY_DIR` (more formally, from the set
![formula](https://latex.codecogs.com/svg.image?L)).

    After that, call the script `experiment.py` with the following parameters:

    - mspsp2smt binary path (generates the encoding for the given instance and
      formalism)

    - instance path

    - formalism

    - solver

    - implied constraint

    More formally, this script runs an experiment
![formula](https://latex.codecogs.com/svg.image?K) for a given combination of
formalism, solver, implied constraints, and the instance indexed by
`SLURM_ARRAY_TASK_ID`
![formula](https://latex.codecogs.com/svg.image?(f,%20s,%20i,%20l)).

    ![formula](https://latex.codecogs.com/svg.image?\forall%20f%20\in%20F,\%20\forall%20s%20\in%20g(f),\%20\forall%20i%20\in%20I,\%20\forall%20l%20\in%20L:\%20K(f,%20s,%20i,%20l))

    **Input:**

    * `$1`: the formalism to be used.

    * `$2`: the solver to be used.

    * `$3`: the implied constraints to be used.

    **Environment variables:**

    * `SLURM_ARRAY_TASK_ID`: index given by the cluster to pick the instance to
      be solved.

    * `W_REPOSITORY_DIR`: parent folder to get the generated list
      (repository of instances). Default: `pwd`.

    * `W_EXPERIMENT_PY_SCRIPT`: path to the script that actually will run the
      experiment.

    * `W_MSPSP2SMT_PATH`: path to the compiled binary for generating encodings.
