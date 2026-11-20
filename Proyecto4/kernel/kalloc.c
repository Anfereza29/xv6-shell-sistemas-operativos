// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.
//
// Project 4: every physical page has a reference count, so that fork()
// can share it between several processes (copy-on-write).

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  uint64 nfree; // pages on freelist
} kmem;

// Number of physical pages between KERNBASE and PHYSTOP, and the slot
// of a physical address in the reference-count array.
#define NPAGES       ((PHYSTOP - KERNBASE) / PGSIZE)
#define PAGE_IDX(pa) (((uint64)(pa) - KERNBASE) / PGSIZE)

// Reference count of every physical page: how many page tables (or
// kernel uses) point to it. It has its own lock, which is never held
// together with kmem.lock, so neither critical section can block the
// other.
struct {
  struct spinlock lock;
  int refs[NPAGES];
  uint64 nshared; // pages with refs > 1
} kref;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&kref.lock, "kref");
  freerange(end, (void *)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE) {
    // These pages never went through kalloc(): give them the reference
    // that kfree() is about to drop. Only one CPU is running, so no lock.
    kref.refs[PAGE_IDX(p)] = 1;
    kfree(p);
  }
}

// Drop one reference to the page of physical memory pointed at by pa,
// which normally should have been returned by a call to kalloc().
// The page goes back to the free list only when nobody else
// references it.
void
kfree(void *pa)
{
  struct run *r;
  int remaining;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&kref.lock);
  if (kref.refs[PAGE_IDX(pa)] < 1)
    panic("kfree: no references");
  remaining = --kref.refs[PAGE_IDX(pa)];
  if (remaining == 1)
    kref.nshared--;
  release(&kref.lock);

  if (remaining > 0)
    return;

  // With the count at 0 nobody else can reach this page, so it is safe
  // to fill it with junk and link it without holding the count lock.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  kmem.nfree++;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
// The page starts with one reference.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if (r) {
    kmem.freelist = r->next;
    kmem.nfree--;
  }
  release(&kmem.lock);

  if (r) {
    acquire(&kref.lock);
    kref.refs[PAGE_IDX(r)] = 1;
    release(&kref.lock);
    memset((char *)r, 5, PGSIZE); // fill with junk
  }
  return (void *)r;
}

// Add a reference to an allocated page (one more page table starts
// pointing to it).
void
krefinc(void *pa)
{
  acquire(&kref.lock);
  if (kref.refs[PAGE_IDX(pa)] < 1)
    panic("krefinc");
  if (++kref.refs[PAGE_IDX(pa)] == 2)
    kref.nshared++;
  release(&kref.lock);
}

// Number of references to the page pa.
int
krefcount(void *pa)
{
  int n;

  acquire(&kref.lock);
  n = kref.refs[PAGE_IDX(pa)];
  release(&kref.lock);
  return n;
}

// Free physical pages in the whole system.
uint64
kfreepages(void)
{
  uint64 n;

  acquire(&kmem.lock);
  n = kmem.nfree;
  release(&kmem.lock);
  return n;
}

// Physical pages referenced from more than one page table.
uint64
ksharedpages(void)
{
  uint64 n;

  acquire(&kref.lock);
  n = kref.nshared;
  release(&kref.lock);
  return n;
}
