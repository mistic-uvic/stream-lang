## Project Description

This projects implements a compiler for a new language for stream processing
in continuation referred to as *The Language*.

The Language is characterized by the following features:
- functional
- typing: strong, statical, implicit
- handy expression structures:
  * iteration (application of expression on consecutive ranges of a sequence of values)
  * reduction (reduction of a sequence of values to a single value by application of expression
    on original values and results of previous applications)

The Language has the following types:
- integer number
- real number
- range - sequence of integer numbers defined by a pair of start and end numbers
- stream - sequence of real numbers in multiple dimensions

## Prerequisits

This project requires flexc++ and bisonc++ lexer and parser generators.
Information about these two programs is available online:
- http://flexcpp.sourceforge.net/
- http://bisoncpp.sourceforge.net/

## Building

This project provides a CMake build system.

The standard building procedure is:
```
mkdir build
cd build
cmake ..
make
```

This will produce the following executables:
- `build/frontend/test` - Test the frontend (lexer + parser) on an input file.

CMake options:
- `FRONTEND_PRINT_TOKENS` - Compile frontend so that it prints all tokens on command-line.

## Usage

Currently, the only executable is the compiler frontend tester, built by
the above procedure at `build/frontend/test`. Invoking it with the `--help`
or `-h` option will print information about its usage and all parameters.
The program performs syntactical and semantical analysis of code and may
print the following information on standard output:

- Each token produced by the lexical scanner (if the `FRONTEND_PRINT_TOKENS`
  CMake option was enabled)

- The abstract syntax tree (AST).

- The top-level symbol declarations (functions and constant expressions).

- The type of result of evaluation of an expression or function with arguments.

- Any syntactical or semantical errors in input code.

## Examples:

Example code files are provided in the `examples` folder. For example,
the following command will use the frotend tester to perform syntactical
processing of the file `matrix-multiply.in` and then semantically analyse
evaluation of the function `matrix_multiply` defined in that file using two
streams as arguments:
```
build/frontend/test examples/matrix-mult.in -e matrix_multiply "[10,3,5]" "[10,5,8]"
```
The function `matrix_multiply` multiplies two sequences of matrices, producing
a new sequence of matrix products. Hence, the above command will finally print
`Result type = [10,3,8]`, meaning that evaluation results in a stream of 10
matrices, each with as many rows as the first matrix and as many columns as the
second matrix given as argument.

**NOTE:** At present, semantic analysis does not support non-constant
expressions in stream slicing. For this reason evaluating the `autocorrelation`
function from `autocorrelation.in` will produce a semantical error.

## Filesystem:

- `frontend` - Contains code for the frontend (lexer + parser).
  - `scanner.l` - Input file for lexer generator flexc++.
  - `parser.y` - Input file for parser generator bisonc++.
  - `ast.hpp` - Abstract Syntax Tree (AST) representation.
  - `ast_printer.hpp` - AST printing.
  - `types.hpp` - Type representation.
  - `environment.*` - Symbolic environment.
  - `semantic.*` - Semantic analysis (type-checking, etc.).
  - `test.cpp` - An executable parser which depends on output of flexc++ and bisonc++.

- `examples` - Contains example code in The Language.
  - `matrix-mult.in` - Multiplication of two sequences of matrices.
  - `autocorrelation.in` - Autocorrelation of a 1D sequence.
  - `spectral-flux.in` - Computes "spectral flux" of a sequence of spectrums (results of DFT).

## Notes

This project includes output files of the lexer and parser generators.
The reason is that the functionality of the flexc++ and bisonc++ input files
is smaller than that of the mainstream flex and bison.
In contrast, part of that functionality may be achieved by modification of
*some* files generated by flexc++ and bisonc++ which are only generated the
first time (if they don't exist) and otherwise not overridden.
This is the approach intended and suggested by the authors of flexc++ and
bisonc++.

## Online repository:

The entire project code is available online at:
https://github.com/jleben/stream-lang

## Author:

Jakob Leben
