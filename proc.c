#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

#include "eevdf.h"

int total_weight = 0;
int virtual_time = 0;

extern int sys_uptime(void);

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
  p->weight = 1;

  total_weight += p->weight;

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;
  eevdf_enqueue_process(p);

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
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
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  np->weight = curproc->weight;
  *np->tf = *curproc->tf;

  // np->request_tick = curproc->request_tick;??????? otherwise garbage value????
  total_weight += np->weight;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  eevdf_enqueue_process(np);
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

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
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

  // process is dead, modify total weight
  total_weight -= curproc->weight;

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

// note: scale before division

int compute_virtual_eligible(struct proc* p)
{
  // vtime_init is already scaled
  return p->virtual_time_init + (p->used_time * SCALE) / p->weight;
}

int compute_virtual_deadline(struct proc* p)
{
  // veligible is already scaled
  return p->virtual_eligible + (p->request_tick * SCALE) / p->weight;
}

int compute_lag(struct proc* p)
{
  // vtime and vtime_init are already scaled
  return ((p->weight) * (virtual_time - p->virtual_time_init) - (p->used_time * SCALE));
}

void update_virtual_time()
{
  if (total_weight > 0){
    if (total_weight >= SCALE)
      {
        // the spec says:
        // "If the total weight exceeds the SCALE value, it is fixed to 1."
        // i assume its talking about (SCALE/total_weight) becoming 1...?
        virtual_time += 1;
      }
      else
      {
        virtual_time += SCALE / total_weight;
      }
  }
}

void print_fake_float(int value)
{
    if (SCALE <= 0) return; // avoid division by zero

    if (value < 0) {
        cprintf("-");
        value = -value;
    }

    int int_part = value / SCALE;
    int frac_part = value % SCALE;

    cprintf("%d.", int_part);

    // pad with leading zeros based on scale
    int padding = SCALE / 10;
    while (padding > 1 && frac_part < padding) {
        cprintf("0");
        padding /= 10;
    }

    cprintf("%d", frac_part);
}

// hold ptable.lock
#define INT_MAX 0x7FFFFFFF // 0b01111111111111111111111111111111

struct proc* find_run_candidate()
{
  int lowest_virtual_deadline = INT_MAX;

  struct proc* candidate_proc = 0;

  #if DEBUG_STORE_LAG
  for (struct proc *p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    p->lag = compute_lag(p);
  }
  #endif

  // iterate over the ptable to find the candidate
  for (struct proc *p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    int lag = compute_lag(p);
    // If a process’s virtual eligible time is less than the current virtual time, the process is considered eligible, and its lag should be reset to 0
    if (p->virtual_eligible < virtual_time)
    {
      lag = 0;
    }

    // "Only RUNNABLE processes with Lag >= 0 are eligible for execution"
    if ((p->state == RUNNABLE && lag >= 0))
    {
      // find one with the smallest virtual deadline:
      // if they have the same virtual deadline, pick the one with the lowest PID
      if (p->virtual_deadline < lowest_virtual_deadline)
      {
        lowest_virtual_deadline = p->virtual_deadline;
        candidate_proc = p;
      }
      else if (p->virtual_deadline == lowest_virtual_deadline)
      {
        if (p->pid < candidate_proc->pid)
        {
          // no need to set lowest_virtual_deadline again
          candidate_proc = p;
        }
      }
      else
      {
        // this process has a higher virtual deadline than the current candidate
        // will be optimized by compiler
        continue;
      }
    }
  }

  // can be null if no process is found btw
  return candidate_proc;
}

void eevdf_enqueue_process(struct proc* p)
{
  p->virtual_time_init = virtual_time;
  p->used_time = 0;
  p->virtual_eligible = compute_virtual_eligible(p); // = virtual_time_init bc used_time = 0
  p->virtual_deadline = compute_virtual_deadline(p); // = virtual_eligible + request_tick / weight
}

void eevdf_update_proc(struct proc* p)
{
  p->virtual_eligible = compute_virtual_eligible(p);
  p->virtual_deadline = compute_virtual_deadline(p);
}

void print_scheduler_metadata()
{
  cprintf("---------GLOBAL SCHED METADATA---------\n");
  cprintf("TOTAL WEIGHT: %d\n", total_weight);
  cprintf("VIRTUAL TIME: ");
  print_fake_float(virtual_time);
  cprintf("\n");
  cprintf("SYS UPTIME: %d\n", sys_uptime());
  cprintf("---------------------------------------\n");
  for (struct proc *p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->state == RUNNABLE)
    {
      cprintf("PID: %d\n", p->pid);
      cprintf("NAME: %s\n", p->name);
      cprintf("VELIGIBLE: ");
      print_fake_float(p->virtual_eligible);
      cprintf("\n");
      cprintf("VTIME INIT: ");
      print_fake_float(p->virtual_time_init);
      cprintf("\n");
      cprintf("VDEADLINE: ");
      print_fake_float(p->virtual_deadline);
      cprintf("\n");
      cprintf("LAG: ");
      print_fake_float(compute_lag(p));
      cprintf("\n");
      cprintf("USED TIME: %d\n", p->used_time);
      cprintf("REQUEST TICK: %d\n", p->request_tick);
      cprintf("WEIGHT: %d\n", p->weight);
      cprintf("---------------------------------------\n");
    }
  }
}

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

    // find candidate process
    p = find_run_candidate();

    if (p == 0)
    {
      // no process found, release lock and spin some more
      release(&ptable.lock);
      continue;
    }

    // print_scheduler_metadata();

    // volatile int i;
    // for(i = 0; i < 100000000; i++) {} // spin for a bit
    // for(i = 0; i < 100000000; i++) {} // spin for a bit
    // for(i = 0; i < 100000000; i++) {} // spin for a bit
    // for(i = 0; i < 100000000; i++) {} // spin for a bit
    // for(i = 0; i < 100000000; i++) {} // spin for a bit

    // Switch to chosen process.  It is the process's job
    // to release ptable.lock and then reacquire it
    // before jumping back to us.
    c->proc = p;
    switchuvm(p);
    p->state = RUNNING;

    swtch(&(c->scheduler), p->context);

    // increment used_time by QUANTUM
    p->used_time += QUANTUM;

    // update the virtual params of the process
    eevdf_update_proc(p);
    print_scheduler_metadata();

    // process used one tick NO YOU RETARD
    // p->used_time += 1;

    switchkvm();

    // Process is done running for now.
    // It should have changed its p->state before coming back.
    c->proc = 0;

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

  // modify total weight by weight of the process that is going to sleep
  // this is because the process is not runnable anymore
  total_weight -= p->weight;

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
    {
      p->state = RUNNABLE;

      // woken up, modify total weight back
      total_weight += p->weight;

      // state change to RUNNABLE -> enqueue process
      eevdf_enqueue_process(p);
    }
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
      {
        p->state = RUNNABLE;
        // woken up, modify total weight back
        total_weight += p->weight;
        // state change to RUNNABLE -> enqueue process
        eevdf_enqueue_process(p);
      }
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
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
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}


int sched_setattr(int request_tick, int weight)
{
  struct proc *p;
  p = myproc();

  if (p == 0)
    return -1;
  if (request_tick <= 0)
    return -1;

  if(weight < 1) weight = 1;
  if(weight > 5) weight = 5;

  // added because of im worried of racing with timer ticks
  acquire(&ptable.lock);

  // sanity checks first, then update total weight
  total_weight += weight - p->weight;

  p->request_tick = request_tick;
  p->weight = weight;

  eevdf_enqueue_process(p);

  release(&ptable.lock);

  // Hint: When implementing the EEVDF scheduler, total weight needs to be updated here.
  return 0;
}

int sched_getattr(int *request_tick, int *weight)
{

  struct proc *p;
  p = myproc();

  if(p == 0)
    return -1;
  if(p->request_tick <= 0 || p->weight < 1 || p->weight > 5)
    return -1;

  *request_tick = p->request_tick;
  *weight = p->weight;

  return 0;
}
