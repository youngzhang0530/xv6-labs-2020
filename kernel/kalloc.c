// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run
{
  struct run *next;
};

struct
{
  struct spinlock lock;
  struct run *freelist;
} kmems[NCPU];

void kinit()
{
  char buffer[8];
  for (int i = 0; i < NCPU; i++)
  {
    snprintf(buffer, 7, "kmem %d", i);
    initlock(&kmems[i].lock, buffer);
  }
  freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa)
{
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

  push_off();
  int cid = cpuid();
  pop_off();
  acquire(&kmems[cid].lock);
  r->next = kmems[cid].freelist;
  kmems[cid].freelist = r;
  release(&kmems[cid].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  push_off();
  int cid = cpuid();
  pop_off();
  acquire(&kmems[cid].lock);
  r = kmems[cid].freelist;
  if (r)
    kmems[cid].freelist = r->next;
  release(&kmems[cid].lock);

  if (!r)
  {
    for (int i = 0; i < NCPU; i++)
    {
      if (i == cid)
        continue;
      acquire(&kmems[i].lock);
      r = kmems[i].freelist;
      if (r)
      {
        kmems[i].freelist = r->next;
        release(&kmems[i].lock);
        break;
      }
      release(&kmems[i].lock);
    }
  }

  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  return (void *)r;
}
