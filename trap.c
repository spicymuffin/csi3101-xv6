#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

// check context (are we running in user or kernel mode?)
void check_context(void) {
    ushort cs;
    asm volatile("mov %%cs, %0" : "=r" (cs));
    if ((cs & 0x3) == 0) {
        cprintf("[DBGMSG] running in kernel mode (cs=0x%x)\n", cs);
    } else {
        cprintf("[DBGMSG] running in user mode (cs=0x%x)\n", cs);
    }
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
//  case T_IRQ0 + IRQ_IDE2:
//	ide2intr();
//	lpaiceoi();
//	break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  case T_PGFLT:
    // a label can only be part of a statement and a declaration is not a statement
    ;
    struct proc* p = myproc();

    if(p == 0){
      panic("non-user PGFLT");
    }

    // get value of CR2 register (faulted virtual address)
    uint va = rcr2();

    // was the access a write?
    uint w = tf->err & 0x2; // mask off 1th bit

    #if DBGMSG_PAGEFAULT
    cprintf("[DBGMSG] pagefault: va: %x\n", va);
    #endif

    pte_t* pte;
    if((pte = get_pte(p, va)) != 0 && *pte & PTE_P){
      // if we are here, pte exists and is present
      // -> page fault was caused by a protection violation
      // or something.

      if(!(*pte & PTE_W) && w){
        #if DBGMSG_PAGEFAULT
        cprintf("[DBGMSG] pagefault: wrote to readonly\n");
        #endif
        p->killed = 1;
        lapiceoi();
        break;
      }
      else if(!(*pte & PTE_U)){
        #if DBGMSG_PAGEFAULT
        cprintf("[DBGMSG] pagefault: accessed non-user memory\n");
        #endif
        p->killed = 1;
        lapiceoi();
        break;
      }
      else{
        #if DBGMSG_PAGEFAULT
        cprintf("[DBGMSG] pagefault: unknown violation\n");
        #endif
        p->killed = 1;
        lapiceoi();
        break;
      }
    }

    // determine whether va belongs to a mmap
    int mmapindx = -1;
    for(int i = 0; i < p->nmmap; i++){
      if(p->mmaps[i].addr_low <= va && va < p->mmaps[i].addr_high){
        mmapindx = i;
        break;
      }
    }

    // found the mmap to which va belongs to, check correspondence with mmap policies
    if(mmapindx != -1){
      if(w && !(p->mmaps[mmapindx].flags & MAP_PROT_WRITE))
      {
        #if DBGMSG_PAGEFAULT
        cprintf("[DBGMSG] pagefault: tried write to mmap with no write permissions set\n");
        #endif
        p->killed = 1;
        lapiceoi();
        break;
      }
      if(!w && !(p->mmaps[mmapindx].flags & MAP_PROT_READ))
      {
        #if DBGMSG_PAGEFAULT
        cprintf("[DBGMSG] pagefault: tried write to mmap with no read permissions set\n");
        #endif
        p->killed = 1;
        lapiceoi();
        break;
      }
    }

    // if we are here then va is either:
    // 1. a valid mmap access
    // 2. a valid memory access which doesnt have a page behind it yet

    int statuscode = lazyalloc(p, va, mmapindx);
    if(statuscode < 0){
      #if DBGMSG_PAGEFAULT
      cprintf("[DBGMSG] pagefault: lazyalloc failed. status code: %d\n", statuscode);
      #endif
      p->killed = 1;
      lapiceoi();
      break;
    }

    lapiceoi();    
    break;

  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
