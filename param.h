#define NPROC        64  // maximum number of processes
#define KSTACKSIZE 4096  // size of per-process kernel stack
#define NCPU          8  // maximum number of CPUs
#define NOFILE       16  // open files per process
#define NFILE       100  // open files per system
#define NINODE       50  // maximum number of active i-nodes
#define NDEV         10  // maximum major device number
#define ROOTDEV       1  // device number of file system root disk
#define MAXARG       32  // max exec arguments
#define MAXOPBLOCKS  10  // max # of blocks any FS op writes
#define LOGSIZE      (MAXOPBLOCKS*3)  // max data blocks in on-disk log
#define NBUF         (MAXOPBLOCKS*3)  // size of disk block cache
#define FSSIZE       1000  // size of file system in blocks

#define NSYSMMAP     16  // maximum number of mmaped files in system
#define NPROCMMAP     4  // maximum number of mmaped files per proc


// DBG params
// #define DBGMSG_LAZYALLOC      1 // lazyalloc's debug messages
// #define DBGMSG_PAGEFAULT      1 // pgflt trap handler's debug messages
// #define DBGMSG_MMAP           1 // mmap debug messages
// #define DBGMSG_MUNMAP         1 // munmap debug messages
// #define DBGMSG_EXEC           1 // exec debug messages
// #define DBGMSG_SBRK           1 // sbrk debug messages
// #define DBGMSG_SYSCLOSE       1 // sys_close debug messages
// #define DBGMSG_EXIT           1 // exit debug messages
// #define DMGMSG_LOAD_EXECTBL   1 // load_executable debug messages