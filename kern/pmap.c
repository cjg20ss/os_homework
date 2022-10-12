/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/mmu.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/pmap.h>
#include <kern/kclock.h>

// These variables are set by i386_detect_memory()
size_t npages;			// Amount of physical memory (in pages)
static size_t npages_basemem;	// Amount of base memory (in pages)

// These variables are set in mem_init()
pde_t *kern_pgdir;		// Kernel's initial page directory
struct PageInfo *pages;		// Physical page state array
static struct PageInfo *page_free_list;	// Free list of physical pages


// --------------------------------------------------------------
// Detect machine's physical memory setup.
// --------------------------------------------------------------

static int
nvram_read(int r)
{
	return mc146818_read(r) | (mc146818_read(r + 1) << 8);
}

static void
i386_detect_memory(void)
{
	size_t basemem, extmem, ext16mem, totalmem;

	// Use CMOS calls to measure available base & extended memory.
	// (CMOS calls return results in kilobytes.)
	basemem = nvram_read(NVRAM_BASELO);
	extmem = nvram_read(NVRAM_EXTLO);
	ext16mem = nvram_read(NVRAM_EXT16LO) * 64;

	// Calculate the number of physical pages available in both base
	// and extended memory.
	if (ext16mem)
		totalmem = 16 * 1024 + ext16mem;
	else if (extmem)
		totalmem = 1 * 1024 + extmem;
	else
		totalmem = basemem;

	npages = totalmem / (PGSIZE / 1024);
	npages_basemem = basemem / (PGSIZE / 1024);

	cprintf("Physical memory: %uK available, base = %uK, extended = %uK\n",
		totalmem, basemem, totalmem - basemem);
}


// --------------------------------------------------------------
// Set up memory mappings above UTOP.
// --------------------------------------------------------------

static void boot_map_region(pde_t *pgdir, uintptr_t va, size_t size, physaddr_t pa, int perm);
static void check_page_free_list(bool only_low_memory);
static void check_page_alloc(void);
static void check_kern_pgdir(void);
static physaddr_t check_va2pa(pde_t *pgdir, uintptr_t va);
static void check_page(void);
static void check_page_installed_pgdir(void);

// This simple physical memory allocator is used only while JOS is setting
// up its virtual memory system.  page_alloc() is the real allocator.
//
// If n>0, allocates enough pages of contiguous physical memory to hold 'n'
// bytes.  Doesn't initialize the memory.  Returns a kernel virtual address.
//
// If n==0, returns the address of the next free page without allocating
// anything.
//
// If we're out of memory, boot_alloc should panic.
// This function may ONLY be used during initialization,
// before the page_free_list list has been set up.
static void *
boot_alloc(uint32_t n)
{
	static char *nextfree;	// virtual address of next byte of free memory
	char *result;

	// Initialize nextfree if this is the first time.
	// 'end' is a magic symbol automatically generated by the linker,
	// which points to the end of the kernel's bss segment:
	// the first virtual address that the linker did *not* assign
	// to any kernel code or global variables.
	if (!nextfree) {	// 这里对static变量nextfree做初始化
		extern char end[];
		nextfree = ROUNDUP((char *) end, PGSIZE);
	}

	// Allocate a chunk large enough to hold 'n' bytes, then update
	// nextfree.  Make sure nextfree is kept aligned
	// to a multiple of PGSIZE.
	//
	// LAB 2: Your code here.

	// return NULL;
	
	// [lab2-exercise1]
	// 参考资料
	// <https://blog.csdn.net/qhaaha/article/details/111430350>
	if (n > 0) {
		// ROUNDUP的含义：以n是0-1023之间的数为例。
		// 如果n是0，那么已经对齐，ROUNDUP之后还是0；
		// 如果n是1-1023，那么没有对齐，ROUNDUP之后变成1024。
		size_t allocated_size = ROUNDUP(n, PGSIZE);	// 分配若干物理页的空间，以容纳nB空间
		if (nextfree + allocated_size > (char *)KERNBASE + npages * PGSIZE) {
			// 强制类型转换为char*是因为这里的表达式是base+offset的形式，
			// base是某个Byte的地址，offset是偏移Byte的个数。
			panic("boot_alloc: run out of memory!\n");
		} else {
			result = nextfree;
			nextfree += allocated_size;
		}

	} else if (n == 0) {
		result = nextfree;
	} else {	// n < 0
		panic("boot_alloc: n should not be a negative number!\n");
	}

	return result;
}

// Set up a two-level page table:
//    kern_pgdir is its linear (virtual) address of the root
//
// This function only sets up the kernel part of the address space
// (ie. addresses >= UTOP).  The user part of the address space
// will be set up later.
//
// From UTOP to ULIM, the user is allowed to read but not write.
// Above ULIM the user cannot read or write.
void
mem_init(void)
{
	uint32_t cr0;
	size_t n;

	// Find out how much memory the machine has (npages & npages_basemem).
	i386_detect_memory();

	// // Remove this line when you're ready to test this function.
	// panic("mem_init: This function is not finished\n");

	//////////////////////////////////////////////////////////////////////
	// create initial page directory.
	kern_pgdir = (pde_t *) boot_alloc(PGSIZE);
	memset(kern_pgdir, 0, PGSIZE);

	//////////////////////////////////////////////////////////////////////
	// Recursively insert PD in itself as a page table, to form
	// a virtual page table at virtual address UVPT.
	// (For now, you don't have to understand the greater purpose of the
	// following line.)

	// Permissions: kernel R, user R
	kern_pgdir[PDX(UVPT)] = PADDR(kern_pgdir) | PTE_U | PTE_P;
	// 向页目录Page Directory的第UVPT[31:22]个PDE项的高20位填入页目录的物理地址
	// 这一项相当于是页目录项PDE指向页目录PD自己，而不是指向某个PT页表。
	// 最后打开PTE_U和PTE_P标志位。

	//////////////////////////////////////////////////////////////////////
	// Allocate an array of npages 'struct PageInfo's and store it in 'pages'.
	// The kernel uses this array to keep track of physical pages: for
	// each physical page, there is a corresponding struct PageInfo in this
	// array.  'npages' is the number of physical pages in memory.  Use memset
	// to initialize all fields of each struct PageInfo to 0.
	// Your code goes here:

	// [lab2-exercise1]
	// 参考资料
	// <https://blog.csdn.net/qhaaha/article/details/111430350>

	// 分配一个由npages个PageInfo结构体组成的数组，并初始化为全0。
	// 这个数组反映了物理页的状态信息，内核据此跟踪、管理物理页。
	size_t page_info_list_size = npages * sizeof(struct PageInfo);
	pages = (struct PageInfo *)boot_alloc(page_info_list_size);	// pages是全局变量
	memset(pages, 0, page_info_list_size);

	//////////////////////////////////////////////////////////////////////
	// Now that we've allocated the initial kernel data structures, we set
	// up the list of free physical pages. Once we've done so, all further
	// memory management will go through the page_* functions. In
	// particular, we can now map memory using boot_map_region
	// or page_insert
	page_init();

	check_page_free_list(1);
	check_page_alloc();
	check_page();

	//////////////////////////////////////////////////////////////////////
	// Now we set up virtual memory

	//////////////////////////////////////////////////////////////////////
	// Map 'pages' read-only by the user at linear address UPAGES
	// Permissions:
	//    - the new image at UPAGES -- kernel R, user R
	//      (ie. perm = PTE_U | PTE_P)
	//    - pages itself -- kernel RW, user NONE
	// Your code goes here:

	//////////////////////////////////////////////////////////////////////
	// Use the physical memory that 'bootstack' refers to as the kernel
	// stack.  The kernel stack grows down from virtual address KSTACKTOP.
	// We consider the entire range from [KSTACKTOP-PTSIZE, KSTACKTOP)
	// to be the kernel stack, but break this into two pieces:
	//     * [KSTACKTOP-KSTKSIZE, KSTACKTOP) -- backed by physical memory
	//     * [KSTACKTOP-PTSIZE, KSTACKTOP-KSTKSIZE) -- not backed; so if
	//       the kernel overflows its stack, it will fault rather than
	//       overwrite memory.  Known as a "guard page".
	//     Permissions: kernel RW, user NONE
	// Your code goes here:

	//////////////////////////////////////////////////////////////////////
	// Map all of physical memory at KERNBASE.
	// Ie.  the VA range [KERNBASE, 2^32) should map to
	//      the PA range [0, 2^32 - KERNBASE)
	// We might not have 2^32 - KERNBASE bytes of physical memory, but
	// we just set up the mapping anyway.
	// Permissions: kernel RW, user NONE
	// Your code goes here:

	// Check that the initial page directory has been set up correctly.
	check_kern_pgdir();

	// Switch from the minimal entry page directory to the full kern_pgdir
	// page table we just created.	Our instruction pointer should be
	// somewhere between KERNBASE and KERNBASE+4MB right now, which is
	// mapped the same way by both page tables.
	//
	// If the machine reboots at this point, you've probably set up your
	// kern_pgdir wrong.
	lcr3(PADDR(kern_pgdir));

	check_page_free_list(0);

	// entry.S set the really important flags in cr0 (including enabling
	// paging).  Here we configure the rest of the flags that we care about.
	cr0 = rcr0();
	cr0 |= CR0_PE|CR0_PG|CR0_AM|CR0_WP|CR0_NE|CR0_MP;
	cr0 &= ~(CR0_TS|CR0_EM);
	lcr0(cr0);

	// Some more checks, only possible after kern_pgdir is installed.
	check_page_installed_pgdir();
}

// --------------------------------------------------------------
// Tracking of physical pages.
// The 'pages' array has one 'struct PageInfo' entry per physical page.
// Pages are reference counted, and free pages are kept on a linked list.
// --------------------------------------------------------------

//
// Initialize page structure and memory free list.
// After this is done, NEVER use boot_alloc again.  ONLY use the page
// allocator functions below to allocate and deallocate physical
// memory via the page_free_list.
//
void
page_init(void)
{
	// The example code here marks all physical pages as free.
	// However this is not truly the case.  What memory is free?
	//  1) Mark physical page 0 as in use.
	//     This way we preserve the real-mode IDT and BIOS structures
	//     in case we ever need them.  (Currently we don't, but...)
	//  2) The rest of base memory, [PGSIZE, npages_basemem * PGSIZE)
	//     is free.
	//  3) Then comes the IO hole [IOPHYSMEM, EXTPHYSMEM), which must
	//     never be allocated.
	//  4) Then extended memory [EXTPHYSMEM, ...).
	//     Some of it is in use, some is free. Where is the kernel
	//     in physical memory?  Which pages are already in use for
	//     page tables and other data structures?
	//
	// Change the code to reflect this.
	// NB: DO NOT actually touch the physical memory corresponding to
	// free pages!

	// [lab2-exercise1]
	// 参考资料
	// <https://blog.csdn.net/qhaaha/article/details/111430350>
	
	size_t i;	// i是遍历整个page_info_list的索引
	page_free_list = NULL;	// 理论上static变量的初值就是0，不需要初始化

	// 1) 物理页0要预留，用于存储实模式IDT和BIOS数据结构
	pages[0].pp_ref = 1;
	pages[0].pp_link = NULL;	// pp_link域表示page_free_list中的下一个节点

	// 2) 基础内存的剩余部分 [PGSIZE, npages_basemem * PGSIZE) 可用
	for (i = 1; i < npages_basemem; i++) {
		pages[i].pp_ref = 0;

		// 把新节点插到链表头部之前成为新的头部								
		pages[i].pp_link = page_free_list;	// 令新节点的link指向链表原来的头指针
		page_free_list = &pages[i];			// 链表新的头指针指向新节点
	}

	// 3) IO hole [IOPHYSMEM, EXTPHYSMEM) 需要预留
	size_t npages_IOPHYSMEM = IOPHYSMEM / PGSIZE;
	size_t npages_EXTPHYSMEM = EXTPHYSMEM / PGSIZE;

	for (i = npages_IOPHYSMEM; i < npages_EXTPHYSMEM; i++) {
		pages[0].pp_ref = 1;
		pages[0].pp_link = NULL;
	}

	// 4) 扩展内存 [EXTPHYSMEM, ...) 中前面若干个连续的物理页已分配，剩余的物理页可用
	size_t npages_free_begin_index = ((size_t)boot_alloc(0) - KERNBASE) / PGSIZE;
	
	for (i = npages_EXTPHYSMEM; i < npages_free_begin_index; i++) {
		pages[i].pp_ref = 1;
		pages[i].pp_link = NULL;
	}
	for (i = npages_free_begin_index; i < npages; i++) {
		pages[i].pp_ref = 0;
		pages[i].pp_link = page_free_list;
		page_free_list = &pages[i];
	}
}

//
// Allocates a physical page.  If (alloc_flags & ALLOC_ZERO), fills the entire
// returned physical page with '\0' bytes.  Does NOT increment the reference
// count of the page - the caller must do these if necessary (either explicitly
// or via page_insert).
//
// Be sure to set the pp_link field of the allocated page to NULL so
// page_free can check for double-free bugs.
//
// Returns NULL if out of free memory.
//
// Hint: use page2kva and memset
struct PageInfo *
page_alloc(int alloc_flags)
{
	// Fill this function in

	// [lab2-exercise1]
	// 参考资料
	// <https://blog.csdn.net/qhaaha/article/details/111430350>

	struct PageInfo * freepage;

	if (page_free_list) {
		freepage = page_free_list;
		page_free_list = page_free_list->pp_link;
		freepage->pp_link = NULL;

		if (alloc_flags & ALLOC_ZERO) {
			// page2kva传入的是一个PageInfo结构体的指针
			// 函数内完成了 PageInfo* => pa => kva 的转换

			// 程序里能够操纵的地址都是va虚拟地址。
			memset(page2kva(freepage), '\0', PGSIZE);
		}
		return freepage;
	} else {
		return NULL;
	}
}

//
// Return a page to the free list.
// (This function should only be called when pp->pp_ref reaches 0.)
//
void
page_free(struct PageInfo *pp)
{
	// Fill this function in
	// Hint: You may want to panic if pp->pp_ref is nonzero or
	// pp->pp_link is not NULL.

	// [lab2-exercise1]
	// 参考资料
	// <https://blog.csdn.net/qhaaha/article/details/111430350>

	if (pp->pp_ref || pp->pp_link) {
		panic("page_free: pp->pp_ref is nonzero or pp->pp_link is not NULL.");
	}

	// 把pp节点插入到链表的头节点之前，并更新头节点。 
	pp->pp_link = page_free_list;
	page_free_list = pp;
}

//
// Decrement the reference count on a page,
// freeing it if there are no more refs.
//
void
page_decref(struct PageInfo* pp)
{
	if (--pp->pp_ref == 0)
		page_free(pp);
}

// Given 'pgdir', a pointer to a page directory, pgdir_walk returns
// a pointer to the page table entry (PTE) for linear address 'va'.
// This requires walking the two-level page table structure.
//
// The relevant page table page might not exist yet.
// If this is true, and create == false, then pgdir_walk returns NULL.
// Otherwise, pgdir_walk allocates a new page table page with page_alloc.
//    - If the allocation fails, pgdir_walk returns NULL.
//    - Otherwise, the new page's reference count is incremented,
//	the page is cleared,
//	and pgdir_walk returns a pointer into the new page table page.
//
// Hint 1: you can turn a PageInfo * into the physical address of the
// page it refers to with page2pa() from kern/pmap.h.
//
// Hint 2: the x86 MMU checks permission bits in both the page directory
// and the page table, so it's safe to leave permissions in the page
// directory more permissive than strictly necessary.
//
// Hint 3: look at inc/mmu.h for useful macros that manipulate page
// table and page directory entries.
//
pte_t *
pgdir_walk(pde_t *pgdir, const void *va, int create)
{
	// Fill this function in
	// return NULL;

	// [lab2-exercise1]
	// 参考资料
	// <https://blog.csdn.net/qhaaha/article/details/111430350>

	size_t pdx = PDX(va);	// 页目录中的索引
	size_t ptx = PTX(va);	// 页表中的索引

	if (pgdir[pdx]) {	// 页目录中找到了第pdx项
		physaddr_t pt_pa = PTE_ADDR(pgdir[pdx]);	// 该PDE项的高20位表示PT的起始物理地址的高20位
		physaddr_t pte_pa = pt_pa + ptx * sizeof(pte_t);	// 找到该PT的第ptx项的物理地址
		pte_t *pte_va = (pte_t *) KADDR(pte_pa);	// 把PTE的物理地址转换成虚拟地址
		// cprintf("[debug] 找到pde，进而尝试寻找pte\n");
		return pte_va;
	} else if (!create) {	// 页目录中找不到第pdx项，且不创建该页表
		// cprintf("[debug] pgdir[pdx] == 0x00000000，没找到pde\n");
		return NULL;
	} else {	// 页目录中找不到第pdx项，创建该页表
		struct PageInfo *allocated_page = page_alloc(ALLOC_ZERO);
		if (!allocated_page) {	// page_alloc没有成功分配物理页
			// cprintf("[debug] 没找到pde，且没有内存分配新页表\n");
			return NULL;
		}
		// cprintf("[debug] 没找到pde，因此创建新页表\n");
		allocated_page->pp_ref++;
		pgdir[pdx] = page2pa(allocated_page) | PTE_P | PTE_U | PTE_W;	// 填入新分配的物理页的起始物理地址

		physaddr_t pt_pa = PTE_ADDR(pgdir[pdx]);	// 该PDE项的高20位表示PT的起始物理地址的高20位
		physaddr_t pte_pa = pt_pa + ptx * sizeof(pte_t);	// 找到该PT的第ptx项的物理地址
		pte_t *pte_va = (pte_t *) KADDR(pte_pa);	// 把PTE的物理地址转换成虚拟地址
		return pte_va;
	}
}

//
// Map [va, va+size) of virtual address space to physical [pa, pa+size)
// in the page table rooted at pgdir.  Size is a multiple of PGSIZE, and
// va and pa are both page-aligned.
// Use permission bits perm|PTE_P for the entries.
//
// This function is only intended to set up the ``static'' mappings
// above UTOP. As such, it should *not* change the pp_ref field on the
// mapped pages.
//
// Hint: the TA solution uses pgdir_walk
static void
boot_map_region(pde_t *pgdir, uintptr_t va, size_t size, physaddr_t pa, int perm)
{
	// Fill this function in

	// [lab2-exercise1]
	// 参考资料
	// <https://blog.csdn.net/qhaaha/article/details/111430350>

	assert(!(size % PGSIZE) && !(va % PGSIZE) && !(pa % PGSIZE));

	// va => pa
	// va + PGSIZE => pa + PGSIZE
	// ...
	// va + size - PGSIZE => pa + size - PGSIZE
	for (size_t i = 0; i < size; i += PGSIZE) {
		pte_t* pte_va = pgdir_walk(pgdir, (void *)(va + i), 1);
		*pte_va = (pa + i) | perm | PTE_P;
	}
}

//
// Map the physical page 'pp' at virtual address 'va'.
// The permissions (the low 12 bits) of the page table entry
// should be set to 'perm|PTE_P'.
//
// Requirements
//   - If there is already a page mapped at 'va', it should be page_remove()d.
//   - If necessary, on demand, a page table should be allocated and inserted
//     into 'pgdir'.
//   - pp->pp_ref should be incremented if the insertion succeeds.
//   - The TLB must be invalidated if a page was formerly present at 'va'.
//
// Corner-case hint: Make sure to consider what happens when the same
// pp is re-inserted at the same virtual address in the same pgdir.
// However, try not to distinguish this case in your code, as this
// frequently leads to subtle bugs; there's an elegant way to handle
// everything in one code path.
//
// RETURNS:
//   0 on success
//   -E_NO_MEM, if page table couldn't be allocated
//
// Hint: The TA solution is implemented using pgdir_walk, page_remove,
// and page2pa.
//
int
page_insert(pde_t *pgdir, struct PageInfo *pp, void *va, int perm)
{
	// Fill this function in
	// return 0;

	// [lab2-exercise1]
	// 参考资料
	// <https://blog.csdn.net/qhaaha/article/details/111430350>

	pte_t *pte_va = pgdir_walk(pgdir, va, 1);
	if (!pte_va) {	// 没有内存分配新页表
		return -E_NO_MEM;
	}

	// 页表中已经把va映射到了物理页pp的情形
	// if (PTE_ADDR(*pte_va) == page2pa(pp)) {
	// 	*pte_va = PTE_ADDR(*pte_va) | perm | PTE_P;
	// 	return 0;
	// }
	
	// 先让pp_ref增加再page_remove，以防止page_remove的时候ref变成0而直接释放该页
	pp->pp_ref++;
	page_remove(pgdir, va);
	*pte_va = page2pa(pp) | perm | PTE_P;	// 把va映射到物理页pp
	return 0;
}

//
// Return the page mapped at virtual address 'va'.
// If pte_store is not zero, then we store in it the address
// of the pte for this page.  This is used by page_remove and
// can be used to verify page permissions for syscall arguments,
// but should not be used by most callers.
//
// Return NULL if there is no page mapped at va.
//
// Hint: the TA solution uses pgdir_walk and pa2page.
//
struct PageInfo *
page_lookup(pde_t *pgdir, void *va, pte_t **pte_store)
{
	// Fill this function in
	// return NULL;

	// [lab2-exercise1]
	// 参考资料
	// <https://blog.csdn.net/qhaaha/article/details/111430350>

	pte_t *pte_va = pgdir_walk(pgdir, va, 0);
	if (pte_va && *pte_va) {	// 能找到pte项且其中记录的物理页地址有效
		if (pte_store) {	// 如果指针pte_store不为NULL，就把pte_va存到该指针
			*pte_store = pte_va;
		}
		return pa2page(PTE_ADDR(*pte_va));	// 把物理地址转换成PageInfo*返回
	} else {	// 没有物理页被映射到va
		return NULL;
	}
}

//
// Unmaps the physical page at virtual address 'va'.
// If there is no physical page at that address, silently does nothing.
//
// Details:
//   - The ref count on the physical page should decrement.
//   - The physical page should be freed if the refcount reaches 0.
//   - The pg table entry corresponding to 'va' should be set to 0.
//     (if such a PTE exists)
//   - The TLB must be invalidated if you remove an entry from
//     the page table.
//
// Hint: The TA solution is implemented using page_lookup,
// 	tlb_invalidate, and page_decref.
//
void
page_remove(pde_t *pgdir, void *va)
{
	// Fill this function in

	// [lab2-exercise1]
	// 参考资料
	// <https://blog.csdn.net/qhaaha/article/details/111430350>
	
	pte_t *pte;
	struct PageInfo *va_page = page_lookup(pgdir, va, &pte);	// 找到va对应的物理页
	if (!va_page) {	// va没有映射到物理页
		return;
	}
	page_decref(va_page);	// va映射到的物理页的ref减少（如果减为0就释放）
	*pte = 0;	// 去除va的映射
	tlb_invalidate(pgdir, va);	// tlb中也去除va的映射（如果有的话）
}

//
// Invalidate a TLB entry, but only if the page tables being
// edited are the ones currently in use by the processor.
//
void
tlb_invalidate(pde_t *pgdir, void *va)
{
	// Flush the entry only if we're modifying the current address space.
	// For now, there is only one address space, so always invalidate.
	invlpg(va);
}


// --------------------------------------------------------------
// Checking functions.
// --------------------------------------------------------------

//
// Check that the pages on the page_free_list are reasonable.
//
static void
check_page_free_list(bool only_low_memory)
{
	struct PageInfo *pp;
	unsigned pdx_limit = only_low_memory ? 1 : NPDENTRIES;
	int nfree_basemem = 0, nfree_extmem = 0;
	char *first_free_page;

	if (!page_free_list)
		panic("'page_free_list' is a null pointer!");

	if (only_low_memory) {
		// Move pages with lower addresses first in the free
		// list, since entry_pgdir does not map all pages.

		// 这段代码有点搞脑子
		// 语法上，理解这个循环的要点是：
		// 假设 int a = 0, b = 1;
		// a = b;	// 这个代码让a的值变成1
		// a = 2;	// 这个代码让a的值变成2。（重要的是，b的值不会变成2）
		// 应该理解为：把2这个值写入&a。即 *(&a) = 2。
		// 不能理解为：因为a==b，所以a=2就等同于b=2。
		// （不能这么解释，因为&a != &b）

		// 把上面的情景延伸到链表：
		// 假设 Node *a, *b, *c;（class Node{ int data; Node* link; }）
		// *a = Node(0, NULL); *b = Node(1, NULL); *c = Node(2, NULL);
		// a->link = b;	// 这个代码让a指向b
		// c->link = a->link; // 这个代码让c指向b
		// c->link = a;	// 这个代码让c指向a。（重要的是，a不会因此而指向a）
		// 应该理解为：把a这个地址写入&(c->link)。即 *(&(c->link)) = a。
		// 不能理解为：因为c->link==a->link，所以c->link=a就等同于a->link=a。
		// （不能这么解释，因为&(c->link) != &(a->link)）
		// 本质：c->link是一个变量，虽然它在上一刻的值与a->link相同，但它们是不同的变量。

		// 下面的循环，就是把page_free_list中的节点根据其对应物理页的起始物理地址的PDX部分是为0还是非0分成两类
		// 两类节点分别加到pp1和pp2中，加的过程也是加到链表头之前并更新头。

		// 循环外的两个赋值语句是收尾。相当于是 pp1 -> pp2 -> NULL ，把两个链表串起来
		// 这样处理之后，page_free_list前面部分的节点就是pp1的节点，也就是所谓的low memory pages
		struct PageInfo *pp1, *pp2;
		struct PageInfo **tp[2] = { &pp1, &pp2 };
		for (pp = page_free_list; pp; pp = pp->pp_link) {
			int pagetype = PDX(page2pa(pp)) >= pdx_limit;
			*tp[pagetype] = pp;
			tp[pagetype] = &pp->pp_link;
		}
		*tp[1] = 0;
		*tp[0] = pp2;
		page_free_list = pp1;
	}

	// if there's a page that shouldn't be on the free list,
	// try to make sure it eventually causes trouble.
	for (pp = page_free_list; pp; pp = pp->pp_link)
		if (PDX(page2pa(pp)) < pdx_limit)
			memset(page2kva(pp), 0x97, 128);

	first_free_page = (char *) boot_alloc(0);
	for (pp = page_free_list; pp; pp = pp->pp_link) {
		// check that we didn't corrupt the free list itself

		// 检验page_free_list中的PageInfo*都处在[(void*)pages, (void*)pages+npages*sizeof(PageInfo))中
		assert(pp >= pages);
		assert(pp < pages + npages);
		// 并且该PageInfo*和(void*)pages的差值应该为sizeof(PageInfo)的整数倍
		assert(((char *) pp - (char *) pages) % sizeof(*pp) == 0);

		// check a few pages that shouldn't be on the free list

		// 物理页0不可用
		assert(page2pa(pp) != 0);
		// IO空洞不可用
		assert(page2pa(pp) != IOPHYSMEM);
		assert(page2pa(pp) != EXTPHYSMEM - PGSIZE);
		assert(page2pa(pp) != EXTPHYSMEM);
		// 可用的物理页起始地址应该<IO空洞 或者 >= free_page_list的第一个可用页的起始地址
		assert(page2pa(pp) < EXTPHYSMEM || (char *) page2kva(pp) >= first_free_page);

		if (page2pa(pp) < EXTPHYSMEM)
			++nfree_basemem;
		else
			++nfree_extmem;
	}

	// 基础内存和扩展内存中都应该有可用页
	assert(nfree_basemem > 0);
	assert(nfree_extmem > 0);

	cprintf("check_page_free_list() succeeded!\n");
}

//
// Check the physical page allocator (page_alloc(), page_free(),
// and page_init()).
//
static void
check_page_alloc(void)
{
	struct PageInfo *pp, *pp0, *pp1, *pp2;
	int nfree;
	struct PageInfo *fl;
	char *c;
	int i;

	// pages应该被分配内存，其起始地址不应为NULL
	if (!pages)
		panic("'pages' is a null pointer!");

	// check number of free pages
	for (pp = page_free_list, nfree = 0; pp; pp = pp->pp_link)
		++nfree;

	// should be able to allocate three pages
	// 检验分配3个物理页的行为
	pp0 = pp1 = pp2 = 0;
	assert((pp0 = page_alloc(0)));
	assert((pp1 = page_alloc(0)));
	assert((pp2 = page_alloc(0)));

	assert(pp0);
	assert(pp1 && pp1 != pp0);
	assert(pp2 && pp2 != pp1 && pp2 != pp0);
	assert(page2pa(pp0) < npages*PGSIZE);
	assert(page2pa(pp1) < npages*PGSIZE);
	assert(page2pa(pp2) < npages*PGSIZE);

	// temporarily steal the rest of the free pages
	// 检验物理页不够分配时的边界情况
	fl = page_free_list;
	page_free_list = 0;

	// should be no free memory
	assert(!page_alloc(0));

	// free and re-allocate?
	// 检验物理页的释放与重新分配
	page_free(pp0);
	page_free(pp1);
	page_free(pp2);
	pp0 = pp1 = pp2 = 0;
	assert((pp0 = page_alloc(0)));
	assert((pp1 = page_alloc(0)));
	assert((pp2 = page_alloc(0)));
	assert(pp0);
	assert(pp1 && pp1 != pp0);
	assert(pp2 && pp2 != pp1 && pp2 != pp0);
	assert(!page_alloc(0));

	// test flags
	// 测试分配物理页时是否有做合理的初始化
	memset(page2kva(pp0), 1, PGSIZE);
	page_free(pp0);
	assert((pp = page_alloc(ALLOC_ZERO)));
	assert(pp && pp0 == pp);

	// cprintf("[debug] %u\n", pp);
	// cprintf("[debug] %u\n", pp0);
	// cprintf("[debug] %u\n", pp && pp0);
	// && 运算符会认为非0为真，0为假
	// C语言没有bool类型，&& 的运算结果中，真为1，假为0
	
	c = page2kva(pp);
	for (i = 0; i < PGSIZE; i++)
		assert(c[i] == 0);

	// give free list back
	page_free_list = fl;

	// free the pages we took
	page_free(pp0);
	page_free(pp1);
	page_free(pp2);

	// number of free pages should be the same
	// 分配和释放之后，检查可用物理页的个数
	for (pp = page_free_list; pp; pp = pp->pp_link)
		--nfree;
	assert(nfree == 0);

	cprintf("check_page_alloc() succeeded!\n");
}

//
// Checks that the kernel part of virtual address space
// has been set up roughly correctly (by mem_init()).
//
// This function doesn't test every corner case,
// but it is a pretty good sanity check.
//

static void
check_kern_pgdir(void)
{
	uint32_t i, n;
	pde_t *pgdir;

	pgdir = kern_pgdir;

	// check pages array
	n = ROUNDUP(npages*sizeof(struct PageInfo), PGSIZE);
	for (i = 0; i < n; i += PGSIZE)
		assert(check_va2pa(pgdir, UPAGES + i) == PADDR(pages) + i);


	// check phys mem
	for (i = 0; i < npages * PGSIZE; i += PGSIZE)
		assert(check_va2pa(pgdir, KERNBASE + i) == i);

	// check kernel stack
	for (i = 0; i < KSTKSIZE; i += PGSIZE)
		assert(check_va2pa(pgdir, KSTACKTOP - KSTKSIZE + i) == PADDR(bootstack) + i);
	assert(check_va2pa(pgdir, KSTACKTOP - PTSIZE) == ~0);

	// check PDE permissions
	for (i = 0; i < NPDENTRIES; i++) {
		switch (i) {
		case PDX(UVPT):
		case PDX(KSTACKTOP-1):
		case PDX(UPAGES):
			assert(pgdir[i] & PTE_P);
			break;
		default:
			if (i >= PDX(KERNBASE)) {
				assert(pgdir[i] & PTE_P);
				assert(pgdir[i] & PTE_W);
			} else
				assert(pgdir[i] == 0);
			break;
		}
	}
	cprintf("check_kern_pgdir() succeeded!\n");
}

// This function returns the physical address of the page containing 'va',
// defined by the page directory 'pgdir'.  The hardware normally performs
// this functionality for us!  We define our own version to help check
// the check_kern_pgdir() function; it shouldn't be used elsewhere.

static physaddr_t
check_va2pa(pde_t *pgdir, uintptr_t va)
{
	pte_t *p;

	pgdir = &pgdir[PDX(va)];	// PDE的地址
	if (!(*pgdir & PTE_P))	// 该PDE项的存在位PTE_P为0
		return ~0;			// 返回0xFFFFFFFF
	p = (pte_t*) KADDR(PTE_ADDR(*pgdir));	// PT的起始地址
	if (!(p[PTX(va)] & PTE_P))	// 该PTE项的存在位PTE_P为0
		return ~0;			// 返回0xFFFFFFFF
	return PTE_ADDR(p[PTX(va)]);	// 返回物理页的起始物理地址
}


// check page_insert, page_remove, &c
static void
check_page(void)
{
	struct PageInfo *pp, *pp0, *pp1, *pp2;
	struct PageInfo *fl;
	pte_t *ptep, *ptep1;
	void *va;
	int i;
	extern pde_t entry_pgdir[];

	// should be able to allocate three pages
	// 检验分配3个物理页的行为
	pp0 = pp1 = pp2 = 0;
	assert((pp0 = page_alloc(0)));
	assert((pp1 = page_alloc(0)));
	assert((pp2 = page_alloc(0)));

	assert(pp0);
	assert(pp1 && pp1 != pp0);
	assert(pp2 && pp2 != pp1 && pp2 != pp0);

	// temporarily steal the rest of the free pages
	fl = page_free_list;
	page_free_list = 0;

	// should be no free memory
	// 检验物理页不够分配时的边界情况
	assert(!page_alloc(0));

	// 到目前为止，kern_pgdir中的内容为全0，除了kern_pgdir[PDX(UVPT)] = PADDR(kern_pgdir) | PTE_U | PTE_P;

	// there is no page allocated at address 0
	// 因为kern_pgdir[PDX(0x0)] == 0，表示PDX(0x0)对应的PDE找不到（无效）
	assert(page_lookup(kern_pgdir, (void *) 0x0, &ptep) == NULL);

	// there is no free memory, so we can't allocate a page table
	// page_free_list里面没有空闲物理页，因此page_insert失败
	assert(page_insert(kern_pgdir, pp1, 0x0, PTE_W) < 0);

	// free pp0 and try again: pp0 should be used for page table
	// 释放pp0，因此为了把虚拟地址0x0映射到物理页pp1，需要分配pp0作为相关的页表
	// 把虚拟地址0x0映射到物理页pp1
	page_free(pp0);
	assert(page_insert(kern_pgdir, pp1, 0x0, PTE_W) == 0);
	assert(PTE_ADDR(kern_pgdir[0]) == page2pa(pp0));
	assert(check_va2pa(kern_pgdir, 0x0) == page2pa(pp1));
	assert(pp1->pp_ref == 1);
	assert(pp0->pp_ref == 1);

	// should be able to map pp2 at PGSIZE because pp0 is already allocated for page table
	assert(page_insert(kern_pgdir, pp2, (void*) PGSIZE, PTE_W) == 0);
	assert(check_va2pa(kern_pgdir, PGSIZE) == page2pa(pp2));
	assert(pp2->pp_ref == 1);

	// should be no free memory
	assert(!page_alloc(0));

	// should be able to map pp2 at PGSIZE because it's already there
	assert(page_insert(kern_pgdir, pp2, (void*) PGSIZE, PTE_W) == 0);
	assert(check_va2pa(kern_pgdir, PGSIZE) == page2pa(pp2));
	assert(pp2->pp_ref == 1);

	// pp2 should NOT be on the free list
	// could happen in ref counts are handled sloppily in page_insert
	// 如果page_insert的实现中，不是先让ref增加，再page_remove，就可能使ref为1的物理页在page_remove的过程中被放入page_free_list。
	assert(!page_alloc(0));

	// check that pgdir_walk returns a pointer to the pte
	ptep = (pte_t *) KADDR(PTE_ADDR(kern_pgdir[PDX(PGSIZE)]));	// PT起始物理地址对应的虚拟地址
	assert(pgdir_walk(kern_pgdir, (void*)PGSIZE, 0) == ptep+PTX(PGSIZE));	// 等式右边：PTE的虚拟地址

	// should be able to change permissions too.
	assert(page_insert(kern_pgdir, pp2, (void*) PGSIZE, PTE_W|PTE_U) == 0);
	assert(check_va2pa(kern_pgdir, PGSIZE) == page2pa(pp2));
	assert(pp2->pp_ref == 1);
	assert(*pgdir_walk(kern_pgdir, (void*) PGSIZE, 0) & PTE_U);	// 检验PTE项的PTE_U位为1
	assert(kern_pgdir[0] & PTE_U);	// 检验PDE项的PTE_U位为1

	// should be able to remap with fewer permissions
	assert(page_insert(kern_pgdir, pp2, (void*) PGSIZE, PTE_W) == 0);
	assert(*pgdir_walk(kern_pgdir, (void*) PGSIZE, 0) & PTE_W);
	assert(!(*pgdir_walk(kern_pgdir, (void*) PGSIZE, 0) & PTE_U));

	// should not be able to map at PTSIZE because need free page for page table
	assert(page_insert(kern_pgdir, pp0, (void*) PTSIZE, PTE_W) < 0);

	// insert pp1 at PGSIZE (replacing pp2)
	assert(page_insert(kern_pgdir, pp1, (void*) PGSIZE, PTE_W) == 0);
	assert(!(*pgdir_walk(kern_pgdir, (void*) PGSIZE, 0) & PTE_U));

	// should have pp1 at both 0 and PGSIZE, pp2 nowhere, ...
	assert(check_va2pa(kern_pgdir, 0) == page2pa(pp1));
	assert(check_va2pa(kern_pgdir, PGSIZE) == page2pa(pp1));
	// ... and ref counts should reflect this
	assert(pp1->pp_ref == 2);
	assert(pp2->pp_ref == 0);

	// pp2 should be returned by page_alloc
	assert((pp = page_alloc(0)) && pp == pp2);

	// unmapping pp1 at 0 should keep pp1 at PGSIZE
	page_remove(kern_pgdir, 0x0);
	assert(check_va2pa(kern_pgdir, 0x0) == ~0);
	assert(check_va2pa(kern_pgdir, PGSIZE) == page2pa(pp1));
	assert(pp1->pp_ref == 1);
	assert(pp2->pp_ref == 0);

	// test re-inserting pp1 at PGSIZE
	assert(page_insert(kern_pgdir, pp1, (void*) PGSIZE, 0) == 0);
	assert(pp1->pp_ref);
	assert(pp1->pp_link == NULL);

	// unmapping pp1 at PGSIZE should free it
	page_remove(kern_pgdir, (void*) PGSIZE);
	assert(check_va2pa(kern_pgdir, 0x0) == ~0);
	assert(check_va2pa(kern_pgdir, PGSIZE) == ~0);
	assert(pp1->pp_ref == 0);
	assert(pp2->pp_ref == 0);

	// so it should be returned by page_alloc
	assert((pp = page_alloc(0)) && pp == pp1);

	// should be no free memory
	assert(!page_alloc(0));

	// forcibly take pp0 back
	// pp0是第0个PT
	assert(PTE_ADDR(kern_pgdir[0]) == page2pa(pp0));
	kern_pgdir[0] = 0;
	assert(pp0->pp_ref == 1);
	pp0->pp_ref = 0;

	// check pointer arithmetic in pgdir_walk
	page_free(pp0);
	va = (void*)(PGSIZE * NPDENTRIES + PGSIZE);
	ptep = pgdir_walk(kern_pgdir, va, 1);
	ptep1 = (pte_t *) KADDR(PTE_ADDR(kern_pgdir[PDX(va)]));	// PT起始物理地址对应的虚拟地址
	assert(ptep == ptep1 + PTX(va));	// 等式右边是指针算术，等式左边是我们实现的pgdir_walk的结果
	kern_pgdir[PDX(va)] = 0;
	pp0->pp_ref = 0;

	// check that new page tables get cleared
	memset(page2kva(pp0), 0xFF, PGSIZE);	// 用于测试page_walk对新分配的页表是否会清0
	page_free(pp0);
	pgdir_walk(kern_pgdir, 0x0, 1);	// 会分配物理页pp0作为第0号页表，且会清0
	ptep = (pte_t *) page2kva(pp0);
	for(i=0; i<NPTENTRIES; i++)
		assert((ptep[i] & PTE_P) == 0);	// 如果清0了，那么PTE_P位就是0而不是1
	kern_pgdir[0] = 0;
	pp0->pp_ref = 0;

	// give free list back
	page_free_list = fl;

	// free the pages we took
	page_free(pp0);
	page_free(pp1);
	page_free(pp2);

	cprintf("check_page() succeeded!\n");
}

// check page_insert, page_remove, &c, with an installed kern_pgdir
static void
check_page_installed_pgdir(void)
{
	struct PageInfo *pp, *pp0, *pp1, *pp2;
	struct PageInfo *fl;
	pte_t *ptep, *ptep1;
	uintptr_t va;
	int i;

	// check that we can read and write installed pages
	pp1 = pp2 = 0;
	assert((pp0 = page_alloc(0)));
	assert((pp1 = page_alloc(0)));
	assert((pp2 = page_alloc(0)));
	page_free(pp0);
	memset(page2kva(pp1), 1, PGSIZE);
	memset(page2kva(pp2), 2, PGSIZE);
	page_insert(kern_pgdir, pp1, (void*) PGSIZE, PTE_W);
	assert(pp1->pp_ref == 1);
	assert(*(uint32_t *)PGSIZE == 0x01010101U);
	page_insert(kern_pgdir, pp2, (void*) PGSIZE, PTE_W);
	assert(*(uint32_t *)PGSIZE == 0x02020202U);
	assert(pp2->pp_ref == 1);
	assert(pp1->pp_ref == 0);
	*(uint32_t *)PGSIZE = 0x03030303U;
	assert(*(uint32_t *)page2kva(pp2) == 0x03030303U);
	page_remove(kern_pgdir, (void*) PGSIZE);
	assert(pp2->pp_ref == 0);

	// forcibly take pp0 back
	assert(PTE_ADDR(kern_pgdir[0]) == page2pa(pp0));
	kern_pgdir[0] = 0;
	assert(pp0->pp_ref == 1);
	pp0->pp_ref = 0;

	// free the pages we took
	page_free(pp0);

	cprintf("check_page_installed_pgdir() succeeded!\n");
}
