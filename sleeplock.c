// Sleeping locks

#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"

void
initsleeplock(struct sleeplock *lk, char *name)
{
  initlock(&lk->lk, "sleep lock");
  lk->name = name;
  lk->locked = 0;
  lk->pid = 0;
}

void
acquiresleep(struct sleeplock *lk)
{
  acquire(&lk->lk);
  while (lk->locked) {
    sleep(lk, &lk->lk);
  }
  lk->locked = 1;
  lk->pid = myproc()->pid;
  release(&lk->lk);
}

void
releasesleep(struct sleeplock *lk)
{
  acquire(&lk->lk);
  lk->locked = 0;
  lk->pid = 0;
  wakeup(lk);
  release(&lk->lk);
}

int
holdingsleep(struct sleeplock *lk)
{
  int r;
  
  acquire(&lk->lk);
  r = lk->locked && (lk->pid == myproc()->pid);
  release(&lk->lk);
  return r;
}

// Locks descriptions

// Lock                    Description
// bcache.lock             Protects allocation of block buffer cache entries
// cons.lock               Serializes access to console hardware, avoids intermixed output
// ftable.lock             Serializes allocation of a struct file in file table
// icache.lock             Protects allocation of inode cache entries
// idelock                 Serializes access to disk hardware and disk queue
// kmem.lock               Serializes allocation of memory
// log.lock                Serializes operations on the transaction log
// pipe’s p->lock          Serializes operations on each pipe
// ptable.lock             Serializes context switching, and operations on proc->state and proctable
// tickslock               Serializes operations on the ticks counter
// inode’s ip->lock        Serializes operations on each inode and its content
// buf’s b->lock           Serializes operations on each block buffer

// sleeplock's mutexlock   Protects the switch to sleeping state of the mutex locked process

struct spinlock mutexlk;

int
mutex_lock(volatile int *l)
{
  acquire(&mutexlk);
  while(xchg((uint*)l, 1) != 0){
    sleep((void*)l, &mutexlk);
  }
  release(&mutexlk);
  return 0; 
}

int
mutex_unlock(volatile int *l)
{
  acquire(&mutexlk);
  wakeup((void*)l);
  *l = 0;
  release(&mutexlk);
  return 0;
}

// struct spinlock printlk;

// int
// print_mutex_lock(volatile int *l)
// {
//   acquire(&printlk);
//   while(xchg((uint*)l, 1) != 0){
//     sleep((void*)l, &printlk);
//   }
//   release(&printlk);
//   return 0; 
// }

// int
// print_mutex_unlock(volatile int *l)
// {
//   acquire(&printlk);
//   while(xchg((uint*)l, 1) != 0){
//     sleep((void*)l, &printlk);
//   }
//   release(&printlk);
//   return 0; 
// }