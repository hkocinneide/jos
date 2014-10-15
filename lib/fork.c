// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.

  pte_t pte = uvpt[PGNUM((uint32_t)addr)];
  if (!(pte & PTE_COW))
  {
    panic("fork: pgfault: not a copy-on-write page");
  }
  if (!(err & FEC_WR))
  {
    panic("fork: pgfault: not a write-caused fault");
  }

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.

	// LAB 4: Your code here.

  if (sys_page_alloc(0, (void *) PFTEMP, PTE_P | PTE_W | PTE_U) < 0)
  {
    panic("fork: pgfault: cannot sys_page_alloc");
  }
  void *va = (void *)ROUNDDOWN((uintptr_t) addr, PGSIZE);
  memmove((void *)PFTEMP, va, PGSIZE);
  if (sys_page_map(0, (void *) PFTEMP, 0, va, PTE_P | PTE_W | PTE_U) < 0)
  {
    panic("fork: pgfault: cannot sys_page_map");
  }
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
  void *va = (void *)(pn * PGSIZE);
  
  if ((uvpt[pn] & PTE_W) || (uvpt[pn] & PTE_COW))
  {
    if ((r = sys_page_map(0, va, envid, va, PTE_P | PTE_U | PTE_COW)) < 0)
    {
      panic("fork: duppage: Error in sys_page_map: %e", r);
    }
    if ((r = sys_page_map(0, va, 0, va, PTE_P | PTE_U | PTE_COW)) < 0)
    {
      panic("fork: duppage: Error is sys_page_map: %e", r);
    }
  }
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
  set_pgfault_handler(pgfault);

  envid_t envid;
  if ((envid = sys_exofork()) < 0)
  {
    panic("fork: could not call exofork");
  }

  // We're in the child
  if (envid == 0)
  {
    thisenv = &envs[ENVX(sys_getenvid())];
    return 0;
  }
	// LAB 4: Your code here.
	panic("fork not implemented");
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
