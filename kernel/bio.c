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

#define NBUCKET 13

struct
{
  struct spinlock stlck;          // Steal lock
  struct spinlock locks[NBUCKET]; // Hash bucket locks
  struct buf buf[NBUF];

  // Hash table of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf heads[NBUCKET];
} bcache;

int hash(int n)
{
  return n % NBUCKET;
}

void remove(struct buf *b)
{
  b->next->prev = b->prev;
  b->prev->next = b->next;
}

void add2head(struct buf *head, struct buf *b)
{
  b->next = head->next;
  b->prev = head;
  head->next->prev = b;
  head->next = b;
}

void add2tail(struct buf *head, struct buf *b)
{
  b->prev = head->prev;
  b->next = head;
  head->prev->next = b;
  head->prev = b;
}

void move2head(struct buf *head, struct buf *b)
{
  remove(b);
  add2head(head, b);
}

void move2tail(struct buf *head, struct buf *b)
{
  remove(b);
  add2tail(head, b);
}

void binit(void)
{
  initlock(&bcache.stlck, "bcache steal");

  for (int i = 0; i < NBUCKET; i++)
  {
    initlock(&bcache.locks[i], "bcache bucket");
    struct buf *head = &bcache.heads[i];
    head->prev = head;
    head->next = head;
  }

  // Create linked list of buffers
  for (int i = 0; i < NBUF; i++)
  {
    struct buf *b = &bcache.buf[i];
    add2head(&bcache.heads[hash(i)], b);
    initsleeplock(&b->lock, "buffer");
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *
bget(uint dev, uint blockno)
{
  int index = hash(blockno);
  acquire(&bcache.locks[index]);
  struct buf *head = &bcache.heads[index];
  // Is the block already cached?
  for (struct buf *b = head->next; b != head; b = b->next)
  {
    if (b->dev == dev && b->blockno == blockno)
    {
      b->refcnt++;
      move2head(head, b);
      release(&bcache.locks[index]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for (struct buf *b = head->prev; b != head; b = b->prev)
  {
    if (b->refcnt == 0)
    {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      move2head(head, b);
      release(&bcache.locks[index]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  acquire(&bcache.stlck);
  // Steal a cache from other bucket.
  for (int i = (index + 1) % NBUCKET; i != index; i = (i + 1) % NBUCKET)
  {
    acquire(&bcache.locks[i]);
    for (struct buf *b = bcache.heads[i].prev; b != &bcache.heads[i]; b = b->prev)
    {
      if (b->refcnt == 0)
      {
        release(&bcache.stlck);
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        remove(b);
        release(&bcache.locks[i]);
        add2head(head, b);
        release(&bcache.locks[index]);
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.locks[i]);
  }

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf *
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid)
  {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
  int index = hash(b->blockno);
  acquire(&bcache.locks[index]);
  move2head(&bcache.heads[index], b);
  release(&bcache.locks[index]);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int index = hash(b->blockno);
  acquire(&bcache.locks[index]);
  b->refcnt--;
  if (b->refcnt == 0)
    move2tail(&bcache.heads[index], b);

  release(&bcache.locks[index]);
}

void bpin(struct buf *b)
{
  int index = hash(b->blockno);
  acquire(&bcache.locks[index]);
  b->refcnt++;
  release(&bcache.locks[index]);
}

void bunpin(struct buf *b)
{
  int index = hash(b->blockno);
  acquire(&bcache.locks[index]);
  b->refcnt--;
  release(&bcache.locks[index]);
}
