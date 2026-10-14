// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

int hash(uint blockno)
{
    return blockno % NBUCKET;
}

struct bucket
{
    struct spinlock lock;
    struct buf head;
};

struct {
  struct buf buf[NBUF];
  struct bucket bucket[NBUCKET];
  struct spinlock main_lock;
} bcache;

void
binit(void)
{
  initlock(&bcache.main_lock, "bcache.main");
  for (int i = 0; i < NBUCKET; i++)
  {
      initlock(&bcache.bucket[i].lock, "bcache.bucket");
      // Create linked list of buffers
      bcache.bucket[i].head.prev = &bcache.bucket[i].head;
      bcache.bucket[i].head.next = &bcache.bucket[i].head;
  }

  for (int i = 0; i < NBUF; i++)
  {
      struct buf *b = &bcache.buf[i];
      initsleeplock(&b->lock, "buffer");
      b->refcnt = 0;
      b->lastuse = 0;
      b->valid = 0;
      b->disk = 0;

      b->dev = (uint)-1;

      b->blockno = i;

      int h = hash(b->blockno);

      // 初始化对应的 hash-table
      struct buf *head = &bcache.bucket[h].head;

      b->next = head->next;
      b->prev = head;
      head->next->prev = b;
      head->next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int h = hash(blockno);

  acquire(&bcache.bucket[h].lock);

  // Is the block already cached in all buckets?
  for (b = bcache.bucket[h].head.next; b != &bcache.bucket[h].head; b = b->next)
  {
      if (b->dev == dev && b->blockno == blockno)
      {
          b->refcnt++;
          release(&bcache.bucket[h].lock);
          acquiresleep(&b->lock);
          return b;
      }
  }
  release(&bcache.bucket[h].lock);

  // cache miss
  // 依然需要检查 bucket[h] 防止其他进程修改
  acquire(&bcache.main_lock);
  acquire(&bcache.bucket[h].lock);
  for (b = bcache.bucket[h].head.next; b != &bcache.bucket[h].head; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.bucket[h].lock);
      release(&bcache.main_lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bucket[h].lock);

  // 找到最晚使用的
  for (;;)
  {
    struct buf *victim = 0;
    uint64 oldest = ~0ULL;
    int source = -1;

    for (int i = 0; i < NBUCKET; i++) {
      acquire(&bcache.bucket[i].lock);
      for (b = bcache.bucket[i].head.next; b != &bcache.bucket[i].head; b = b->next)
      {
            if (b->refcnt == 0 && (victim == 0 || b->lastuse < oldest))
            {
                victim = b;
                oldest = b->lastuse;
                source = i;
            }
      }
      release(&bcache.bucket[i].lock);
    }
    if (victim == 0)
      panic("bget: no buffers");

    acquire(&bcache.bucket[source].lock);
    // 在扫描期间可能又产生 refcnt
    if (victim->refcnt != 0) {
      release(&bcache.bucket[source].lock);
      continue;
    }
    b = victim;
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->refcnt = 1;
    release(&bcache.bucket[source].lock);

    acquire(&bcache.bucket[h].lock);
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->next = bcache.bucket[h].head.next;
    b->prev = &bcache.bucket[h].head;
    b->next->prev = b;
    b->prev->next = b;
    release(&bcache.bucket[h].lock);
    release(&bcache.main_lock);
    acquiresleep(&b->lock);
    return b;
  }
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Record when the buffer becomes unused for LRU replacement.
void
brelse(struct buf *b)
{
    if(!holdingsleep(&b->lock))
        panic("brelse");

    releasesleep(&b->lock);

    int h = hash(b->blockno);

    acquire(&tickslock);
    uint now = ticks;
    release(&tickslock);

    acquire(&bcache.bucket[h].lock);
    b->refcnt--;
    if (b->refcnt == 0)
        b->lastuse = now;

    release(&bcache.bucket[h].lock);
}

void bpin(struct buf *b)
{
    int h = hash(b->blockno);
    acquire(&bcache.bucket[h].lock);
    b->refcnt++;
    release(&bcache.bucket[h].lock);
}

void
bunpin(struct buf *b) {
    int h = hash(b->blockno);

    acquire(&tickslock);
    uint now = ticks;
    release(&tickslock);

    acquire(&bcache.bucket[h].lock);
    b->refcnt--;
    if (b->refcnt == 0)
        b->lastuse = now;
    release(&bcache.bucket[h].lock);
}


