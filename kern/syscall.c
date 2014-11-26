/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/jthread.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>
#include <kern/sched.h>
#include <kern/time.h>
#include <kern/e1000.h>

#define DEBUGTHREAD 1

// Print a string to the system console.
// The string is exactly 'len' characters long.
// Destroys the environment on memory errors.
static void
sys_cputs(const char *s, size_t len)
{
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.

	// LAB 3: Your code here.

  user_mem_assert(curenv, (void *)s, len, PTE_U);

	// Print the string supplied by the user.
	cprintf("%.*s", len, s);
}

// Read a character from the system console without blocking.
// Returns the character, or 0 if there is no input waiting.
static int
sys_cgetc(void)
{
	return cons_getc();
}

// Returns the current environment's envid.
static envid_t
sys_getenvid(void)
{
	return curenv->env_id;
}

// Destroy a given environment (possibly the currently running environment).
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_destroy(envid_t envid)
{
	int r;
	struct Env *e;

	if ((r = envid2env(envid, &e, 1)) < 0)
		return r;
	env_destroy(e);
	return 0;
}

// Deschedule current environment and pick a different one to run.
static void
sys_yield(void)
{
	sched_yield();
}

// Allocate a new environment.
// Returns envid of new environment, or < 0 on error.  Errors are:
//	-E_NO_FREE_ENV if no free environment is available.
//	-E_NO_MEM on memory exhaustion.
static envid_t
sys_exofork(void)
{
	// Create the new environment with env_alloc(), from kern/env.c.
	// It should be left as env_alloc created it, except that
	// status is set to ENV_NOT_RUNNABLE, and the register set is copied
	// from the current environment -- but tweaked so sys_exofork
	// will appear to return 0.

  assert(thiscpu->cpu_env);

  struct Env *newenv;
  int ret;
  if ((ret = env_alloc(&newenv, thiscpu->cpu_env->env_id)) < 0)
  {
    return ret;
  }

  newenv->env_tf = thiscpu->cpu_env->env_tf;
  newenv->env_tf.tf_regs.reg_eax = 0;
  newenv->env_status = ENV_NOT_RUNNABLE;
  return newenv->env_id;
}

// Set envid's env_status to status, which must be ENV_RUNNABLE
// or ENV_NOT_RUNNABLE.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if status is not a valid status for an environment.
static int
sys_env_set_status(envid_t envid, int status)
{
	// Hint: Use the 'envid2env' function from kern/env.c to translate an
	// envid to a struct Env.
	// You should set envid2env's third argument to 1, which will
	// check whether the current environment has permission to set
	// envid's status.

	// LAB 4: Your code here.

  // Check valid status
  if (status != ENV_RUNNABLE && status != ENV_NOT_RUNNABLE)
  {
    return -E_INVAL;
  }

  struct Env *e;
  if (envid2env(envid, &e, 1) < 0)
  {
    return -E_BAD_ENV;
  }

  e->env_status = status;

  return 0;
}

// Set envid's trap frame to 'tf'.
// tf is modified to make sure that user environments always run at code
// protection level 3 (CPL 3) with interrupts enabled.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
	// LAB 5: Your code here.
	// Remember to check whether the user has supplied us with a good
	// address!
  struct Env *e;
  int r;
  if ((r = envid2env(envid, &e, 1)) < 0)
  {
    cprintf("sys_env_set_trapframe: could not get the environment, error %e\n", r);
    return r;
  }

  cprintf("Setting trapframe with esp: %08x\n", tf->tf_esp);

  user_mem_assert(e, tf, sizeof(struct Trapframe), PTE_U);
  e->env_tf = *tf;
  e->env_tf.tf_cs |= GD_UT | 3;
  e->env_tf.tf_eflags |= FL_IF;

  return 0;
}

// Set the page fault upcall for 'envid' by modifying the corresponding struct
// Env's 'env_pgfault_upcall' field.  When 'envid' causes a page fault, the
// kernel will push a fault record onto the exception stack, then branch to
// 'func'.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
	// LAB 4: Your code here.

  struct Env *e;
  if (envid2env(envid, &e, 1) < 0)
  {
    return -E_BAD_ENV;
  }

  e->env_pgfault_upcall = func;
  return 0;
}

// Allocate a page of memory and map it at 'va' with permission
// 'perm' in the address space of 'envid'.
// The page's contents are set to 0.
// If a page is already mapped at 'va', that page is unmapped as a
// side effect.
//
// perm -- PTE_U | PTE_P must be set, PTE_AVAIL | PTE_W may or may not be set,
//         but no other bits may be set.  See PTE_SYSCALL in inc/mmu.h.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
//	-E_INVAL if perm is inappropriate (see above).
//	-E_NO_MEM if there's no memory to allocate the new page,
//		or to allocate any necessary page tables.
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
	// Hint: This function is a wrapper around page_alloc() and
	//   page_insert() from kern/pmap.c.
	//   Most of the new code you write should be to check the
	//   parameters for correctness.
	//   If page_insert() fails, remember to free the page you
	//   allocated!

	// LAB 4: Your code here.

  // Check virtual address
  if ((uintptr_t) va >= UTOP || (uintptr_t) va % PGSIZE != 0)
  {
    cprintf("sys_page_alloc: invalid va\n");
    return -E_INVAL;
  }
  // Check permissions
  if ((perm & PTE_U ) == 0 || (perm & PTE_P) == 0 ||
      (perm & ~PTE_SYSCALL) != 0)
  {
    cprintf("sys_page_alloc: invalid permissions\n");
    return -E_INVAL;
  }

  struct Env *e;
  struct PageInfo *p;
  if (envid2env(envid, &e, 1) < 0)
  {
    cprintf("sys_page_alloc: bad environment\n");
    return -E_BAD_ENV;
  }
  p = page_alloc(ALLOC_ZERO);
  if (!p)
  {
    cprintf("sys_page_alloc: ran out of memory\n");
    return -E_NO_MEM;
  }
  if (page_insert(e->env_pgdir, p, va, perm) != 0)
  {
    cprintf("sys_page_alloc: could not insert in pgdir\n");
    page_free(p);
    return -E_NO_MEM;
  }
  return 0;
}

// Map the page of memory at 'srcva' in srcenvid's address space
// at 'dstva' in dstenvid's address space with permission 'perm'.
// Perm has the same restrictions as in sys_page_alloc, except
// that it also must not grant write access to a read-only
// page.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if srcenvid and/or dstenvid doesn't currently exist,
//		or the caller doesn't have permission to change one of them.
//	-E_INVAL if srcva >= UTOP or srcva is not page-aligned,
//		or dstva >= UTOP or dstva is not page-aligned.
//	-E_INVAL is srcva is not mapped in srcenvid's address space.
//	-E_INVAL if perm is inappropriate (see sys_page_alloc).
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in srcenvid's
//		address space.
//	-E_NO_MEM if there's no memory to allocate any necessary page tables.
static int
sys_page_map(envid_t srcenvid, void *srcva,
	     envid_t dstenvid, void *dstva, int perm)
{
	// Hint: This function is a wrapper around page_lookup() and
	//   page_insert() from kern/pmap.c.
	//   Again, most of the new code you write should be to check the
	//   parameters for correctness.
	//   Use the third argument to page_lookup() to
	//   check the current permissions on the page.

	// LAB 4: Your code here.

  if (((uintptr_t) srcva >= UTOP || (uintptr_t) srcva % PGSIZE != 0) ||
      ((uintptr_t) dstva >= UTOP || (uintptr_t) dstva % PGSIZE != 0))
  {
    cprintf("sys_page_map: va past UTOP or not aligned\n");
    return -E_INVAL;
  }

  if ((perm & PTE_P) == 0 || (perm & PTE_U) == 0 ||
      (perm & ~PTE_SYSCALL) != 0)
  {
    cprintf("sys_page_map: permission error\n");
    return -E_INVAL;
  }

  struct Env *srcenv, *dstenv;
  if (envid2env(srcenvid, &srcenv, 1) < 0 ||
      envid2env(dstenvid, &dstenv, 1) < 0)
  {
    cprintf("sys_page_map: bad env\n");
    return -E_BAD_ENV;
  }

  pte_t *srcpte;
  struct PageInfo *p = page_lookup(srcenv->env_pgdir, srcva, &srcpte);
  if (!p)
  {
    cprintf("sys_page_map: cannot find source page\n");
    return -E_INVAL;
  }

  if ((perm & PTE_W) != 0 && (*srcpte & PTE_W) == 0)
  {
    cprintf("sys_page_map: read-only page being mapped to write\n");
    return -E_INVAL;
  }

  if (page_insert(dstenv->env_pgdir, p, dstva, perm) < 0)
  {
    cprintf("sys_page_map: cannot insert page\n");
    return -E_NO_MEM;
  }

  return 0;
}

// Unmap the page of memory at 'va' in the address space of 'envid'.
// If no page is mapped, the function silently succeeds.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
static int
sys_page_unmap(envid_t envid, void *va)
{
	// Hint: This function is a wrapper around page_remove().

	// LAB 4: Your code here.
  if ((uintptr_t) va >= UTOP || (uintptr_t) va % PGSIZE != 0)
  {
    return -E_INVAL;
  }

  struct Env *e;
  if (envid2env(envid, &e, 1))
  {
    return -E_BAD_ENV;
  }

  page_remove(e->env_pgdir, va);
  return 0;
}

// Try to send 'value' to the target env 'envid'.
// If srcva < UTOP, then also send page currently mapped at 'srcva',
// so that receiver gets a duplicate mapping of the same page.
//
// The send fails with a return value of -E_IPC_NOT_RECV if the
// target is not blocked, waiting for an IPC.
//
// The send also can fail for the other reasons listed below.
//
// Otherwise, the send succeeds, and the target's ipc fields are
// updated as follows:
//    env_ipc_recving is set to 0 to block future sends;
//    env_ipc_from is set to the sending envid;
//    env_ipc_value is set to the 'value' parameter;
//    env_ipc_perm is set to 'perm' if a page was transferred, 0 otherwise.
// The target environment is marked runnable again, returning 0
// from the paused sys_ipc_recv system call.  (Hint: does the
// sys_ipc_recv function ever actually return?)
//
// If the sender wants to send a page but the receiver isn't asking for one,
// then no page mapping is transferred, but no error occurs.
// The ipc only happens when no errors occur.
//
// Returns 0 on success, < 0 on error.
// Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist.
//		(No need to check permissions.)
//	-E_IPC_NOT_RECV if envid is not currently blocked in sys_ipc_recv,
//		or another environment managed to send first.
//	-E_INVAL if srcva < UTOP but srcva is not page-aligned.
//	-E_INVAL if srcva < UTOP and perm is inappropriate
//		(see sys_page_alloc).
//	-E_INVAL if srcva < UTOP but srcva is not mapped in the caller's
//		address space.
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in the
//		current environment's address space.
//	-E_NO_MEM if there's not enough memory to map srcva in envid's
//		address space.
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
  struct Env *e;
  if (envid2env(envid, &e, 0) < 0)
  {
    return -E_BAD_ENV;
  }
  if (!e->env_ipc_recving)
  {
    return -E_IPC_NOT_RECV;
  }

  e->env_ipc_recving = 0;
  e->env_ipc_from = curenv->env_id;
  e->env_ipc_value = value;
  e->env_ipc_perm = 0;

  if ((uintptr_t)srcva < UTOP && (uintptr_t)e->env_ipc_dstva < UTOP)
  {
    if ((uintptr_t)srcva % PGSIZE != 0)
    {
      cprintf("sys_ipc_try_send: srcva not page aligned\n");
      return -E_INVAL;
    }
    if ((perm & PTE_P) == 0 || (perm & PTE_U) == 0 ||
         (perm & ~PTE_SYSCALL) != 0)
    {
      cprintf("sys_ipc_try_send: permissions are invalid\n");
      return -E_INVAL;
    }

    pte_t *srcpte;
    struct PageInfo *p = page_lookup(curenv->env_pgdir, srcva, &srcpte);
    if (!p)
    {
      cprintf("sys_ipc_try_send: cannot find source page\n");
      return -E_INVAL;
    }

    if ((perm & PTE_W) != 0 && (*srcpte & PTE_W) == 0)
    {
      cprintf("sys_ipc_try_send: read-only page being mapped to write\n");
      return -E_INVAL;
    }

    if (page_insert(e->env_pgdir, p, e->env_ipc_dstva, perm) < 0)
    {
      cprintf("sys_ipc_try_send: was not able to map page");
      return -E_NO_MEM;
    }
  }

  e->env_ipc_perm = perm;
  e->env_status = ENV_RUNNABLE;

  return 0;
}

// Block until a value is ready.  Record that you want to receive
// using the env_ipc_recving and env_ipc_dstva fields of struct Env,
// mark yourself not runnable, and then give up the CPU.
//
// If 'dstva' is < UTOP, then you are willing to receive a page of data.
// 'dstva' is the virtual address at which the sent page should be mapped.
//
// This function only returns on error, but the system call will eventually
// return 0 on success.
// Return < 0 on error.  Errors are:
//	-E_INVAL if dstva < UTOP but dstva is not page-aligned.
static int
sys_ipc_recv(void *dstva)
{
  if ((uintptr_t)dstva < UTOP && (uintptr_t)dstva % PGSIZE != 0)
  {
    cprintf("sys_ipc_recv: invalild dstva\n");
    return -E_INVAL;
  }

  curenv->env_ipc_recving = 1;
  curenv->env_ipc_dstva = dstva;

  curenv->env_status = ENV_NOT_RUNNABLE;

  return 0;
}

// Return the current time.
static uint32_t
sys_time_msec(void)
{
	// LAB 6: Your code here.
  return time_msec();
}

static int
sys_net_transmit(uint8_t *data, uint32_t len)
{
  if ((uintptr_t) data >= UTOP)
    return -E_INVAL;

  return e1000_transmit(data, len);
}

static int
sys_net_receive(uint8_t *data, uint32_t *len)
{
  if ((uintptr_t) data >= UTOP)
    return -E_INVAL;

  int ret = e1000_receive(data);
  if (ret > 0)
  {
    *len = ret;
    ret = 0;
  }

  return ret;
}

static int
sys_kthread_create(jthread_t tid, void *entry, void *start, void *arg)
{
  if (DEBUGTHREAD)
    cprintf("[%08x] Making new thread with id: %08x\n", curenv->env_id, tid);
  // Find the Env
  struct Env *e;
  if (envid2env(tid, &e, 0) < 0)
    return -E_INVAL;

  // Set thread state

  e->env_child_thread = true;
  e->env_process_envid = curenv->env_process_envid;

  struct Env *process;
  if (envid2env(e->env_process_envid, &process, 0) < 0)
    return -E_INVAL;
  if (DEBUGTHREAD)
    cprintf("Our process's envid: %08x\n", process->env_id);
  // Sanity check about the calling process
  if (!(process->env_status == ENV_NOT_RUNNABLE ||
        process->env_status == ENV_RUNNABLE ||
        process->env_status == ENV_RUNNING))
    return -E_INVAL;
  process->env_num_threads++;
  int threadnum = process->env_num_threads;
  if (DEBUGTHREAD)
    cprintf("%08x's Thread number %d\n", e->env_id, threadnum);

  // Find the last thread on the linked list
  struct Env *next_thread = curenv;
  while (next_thread->env_next_thread)
    next_thread = next_thread->env_next_thread;
  // Add this env to the thread list
  next_thread->env_next_thread = e;

  // Use the same address space and pgfault handler
  e->env_pgdir = curenv->env_pgdir;
  e->env_pgfault_upcall = curenv->env_pgfault_upcall;

  // Allocate new stack
  // TODO: Make this scale for more than one thread

  struct PageInfo *page;
  void *va = (void*)(USTACKTOP - ((threadnum * 2 + 1) * PGSIZE));
  int perm = PTE_P | PTE_U | PTE_W;
  if (!(page = page_alloc(0)))
    return -E_NO_MEM;
  if (page_insert(e->env_pgdir, page, va, perm) < 0)
    return -1;
  if (DEBUGTHREAD)
    cprintf("New page mapped at va:0x%08x\n", (uintptr_t)va);

  // Put the arguments on the stack
  void *kva = page2kva(page);
  *(uint32_t *)(kva + PGSIZE - 4) = (uint32_t)arg;
  *(uint32_t *)(kva + PGSIZE - 8) = (uint32_t)start;

  // Set the eip and esp to the new values
  e->env_tf.tf_eip = (uintptr_t)entry;
  e->env_tf.tf_esp = (uintptr_t)(va + PGSIZE - 12);

  e->env_status = ENV_RUNNABLE;
  
  if (DEBUGTHREAD)
  {
    cprintf("Made a runnable thread\n");
    cprintf("Thread's eip: %08x\n", e->env_tf.tf_eip);
    cprintf("Thread's esp: %08x\n", e->env_tf.tf_esp);
  }
  return 0;
}

static int
sys_kthread_join(jthread_t tid, void **retstore)
{
  struct Env *e;
  if (envid2env(tid, &e, 0) < 0)
    return -1;
  if (e->env_thread_status != THREAD_ZOMBIE)
    return -1;

  // Set retval in the calling env's vm space
  struct PageInfo *page = page_lookup(e->env_pgdir, (void *)retstore, NULL);
  uint32_t *kva = (uint32_t *)(page2kva(page) + PGOFF(retstore));
  *kva = (uint32_t)e->env_thread_retval;

  // Free the env we reaped
  // env_destroy(e);

  return 0;
}

static int
sys_kthread_exit(void *retval)
{
  cprintf("In jthread_exit\n");
  curenv->env_thread_status = THREAD_ZOMBIE;
  curenv->env_thread_retval = retval;
  curenv->env_status = ENV_NOT_RUNNABLE;
  return 0;
}

// Dispatches to the correct kernel function, passing the arguments.
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	// Call the function corresponding to the 'syscallno' parameter.
	// Return any appropriate return value.
	// LAB 3: Your code here.

	switch (syscallno) {
    case SYS_cputs:
      sys_cputs((char *)a1, (size_t)a2);
      return 0;
    case SYS_cgetc:
      return sys_cgetc();
    case SYS_getenvid:
      return sys_getenvid();
    case SYS_env_destroy:
      return sys_env_destroy((envid_t)a1);
    case SYS_page_alloc:
      return sys_page_alloc((envid_t)a1, (void *) a2, (int) a3);
    case SYS_page_map:
      return sys_page_map((envid_t) a1,
                          (void *) a2,
                          (envid_t) a3,
                          (void *) a4,
                          (int)a5);
    case SYS_page_unmap:
      return sys_page_unmap((envid_t) a1, (void *) a2);
    case SYS_exofork:
      return sys_exofork();
    case SYS_env_set_status:
      return sys_env_set_status((envid_t) a1, (int) a2);
    case SYS_env_set_trapframe:
      return sys_env_set_trapframe((envid_t) a1, (struct Trapframe *) a2);
    case SYS_env_set_pgfault_upcall:
      return sys_env_set_pgfault_upcall((envid_t) a1, (void *) a2);
    case SYS_yield:
      sys_yield();
    case SYS_ipc_try_send:
      return sys_ipc_try_send((envid_t) a1, (uint32_t) a2, (void *) a3, (unsigned) a4);
    case SYS_ipc_recv:
      return sys_ipc_recv((void *) a1);
    case SYS_time_msec:
      return sys_time_msec();
    case SYS_net_transmit:
      return sys_net_transmit((uint8_t *)a1, (uint32_t)a2);
    case SYS_net_receive:
      return sys_net_receive((uint8_t *)a1, (uint32_t *)a2);
    case SYS_kthread_create:
      return sys_kthread_create((jthread_t)a1, (void *)a2, (void *)a3, (void *)a4);
    case SYS_kthread_join:
      return sys_kthread_join((jthread_t)a1, (void **)a2);
    case SYS_kthread_exit:
      return sys_kthread_exit((void *)a1);
	default:
		return -E_NO_SYS;
	}
}
