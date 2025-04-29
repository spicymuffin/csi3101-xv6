// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

int kfree_printflag = 0;
int kalloc_printflag = 0;

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

static int frees = 0;

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  int use_lock;
  struct run *freelist;
} kmem;

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  kmem.use_lock = 0;
  freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);
  kmem.use_lock = 1;
}

void
freerange(void *vstart, void *vend)
{
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE)
    kfree(p);
}

//PAGEBREAK: 21
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  struct run *r;

  if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP) panic("kfree");

  if(kmem.use_lock) acquire(&kmem.lock);

  r = (struct run*)v;

  #if DBGMSG_KFREE
  if (kfree_printflag) {
    cprintf("[DBGMSG] kfree: attempting freeing page %p\n", r);
    cprintf("         | refcnt before: %d\n", getrefcnt(V2P(r)));
    cprintf("         | refcnt after: %d\n", getrefcnt(V2P(r)) - 1);
  }
  #endif

  // decrement reference count for the frame
  decref(V2P(r));

  // if the reference count is zero then we can free the page
  // meaning we can add it to the free list
  if (getrefcnt(V2P(r)) > 0) {
    if(kmem.use_lock) release(&kmem.lock);
    return;
  }

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  // insert into beginning of free list
  // note: order of free list gets scrambled
  r->next = kmem.freelist;
  kmem.freelist = r;
  frees++;

  #if DBGMSG_KFREE
  if (kfree_printflag) {
    cprintf("         | refcnt=0, freeing page %p\n", r);
  }
  #endif

  if(kmem.use_lock) release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  struct run *r;

  if(kmem.use_lock) acquire(&kmem.lock);
  r = kmem.freelist;

  #if DBGMSG_KALLOC
  if (kalloc_printflag) {
    if (r) {
      cprintf("[DBGMSG] kalloc: allocating page %p\n", r);
      cprintf("         | refcnt before: %d\n", getrefcnt(V2P(r)));
      cprintf("         | refcnt after: %d\n", getrefcnt(V2P(r)) + 1);
    } else {
      cprintf("[DBGMSG] kalloc: no free pages available\n");
    }
  }
  #endif

  if(r) {
    kmem.freelist = r->next;
	  frees--;
  }

  if(kmem.use_lock) release(&kmem.lock);

  // increment reference count for the frame
  // if the frame is not null
  if (r) incref(V2P(r));
  return (char*)r;
}

int sys_frees(void)
{
	return frees;
}
