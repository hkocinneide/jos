CPSC 422 Final Project
November 2014
Hugh O'Cinneide

# Overview

For my final project, I implemented a pthread-like interface for making new
threads in JOS. I named it jthread, and the functions can be found in
`inc/jthread.h` and `lib/jthread.c`.

The interface are the below six functions:

* `jthread_create`
* `jthread_join`
* `jthread_exit`

* `jthread_mutex_lock`
* `jthread_mutex_trylock`
* `jthread_mutex_unlock`

the first three of which map onto the new system calls:

* `sys_kthread_create`
* `sys_kthread_join`
* `sys_kthread_exit`

and the mutexes use the same xchg strategy as is used in the spinlock.

# Performance

To test this code I used the program at `user/matrix.c`. This calculates the
determinant of a `MATRIXSIZE`-by-`MATRIXSIZE` matrix using a super-exponential
recursive algorithm.

In the serial case, I just let the code run in one environment.
In the thread and ipc case, I split the work into finding the determinant of 
`MATRIXSIZE` `(MATRIXSIZE - 1)`-by-`(MATRIXSIZE - 1)` matrices, each operation
running on its own thread/environment. I use the return values when using
threads, I use IPC to return values when using whole environments.

The following results were taken as the average of 10 runs on each size of
matrix, as you can by tweaking `MATRIXSIZE` and `NUMTRIALS`. Times are in
milliseconds, and these are all performed with `CPUS=4`.

| Size | Serial | Parallel |  IPC |
|:----:|-------:|---------:|-----:|
|   3  |       0|         2|    13|
|   4  |       0|         1|    17|
|   5  |       0|         2|    24|
|   6  |       0|         3|    25|
|   7  |       1|         4|    33|
|   8  |       7|         7|    47|
|   9  |      56|        57|   104|
|  10  |     558|       544|   600|
|  11  |    5911|      5774|  5855|
|  12  |   71483|     69337| 70126|

It seems that QEMU does not physically back its CPUs, because the serial and IPC
performance did not get better over larger matrices when compared to the serial
performance. Or perhaps the zoo does not let me use more than one of its eight
cores.

The results show that the calculations using jthreads are at least as performant
as those done on a single thread of execution (in this case, 2% more performant
on the more stressful tests). Thus, with physically-backed cores, we would
expect to see Parallel and IPC implementations significantly beating the Serial
one.

We also see the large cost involved in the fork-and-ipc in the smaller tasks.
IPC does not become competitive with the serial or parallel implementations
until we are multipling 11-by-11 matrices.

We see that the jthread library provides light weight threads to the JOS kernel.

# Implementation

The basic strategy for this project was to make a `fork()`-like call where a new
environment object would be made, but it would use the same memory space as the
original caller, would start at a function, and would be allocated a new stack.

## New `struct Env` state

I added six fields to struct Env to achieve this

* `bool env_child_thread`
* `envid_t env_process_envid`
* `struct Env *env_next_thread`
* `unsigned env_thread_status`
* `void *env_thread_retval`
* `int env_num_threads`

**`env_child_thread`**: identifies if an environment is a child or not
**`env_process_envid`**: if the environment is a child thread, this identifies
the parent's envid.
**`env_next_thread`**: used as a linked list of environments in a process so
that on main thread exit we can clean up all the child threads.
**`env_thread_status`**: used to identify threads whos values we can reap
**`env_thread_retval`**: the return value of the environment after it exits
**`env_num_thread`**: only valid for the main thread's env, used to count how
many threads the process has, in order to allocate the next thread's stack in
the right place.

## Creating a thread

A thread is created using the `jthread_create` call, which takes a `jthread_t`
store, some attributes that don't matter because I didn't replicate that
pthread functionality, a `void *(*)(void *)` function pointer for the new thread
to start in, and a `void *` argument to be passed to the function.

When the system call `sys_kthread_create` traps into kernel, it exoforks
(which I chose not to leave up to the user so that it can reclaim `THREAD_DONE`
environments (TODO)), and sets the state of the new environment to have the
appropriate page directory, instruction pointer, stack state, and stack pointer.

The process page directory is mapped page-by-page into the new environment's
page directory, to make it cleaner to exit a multithreaded application in a 
multiprocessor environment.

The new instruction pointer points to the function `jthread_main`, which wraps
around the call to the function pointer, and calls the appropriate
`jthread_exit` with return value if the function pointed to does not.

## Allocating the stacks

The new macro `NSTACKPAGES` defines how many pages each environment's user stack
should get. This also dictates how many pages a thread gets. These pages are
mapped immediately, not on demand.

## Reaping threads and their return values

This is a simple matter of checking a environment's `env_thread_status`. If it
is `THREAD_ZOMBIE`, we can grab its return value and mark it as `THREAD_DONE`.

## Returning resources

When a child thread is done, is calls `jthread_exit` (explicitly or implicitly
after return) which give the `void *` return value to the struct Env so another
thread can read it.

A child thread will not return its environment to the kernel until its main
thread exits, at which point each child environment is destroyed via the linked
list.

## Mutexes

Although not a featured part of my tests, this part of the library makes it
reasonably usable. I use the same technique as used in the kernel spinlock to
make a user-space lock.
