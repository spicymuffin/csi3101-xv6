//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"
#include "x86.h"

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *curproc = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd] == 0){
      curproc->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

int
sys_dup(void)
{
  struct file *f;
  int fd;

  if(argfd(0, 0, &f) < 0)
    return -1;
  if((fd=fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

int
sys_read(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return fileread(f, p, n);
}

int
sys_write(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return filewrite(f, p, n);
}

int
sys_close(void)
{
  int fd;
  struct file *f;

  if(argfd(0, &fd, &f) < 0)
    return -1;
 
  struct proc* p = myproc();

  #if DBGMSG_SYSCLOSE
  cprintf("[DBGMSG] sys_close: cycling through %d mmap(s)\n", p->nmmap);
  cprintf("         | closing file properties:\n");
  cprintf("         | size: %d\n", f->ip->size);
  cprintf("         | inum: %d\n", f->ip->inum);
  #endif

  // cycle through p->ptrs to see if the file was mmaped
  for(int i = 0; i < p->nmmap; i++){
    #if DBGMSG_SYSCLOSE
    cprintf("[DBGMSG] sys_close: comparing with\n", p->nmmap);
    cprintf("         | compared file properties:\n");
    cprintf("         | size: %d\n", f->ip->size);
    cprintf("         | inum: %d\n", f->ip->inum);
    #endif

    // inode number is unique; use it to test file identicality
    if(p->ptrs[i]->fd->ip->inum == f->ip->inum){
      #if DBGMSG_SYSCLOSE
      cprintf("[DBGMSG] sys_close: found mmap of closing file\n");
      #endif
      void* addr = (void*)p->ptrs[i]->addr_low;
      int size = p->ptrs[i]->size;
      if(munmap(addr, size) < 0){
        #if DBGMSG_SYSCLOSE
        cprintf("[DBGMSG] sys_close: munmap failed\n");
        #endif
        return -1;
      }

      #if DBGMSG_SYSCLOSE
      cprintf("[DBGMSG] sys_close: munmap successful\n");
      #endif
    }
  }

  p->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

int
sys_fstat(void)
{
  struct file *f;
  struct stat *st;

  if(argfd(0, 0, &f) < 0 || argptr(1, (void*)&st, sizeof(*st)) < 0)
    return -1;
  return filestat(f, st);
}

// Create the path new as a link to the same inode as old.
int
sys_link(void)
{
  char name[DIRSIZ], *new, *old;
  struct inode *dp, *ip;

  if(argstr(0, &old) < 0 || argstr(1, &new) < 0)
    return -1;

  begin_op();
  if((ip = namei(old)) == 0){
    end_op();
    return -1;
  }

  ilock(ip);
  if(ip->type == T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }

  ip->nlink++;
  iupdate(ip);
  iunlock(ip);

  if((dp = nameiparent(new, name)) == 0)
    goto bad;
  ilock(dp);
  if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
    iunlockput(dp);
    goto bad;
  }
  iunlockput(dp);
  iput(ip);

  end_op();

  return 0;

bad:
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  end_op();
  return -1;
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
  int off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if(de.inum != 0)
      return 0;
  }
  return 1;
}

//PAGEBREAK!
int
sys_unlink(void)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], *path;
  uint off;

  if(argstr(0, &path) < 0)
    return -1;

  begin_op();
  if((dp = nameiparent(path, name)) == 0){
    end_op();
    return -1;
  }

  ilock(dp);

  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(ip->type == T_DIR && !isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(ip->type == T_DIR){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  end_op();

  return 0;

bad:
  iunlockput(dp);
  end_op();
  return -1;
}

static struct inode*
create(char *path, short type, short major, short minor)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;
  ilock(dp);

  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    if(type == T_FILE && ip->type == T_FILE)
      return ip;
    iunlockput(ip);
    return 0;
  }

  if((ip = ialloc(dp->dev, type)) == 0)
    panic("create: ialloc");

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    dp->nlink++;  // for ".."
    iupdate(dp);
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      panic("create dots");
  }

  if(dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");

  iunlockput(dp);

  return ip;
}

int
sys_open(void)
{
  char *path;
  int fd, omode;
  struct file *f;
  struct inode *ip;

  if(argstr(0, &path) < 0 || argint(1, &omode) < 0)
    return -1;

  begin_op();

  if(omode & O_CREATE){
    ip = create(path, T_FILE, 0, 0);
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    if((ip = namei(path)) == 0){
      end_op();
      return -1;
    }
    ilock(ip);
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op();
      return -1;
    }
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  end_op();

  f->type = FD_INODE;
  f->ip = ip;
  f->off = 0;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
  return fd;
}

int
sys_mkdir(void)
{
  char *path;
  struct inode *ip;

  begin_op();
  if(argstr(0, &path) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_mknod(void)
{
  struct inode *ip;
  char *path;
  int major, minor;

  begin_op();
  if((argstr(0, &path)) < 0 ||
     argint(1, &major) < 0 ||
     argint(2, &minor) < 0 ||
     (ip = create(path, T_DEV, major, minor)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_chdir(void)
{
  char *path;
  struct inode *ip;
  struct proc *curproc = myproc();
  
  begin_op();
  if(argstr(0, &path) < 0 || (ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);
  if(ip->type != T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  iput(curproc->cwd);
  end_op();
  curproc->cwd = ip;
  return 0;
}

int
sys_exec(void)
{
  char *path, *argv[MAXARG];
  int i;
  uint uargv, uarg;

  if(argstr(0, &path) < 0 || argint(1, (int*)&uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv))
      return -1;
    if(fetchint(uargv+4*i, (int*)&uarg) < 0)
      return -1;
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    if(fetchstr(uarg, &argv[i]) < 0)
      return -1;
  }
  return exec(path, argv);
}

int
sys_pipe(void)
{
  int *fd;
  struct file *rf, *wf;
  int fd0, fd1;

  if(argptr(0, (void*)&fd, 2*sizeof(fd[0])) < 0)
    return -1;
  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      myproc()->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  fd[0] = fd0;
  fd[1] = fd1;
  return 0;
}

int sys_swapread(void)
{
	char* ptr;
	int blkno;

	if(argptr(0, &ptr, PGSIZE) < 0 || argint(1, &blkno) < 0 )
		return -1;

	swapread(ptr, blkno);
	return 0;
}

int sys_swapwrite(void)
{
	char* ptr;
	int blkno;

	if(argptr(0, &ptr, PGSIZE) < 0 || argint(1, &blkno) < 0 )
		return -1;

	swapwrite(ptr, blkno);
	return 0;
}

#define	MAP_FAILED    (int)((void	*)-1)  // 0xfffffffff.....ff

int mmaps_active = 0;

#include "memlayout.h"

// <------------- mmap util ------------->

// find a cell in PCB's mmaps to store mmap's data.
int
mmap_find_empty_mmap_slot(const struct proc* p)
{
  for(int i = 0; i < NPROCMMAP; i++){
    if(p->mmaps[i].addr_low == -1){
      return i;
    }
  }
  return -1; // slot not found (this should never happen)
}

// return the index at which we should insert the new mmap 
// should mark as a mmap to make space for the entire mmap
//
// size is the length of the mmap that we are trying to fit 
// after offset and len contraints page alligned. (is a multiple of PGSIZE)
//
// returns -1 on search error
int
mmap_find_available_memory_block(const struct proc* p, int size)
{
  if(p->nmmap == 0){
    return 0;
  }

  uint prev_lowest_addr = KERNBASE;
  uint next_highest_addr = p->ptrs[1]->addr_high;

  cprintf("[DBGMSG] mmap: scanning (KERNBASE; sz] for available memory regions\n");

  for(int i = 1; i <= p->nmmap; i++){
    #if DBGMSG_MMAP
    char tmp[11];
    get_padded_hex_addr(prev_lowest_addr, tmp);
    cprintf("[DBGMSG] mmap: comparing %s and ");
    get_padded_hex_addr(next_highest_addr, tmp);
    cprintf("%s\n");
    #endif
    if (prev_lowest_addr - next_highest_addr >= size){
      return i;
    }
    // current map index - 1 is the previous
    prev_lowest_addr  = p->ptrs[i-1]->addr_low;
    if(i == p->nmmap){
      // first address not allocated by proc
      next_highest_addr = p->sz;
    }
    else{
      // next mmap in the mmaps_ptr array
      next_highest_addr = p->ptrs[i]->addr_high;
    }
  }

  return -1;
}

// return the index of the mmap with low_addr addr 
// in the ordered mmap array
int
mmap_find_mmap_by_addr_low(const uint addr)
{
  struct proc* p = myproc();

  for(int i = 0; i < NPROCMMAP; i++){
    if(p->ptrs[i]->addr_low == addr){
      return i;
    }
  }
  return -1; // slot not found
}


// return the index of the mmap with addr 
// in the ordered mmap array
int
mmap_find_mmap_by_addr(const uint addr)
{
  struct proc* p = myproc();

  for(int i = 0; i < NPROCMMAP; i++){
    if(p->ptrs[i]->addr_low <= addr && addr < p->ptrs[i]->addr_high){
      return i;
    }
  }
  return -1; // slot not found
}

// find the index of the mmap with low addr
// addr in the ptrs array
int
mmap_find_ptr_indx(const uint addr)
{
  struct proc* p = myproc();
  
  for(int i = 0; i < p->nmmap; i++){
    if(addr == p->ptrs[i]->addr_low){
      return i;
    }
  }

  return -1;
}

// <------------- mmap util ------------->

int mmap(struct file* f, int off, int len, int flags)
{
  struct proc* p = myproc();
  
  // file descriptor is improper (1)
  if(f < 0){
    #if DBGMSG_MMAP
    cprintf("[DBGMSG] mmap: failed - improper filedescriptor (f < 0)\n");
    #endif
    return MAP_FAILED; 
  }
  // file descriptor is improper (2)
  if(p->ofile[(int)f] == 0){
    #if DBGMSG_MMAP
    cprintf("[DBGMSG] mmap: failed - improper filedescriptor (ofile ref is null)\n");
    #endif
    return MAP_FAILED;
  }
  // call is improper
  if(off < 0 || 
     len <= 0 || 
     off + len > f->ip->size){
    #if DBGMSG_MMAP
    cprintf("[DBGMSG] mmap: failed - improper call\n");
    #endif
    return MAP_FAILED;
  }
  // file is not readable
  if(!f->readable){
    #if DBGMSG_MMAP
    cprintf("[DBGMSG] mmap: failed - file unreadable\n");
    #endif
    return MAP_FAILED;
  }
  // proc has filled mmap quota
  if(p->nmmap >= NPROCMMAP){
    #if DBGMSG_MMAP
    cprintf("[DBGMSG] mmap: failed - proc has filled mmap quota\n");
    #endif
    return MAP_FAILED;
  }
  // system has filled mmap quota
  if(mmaps_active >= NSYSMMAP){
    #if DBGMSG_MMAP
    cprintf("[DBGMSG] mmap: failed - system has filled mmap quota\n");
    #endif
    return MAP_FAILED;
  }

  // acquire ptable lock because we are going to mess with the ptable now
  acquire_ptable_lock();

  // find allocation size in pagetable multiples
  // can potentially overflow, so need to check later again
  int pgalligned_true_mmap_size = ((len + (PGSIZE - 1)) / PGSIZE) * PGSIZE;
  #if DBGMSG_MMAP
  cprintf("[DBGMSG] mmap: allocation size=%d\n", pgalligned_true_mmap_size);
  #endif

  // find a big enough memory block
  int indx = mmap_find_available_memory_block(p, pgalligned_true_mmap_size);

  if(indx == -1){
    #if DBGMSG_MMAP
    cprintf("[DBGMSG] mmap: couldnt find available memory_block\n");
    #endif
    release_ptable_lock();
    return MAP_FAILED;
  }

  // shouldnt fail bc we already checked that we have slots
  int mmapslot = mmap_find_empty_mmap_slot(p);
  
  if(mmapslot == -1){
    panic("mmap");
  }

  // establish a reference to a mmap struct in the PCB
  p->ptrs[indx] = &(p->mmaps[mmapslot]);

  // shift ptrs after index by one to right
  struct mmapdata* cache = p->ptrs[indx];
  struct mmapdata* tmp;

  // we at most have 3 mmaps so we can go beyond p->nmmap by one
  for(int i = indx + 1; i < p->nmmap + 1; i++){
    tmp = p->ptrs[i];
    p->ptrs[i] = cache;
    cache = tmp;
  }

  // previous mmap's lowest address
  uint prev_lowest_addr;
  
  // find start addr based on indx
  if(indx == 0){
    #if DBGMSG_MMAP
    cprintf("[DBGMSG] mmap: mmap is first by proc\n");
    #endif
    // if this mmap is first, previous lowest address is KERNBASE
    prev_lowest_addr = KERNBASE; 
  }
  else{
    #if DBGMSG_MMAP
    cprintf("[DBGMSG] mmap: mmap is %dth\n", indx);
    #endif
    // if it is not zero, then there is a map that comes before it
    prev_lowest_addr = p->ptrs[indx - 1]->addr_low;
  }

  #if DBGMSG_MMAP
  char charbuf[11];
  get_padded_hex_addr(prev_lowest_addr, charbuf);
  cprintf("[DBGMSG] mmap: prev_lowest_addr=%s\n", charbuf);
  #endif

  // mmap is created
  p->nmmap++;
  mmaps_active++;

  // store mmapdata into PCB for lazyalloc
  p->ptrs[indx]->addr_low = prev_lowest_addr - pgalligned_true_mmap_size; // find the start address of this mmap
  p->ptrs[indx]->addr_high = prev_lowest_addr;
  p->ptrs[indx]->size = len; // note: not page alligned just the length of the contents
  p->ptrs[indx]->fd = f;
  p->ptrs[indx]->flags = flags;
  p->ptrs[indx]->offset = off;

  #if DBGMSG_MMAP
  get_padded_hex_addr(p->ptrs[indx]->addr_low, charbuf);
  cprintf("[DBGMSG] mmap: adrr_low=%s\n", charbuf);
  get_padded_hex_addr(p->ptrs[indx]->addr_high, charbuf);
  cprintf("[DBGMSG] mmap: adrr_high=%s\n", charbuf);

  cprintf("[DBGMSG] mmap: assert mmap's addr_low is page alligned=%d\n", p->ptrs[indx]->addr_low == PGROUNDDOWN(p->ptrs[indx]->addr_low));
  #endif

  // // allocate fully the memblock (change later)
  // char *mem;
  // uint a = p->ptrs[indx]->addr_low;

  // for(; a < p->ptrs[indx]->addr_high; a += PGSIZE){ // keep in mind that += PGSIZE is run after the condition is checked lol
  //   // this is allocating a page literally. we will check later if it is mapped or not
  //   mem = kalloc(); // allocate a page somewhere in phys mem, return PA to store in mem
  //   if(mem == 0){ // kalloc failed
  //     cprintf("[DEBUGMSG] mmap: allocuvm out of memory\n");
  //     // cleanup mmap struct
  //     return 0;
  //   }
  //   memset(mem, 0, PGSIZE); // zero out the page we just allocated with kalloc()
  //   if(mappages(p->pgdir, (char*)a, PGSIZE, V2P(mem), flags | PTE_U | PTE_W) < 0){ // map the single page that we allocated (create pte if needed and etc.)
  //                                                                                  // set writable and set user bits to 1
  //                                                                                  // we can use v2p bc page tables, directories are stored in the kernel (so after kernbase in the vram)
  //     cprintf("[DEBUGMSG] mmap: allocuvm out of memory (2)\n");
  //     kfree(mem);
  //     return 0;
  //   }
  // }

  // ilock(f->ip);
  // if(readi(f->ip, (char*)p->ptrs[indx]->addr_low, off, len) == -1){
  //   cprintf("[DEBUGMSG] mmap: readi failed\n");
    
  //   // we are not going to implement this since we will probably never reach this point
  //   // cleanup mmap struct
  //   // cleanup ordering in p->ptrs
  //   p->nmmap--;
  //   mmaps_active--;

  //   iunlock(f->ip);
  //   // release(&ptable.lock);
  //   return MAP_FAILED;
  // }
	// iunlock(f->ip);

  #if DBGMSG_MMAP
  // not really an assert but i cbb changing anything lmao
  cprintf("[DBGMSG] mmap: assert is upper addr equal to KERNBASE?=%d\n", KERNBASE == p->ptrs[indx]->addr_high);
  #endif

  release_ptable_lock();
  // return map start address
  return p->ptrs[indx]->addr_low;
}

int sys_mmap(void)
{
	struct file *f;
	int off, len, flags;
	if ( argfd(0, 0, &f) < 0 || argint(1, &off) < 0 || 
			argint(2, &len) < 0 || argint(3, &flags) < 0 )
		return -1;
	return mmap(f, off, len, flags);
}

int munmap(void* ptr, int len)
{
  struct proc* p = myproc();
  // ptr can be wherever inside a mmap
  int indx = mmap_find_mmap_by_addr((uint)ptr);

  // if addr is not a multiple of PGSIZE return -1 (idk why)
  if((uint)ptr % PGSIZE != 0){
    #if DBGMSG_MUNMAP
    cprintf("[DBGMSG] munmap: failed - addr is not a multiple of PGSIZE\n");
    #endif
    return -1;
  }

  // REVISIT
  // if indx is -1 (low addr not found) then the call is illegal
  if(indx == -1){
    #if DBGMSG_MUNMAP
    cprintf("[DBGMSG] munmap: failed - addr indx is -1\n");
    #endif
    return -1;
  }

  // check whether the length argument is valid
  struct mmapdata* mmpdt = p->ptrs[indx];
  if(len != mmpdt->size){
    #if DBGMSG_MUNMAP
    cprintf("[DBGMSG] munmap: length arg doesn't match original length\n");
    cprintf("         | %d != %d (written != provided)\n", mmpdt->size, len);
    #endif
    release_ptable_lock();
    return -1;
  }

  // write dirty pages back
  uint current_page_addr_low = mmpdt->addr_low;
  uint pa;

  begin_op();
  ilock(mmpdt->fd->ip);
  for(; current_page_addr_low < mmpdt->addr_high; current_page_addr_low += PGSIZE){
    pte_t* pte = get_pte(p, current_page_addr_low);
    uint current_page_addr_high = current_page_addr_low + PGSIZE;

    if(pte == 0){
      // pagetable doesn't exist (the mmap was really big lmfao)
      continue;
    }
    if(!(*pte & PTE_P)){
      // page not present
      continue;
    }

    if(*pte & PTE_D){
      // write but respect file length (or smt idk i did this when i was debugging but it doesnt matter)
      if(writei(mmpdt->fd->ip, 
                (char*)current_page_addr_low,
                current_page_addr_low - mmpdt->addr_low,
                (current_page_addr_high == mmpdt->addr_high ? (mmpdt->size % PGSIZE) : (PGSIZE))) < 0){
        #if DBGMSG_MUNMAP
        cprintf("[DBGMSG] munmap: writei failed\n");
        #endif
        iunlock(mmpdt->fd->ip);
        end_op();
        return -1;
      }
      
      #if DBGMSG_MUNMAP
      cprintf("[DBGMSG] munmap: writei wrote data to disk\n");
      #endif
    }
    // free the page and delete pte
    pa = PTE_ADDR(*pte);
    // if physical pointed by the PTE is zero
    if(pa == 0)
      panic("kfree");
    // convert to virtual with p2v bc we are in kernelspace
    char *v = P2V(pa);
    // free the pageframe pointed to by the va
    kfree(v);
    *pte = 0;
  }
  flush_tlb(); // Q: is this bad design...? can hardware be "controlled" inside syscalls....?

  iunlock(mmpdt->fd->ip);
  end_op();
  
  acquire_ptable_lock();

  mmpdt->addr_low = -1;
  mmpdt->addr_high = -1;
  mmpdt->size = -1;
  mmpdt->fd = 0;
  mmpdt->flags = -1;
  mmpdt->offset = -1;
	
  // cleanup the ordered array
  int ptr_indx = mmap_find_ptr_indx(mmpdt->addr_low);
  ptr_indx = 0;
  for(int i = ptr_indx; i < p->nmmap; i++){
    struct mmapdata* tmp = p->ptrs[i];
    p->ptrs[i] = p->ptrs[i+1];
    p->ptrs[i+1] = tmp;
  }

  mmaps_active--;
  p->nmmap--;

  release_ptable_lock();

  return 0;
}

int sys_munmap(void)
{
	int ptr, len;
	if ( argint(0, &ptr) < 0 || argint(1, &len) < 0)
		return -1;
	return munmap((void*)ptr, len);
}
