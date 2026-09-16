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

#define NPAGE ((PHYSTOP - KERNBASE) / PGSIZE)

struct
{
    struct spinlock lock;
    int count[NPAGE];
} refcnt;  // 全局变量，统计每个物理页面有多少引用

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&refcnt.lock, "refcnt");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
  {
      acquire(&refcnt.lock);
      refcnt.count[pa_index((uint64)p)] = 1;
      release(&refcnt.lock);
      kfree(p);
  }
}

int
pa_index(uint64 pa)
{
    return (pa - KERNBASE) / PGSIZE;
}

void
krefinc(uint64 pa)
{
    acquire(&refcnt.lock);
    refcnt.count[pa_index(pa)]++;
    release(&refcnt.lock);
} // refcnt 递增函数

int
krefdec(uint64 pa)
{
    int n;
    acquire(&refcnt.lock);
    n = --refcnt.count[pa_index(pa)];
    release(&refcnt.lock);
    return n;
} // refcnt 递减函数
  
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{

  struct run *r;
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");  
  if (krefdec((uint64)pa) == 0) // 当引用计数降为0时才正常 free
  {
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }      
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if (r)
  {
      kmem.freelist = r->next;
      acquire(&refcnt.lock); // 加锁，防止多个进程同时修改 refcnt
      refcnt.count[pa_index((uint64)r)] = 1;
      release(&refcnt.lock);
  }
    
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
