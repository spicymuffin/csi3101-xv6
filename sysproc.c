#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;
  struct proc* p = myproc();

  if(argint(0, &n) < 0)
    return -1;

  addr = myproc()->sz;
  myproc()->sz = addr + n;

#if DBGMSG_SBRK
  cprintf("[DBGMSG] sbrk: added %d to sz\n", n);
#endif

  // dealloc memory if we are shrinking proc
  if(n < 0){
    p->sz = deallocuvm(p->pgdir, p->sz, p->sz + n);
  }
  
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int
sys_yield(void)
{
	yield();
	return 0;
}

int
sys_nice(void)
{
	int value;
	if ( argint(0, &value) < 0 )
		return -1;
	return nice(value);
}

int
sys_vmemlayout(void)
{
  vmemlayout();
  return 0;
}
