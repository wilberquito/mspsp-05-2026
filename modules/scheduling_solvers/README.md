# scheduling_solvers

Solvers for different scheduling problems of the family of the
Resource-Constrained Project Scheduling Problem, based on translations to SAT,
MaxSAT, PB, or SMT

This repository works with an external repository `smtapi` as a submodule.

## How to use the solvers?

1) Clone the repository and its submodules.

```sh
git clone git@github.com:udg-lai/scheduling_solvers.git --recurse-submodules
```

2) Build the project if needed with `make`.

```sh
make clean && make

```

3) Call any of the executable programs under the `bin` path, e.g.,

```sh
./bin/release/mspsp2smt -e=1 -f=opb -E=timepb --print-nonoptimal=0 rcpsp-instances/MSPSP/set-1a/inst_set1a_sf0.5_nc1.5_n20_m10_00.dzn.txt > test.opb

```

## Submodules

As said, this repository contains external repositories under the `submodules` path.

**Clone the experimental repository:**

```sh
git clone git@github.com:udg-lai/MSPSP.git --recurse-submodules

```

**Pull the latest changes from submodules:**

```sh
git submodule update --remote --recursive

```

**Persist the commit of submodules:**

```sh
git submodule sync

```
                                                                                                                       
