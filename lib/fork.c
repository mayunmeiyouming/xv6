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
	pte_t pte = uvpt[PGNUM((uintptr_t)addr)];
	if(!(err & FEC_WR) || !(pte & PTE_COW))
	{
		cprintf("[%08x] user fault va %08x ip %08x\n", sys_getenvid(), addr, utf->utf_eip);
		panic("Page Fault!");
	}

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	uintptr_t start_addr = ROUNDDOWN((uintptr_t)addr, PGSIZE);
	if((r = sys_page_alloc(0, PFTEMP, PTE_W | PTE_U | PTE_P)) < 0)
		panic("Page Alloc Failed: %e", r);
	memmove((void*)PFTEMP, (void*)start_addr, PGSIZE);
	if((r = sys_page_map(0, (void*)PFTEMP, 0, (void*)start_addr, PTE_W | PTE_U | PTE_P)) < 0)
		panic("Page Map Failed: %e", r);
	
	if ((r = sys_page_unmap(0, PFTEMP)) != 0) {
        panic("pgfault: %e", r);
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
    void *addr = (void*) (pn * PGSIZE);
    if (uvpt[pn] & PTE_SHARE) {
        sys_page_map(0, addr, envid, addr, PTE_SYSCALL);        //对于标识为PTE_SHARE的页，拷贝映射关系，并且两个进程都有读写权限
    } else if ((uvpt[pn] & PTE_W) || (uvpt[pn] & PTE_COW)) { //对于UTOP以下的可写的或者写时拷贝的页，拷贝映射关系的同时，需要同时标记当前进程和子进程的页表项为PTE_COW
        if ((r = sys_page_map(0, addr, envid, addr, PTE_COW|PTE_U|PTE_P)) < 0)
            panic("sys_page_map：%e", r);
        if ((r = sys_page_map(0, addr, 0, addr, PTE_COW|PTE_U|PTE_P)) < 0)
            panic("sys_page_map：%e", r);
    } else {
        sys_page_map(0, addr, envid, addr, PTE_U|PTE_P);    //对于只读的页，只需要拷贝映射关系即可
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
	// LAB 4: Your code here.
	// Set up our page fault handler appropriately.
	set_pgfault_handler(pgfault);
	// Create a child.
	envid_t envid = sys_exofork();
	// Copy our address space and page fault handler setup to the child.
	if (envid == 0) {
		// child
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	} else {
		//parent
		int r;
		for (uintptr_t va = 0; va < USTACKTOP; va += PGSIZE) {
			if ((uvpd[PDX(va)] & PTE_P) && (uvpt[PGNUM(va)] & PTE_P)) {
				//uvpd指向页目录，uvpt指向页表，页目录是一页，页表有1024页
				duppage(envid, PGNUM(va));
			}
		}
		// 映射异常堆栈
		if ((r = sys_page_alloc(envid, (void *) (UXSTACKTOP - PGSIZE), PTE_U | PTE_W | PTE_P)) < 0) {
			return r;
		}
		extern void _pgfault_upcall(void);
		if ((r = sys_env_set_pgfault_upcall(envid, _pgfault_upcall)) < 0) {
			return r;
		}
		sys_env_set_status(envid, ENV_RUNNABLE);

		return envid;
	}
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
