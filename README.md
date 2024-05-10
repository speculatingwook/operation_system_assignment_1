# Operating Systems Assignment #1: KUMoo~~~

## Instructions

- Read the provided files carefully to understand the assignment requirements.
- Implement the following functions in `kumoo.h`:
- `ku_pgfault_handler`: Page fault handler
- `ku_scheduler`: Round-robin scheduler
- `ku_proc_exit`: Process exit function
- `ku_proc_init`: Initialize execution processes

## Overview

This assignment aims to simulate a simple operating system with memory management, process management, and I/O handling.

### Memory Management

- Manage physical memory (`pmem`) and swap space (`swaps`)
- Use page tables and page directories to translate virtual addresses (VA) to physical addresses (PA)
- Implement `ku_traverse` function for address translation

### Process Management

- Handle process creation, termination, and scheduling
- Register handlers for scheduling, page faults, and process exits using `ku_reg_handler`
- Execute processes with `ku_run_procs`

### I/O Processing

- Implement `op_read` to read values from virtual memory addresses
- Implement `op_write` to write values to virtual memory addresses
- Handle page faults by calling the appropriate handler

### Debugging

- `ku_dump_pmem` to dump the contents of physical memory
- `ku_dump_swap` to dump the contents of swap space


## Code skeleton

### `kumoo.h`

- Defines `struct pcb` (Process Control Block)
- Prototypes for functions to be implemented

### `kumoo.c`

- Provides the `main` function
- Calls `ku_os_init`, `ku_proc_init`, and `ku_run_procs`

## System Initialization (`ku_os_init`)

- Initializes physical memory and swap space
- Allocates memory for `pmem` (physical memory) and `swaps` (swap space)
- Initializes free lists for physical memory and swap space frames
- Registers page fault handler (`ku_pgfault_handler`) using `ku_reg_handler`
- Registers scheduler (`ku_scheduler`) using `ku_reg_handler`
- Registers exit function (`ku_proc_exit`) using `ku_reg_handler`

## Per-Process Initialization (`ku_proc_init`)

- Initializes PCBs (Process Control Blocks) and page directories for processes
- Allocates and zero-fills page directories
- Page directories are not swapped out

## Process Execution (`ku_run_procs`)

- Executes instructions from the executable file of the current process
- Translates virtual addresses to physical addresses using `ku_traverse`
- Raises a fault (invokes page fault handler) if address translation fails
- Generates timeouts (invokes scheduler)
- Prints results:
- `PID: VA -> PA (S/F/E)`
  - S: Success
  - F: Page fault
  - E: Error
- Examples:
- `1: 32 -> 0 (F)`
- `1: 34 -> 2 (S)`
- `1: 32 -> 0 (S)`
- `2: 10 -> 26 (F)`
- `2: 1000 -> (E)`
- Calls the exit function (`ku_proc_exit`) if the process terminates (`e` instruction) or a segmentation fault occurs

## Miscellaneous Functions

- `ku_dump_pmem`: Prints the contents of physical memory in hexadecimal
- `ku_dump_swap`: Prints the contents of swap space in hexadecimal

## Functions to Implement

| Name | Description | Implementation |
|------|--------------|-----------------|
| `ku_os_init` | Registers handlers, initializes memory | Provided (`kumoo.c`) |
| `ku_reg_handler` | Registers handlers | Provided (`kumoo.c`) |
| `ku_pgfault_handler` | Page fault handler | You need to implement (`kumoo.h`) |
| `ku_scheduler` | Round-robin scheduler | You need to implement (`kumoo.h`) |
| `ku_proc_exit` | Process exit function | You need to implement (`kumoo.h`) |
| `ku_proc_init` | Initialize execution processes | You need to implement (`kumoo.h`) |
| `ku_run_procs` | Simulation execution | Provided (`kumoo.c`) |

## Provided Files

- `kumoo.c`: Main function
- `kumoo.h`: Prototypes of PCB and functions to be implemented
- Input files: Sample input file and executable files (will be uploaded by 4/29)

## Test Levels

1. Single process cases (without swapping)
2. Multi-process cases (without swapping)
3. Single process cases (with swapping)
4. Multi-process cases (with swapping)

## Submission

- Submit your source codes (`kumoo.h`) and a document (3-5 pages in PDF, HWP, or DOC format) containing:
- Basic design
- Description of important functions (name, functionality, parameters, return value)
- Deadline: May 12, 2024 (Sunday), 11:59 PM
- Cheating, plagiarism, and other anti-intellectual behavior will be dealt with severely

Note: This document is a condensed version of the assignment slides provided by the instructor. Please refer to the original slides for more detailed information and examples.


This assignment is from [konkuk university System Software Laboratory](https://sslab.konkuk.ac.kr/)