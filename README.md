Open-David
====

# About

Open-David is OSS version of David,
which is a successor to [Phillip](https://github.com/kazeto/phillip),
the first-order logic abductive reasoner in C++.

-----

# Install

## System requirement

Open-David works under Linux and Windows (and maybe Mac OS X).  
Installing this software, the following softwares / libraries is needed.

- C++ compiler supporting C++11
- ILP solver

In the current version, following solvers are available.

- [Gurobi optimizer](http://www.gurobi.com)
- [lp_solve 5.5](http://lpsolve.sourceforge.net/5.5/)
- [CBC](https://projects.coin-or.org/Cbc)
- [SCIP](https://scip.zib.de/)

## Compiling on Linux


1. Get a clone of Open-David from GitHub.

2. Configure environment variables for ILP solvers.

    - If you use Gurobi optimizer:
        + Add the path of the include directory of Gurobi optimizer to `CPLUS_INCLUDE_PATH`
        + Add the path of the library directory of Gurobi optimizer to `LD_LIBRARY_PATH`
        + If the version of your Gurobi optimizer is not 6.5x, please modify `makefile`

    - If you use lp_solve 5.5:
        + Add the path of the include directory of lp_solve to `CPLUS_INCLUDE_PATH`
        + Add the path of the library directory of lp_solve to `LD_LIBRARY_PATH`

    - If you use CBC:
        + Set the path of the directory of CBC to `CBC_HOME`
    
    - If you use SCIP:
        + Set the path of the directory of SCIP to `SCIP_HOME`

3. Compile Open-David by executing `make` command in the directory where you cloned Open-David.

    - To specify ILP solvers which you use, set name of the ILP solvers to the variable `solver`:
        + Gurobi optimizer :: `gurobi`
        + lp_solve :: `lpsolve`
        + CBC :: `cbc`
        + SCIP :: `scip`
        + For example, if you use Gurobi optimizer and CBC, execute following command:

                $ make solver=gurobi,cbc

-----

# Getting Started

## Prepare Input

At first, write your observation and knowledge base in David-file format.

For example, let's consider following knowledge base and observation for coreference resolution task.

- kb.dav

        rule kb2 { steal-vb(x, y) => criminal-jj(x) }
        rule kb1 { criminal-jj(x) => arrest-vb(y, x) }

- obs.dav

        # "Tom stole jewels. Police arrested him."
        problem obs1 {
            observe { steal-vb(Tom, Jewel) ^ arrest-vb(Police, he) }
        }

`tools/david-mode.el` provides Emacs mode for David-file format.
If you are an Emacs user, this may support you to write input files.

## Compile

As well as Phillip, Open-David uses the compiled knowledge base on inference.
You need to compile your knowledge base at first.

    $ bin/david compile -k <KB_PREFIX> [OPTIONS] [INPUT]

For example, the following command compiles rules in `kb.dav` and generates the compiled KB at `compiled/kb`.

    $ bin/david compile -k compiled/kb kb.dav

Each time you change the knowledge base, you need to compile it.

## Inference

David takes files of observations as input and outputs the inference results in JSON format.

    $ bin/david infer -c <C1>,<C2>,<C3> -k <KB_PREFIX> [OPTIONS] [INPUTS]

`-c` option specifies how to perform abductive reasoning and `-k` option specifies the path of the compiled KB.
These two options are necessary.

The arguments of `-c` option (`C1`, `C2` and `C3`) specifies algorithms to use in each step in inference.

1. `C1` specifies how to generate candidates of hypotheses.

    - `naive` :: Simple, beadth first search-like algorithm
    - `astar` :: A* search-like algorithm

2. `C2` specifies how to evaluate the goodness of each candidate.

    - `weighted` :: Weighted Abduction (Hobbs et al. 1993)
    - `etcetera` :: Etcetera Abduction (Gordon, 2016)

3. `C3` specifies which solver to use.

    - `lpsolve` :: lp_solve 5.5
    - `gurobi` :: Gurobi optimizer
    - `cbc` :: CBC
    - `scip` :: SCIP

For example, the following command executes abduction to `obs.dav` with using Weighted Abduction and Gurobi optimizer.

    $ bin/david infer -c naive,weighted,gurobi -k compiled/kb obs.dav

Open-David outputs the inference result as JSON.

