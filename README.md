# IMPERIVM

A compiler for a subset of the C language written in C for educational purposes.

It includes a recursive descent parser on the frontend, a flexible three-address IR, and a translator into AMD64 assembly with register allocation in the backend.

## Installation
Clone the repo, run `meson setup` to create a build directory, and run `meson compile` from that directory.
Meson and Ninja must be installed on the system.

Alternatively, one could compile the project "by hand," with a command like 

`gcc main.c frontend/*.c IR/*.c [...] -Iinclude -o imc`, but using the given build system configuration is preferable.

GCC is assumed to be present on the system, and it's called to assemble and link the assembly code emitted by the compiler. This is not necessary if the option `--asm-only` is given.

It's only possible to compile it on GNU/Linux, as `imc` uses GNU's `getopt_long` for parsing command-line arguments.

## Usage
Typical usage would be `./imc [source file] -o [executable]`. The compiler offers a comprehensive help option (`--help`) which informs the user about all the available command-line options.

The compiler has static linking capabilities with `--static`, owing to GNU Binutils' `ld`.

Several debug/educational options are provided, such as `--verbose-asm`, `--print-blocks`, and `--ir`.

## Frontend
The lexer, being the first part of the project made, was done somewhat shoddily, but it works as expected for any sane input.

The parser, on the other hand, is practically bullet-proof, as it was remade after the compiler was finished. It does recursive descent with Pratt-like expression parsing, and packs the input from the lexer into an AST.

The vector code found in the frontend directory (`frontend/vector.c`, with its header in `include/vector.h`), which uses type erasure, was originally used for the parser, but later I switched to a typed vector for all parts of the compiler codebase that require it. The typed vector is done through template-like macro hacks that generate code for every type that is needed. The code can be found in `include/templates/vector.h`.

## Middle-end
The IR is based on Chapter 6 of what is colloquially known as the Dragon Book, save for the `PARAM` IR instruction that is done differently. Dragon Book's `PARAM` has assumptions about the architecture that would make it more cumbersome to write a backend for architectures that have unusual argument passing, thus my IR stores function/procedure call arguments in the call instruction itself. GCC's GIMPLE also took issue with `PARAM`.

Platform-independent optimizations are scant, but a minimal framework for them does exist. Two optimizations are performed: short-circuiting of logical `AND` and `OR` (which is demanded by the C standard), and removal of redundant IR assignments. Short-circuiting is done during AST lowering, and redundant assignment removal is a separate pass after the whole IR has been formed. It works as follows: for instance, the AST expression

`a = b + c * d`

would be lowered into the IR as

`t1 = c * d`

`t2 = b + t1`

`a = t2`

The final assignment is elided, and instead it turns into

`t1 = c * d`

`a = b + t1`

The corresponding code can be found in `IR/IR_optimize.c`.

## Backend
The main optimization done in the backend is register allocation. It would have been much simpler to emit constant load-store instructions for every operation, but the compiler does register coloring on each basic block in the IR and keeps track internally of which variable is in which register at any given moment, and whether the variable's value in memory is consistent with its register.

During register coloring, if it is not possible to assign a register to every variable in the basic block, the compiler prunes the interference graph by removing variables one by one based on LFU, and repeats the process until the graph can be colored with the number of available general-purpose registers (16 on AMD64), save for `RSP`, `RBP`, and `R15`. The code for this is in `backend/amd64/amd64.c`.

`RSP` and `RBP` are conserved because of stack frame management, and `R15` is reserved for operations on all the variables which didn't have a register assigned to them. `R15` can be assumed throughout the whole backend that it is free and can be used for any operation which benefits from an additional register, owing to the fact that every variable which goes into it is spilled back into memory immediately after the operation has been performed.
