#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
// #include "elf.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"

extern char data[];  // defined by kernel.ld
pde_t *kpgdir;  // for use in scheduler()

// Set up CPU's kernel segment descriptors.
// Run once on entry on each CPU.
void
seginit(void)
{
  struct cpu *c;

  // Map "logical" addresses to virtual addresses using identity map.
  // Cannot share a CODE descriptor for both kernel and user
  // because it would have to have DPL_USR, but the CPU forbids
  // an interrupt from CPL=0 to DPL=3.
  c = &cpus[cpuid()];
  c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
  c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
  lgdt(c->gdt, sizeof(c->gdt));
}

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc!=0,
// create any required page table pages.
static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)];
  if(*pde & PTE_P){ // if the pgdir 's entry no. PDX(va) is present (the pagetable exists) then just return it
    pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
  } else {
    if(!alloc || (pgtab = (pte_t*)kalloc()) == 0)
      return 0; // the pagetable didnt exist, and we dont want to create it or we couldnt create it

    // the pagetable didnt exisst, but we were successfull at allocating space for it

    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
    // page directory entry's value is pgtab's base address, obviously
    *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)];
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned.
// page directory, va start, # of entries, pa start, permissions 
static int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm)
{
  char *a, *last;
  pte_t *pte;

  a = (char*)PGROUNDDOWN((uint)va);
  last = (char*)PGROUNDDOWN(((uint)va) + size - 1); // -1 because last is on last, so we want last to be last ()
  // a is virtual address
  for(;;){
    if((pte = walkpgdir(pgdir, a, 1)) == 0) // integer argument is one so we are requesting to allocate a page table if the page we want
                                            // to map corresponds to a pagetable that is not present
      return -1;
    if(*pte & PTE_P) // mask off the PTE_P bit literally and if it masks and the bit is one then true else everything is zero so false
      panic("remap");
    *pte = pa | perm | PTE_P;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }

  flush_tlb(); // Q: is this bad design...? can hardware be "controlled" inside syscalls....?

  return 0;
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

// This table defines the kernel's mappings, which are present in
// every process's page table.
static struct kmap {
  void *virt;
  uint phys_start;
  uint phys_end;
  int perm;
} kmap[] = {
 { (void*)KERNBASE, 0,             EXTMEM,    PTE_W}, // I/O space
 { (void*)KERNLINK, V2P(KERNLINK), V2P(data), 0},     // kern text+rodata
 { (void*)data,     V2P(data),     PHYSTOP,   PTE_W}, // kern data+memory
 { (void*)DEVSPACE, DEVSPACE,      0,         PTE_W}, // more devices
};

// Set up kernel part of a page table.
pde_t*
setupkvm(void)
{
  pde_t *pgdir;
  struct kmap *k;

  if((pgdir = (pde_t*)kalloc()) == 0)
    return 0;
  memset(pgdir, 0, PGSIZE);
  if (P2V(PHYSTOP) > (void*)DEVSPACE)
    panic("PHYSTOP too high");
  for(k = kmap; k < &kmap[NELEM(kmap)]; k++)
    if(mappages(pgdir, k->virt, k->phys_end - k->phys_start,
                (uint)k->phys_start, k->perm) < 0) {
      freevm(pgdir);
      return 0;
    }
  return pgdir;
}

// Allocate one page table for the machine for the kernel address
// space for scheduler processes.
void
kvmalloc(void)
{
  kpgdir = setupkvm();
  switchkvm();
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void
switchkvm(void)
{
  lcr3(V2P(kpgdir));   // switch to the kernel page table
}

// Switch TSS and h/w page table to correspond to process p.
void
switchuvm(struct proc *p)
{
  if(p == 0)
    panic("switchuvm: no process");
  if(p->kstack == 0)
    panic("switchuvm: no kstack");
  if(p->pgdir == 0)
    panic("switchuvm: no pgdir");

  pushcli();
  mycpu()->gdt[SEG_TSS] = SEG16(STS_T32A, &mycpu()->ts,
                                sizeof(mycpu()->ts)-1, 0);
  mycpu()->gdt[SEG_TSS].s = 0;
  mycpu()->ts.ss0 = SEG_KDATA << 3;
  mycpu()->ts.esp0 = (uint)p->kstack + KSTACKSIZE;
  // setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
  // forbids I/O instructions (e.g., inb and outb) from user space
  mycpu()->ts.iomb = (ushort) 0xFFFF;
  ltr(SEG_TSS << 3);
  lcr3(V2P(p->pgdir));  // switch to process's address space
  popcli();
}

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
void
inituvm(pde_t *pgdir, char *init, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pgdir, 0, PGSIZE, V2P(mem), PTE_W|PTE_U);
  memmove(mem, init, sz);
}

// Load a program segment into pgdir.  addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.
int
loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz)
{
  uint i, pa, n;
  pte_t *pte;

  if((uint) addr % PGSIZE != 0)
    panic("loaduvm: addr must be page aligned");
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, addr+i, 0)) == 0)
      panic("loaduvm: address should exist");
    pa = PTE_ADDR(*pte);
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if(readi(ip, P2V(pa), offset+i, n) != n)
      return -1;
  }
  return 0;
}

int 
lazyalloc(struct proc * p, uint va, int mpindx)
{
  pte_t *pte;
  
  if(va >= p->sz && mpindx == -1){
    #if DBGMSG_LAZYALLOC
    cprintf("[DBGMSG] lazyalloc: virtual address is above p->sz and not in a mmaped area\n");
    #endif
    return -1; // outside of memory allocated by proc
  }
  
//   if(va < PGROUNDDOWN(p->tf->esp)){

// #if DBGMSG_LAZYALLOC
//     cprintf("[DBGMSG] lazyalloc: virtual address is below stackpointer\n");
// #endif

//     return -2; // below proc's stackpointer's page
//   }
  
  // walk pgdir, allocate a pgtab if necessary
  if((pte = walkpgdir(p->pgdir, (void*)va, 1)) == 0){
    #if DBGMSG_LAZYALLOC
    cprintf("[DBGMSG] lazyalloc: out of space to allocate a new pgtab\n");
    #endif
    return -3; // out of space for a new pgtab
  }

  char* mem;
  if((mem = kalloc()) == 0){
    #if DBGMSG_LAZYALLOC
    cprintf("[DBGMSG] lazyalloc: out of space to allocate a new page\n");
    #endif
    return -4; // out of space for a new page
  }

  // if pte was already present we messed up
  if(*pte & PTE_P)
    panic("remap");

  // establish faulted adress's page bounds
  uint flt_pg_addr_low = PGROUNDDOWN(va);
  uint flt_pg_addr_high = flt_pg_addr_low + PGSIZE;

  // map the pte (give all permissions and clean basically at first)
  *pte =  ((uint)(V2P(mem)) | PTE_W | PTE_U | PTE_P) & ~PTE_D;

  // clear out the page we allocated
  memset(mem, 0, PGSIZE);

  #if DBGMSG_LAZYALLOC
  char tmp[11];
  cprintf("[DBGMSG] lazyalloc: successfully allocated a new page\n");
  get_padded_hex_addr(va, tmp);
  cprintf("         | address: %s\n", tmp);
  get_padded_hex_addr(*pte, tmp);
  cprintf("         | pte:     %s\n", tmp);
  #endif

  // read in file if the page was a part of a mmap
  int mmapindx = mpindx;

  // found the mmap to which va belongs to
  if(mmapindx != -1){

    struct mmapdata *mmpdt = &(p->mmaps[mmapindx]);

    ilock(mmpdt->fd->ip);
    if(readi(mmpdt->fd->ip,
             (char*)flt_pg_addr_low,
             (mmpdt->offset + (flt_pg_addr_low - mmpdt->addr_low)),
             (flt_pg_addr_high == mmpdt->addr_high ? (mmpdt->size % PGSIZE) : (PGSIZE))) == -1)
    {
      #if DBGMSG_LAZYALLOC
      cprintf("[DBGMSG] lazyalloc: read in for mmap page failed\n");
      #endif
      iunlock(mmpdt->fd->ip);
      return -5; // read-in fail
    }

    #if DBGMSG_LAZYALLOC
    cprintf("[DBGMSG] lazyalloc: read in for mmap page successful\n");
    #endif
    iunlock(mmpdt->fd->ip);

    // manage permissions
    if(!(mmpdt->flags & MAP_PROT_WRITE)){
      *pte = *pte & ~PTE_W;
    }

    // unset dirty bit after reading in
    *pte = *pte & ~PTE_D;

    flush_tlb(); // Q: is this bad design...? can hardware be "controlled" inside syscalls....?

    // bad code - i hate the control flow but oh well
    return 1; // read-in success
  }

  // determine whether va belongs to bss/data/text segment
  // load in executable binary if va is lower than p->ed.bdtbound
  if(va < p->ed.bdtbound){
    // if va also belongs to a mmap something really bad happened
    if(mmapindx != -1)
      panic("mmap & executable");
    
    int ld_exe_status_code = load_executable(p, p->ed.ip, va);
    if(ld_exe_status_code < 0){
      #if DBGMSG_LAZYALLOC
      cprintf("[DBGMSG] lazyalloc: read in for bdt page failed\n");
      #endif
    }
    #if DBGMSG_LAZYALLOC
    cprintf("[DBGMSG] lazyalloc: read in for bdt page successful\n");
    #endif
    return 2; // executable read-in success
  }

  flush_tlb(); // Q: is this bad design...? can hardware be "controlled" inside syscalls....?

  // bad code - i hate the control flow but oh well
  return 0; // general success
}

uint*
get_pte(struct proc* p, uint va)
{
  pte_t* pte;
  if((pte = walkpgdir(p->pgdir, (void*)va, 0)) == 0){
    return 0;
  }
  return pte;
}

// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
// newsz is the first unallocated addr
int
allocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  char *mem;
  uint a;

  if(newsz >= KERNBASE)
    return 0;
  if(newsz < oldsz)
    return oldsz;

  a = PGROUNDUP(oldsz);
  for(; a < newsz; a += PGSIZE){ // keep in mind that += PGSIZE is run after the condition is checked lol
    // this is allocating a page literally. we will check later if it is mapped or not
    mem = kalloc(); // allocate a page somewhere in phys mem, return PA to store in mem
    if(mem == 0){ // kalloc failed
      cprintf("allocuvm out of memory\n");
      deallocuvm(pgdir, newsz, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE); // zero out the page we just allocated with kalloc()
    if(mappages(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){ // map the single page that we allocated (create pte if needed and etc.)
                                                                      // set writable and set user bits to 1
                                                                      // we can use v2p bc page tables, directories are stored in the kernel (so after kernbase in the vram)
      cprintf("allocuvm out of memory (2)\n");
      deallocuvm(pgdir, newsz, oldsz);
      kfree(mem);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
int
deallocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  pte_t *pte;
  uint a, pa;

  if(newsz >= oldsz)
    return oldsz;

  a = PGROUNDUP(newsz);
  // while the page we are in is unneded (lower than oldsz)
  for(; a  < oldsz; a += PGSIZE){
    pte = walkpgdir(pgdir, (char*)a, 0);
    // page table (entry behind pde is not present)
    if(!pte)
      // move a by a page directory entry
      a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
    
    // if pte's page is present
    else if((*pte & PTE_P) != 0){
      pa = PTE_ADDR(*pte);
      // if physicall pointed by the PTE is zero
      if(pa == 0)
        panic("kfree");
    
      // convert to virtual with p2v bc we are in kernelspace
      char *v = P2V(pa);
      // free the pageframe pointed to by the va
      kfree(v);
      *pte = 0;
    }
    flush_tlb();
  }
  return newsz;
}

// Free a page table and all the physical memory pages
// in the user part.
void
freevm(pde_t *pgdir)
{
  uint i;

  if(pgdir == 0)
    panic("freevm: no pgdir");
  deallocuvm(pgdir, KERNBASE, 0);
  for(i = 0; i < NPDENTRIES; i++){
    if(pgdir[i] & PTE_P){
      char * v = P2V(PTE_ADDR(pgdir[i]));
      kfree(v);
    }
  }
  kfree((char*)pgdir);
}

// Clear PTE_U on a page. Used to create an inaccessible
// page beneath the user stack.
void
clearpteu(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if(pte == 0)
    panic("clearpteu");
  *pte &= ~PTE_U;
}

// Given a parent process's page table, create a copy
// of it for a child.
pde_t*
copyuvm(pde_t *pgdir, uint sz)
{
  pde_t *d;
  pte_t *pte;
  uint pa, i, flags;
  char *mem;

  if((d = setupkvm()) == 0)
    return 0;
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)
      panic("copyuvm: pte should exist");
    if(!(*pte & PTE_P))
      continue;
      // panic("copyuvm: page not present"); // why was this already commented out lol
    pa = PTE_ADDR(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto bad;
    memmove(mem, (char*)P2V(pa), PGSIZE);
    if(mappages(d, (void*)i, PGSIZE, V2P(mem), flags) < 0) {
      kfree(mem);
      goto bad;
    }
  }
  return d;

bad:
  freevm(d);
  return 0;
}

//PAGEBREAK!
// Map user virtual address to kernel address.
char*
uva2ka(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if((*pte & PTE_P) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  return (char*)P2V(PTE_ADDR(*pte));
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
int
copyout(pde_t *pgdir, uint va, void *p, uint len)
{
  char *buf, *pa0;
  uint n, va0;

  buf = (char*)p;
  while(len > 0){
    va0 = (uint)PGROUNDDOWN(va);
    pa0 = uva2ka(pgdir, (char*)va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (va - va0);
    if(n > len)
      n = len;
    memmove(pa0 + (va - va0), buf, n);
    len -= n;
    buf += n;
    va = va0 + PGSIZE;
  }
  return 0;
}

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.

