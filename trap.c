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
    ; // yes, this is needed to avoid a syntax error
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
    if((pte = get_pte(p, va)) != 0){
      // if we are here, pte exists and is present
      // -> page fault was caused by a protection violation
      // or something....
      if(*pte & PTE_COW){
        #if DBGMSG_PAGEFAULT
        cprintf("[DBGMSG] pagefault: denied write access on COW page\n");
        #endif

        // separate the two pages into separate copies
        // first alloc a new frame
        char* mem = kalloc();
        if(mem == 0){
          // out of memory
          #if DBGMSG_PAGEFAULT
          cprintf("[DBGMSG] pagefault: out of memory on COW\n");
          #endif

          p->killed = 1;
          lapiceoi();
          break;
        }

        // align the faulting address to the page size
        uint faulting_page_addr = PGROUNDDOWN(va);

        // copy the faulting page over to the new page
        memmove(mem, (char*)faulting_page_addr, PGSIZE);

        // get the physical address of the page that caused the fault
        uint pa = PTE_ADDR(*pte);

        // try to free it: if refcount is 0 its going to be actually freed
        // otherwise refcnt will be decremented
        kfree((char*)P2V(pa));

        // set this process's PTE to point to V2P(mem)
        // and set the PTE_W bit because it is a private copy (for now)
        // unset the PTE_COW bit in this process's PTE as well
        // copy existing flags to kinda preserve them
        // also set the PTE_W bit cuz we wouldnt have unset it if the page was readonly
        uint newflags = (PTE_FLAGS(*pte) | PTE_W) & ~PTE_COW;
        *pte = V2P(mem) | newflags;

        // flush the TLB bc we modified the page table
        flush_tlb();
        lapiceoi();
        break;
      }
      else if (!(*pte & PTE_P)){
        #if DBGMSG_PAGEFAULT
        cprintf("[DBGMSG] pagefault: page not present\n");
        #endif
        p->killed = 1;
        lapiceoi();
        break;
      }
      else if(!(*pte & PTE_W) && w){
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
