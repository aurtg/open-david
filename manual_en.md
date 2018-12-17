# Open-David

The target of this manual is `open-david 1.73`.

-----

# Installation

This section shows how to install Open-David on Linux.

At first, in order to install Open-David, following softwares are necessary.

- C++ compiler supporting C++11
- At least one from the following ILP solvers
	- [lp_solve](http://lpsolve.sourceforge.net/5.5/)
	- [Gurobi Optimizer](https://www.octobersky.jp/products/gurobi.html)
	- [CBC](https://projects.coin-or.org/Cbc)
	- [SCIP](http://scip.zib.de/)

Next, configure environment variables as follows:

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

Finally, compile the binary by `make` command.  
Here, specify the ILP solvers which you use by setting the following values to the variable `solver`:

- `lpsolve` :: lp_solve
- `gurobi` :: Gurobi optimizer
- `cbc` :: CBC
- `scip` :: SCIP

For example, if you use Gurobi optimizer and CBC, the compile command will be as follows.

    $ make solver=gurobi,cbc

-----

# Command

The command to execute Open-David takes following format.

	$ ./david <MODE> <OPTIONS> <INPUTS>

## Modes

The first argument specifies the basic behavior ("mode") of Open-David.
The following two modes are available.

- `compile` :: Takes rules as input and constructs a database of background knowledge.
- `infer` :: Takes observations as input and performs abductive reasoning.

The capital letters of the modes (`c` and `i`) are available as the name of modes, too.

## Options

You can control Open-David's behavior via command options.
The format of command options in Open-David is similar to general Linux commands.

- `-x` :: Short option without argument
- `-x yyy` :: Short option with argument
- `--xxx` :: Long option without argument
- `--xxx=yyy` :: Long option with argument

The short options are to control the basic configuration.
It throws an exception to set a wrong short option.

The basic options are as follows:

- `-k PATH` :: Specifies the path of compiled background knowledge. This option is necessary in all modes.
- `-H KEYWORD` :: Specifies the type of heuristics to use in KB compilation. Refer to *Heuristics* section for detail. On default, `-H basic` is set.
- `-C` :: In `infer` mode, performs KB compilation before abductive inference.
- `-c KEYWORDS` :: Specifies components to use in inference. Refer to *Components for inference* section for detail.
- `-o KEYWORD` :: Specifies what to output as the result. On default, `-o mini` is set.
	- `-o mini` :: Outputs only the elements included in the solution hypothesis.
	- `-o ilp` :: Outputs the ILP problem and its solution.
	- `-o full` :: Outputs all elements included in the latent hypotheses set.
- `-o KEYWORD:PATH` :: Outputs the result of the specified type to the path given.
	- `-o mini:PATH` :: Outputs the elements included in the solution hypothesis to `PATH`.
	- `-o ilp:PATH` :: Outputs the ILP problem and its solution to `PATH`.
	- `-o full:PATH` :: Outputs all elements included in the latent hypotheses set to `PATH`.
- `-t STR` :: Performs abductive reasoning to only observations which match the condition given.
- `-t !STR` :: Performs abductive reasoning to only observations which do not match the condition given.
- `-T TIME` :: Set timeout.
- `-T lhs:TIME` :: Set timeout for the process of the latent hypotheses set generation.
- `-T cnv:TIME` :: Set timeout for the process to convert LHS to ILP problem.
- `-T sol:TIME` :: Set timeout for the process to find the solution with ILP solver.
- `-p` :: Uses the perturbation method in ILP conversion.
- `-P NUM` :: Specifies the maximum number of parallel threads.
- `-v INT` :: Specifies the verboseness level. On default, `-v 1` is set.
- `-h` :: Shows the help.

Following sections provide additional explanations about some options.

### Component Option (`-c`)

On `infer` mode, it is necessary to set `-c` option and specify the components to use.
The components are categorized to the following three types.

1. Components to generate latent hypotheses set from observation
2. Components to convert latent hypotheses sets into ILP problems
3. Components to find the solution to ILP problem with ILP solver

The components are written in comma-separated, such as `-c xxx,yyy,zzz`.
The order is same as shown above.

On default, `-c astar,weighted,gurobi` is set.

If invalid components are set, Open-David will throw an exception.
Refer to *Components* section for the components available now.

### Output Option (`-o`)

You can following preprocessor in `-o` option:

- `$TIME` :: Will be replaced with the string of the time when Open-David is called.
    - Ex. `"out/$TIME.json"` → `"out/20170602_173520.json"`
- `$DAY` :: Will be replaced with the string of the day when Open-David is called.
    - Ex. `"$DAY/out.json"` → `"20170602/out.json"`

### Target Option (`-t`)

Arguments of `-t` option express the matching patterns to specify which observation to infer.

`-t` option interprets its argument as follows:

- Generally, it matches observations which names match the optional argument.
- If the optional argument starts with `*`, it is interpreted as wild card of backward matching.
- If the optional argument ends with `*`, it is interpreted as wild card of forward matching.
- If the optional argument starts with `*` and ends with `*`, it is interpreted as wild card of partial matching.
- If the optional argument is the format of `i:NUM`, it matches observations which indices match `NUM`.
- If the optional argument starts with `!`, it matches observations which do not match the pattern.
- Negation (`!`) can be used together with wild cards or index matching (Ex. `-t !i:3`).
- Using multiple `-t` options, Open-David targets observations which match any of them.

### Timeout Option (`-T`)

Characters of time units (`s`, `m`, `h`) are available in `-T` option.
We show some examples as follows:

- `-T 30s` :: Sets timeout of 30 seconds.
- `-T 1.5m` :: Sets timeout of 1.5 minutes = 90 seconds.
- `-T 0.5h` :: Sets timeout of 0.5 hour = 30 mintes = 1800 seconds.

### Perturbation Option (`-p`)

Perturbation option adds small noise to coefficients of ILP variables in the objective function of an ILP problem.
It can improve the computational efficiency and can partially control which solution is output in the situation where the multiple best solutions exist.

Generally `-p` option adds noises of positive values.
Therefore, a solution consisting more true-variable will be selected in a maximization problem.
(Opposite in a minimization problem)

Using `--negative-pertuabation` option together with `-p` option, it adds noises of negative values.
Here, a solution consisiting less true-variable will be selected in a maximization problem.

### Verbosity Option (`-v`)

Open-David writes logs to STDERR.
You can control the verbosity of the logs by `-v` option.

`-v 0` log nothing and `-v 5` is the most verbose.

## Inputs

Command arguments being not command options will be interpreted as input files.

If the command contains no input files, Open-David will read input from STDIN.

-----

# Heuristics

Open-David computes relatedness between predicates in KB in advance,
and uses it as heuristics in searching candidate hypotheses.

## Null Heuristic (`null`)

This heuristic function returns a fixed value regardless of the knowledge base.

This cannot improve the computational efficiency of inference,
but has the advantage that its compilation process is computationally light.

## Predicate Distance (`basic`)

This computes relatedness between predicates in KB in advance.

Regarding this, the following options are available.

- `--max-distance=FLOAT` :: Specifies the maximum distance. The default value is infinity.
- `--max-depth=INT` :: Specifies the maximum depth. The default value is `5`.

*Depth* means the number of rules between a predicate pair.

-----

# Components for Inference

This section provides the explanation of components specified by `-c` option.

## LHS Generator

Components in this category generate the latent hypotheses set (LHS) from the observation.

Regarding these components, the following options are commonly available.

- `--max-depth=NUM` :: Specifies the maximum depth from observation. The default value is `9`.
- `--max-nodes=NUM` :: Specifies the maximum number of nodes in LHS. The default value is infinity.
- `--max-edges=NUM` :: Specifies the maximum number of edges in LHS. The default value is infinity.

Currently, the following components are avaiable.

### Naive Generator (`naive`)

This generate LHS without using heuristics.
This corresponds to `-c lhs=depth` option in Phillip.

### A-Star-based Generator (`astar`)

This generate LHS in A* search-like algorithm with using heuristics.
This corresponds to `-c lhs=a*` option in Phillip.

## ILP Converter

Components in this category converts the latent hypotheses set into ILP problem.

Regarding these components, the following options are commonly available.

- `--disable-cost-provider-warning` :: Disables warnings for wrong weights.
- `--disable-parallel-johnson` :: Disables the parallel processing in the loop detection.
- `--set-const-true=INT` :: Fix the values of the ILP-variables of the given indices (comma separated) to `1.0`.
- `--set-const-false=INT` :: Fix the values of the ILP-variables of the given indices (comma separated) to `0.0`.
- `--true-atom=ATOM` :: Fix the values of the ILP-variables corresponding to the given logical fomulas (comma separated) to `1.0`.
- `--false-atom=ATOM` :: Fix the values of the ILP-variables corresponding to the given logical fomulas (comma separated) to `0.0`.
- `--pseudo-positive` :: Defines ILP-constraints to obtain the solution which satisfies the logical formula of `require` in the observation.
- `--pseudo-negative` :: Defines ILP-constraints to obtain the solution which does not satisfy the logical formula of `require` in the observation.

Currently, the following components are avaiable.

### Null Converter (`null`)

This converts only the structure of LHS.
(Generally used for debugging)

### Weighted-Abduction Converter (`weighted`)

This is based on the evaluation function of *Weighted Abduction* (Hobbs et al., 1993).

Regarding this component, the following options are available.

- `--default-cost` :: Sets the default value of observation costs. On default, `10.0` is set.
- `--default-weight` :: Sets the default value of the sum of weights of a rule. On default, `1.2` is set.
- `--legacy-loop-prevention` :: Uses the old algorithm to find loop structures in LHS.

### Cost-based Abduction Converter (`linear`)

This is based on Cost-based Abduction.
In this component, cost of each node is the sum of weights of rules used to hypothesize the node.

The same options as ones for *Weighted-Abduction Converter* are available.

### Probabilistic Cost-based Abduction (`prob-cost`)

This is based on probabilistic Cost-based Abduction.
In this component, cost of each node is the sum of negative logatithmic values of probabilities of rules used to hypothesize the node.

The same options as ones for *Weighted-Abduction Converter* are available.

### Etcetera-Abduction Converter (`etc`, `etcetera`)

This is based on the evaluation function of *Etcetera Abduction* (Gordon, 2016).

In this component, parameters on facts are interpreted as the probabilities of the facts.
Parameters on queries are ignored.

Regarding this component, the following options are available.

- `--default-weight` :: Sets the default value of probability of a fact or a rule. On default, `1.0` is set.

### CEAEA Converter (`ceaea`)

This is based on the evaluation function of *Cost-based Etcetera and Anti-Etcetera Abduction* (Yamamoto, 201x).

In this component, parameters on facts are interpreted as the probabilities of the facts,
and parameters on queries are interpreted as rewards.

Regarding this component, the following options are available.

- `--default-probability` :: Sets the default value of probability of a fact. On default, `1.0` is set.
- `--default-query-reward` :: Sets the deefault value of reward of a query. On default, `1.0` is set.
- `--default-reward` :: Sets the value of reward to be given when a hypothesis can explain no query. The default value is `1.0`.
- `--default-backward-weight` :: Sets the default value of backward probability of a rule. On default, `0.8` is set.
- `--default-forward-weight` :: Sets the default value of forward probability of a rule. On default, `1.0` is set.
- `--default-weight` :: Sets the default value of backward / forward probability.

Here `--default-backward-weight` and `--default-forward-weight` have higher priority than `--default-weight`.

## ILP Solver

Components in this category solve ILP-problems with using an external ILP solver.

### Null Solver (`null`)

This does nothing.

This may be used when there is no need to find the best solution.

### Lp-Solve (`lpsolve`)

This employs [lp_solve](http://lpsolve.sourceforge.net/5.5/) to find the solution.
If lp_solve is not available, this will throw an exception.

Regarding this component, the following options are available.

- `--print-lpsolve-log` :: Enable lp_solve to print logs.

### Gurobi (`gurobi`)

This employs [Gurobi optimizer](http://www.gurobi.com) to find the solution.
If Gurobi optimizer is not available, this will throw an exception.

Using `-P` option, you can set the number of parallel threads used in the computation of Gurobi optimizer.

Regarding this component, the following options are available.

- `--print-gurobi-log` :: Enable Gurobi optimizer to print logs.

### Gurobi with Cutting Plane Inference (`gurobi-cpi`)

This performs Cutting Plane Inference (Inoue et al., 2013) with using Gurobi optimizer.
If Gurobi optimizer is not available, this will throw an exception.

In many case, this component gets more efficiency than one without Cutting Plane Inference.

The same options as ones for No-CPI version (`gurobi`) are avaiable.
This component uses the value of `-P` option as the number of parallel threads for Gurobi optimizer.

### K-best Solver with Gurobi (`gurobi-kbest`)

This finds k-best solutions with using Gurobi optimizer.
If Gurobi optimizer is not available, this will throw an exception.

This searches the solutions so that there are certain amounts of structural difference among the solutions.
More specifically, supposing a solution pair and a margin of positive value, the difference of nodes in the solutions must be more than the margin.

The same options as ones for the one-best version (`gurobi`) are avaiable.
Additionally, the following component-specific options are available.

- `--max-solution-num=INT` :: Sets the maximum number of solutions. The default value is `5`.
- `--max-eval-delta=FLOAT` :: Sets the maximum difference from the evaluation value of the best solution. The default value is `5.0`.
- `--eval-margin=INT` :: Sets the margin. The default value is `3`.

### K-best Solver with Gurobi and CPI (`gurobi-kbest-cpi`)

This finds k-best solutions with using Cutting Plane Inference and Gurobi optimizer.

The same options as ones for the component `gurobi-cpi` and the component `gurobi-kbest` are available.

### SCIP (`scip`)

This employs [SCIP](http://scip.zib.de/) to find the solution.
If SCIP is not available, this will throw an exception.

Regarding this component, the following options are available.

- `--gap-limit` :: Sets the minimum gap in SCIP. On default, the default value of SCIP is used.

### SCIP with Cutting Plane Inference (`scip-cpi`)

This performs Cutting Plane Inference (Inoue et al., 2013) with using SCIP.
If SCIP is not available, this will throw an exception.

In many case, this component gets more efficiency than one without Cutting Plane Inference.

The same options as ones for No-CPI version (`scip`) are avaiable.

### K-best Solver with SCIP (`scip-kbest`)

This finds k-best solutions with using SCIP.
If SCIP is not available, this will throw an exception.

The basic behavior is same as *K-best Solver with Gurobi*.

The same options as ones for the one-best version (`scip`) and the options shown in *K-best Solver with Gurobi* are available.

### K-best Solver with SCIP and CPI (`scip-kbest-cpi`)

This finds k-best solutions with using Cutting Plane Inference and SCIP.

The same options as ones for the component `scip-kbest` are available.

### CBC (`cbc`)

This employs [CBC](https://projects.coin-or.org/Cbc) to find the solution.
If CBC is not available, this will throw an exception.

Regarding this component, the following options are available.

- `--gap-limit` :: Sets the minimum gap in CBC. On default, the default value of CBC is used.

### CBC with Cutting Plane Inference (`cbc-cpi`)

This performs Cutting Plane Inference (Inoue et al., 2013) with using CBC.
If CBC is not available, this will throw an exception.

In many case, this component gets more efficiency than one without Cutting Plane Inference.

The same options as ones for No-CPI version (`cbc`) are avaiable.

### K-best Solver with CBC (`cbc-kbest`)

This finds k-best solutions with using CBC.
If CBC is not available, this will throw an exception.

The basic behavior is same as *K-best Solver with Gurobi*.

The same options as ones for the one-best version (`cbc`) and the options shown in *K-best Solver with Gurobi* are available.

### K-best Solver with CBC and CPI (`cbc-kbest-cpi`)

This finds k-best solutions with using Cutting Plane Inference and CBC.

The same options as ones for the component `cbc-kbest` are available.

-----

# Other Options

This section provides explanations to other options.

### `--disable-kb-cache`

Generally, Open-David caches rules read from KB on the memory for the computational efficiency.
This option disables the cache function.

-----

# Input files

Input files of Open-David are written in the original syntax.
This syntax is partially based on C++ and Javascript.

In input files, strings after character `#` are interpreted as comments and ignored.

## Literals

In Open-David, a literal is an atomic formula with negation in first order logic.
Each literal is written in the similar format to functions in C++, such as `dog(x)` and `eat(x, y)`.

One can use two kinds of negation, namely Typical Negation and Negation as Failure.
We show some examples as follows.

- `dog(x)` :: A variable `x` is a dog.
- `!dog(x)` :: A variable `x` is not a dog.
- `not dog(x)` :: It is not proved that a variable `x` is a dog.
- `not !dog(x)` :: It is not proved that a variable `x` is not a dog.

Typical Negation is avaiable everywere,
but Negation as Failure is avaiable only in definitions of rules.

Name of each argument of a literal must be a string consisting of alphabets and underscores or a string enclosed by quotation marks.

1. If an argument is enclosed by quotation marks, it will be interpreted as a constant.
2. If an argument is not enclosed by quotation marks and starts with lower case letter, it will be interpreted as a variable.
3. The others are constants.
4. If an argument starts with an underscore, the underscore will be ignored in the above judge.

One can set an optional parameter to a literal.
For example, if one write a literal of `dog(x):foo`, a literal `dog(x)` with a parameter `foo` is defined.
To set multiple parameters, write them separated with `:`, such as `dog(x):foo:var`.

In many cases, optional parameters are used to define observation costs and weights on rules.

## Conjunctions

To write a conjunction of literals, write literals with separated with `^`, such as `dog(x) ^ cat(y)`.

As well as literals, conjunctions can have optional parameters.
For example, writing `{ dog(x) ^ cat(y) }:foo`, a conjunction which has a parameter `foo` is defined.

## Rules

At first, we show a simple example.

	rule monkey_likes_banana { { monkey(x) } => { eat(x, y) ^ banana(y) }}

To define a rule, write `rule` and the name of the rule at first.
In this example, `monkey_likes_banana` is the name of the rule.
Names of rules are generally used only in output files and change nothing about inference.

In a definition of a rule, its body and its head are separated with `=>`.
Curly braces on conjunctions can be omitted, and then the example above can be rewritten as follows.

	rule monkey_likes_banana { monkey(x) => eat(x, y) ^ banana(y) }

To define the weights of a rule, use parameters.

By putting `false` to the head of a rule, one can define an exclusive rule, such as `dog(x) ^ cat(x) => false`.
Rules of this type are not used in backward chaining but used as logical constraints.

### Negation as Failure in Rules

Negation as Failure (NAF) is avaiable in definitions of rules.

    rule { p(x) ^ not r(x) => q(x) ^ not s(x) }

On applying backward chaining of a rule, literals with NAF in a rules will be hypothesized with replacing NAF with Typical Negation.
For example, applying backward chaining of the rule above to `q(y)`, a conjunction `p(y) ^ !r(y) ^ !s(y)` is hypothesized.

> This behavior is because we assume Closed World Assumption for logical formulas of participants of the reasoning.

If a rule has a head consisting of only NAF literals,
its head is essentially empty and then the rule will not be used in backward chaining.

### Equality in Rules

A rule can include equality literals.

    rule { p(x) ^ (x = A) => q(y) ^ (x != y) ^ not (y = B) }

The behavior of an equality literal in a rule dependes on the existence of negation and its position.

- If an equality literal is put in the head, it will be interpreted as the precondition of the rule.
- If an equality literal is put in the body, it will be hypothesized as the result of backward chaining.
- If an equality literal in the body is negated with NAF, the NAF is replaced with Typical Negation.
- If an equality literal in the head is not observed, it will be hypothesized as the result of backward chaining. 

For example, applying backward chaining of the rule above to `q(s)`,
the precondition of the rule will be `not (s = B)` and
a conjunction `p(_u) ^ (_u = A) ^ (s != _u)` will be hypothesized.
Here `(s != _u)` is in the head of the rule but is not observable because the argument `_u` is unbound.
Therefore it is not the precondition but the hypothesized literal.

As well as Negation as Failure, if the head of a rule consists of only equality literals,
it will not be used in backward chaining.

### Rules as Constraints

Open-David considers each rule as logical constraint in inference.

For example, let consider the following rules.

    rule r1 { p(x) ^ q(x) => false }
	rule r2 { s(x) => t(x) }

In the abductive inference with these rule, the rule `r1` prohibits hypotheses that satisfy `∃x. p(x) ^ q(x)`,
and the rule `r2` prohibits hypotheses that satisfy `∃x. s(x) ^ !t(x)`.

Next, let consider rules including Negation as Failure.

    rule r3 { p(x) ^ not s(x) => q(x) ^ not t(x) }

This rule may impose the following constraints in inference for any variable `x`.

    p(x) ^ not s(x) ^ q(x) => false
    p(x) ^ not s(x) ^ t(x) => false

Here, it shoud be noted that `not !p` is equal to `p`.

## Observations

At first, we show a simple example of observation.

	problem test {
	    observe { go(x,Store) ^ have(x,Gun) }
	    require { robbing(x) }
	}

To define observation, write `problem` and the name of the observation.
In the above example, `test` is the name of the observation.

One can define items of the following 4 types in `problem` statement.

- `observe` :: Defines observable literals.
- `fact` :: Defines literals of facts.
- `query` :: Defines literals of queries.
- `require` :: Defines literals of requirements.

Observable literals are internally divided into facts and queries,
because the some of abductive models (such as Etcetera Abduction) deal with facts and queries differently.
If one defines observable literals with `observe` statement,
the equality literals of them are interpreted as facts and the others are interpreted as queries.

`require` statement is used to define formulas which one want to check the satisfiability of.
If one define the `require` item, Open-David will check whether the solution hypothesis satisfies it and write the result to the output.

Using `--pseudo-positive` option or `--pseudo-negative` option,
one can get a solution which satisfies / does not satisfy the required formulas.

In each problem, query literals are necessary.
Therefore either of `observe` and `query` must be used in one `problem` statement.

### Forall

In definitions of observation, one can use `forall` to write universally quantified literals.
We show simple example as follows.

    problem test {
		observe { man(A) ^ man(B) ^ forall !have(x,Gun) }
	}

This expresses logical formula `∃ A, B, Gun ∀ x { man(A) ^ man(B) ^ !have(x,Gun) }`.
Therefore the hypotheses that satisfy `∃x. have(x, Gun)` will be excluded from the candidates.

`forall` statement is not available together with `not`.

### Any

In `require` statement, one can use `any` to express that any term is OK.

For example, if one sets `p(x, any)` to the requirement,
this literal can match all literals which has the binary predicate `p` and the variable `x` as the first argument.

## Numerical Arguments

Open-David can deal with numerical expressions partially.

One can use an integer (ex. `15`) as an argument of a formula,
and can perform numerical calculation by define a rule like `p(x+5) => q(x)`.
Applying backward chaining of this rule to `q(20)`, `p(25)` will be hypothesized.

In current version, Open-David can deal with only additions and subtractions.
For example, `p(x+5) => q(x)` and `p(x) => q(x-5)` are equal to each other.
It is not avaialble to operate a pair of variables, such as `p(x+y)`.

One cannot use arguments of numerical operation such as `x+5` in definitions of observations.
In that case, an exception will be thrown.

## Predicate Properties

Open-David can define properties of predicates and can reflect them to inference.
At first, we show a simple example.

    property pred/3 { transitive:2, symmetric:1:3 }

This example means that ternary predicate `pred` has the following properties:

- The second argument and the third argument are transitive. (i.e. `pred(s,x,y) ^ pred(t,y,z) => pred(u,x,z)`)
- The first argument and the third argument are symmetric. (i.e. `pred(x,y,z) => pred(z,y,x)`)

To define predicate properties, write `property` and the target predicate with arity.
For example, to define predicate properties of the unary predicate `dog`, write `dog/1`.

Next, write the properties of the target predicate separated with commas.
Each property consisits of the type of property and the index of the target argument, separated with `:`.

In a definition of a binary relation, if one omit the second index, Open-David uses the next of the first index.
For example, `symmetric:1` is equal to `symmetric:1:2`.

The following types of properties are currently avaialble.

- `transitive` :: Defines transitivity between the specified argument pair.
- `symmetric` :: Defines symmetricity between the specified argument pair.
- `asymmetric` :: Defines asymmetricity between the specified argument pair.
- `irreflexive` :: Defines irreflexivity between the specified argument pair.
- `right-unique` :: Defines right-uniqueness between the specified argument pair.
- `left-uniqe` :: Defines left-uniqueness between the specified argument pair.
- `closed` :: Defines a constraint that the specified argument must be equal to any constant.
- `abstract` :: Defines a constraint that one cannot hypothesize equalities between variables at the specified position only from unifications between literals of this predicate.

If one defines invalid property (such as `symmetric:1` and `asymmetric:1`), Open-David will throw an error.

## Mutual Exclusions

Using `mutual-exclusion` statement, one can defines rules of mutual exclusions at once.
We show a simple example as follows.

    mutual-exclusion animal { dog(x) v cat(x) v mouse(x) }

This is equal to the following rules:

    rule animal { dog(x) ^ cat(x) => false }
	rule animal { cat(x) ^ mouse(x) => false }
	rule animal { mouse(x) ^ dog(x) => false }

Note the following:

- One can omit the name of rule (i.e. `animal` in the above example).
- One cannot add conjunctions in a `mutual-exclusion` statement.
- One cannot add equality literals in a `mutual-exclusion` statement.
- At least one argument must be shared with all literals in a `mutual-exclusion` statement.

## Emacs Mode

We provides Emacs mode for input files of Open-David.

Adding `tools/david-mode.el` to your Emacs configuration, you can use that mode.
Generally, add the following operation to `~/.emacs` or `~/.emacs.d/init.el`.
(Rewrite the argument to an appropriate value as the relative path from the Emacs configuration file to `david-mode.el`.)

	(load "david-mode.el")

On default, this mode is activated in editting files with the `.dav` extension.

-----

# Output files

Open-David write the result of abductive reasoning in JSON format.

One can select the output format with `-o` option.
The following three formats are currently available.

- `mini` :: Outputs only the elements included in the solution hypothesis.
- `ilp` :: Outputs the ILP problem and its solution.
- `full` :: Outputs all elements included in the latent hypotheses set.

JSON objects output by Open-David have the following items.

- `"output-type"` :: The output format specified by `-o` option.
- `"kernel"` :: The meta information about the inference engine.
	- `"version"` :: The version of Open-David.
	- `"executed"` :: The time when Open-David was executed.
	- `"lhs-generator"` :: The information about the component used to generate LHS.
	- `"ilp-converter"` :: The information about the component used to convert LHS into ILP problems.
	- `"ilp-solver"` :: The information about the component used to solve ILP problems.
- `"knowledge-base"` :: The information about the knowledge base used.
	- `"path"` :: The path of the knowledge base.
	- `"version"` :: The version of the knowledge base.
	- `"rules-num"` :: The number of rules in the knowledge base.
	- `"predicates-num"` :: The number of predicates in the knowledge base.
	- `"compiled"` :: The time when the knowledge base was compiled.
- `"results"` :: Array of inference results.
	- `"index"` :: The index number of the problem.
	- `"name"` :: The name of the problem.
	- `"elapsed-time"` :: The information about the time taken for the inference.
		- `"lhs"` :: The time taken for LHS generation in seconds.
		- `"cnv"` :: The time taken for ILP conversion in seconds.
		- `"sol"` :: The time taken for ILP solving in seconds.
		- `"all"` :: The time taken for the whole of the inference in seconds.
	- `"solution"` :: An inference result.
	- `"solutions"` :: Array of inference results.

The contents of `solution` and elements in `solutions` will be different, depending on the output-type.

## Minimal Proof Graphs

If `-o mini` option is used, JSON objects of inference results have the following items.

- `"state"` :: The optimality of the result.
    - `"optimal"` :: Means that Open-David could find the best solution from the candidates.
	- `"sub-optimal"` :: Means that the result is not the best solution but is valid as a solution.
	- `"not-available"` :: Means that the result is invalid as a solution.
- `"objective"` :: The value of evaluation function to the resulting hypothesis.
- `"size"` :: The information about the size of the latent hypotheses set.
	- `"node"` :: The number of nodes in the LHS.
	- `"hypernode"` :: The number of hypernodes in the LHS.
	- `"rule"` :: The number of rules used in the LHS.
	- `"edge"` :: The number of edges used in the LHS.
	- `"exclusion"` :: The number of exclusive constraints defined in the LHS.
- `"requirement"` :: The information about the satisfiability of `require` statement in the observation.
	- `"satisfied"` :: Shows whether all of the items in `require` statement are satisfied.
	- `"detail"` :: Shows information about the satisfiability of each item in `require` statement.
- `"nodes"` :: Array of the nodes in the LHS.
- `"hypernodes"` :: Array of the hypernodes in the LHS.
- `"edges"` :: Array of the edges in the LHS.
- `"rules"` :: Array of the rules used in the LHS.
- `"violated"` :: Array of the exclusive constraints which the resulting hypothesis violates.
- `"cost-payment"` :: If a cost-based evaluation function is used, this item will be added. Shows the information about the payment of costs.

## Full Proof Graphs

If `-o full` option is used, JSON objects of inference results have the following items.

- `"state"` :: The optimality of the result.
    - `"optimal"` :: Means that Open-David could find the best solution from the candidates.
	- `"sub-optimal"` :: Means that the result is not the best solution but is valid as a solution.
	- `"not-available"` :: Means that the result is invalid as a solution.
- `"objective"` :: The value of evaluation function to the resulting hypothesis.
- `"size"` :: The information about the size of the latent hypotheses set.
	- `"node"` :: The number of nodes in the LHS.
	- `"hypernode"` :: The number of hypernodes in the LHS.
	- `"rule"` :: The number of rules used in the LHS.
	- `"edge"` :: The number of edges used in the LHS.
	- `"exclusion"` :: The number of exclusive constraints defined in the LHS.
- `"requirement"` :: The information about the satisfiability of `require` statement in the observation.
	- `"satisfied"` :: Shows whether all of the items in `require` statement are satisfied.
	- `"detail"` :: Shows information about the satisfiability of each item in `require` statement.
- `"active"` :: Shows elements in the solution hypothesis.
	- `"nodes"` :: Array of the nodes in the solution hypothesis.
	- `"hypernodes"` :: Array of the hypernodes in the solution hypothesis.
	- `"edges"` :: Array of the edges in the solution hypothesis.
- `"not-active"` :: Shows elements in the LHS but not in the solution hypothesis.
	- `"nodes"` :: Array of the nodes not in the solution hypothesis.
	- `"hypernodes"` :: Array of the hypernodes not in the solution hypothesis.
	- `"edges"` :: Array of the edges not in the solution hypothesis.
- `"rules"` :: Array of the rules used in the LHS.
- `"exclusions"` :: Shows the information about the exclusive constraints in the LHS.
	- `"satisfied"` :: Array of the exclusive constraints which the resulting hypothesis satisfies.
	- `"violated"` :: Array of the exclusive constraints which the resulting hypothesis violates.
- `"cost-payment"` :: If a cost-based evaluation function is used, this item will be added. Shows the information about the payment of costs.

## ILP Solutions

If `-o ilp` option is used, JSON objects of inference results have the following items.

- `"maximize"` :: If `true`, this is a maximization problem, otherwise a minimization problem.
- `"economize"` :: Shows whether the function to reduce the number of ILP-variables is activated.
- `"objective"` :: The value of objective function for the solution hypothesis.
- `"state"` :: The optimality of the result.
    - `"optimal"` :: Means that Open-David could find the best solution from the candidates.
	- `"sub-optimal"` :: Means that the result is not the best solution but is valid as a solution.
	- `"not-available"` :: Means that the result is invalid as a solution.
- `"size"` :: Shows the information about the size of the ILP problem.
	- `"variable"` :: The number of variables in the ILP problem.
	- `"constraint"` :: The number of constraints in the ILP problem.
- `"variables"` :: Shows the information about the variables in the ILP problem.
	- `"positive"` :: Array of the variables which values are not `0.0`.
	- `"negative"` :: Array of the variables which values are `0.0`.
- `"constraints"` :: Shows the information about the constraints in the ILP problem.
	- `"satisfied"` :: Array of the constraints which are satisfied.
	- `"violated"` :: Array of the constraints which are not satisfied.

## Atoms

In JSON objects, each literal is expressed as a string, such as `"not dog(x)"`.
This is for the efficiency of the file size.

## Conjunctions

In JSON objects, each conjunction is expressed as an array of strings.

For example, a conjunction `dog(x) ^ cat(y)` will be expressed as `[ "dog(x)", "cat(y)" ]` in JSON objects.

## Rules

JSON objects of rules have the following items.

- `"rid"` :: The ID number of the rule.
- `"name"` :: The name of the rule.
- `"class"` :: The class of the rule. If the rule does not belong to any class, this is omitted.
- `"left"` :: The conjunction on the left hand side of the rule.
- `"right"` :: The conjunction on the right hand side of the rule.
- `"cond"` :: The precondition of the rule.

## Nodes

JSON objects of nodes in an LHS have the following items.

- `"index"` :: The index number of the node in the LHS.
- `"type"` :: The type of the node.
	- `"observable"` :: The node is contained in observation.
	- `"hypothesized"` :: The node was hypothesized by abductive reasoning.
- `"atom"` :: The literal corresponding to the node.
- `"depth"` :: The depth of the node, namely, the minimum number of edges to connect between the node and any observation.
- `"master"` :: The index of the master-hypernode of the node.

The *master-hypernode* is the oldest hypernode in ones which contains the node.
The existence of the node always corresponds to the existence of its master-hypernode.

## Hypernodes

JSON objects of hypernodes in an LHS have the following items.

- `"index"` :: The index number of the hypernode in the LHS.
- `"nodes"` :: Array of indices of the nodes which are the member of the hypernode.

## Edges

JSON objects of edges in an LHS have the following items.

- `"index"` :: The index number of the edge in the LHS.
- `"type"` :: The type of the edge.
	- `"hypothesize"` :: The edge is backward chaining.
	- `"implicate"` :: The edge is forward chaining.
	- `"unify"` :: The edge is unification.
- `"rule"` :: The ID number of the rule used in the edge.
- `"tail"` :: The index number of the hypernode of the edge's tail.
- `"head"` :: The index number of the hypernode of the edge's head.

The edge's tail corresponds to the input of the operation of the edge,
and the edge's head corresponds to the outptu of the operation of the edge.

For instance, supposing an edge of backward chaining,
its tail corresponds to the right-hand-side of the rule and its head corresponds to the left-hand-side of the rule.
Supposing an edge of unification, its tail corresponds to the unified nodes and its head corresponds to the equality literals which are hypothesized by the unification.

If the output is empty, the value of `"head"` will be `-1`.

## Exclusions

Exclusive constraints are the conjunctions which a hypothesis must not satisfy.
These constraints are defined from predicate properties and rules.

JSON objects of exclusive constraints have the following items.

- `"index"` :: The index number of the exclusive constraints in the LHS.
- `"type"` :: The origin of the exclusive constraints.
	- `"counterpart"` :: Exclusive constraints made from literals with/without negation (e.g. `p(x) ^ !p(x)`).
	- `"transitive"` :: Exclusive constraints made from transitive predicates.
	- `"asymmetric"` :: Exclusive constraints made from asymmetric predicates.
	- `"irreflexive"` :: Exclusive constraints made from irreflexive predicates.
	- `"right-unique"` :: Exclusive constraints made from right-unique predicates.
	- `"left-unique"` :: Exclusive constraints made from left-unique predicates.
	- `"rule"` :: Exclusive constraints made from inference rules.
	- `"rule-class"` :: Exclusive constraint made from rule-classes.
	- `"unknown"` :: Exclusive constraint made by an unknown reason.
- `"atoms"` :: The conjunction which the exclusive constraint consists of.
- `"rid"` :: The ID number of the rule which makes the exclusive constraint.

## Cost Payment

JSON objects of the information about cost payments have the following items.

- `"node"` :: The index of the node being imposed the cost.
- `"cost"` :: The amount of the cost.
- `"paid"` :: Shows whether the cost was paid.

## ILP Variables

JSON objects of ILP variables have the following items.

- `"index"` :: The index of the variable in the ILP problem.
- `"name"` :: The name of the variable.
- `"coefficient"` :: The coefficient on the variable in the objective function of the ILP problem.
- `"fixed"` :: If the value of the variable is fixed, shows the fixed value.

## ILP Constraints

JSON objects of ILP constraints have the following items.

- `"index"` :: The index of the constraint in the ILP problem.
- `"name"` :: The name of the constraint.
- `"terms"` :: The formula of the constraint.
- `"range"` :: The range which the formula must satisfy.
    - `"= X"` :: the value of the formula must be equal to `X`.
	- `"<= X"` :: the value of the formula must be equal to `X` or less than `X`.
	- `">= X"` :: the value of the formula must be equal to `X` or more than `X`.
	- `"X ~ Y"` :: the value of the formula must be in the range from `X` to `X`.

Each string in the array of `"terms"` means the index number of ILP-variable and its coefficient.
For example, if the array of `"terms"` is `["1.0*[3]", "-1.0*[4]"]`, the value of the formula is the sum of the following value.

- The multiplication of the value of the ILP variable which index is `3` by the coefficient `1.0`
- The multiplication of the value of the ILP variable which index is `4` by the coefficient `-1.0`

-----

# Products of KB Compilation

Compiling the knowledge base, some files are generated at the path specified by `-k` option.
Those files contains the following information.

- `*.base.dat.cdb` :: The database of rules in KB.
- `*.base.idx.cdb` :: The map from a rule-ID to the pointer to the corresponding rule in the database.
- `*.cls.cdb` :: The map from a rule-class to the corresponding set of rules.
- `*.lhs.cdb` :: The map from a predicate to the set of rules which the body contains the predicate.
- `*.rhs.cdb` :: The map from a predicate to the set of rules which the head contains the predicate.
- `*.ft1.cdb` :: The map from a predicate to the set of conjunctions which contain the predicate.
- `*.ft1.cdb` :: The map from a conjunction to the set of rules which contain the conjunction.
- `*.predicate` :: The list of predicates in KB.
- `*.heuristic` :: The heuristics for the LHS generation.
- `*.spec.txt` :: The text file which shows the outline of the KB.
    - `kb-version` :: The version of the KB.
	- `time-stamp` :: The time when the KB was compiled.
	- `num-rules` :: The number of rules in the KB.
	- `num-predicates` :: The number of predicates in the KB.
	- `heuristic` :: The type of heuristics used.

These files are necessary for inference.
If you removed any of them, you need to compile the KB again.

-----

# Advanced Functions

This section provides the usages of advanced functions.

## Rule Class

If the name of a rule contains `:`, the string before `:` will be interpreted as the class of the rule.
For example, a rule which has the name of `animal:dog` belongs to the `animal` class.

Rules in the same class cannot be applied to the same formulas together.
Using this function, one can defines mutual exclusiveness among siblings of a super-concept (like the following example).

	rule animal:dog { dog(x) => animal(x) }
	rule animal:cat { cat(x) => animal(x) }
	rule animal:rat { rat(x) => animal(x) }

Although the similar function in Phillip defines structural constraints for a proof-graph,
on the other hand, this function defines logical constraints.

Each rule can belong to only one class.
For example, supposing a rule having the name of `aaa:bbb:ccc`, the rule belongs to only `aaa` class.

-----

# DavDeb (David Debugger)

We provide a script for debugging inference results, `davdeb.py`.
This section provides the usage of the script.

## Input

In order to use `davdeb.py`, the following things are needed.

- The compiled KB
- An observation which includes `require` statement

`davdeb.py` performs abductive reasoning with using OpenDavid, but it does not perform KB compilation.
So it is necessary to compile your KB in advance.

`davdeb.py` will target only the first problem in an input file,
ever if multiple problems are contained in the input.

## Usage

Execute `davdeb.py` with the same option as ones on calling OpenDavid.

For example, supposing the following first command being the one of usual inference,
the command to run `davdeb.py` will be the following second command.

~~~~~
$ bin/david infer -k compiled -c naive,weighted,gurobi --max-depth=5 input.dav
$ tools/davdeb.py -k compiled -c naive,weighted,gurobi --max-depth=5 input.dav
~~~~~

## Output

Running `davdeb.py`, it checks whether the given observation and the KB can satisfy the required formulas.

We shows the list of the output of `davdeb.py`.

- `error1`

	Some error was thrown in a normal inference.
	Refer the error message printed.

- `error2`

	Some error was thrown in a inference to obtain the ideal hypothesis with `--pseudo-positive` option.
	Refer the error message printed.

- `error3`

	Some error was thrown in a inference to obtain the ideal hypothesis with `--set-const-true` option.
	Refer the error message printed.

- `ok`

	Open-David found the ideal hypothesis as the best solution.

- `tie_score`

	There are multiple best solutions, including the ideal hypothesis.
	It depends on the internal state of the ILP solver whether the ideal hypothesis is output as the best solution.

- `not_best`

	The ideal hypothesis is contained in the search space, but it is not the best solution.

- `impossible`

	The ideal hypothesis is not contained in the search space.

-----

# Libraries

Open-David is including the following libraries.

## cdb++

- Author :: Naoaki Okazaki
- License :: [modified BSD](https://opensource.org/licenses/bsd-license.php)
- URL :: http://www.chokkan.org/software/cdbpp/
