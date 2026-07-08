# Custom Memory Allocator (C, Linux)

A custom user-space memory allocator implemented in C for Linux using mmap(). The project recreates the core ideas behind malloc() and free() to understand how dynamic memory management works internally.


# Features

* OS-backed heap using mmap()
* Explicit free list
* First-fit allocation strategy
* Block splitting
* Boundary tags (header + footer)
* Forward and backward coalescing
* 16-byte aligned allocations
* Runtime profiling counters
* Demonstration test cases


# Architecture

                 User Program
                       │
              my_malloc() / my_free()
                       │
        ┌──────────────┴──────────────┐
        │     Custom Memory Allocator │
        └──────────────┬──────────────┘
                       │
                  mmap() / munmap()
                       │
                 Linux Kernel
                 

# Motivation

Dynamic memory allocation is one of the most fundamental services provided by an operating system and runtime library. While applications typically rely on `malloc()` and `free()`, understanding how these functions work internally requires implementing an allocator from scratch.

This project was built to explore:

* Heap management
* Memory fragmentation
* Pointer arithmetic
* Metadata design
* Virtual memory
* Low-level systems programming


The allocator requests a contiguous memory region from the operating system using `mmap()` and manages allocations entirely within that region.


# Heap Layout

Each block in the heap has the following layout:

+----------------+----------------------+----------------+
| Header         | User Payload         | Footer         |
+----------------+----------------------+----------------+

# Header

Each block begins with metadata:

struct block {
    size_t size;
    bool free;
    struct block* next;
};

The header stores:

* Payload size
* Allocation status
* Pointer used in the explicit free list


# Footer (Boundary Tag)

Each block ends with a footer containing:

size_t size

The footer duplicates the payload size stored in the header.

Boundary tags allow the allocator to locate the previous physical block in **O(1)** time, enabling efficient backward coalescing.


# Allocation Algorithm

The allocator uses an explicit free list containing only free blocks.

# Allocation Steps

1. Align requested size to 16 bytes.
2. Traverse the free list using the **First-Fit** strategy.
3. Find the first free block large enough.
4. Decide whether to:

   * Split the block
   * Allocate the entire block
5. Remove the allocated block from the free list.
6. Return a pointer to the payload.


# Block Splitting

When a free block is significantly larger than the requested allocation, it is divided into two blocks.

Before:

+---------------------------------------------+
|              Large Free Block               |
+---------------------------------------------+

After:

+-------------+-------------------------------+
| Allocated   | Remaining Free Block          |
+-------------+-------------------------------+

The remaining free block receives:

* New header
* New footer
* Updated free-list links

This reduces internal memory waste.


# Freeing Memory

When my_free() is called:

1. Convert the user pointer back to the block header.
2. Mark the block as free.
3. Attempt backward coalescing.
4. Attempt forward coalescing.
5. Insert the merged block back into the free list.


# Coalescing

Fragmentation causes adjacent free blocks to accumulate over time.

Example:

+------+------+
|Free  |Free  |
+------+------+

After coalescing:

+-------------+
| Larger Free |
+-------------+


The allocator performs:

* Forward coalescing
* Backward coalescing

Boundary tags make backward coalescing possible without scanning the heap.


# Alignment

All allocations are aligned to **16-byte boundaries**.

Reasons:

* ABI compliance
* Correct storage of all primitive types
* SIMD compatibility
* Improved memory access efficiency

Alignment is implemented by rounding requested sizes to the nearest multiple of 16.


# Runtime Statistics

The allocator tracks:

* Allocation calls
* Free calls
* Block splits
* Coalescing operations
* Peak allocated memory

These counters help understand allocator behavior and fragmentation patterns.

Example output:

==== allocator stats ====
alloc calls: 3
free calls: 3
splits: 3
coalesces: 3
bytes requested: 160
peak bytes in use: 160
=========================


# Demonstration Tests

The project includes deterministic tests demonstrating:

* Basic allocation
* Block splitting
* Memory reuse
* Forward and backward coalescing
* Alignment verification


# Project Structure

```
.
├── main.c
├── Makefile
└── README.md
```


# Future Improvements

Possible extensions include:

* Heap growth using multiple `mmap()` regions
* Best-fit or segregated free lists
* Allocation benchmarking


# Concepts Explored

* Dynamic Memory Allocation
* Heap Management
* Virtual Memory
* mmap()
* Pointer Arithmetic
* Explicit Free Lists
* Boundary Tags
* Block Splitting
* Memory Coalescing
* Internal and External Fragmentation
* Memory Alignment
* Systems Programming in C


# Learning Outcomes

Through this project I gained practical experience with:

* Designing memory management data structures
* Managing heap metadata
* Implementing allocation and deallocation algorithms
* Working directly with Linux virtual memory APIs
* Reasoning about memory layout and pointer arithmetic
* Understanding fragmentation and allocator design trade-offs


# Build
make

Run:
./main

