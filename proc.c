#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

void
acquire_ptable_lock(void)
{
  acquire(&ptable.lock);
}

void
release_ptable_lock(void)
{
  release(&ptable.lock);
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  cprintf("%p %p\n", _binary_initcode_start, _binary_initcode_size);
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S
  p->nice = 2;

  p->nmmap = 0;

  for(int i = 0; i < NPROCMMAP; i++){
    p->mmaps[i].addr_high = -1;
    p->mmaps[i].addr_low = -1;
    p->mmaps[i].fd = 0;
    p->mmaps[i].flags = -1;
    p->mmaps[i].size = -1;
    p->mmaps[i].offset = -1;
  }

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.

// allocates on call of sys_sbrk
// change it so just marks PTEs as invalid
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  // mmaps are not copied bc we copy from 0 to sz
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    // kfree the mmaps of parent but in child
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  } 
  np->sz = curproc->sz;
  np->parent = curproc;
  np->nice = curproc->nice;
  np->nmmap = 0; // we dont inherit mmaps

  // inherit execution data
  np->ed.bdtbound = curproc->ed.bdtbound;
  np->ed.elf = curproc->ed.elf;
  np->ed.ip = curproc->ed.ip;
  
  // reset maps
  for(int i = 0; i < NPROCMMAP; i++){
    np->mmaps[i].addr_high = -1;
    np->mmaps[i].addr_low = -1;
    np->mmaps[i].fd = 0;
    np->mmaps[i].flags = -1;
    np->mmaps[i].size = -1;
    np->mmaps[i].offset = -1;
  }

  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  #if DBGMSG_EXIT
  cprintf("[DBGMSG] sys_exit: cycling through process's files\n");
  #endif

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      struct file *f = curproc->ofile[fd];

      // cycle through curproc->ptrs to see if the file was mmaped
      for(int i = 0; i < curproc->nmmap; i++){
        #if DBGMSG_EXIT
        cprintf("[DBGMSG] sys_exit: comparing with\n", curproc->nmmap);
        cprintf("         | compared file properties:\n");
        cprintf("         | size: %d\n", f->ip->size);
        cprintf("         | inum: %d\n", f->ip->inum);
        #endif

        // inode number is unique; use it to test file identicality
        if(curproc->ptrs[i]->fd->ip->inum == f->ip->inum){
          #if DBGMSG_EXIT
          cprintf("[DBGMSG] sys_exit: found mmap of closing file\n");
          #endif
          void* addr = (void*)curproc->ptrs[i]->addr_low;
          int size = curproc->ptrs[i]->size;
          if(munmap(addr, size) < 0){
            #if DBGMSG_EXIT
            cprintf("[DBGMSG] sys_exit: munmap failed\n");
            #endif
            return;
          }

          #if DBGMSG_EXIT
          cprintf("[DBGMSG] sys_exit: munmap successful\n");
          #endif
        }
      }
      
      #if DBGMSG_EXIT
      cprintf("[DBGMSG] sys_exit: closing file\n");
      cprintf("         | file properties:\n");
      cprintf("         | size:      %d\n", f->ip->size);
      cprintf("         | inum:      %d\n", f->ip->inum);
      cprintf("         | ofileindx: %d\n", fd);
      #endif

      // this also takes care of iputting the executable
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }


  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

int 
nice(int value)
{
	int new_nice;
	struct proc *p = myproc();
	acquire(&ptable.lock);
	new_nice = p->nice + value;
	if ( new_nice > 4 )
		new_nice = 4;
	else if (new_nice < -5 )
		new_nice = -5;
	p->nice = new_nice;
	release(&ptable.lock);

	return p->nice;
}

void 
get_padded_hex_addr(uint addr, char *buf)
{
  char map[16] = {
      '0',
      '1',
      '2',
      '3',
      '4',
      '5',
      '6',
      '7',
      '8',
      '9',
      'A',
      'B',
      'C',
      'D',
      'E',
      'F',
  };
  buf[0] = '0';
  buf[1] = 'x';
  buf[10] = '\0';
  for (int i = 9; i >= 2; i--)
  {
    buf[i] = map[addr % 16];
    addr = addr / 16;
  }
}


int
vmemlayout(void)
{
  struct proc* p = myproc();
  cprintf("<%s's vmemlayout>\n", p->name);
  char tmp[11]; // 0x--------\0
  cprintf("stat:\n");
  get_padded_hex_addr(KERNBASE, tmp);
  cprintf(" kernbase: %s\n", tmp);
  get_padded_hex_addr(p->sz, tmp);
  cprintf(" proc->sz: %s\n", tmp);
  get_padded_hex_addr(p->tf->esp, tmp);
  cprintf(" proc->sp: %s\n", tmp);

  cprintf("layout:\n");
  cprintf("+-----------------------+\n");
  cprintf("|       KERNBASE        |\n");
  cprintf("+-----------------------+\n");
  int unalloc_pg_flag = 0;
  int alloc_pg_flag = 0;
  pte_t* pgdir = p->pgdir;
  for(int i = PDX(KERNBASE) - 1; i >= 0; i--){
    pte_t* pde = &pgdir[i]; // [] opearator has precedence
    
    if(!(*pde & PTE_P)){
      // unallocated pgtab
      alloc_pg_flag = 0;
      unalloc_pg_flag = 0;
      continue;
    }

    pte_t* pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
    for(int j = 1023; j >= 0; j--){
      pte_t* pte = &pgtab[j];
      if(!(*pte & PTE_P)){
        // page not allocated
        if(unalloc_pg_flag == 0)
        {
          if(alloc_pg_flag == 2){
            get_padded_hex_addr((i << 22) | (j << 12) | 4095, tmp);
            cprintf("| %s~", tmp);
            get_padded_hex_addr((i << 22) | (j << 12) | 0, tmp);
            cprintf("%s |\n", tmp);
          }
          cprintf("|   page__unallocated   |\n");
          unalloc_pg_flag = 1;
          alloc_pg_flag = 0;
        }
        else if(unalloc_pg_flag == 1){
          cprintf("|          ...          |\n");
          unalloc_pg_flag = 2;
        }
      }
      else{
        // page allocated
        if(alloc_pg_flag == 0){
          if(unalloc_pg_flag == 2){
            cprintf("|   page__unallocated   |\n");
          }
          get_padded_hex_addr((i << 22) | (j << 12) | 4095, tmp);
          cprintf("| %s~", tmp);
          get_padded_hex_addr((i << 22) | (j << 12) | 0, tmp);
          cprintf("%s |\n", tmp);
          alloc_pg_flag = 1;
          unalloc_pg_flag = 0;
        }
        else if(alloc_pg_flag == 1){
          cprintf("|          ...          |\n");
          alloc_pg_flag = 2;
        }
      }
    }
    if(alloc_pg_flag == 2){
      get_padded_hex_addr((0 << 22) | (0 << 12) | 4095, tmp);
      cprintf("| %s~", tmp);
      get_padded_hex_addr((0 << 22) | (0 << 12) | 0, tmp);
      cprintf ("%s |\n", tmp);
      alloc_pg_flag = 0;
    }
    if(unalloc_pg_flag == 2){
      cprintf("|   page__unallocated   |\n", tmp);
      unalloc_pg_flag = 0;
    }
  }
  cprintf("+-----------------------+\n");
  return 0;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %d %s %s", p->pid, p->nice, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
