#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
// #include "elf.h"

#if DBGMSG_EXEC
  char tmp1[11];
  char tmp2[11];
#endif

int
max(uint a, uint b)
{
  if(a >= b){
    return a;
  }
  return b;
}

int
min(uint a, uint b)
{
  if(a <= b){
    return a;
  }
  return b;
}

int
load_executable(struct proc* p, struct inode *ip, uint va)
{
  // initialize
  begin_op();
  ilock(ip);

  int i, off;

  struct elfhdr elf = p->ed.elf;
  struct proghdr ph;
  pde_t *pgdir = p->pgdir;
  
  #if DMGMSG_LOAD_EXECTBL
  char tmp[11];
  get_padded_hex_addr(va, tmp);
  cprintf("[DBGMSG] load_executable: scanning segments for faulting addr va=%s\n", tmp);
  #endif
  
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    // the page required to store the data was already allocated in lazyalloc()
    // if((flt_pg_addr_low = allocuvm(pgdir, flt_pg_addr_low, ph.vaddr + ph.memsz)) == 0)
    //   goto bad;
    if(ph.vaddr % PGSIZE != 0)
      goto bad;


    uint flt_pg_addr_low = PGROUNDDOWN(va);
    uint flt_pg_addr_high = flt_pg_addr_low + PGSIZE;

    uint seg_start = ph.vaddr;
    uint seg_end = ph.vaddr + ph.memsz;
    uint seg_fileend = ph.vaddr + ph.filesz;

    #if DMGMSG_LOAD_EXECTBL
    cprintf("[DBGMSG] load_executable: segment no. %d bounds\n", i);
    get_padded_hex_addr(flt_pg_addr_low, tmp);
    cprintf("         | flt_pg_addr_low:  %s\n", tmp);
    get_padded_hex_addr(flt_pg_addr_high, tmp);
    cprintf("         | flt_pg_addr_high: %s\n", tmp);
    get_padded_hex_addr(seg_start, tmp);
    cprintf("         | seg_start:        %s\n", tmp);
    get_padded_hex_addr(seg_end, tmp);
    cprintf("         | seg_end:          %s\n", tmp);
    get_padded_hex_addr(seg_fileend, tmp);
    cprintf("         | seg_fileend:      %s\n", tmp);
    #endif

    if(!(flt_pg_addr_low >= seg_end || flt_pg_addr_high < seg_start)){
      uint load_start = max(flt_pg_addr_low, seg_start);
      uint load_end = min(flt_pg_addr_high, seg_fileend);
      uint load_size = load_end - load_start;
      uint file_offset = ph.off + (load_start - seg_start);

      #if DMGMSG_LOAD_EXECTBL
      cprintf("[DBGMSG] load_executable: loading segment no. %d\n", i);
      get_padded_hex_addr(load_start, tmp);
      cprintf("         | load_start:  %s\n", tmp);
      get_padded_hex_addr(load_end, tmp);
      cprintf("         | load_end:    %s\n", tmp);
      get_padded_hex_addr(load_size, tmp);
      cprintf("         | load_size:   %s\n", tmp);
      get_padded_hex_addr(file_offset, tmp);
      cprintf("         | file_offset: %s\n", tmp);
      #endif


      if (loaduvm(pgdir, (char*)load_start, ip, file_offset, load_size) < 0)
        goto bad;
    }
  }

  // cleanup
  iunlock(ip);
  end_op();
  return 0;

 bad:
  // if bad cleanup here, pagefault handler will just kill the process
  if(pgdir)
    freevm(pgdir);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -1;
}

int
exec(char *path, char **argv)
{
  char *s, *last;
  int i, off;
  uint argc, sz, sp, ustack[3+MAXARG+1];
  struct inode *ip;
  struct proghdr ph;
  pde_t *pgdir, *oldpgdir;
  struct proc *curproc = myproc();

  begin_op();

  if((ip = namei(path)) == 0){
    end_op();
    cprintf("exec: fail\n");
    return -1;
  }

  ilock(ip);
  pgdir = 0;

  // Check ELF header
  if(readi(ip, (char*)&(curproc->ed.elf), 0, sizeof((curproc->ed.elf))) != sizeof((curproc->ed.elf))){
    #if DBGMSG_EXEC
    cprintf("[DBGMSG] exec: bad elf header (1)\n");
    #endif
    goto bad;
  }
  if((curproc->ed.elf).magic != ELF_MAGIC){
    #if DBGMSG_EXEC
    cprintf("[DBGMSG] exec: bad elf header (2)\n");
    #endif
    goto bad;
  }
  if((pgdir = setupkvm()) == 0){
    #if DBGMSG_EXEC
    cprintf("[DBGMSG] exec: setupkvm() fail\n");
    #endif
    goto bad;
  }


  // do a bunch of checks
  sz = 0;
  for(i=0, off=(curproc->ed.elf).phoff; i<(curproc->ed.elf).phnum; i++, off+=sizeof(ph)){
    if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    
    // we don't allocate
    // if((sz = allocuvm(pgdir, sz, ph.vaddr + ph.memsz)) == 0)
    //   goto bad;

    // emulate allocuvm behavior to set sz correctly
    // else and else ifs to emulate "return behavior" (control flow i mean)
    if(ph.vaddr + ph.memsz >= KERNBASE){
      sz = 0;
      goto bad;
    }
    else if(ph.vaddr + ph.memsz < sz){
      sz = sz; // uhhh lol
      goto bad;
    }
    else{
      sz = ph.vaddr + ph.memsz;
    }

    if(ph.vaddr % PGSIZE != 0)
      goto bad;

    // we don't load
    // if(loaduvm(pgdir, (char*)ph.vaddr, ip, ph.off, ph.filesz) < 0)
    //   goto bad;
  }

  // record data for later use
  curproc->ed.ip = ip;
  curproc->ed.bdtbound = sz;

  // we still need to unlock stuff bc of the elf checking
  iunlock(ip);
  end_op();
  ip = 0;

  // Allocate two pages at the next page boundary.
  // Make the first inaccessible.  Use the second as the user stack.

#if DBGMSG_EXEC
  get_padded_hex_addr(PGROUNDUP(sz), tmp1);
  get_padded_hex_addr(PGROUNDUP(sz) + PGSIZE, tmp2);
  cprintf("[DBGMSG] exec: %s's stackguard is %s~%s\n", curproc->name, tmp2, tmp1);
#endif

  // stack guard?
  sz = PGROUNDUP(sz);
  if((sz = allocuvm(pgdir, sz, sz + PGSIZE)) == 0)
    goto bad;
  clearpteu(pgdir, (char*)(sz - PGSIZE));

#if DBGMSG_EXEC
  get_padded_hex_addr(sz + PGSIZE*3, tmp1);
  get_padded_hex_addr(sz + PGSIZE*4, tmp2);
  cprintf("[DBGMSG] exec: %s's first stack page is %s~%s\n", curproc->name, tmp2, tmp1);
#endif

  // allocate user stack, but skip 3 pages first
  if((sz = allocuvm(pgdir, sz + PGSIZE*3, sz + PGSIZE*4)) == 0)
	goto bad;

  // stack pointer is at sz
  sp = sz;

  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp = (sp - (strlen(argv[argc]) + 1)) & ~3;
    // i think this is copying argc into stack
    if(copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[3+argc] = sp;
  }
  ustack[3+argc] = 0;


  // Q: what is this (fake return PC)
  ustack[0] = 0xffffffff;  // fake return PC
  ustack[1] = argc;
  ustack[2] = sp - (argc+1)*4;  // argv pointer

  sp -= (3+argc+1) * 4;
  // this is copying stuff in argv into stack
  if(copyout(pgdir, sp, ustack, (3+argc+1)*4) < 0)
    goto bad;

  // stored inside kernelspace (doesnt matter for objective 3)
  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(curproc->name, last, sizeof(curproc->name));

  // Commit to the user image.
  oldpgdir = curproc->pgdir;
  curproc->pgdir = pgdir;
  curproc->sz = sz;
  curproc->tf->eip = (curproc->ed.elf).entry;  // main
  curproc->tf->esp = sp;
  switchuvm(curproc);
  freevm(oldpgdir);
  // vmemlayout();
  return 0;

 bad:
  if(pgdir)
    freevm(pgdir);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -1;
}
