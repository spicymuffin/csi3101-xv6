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

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
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
sys_clone(void)
{
	char* stack;
	if(argptr(0, &stack, 4096) < 0 )
		return -1;
	return clone(stack);
}

int
sys_join(void)
{
	return join();
}

int
sys_mutex_lock(void)
{
	char* l;
	if ( argptr(0, &l, 4) < 0 )
		return -1;
	return mutex_lock((int*)l);
}

int
sys_mutex_unlock(void)
{
	char *l;
	if ( argptr(0, &l, 4) , 0)
		return -1;
	return mutex_unlock((int*)l);
}

void
sys_stackdump(void)
{
  stackdump();
  return;
}

void
sys_vmemlayout(void)
{
  vmemlayout();
  return;
}