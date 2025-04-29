# Changes I made from last commit
1. i noted major bugs with // [MAJOR BUG] ...     definitely take a look at them (ctrl+F for [MAJOR BUG]) if your code was influenced by mine in any capacity

# Things to check before turning in your assignment
## mmap
1. the size of the mmap is not capped by the length of the file we are trying to mmap. any frames that go over the file size should be zeroed out.this mechanism is used to append to a file through an mmap
2. check that you are guarding against prevention with spinlocks. because timer interrupts can distrupt the execution of the mmap syscall, we need to protect against that when we modify the proc structure (the ptable)
3. check mmap read protect flag and mmap write protect flag policy enforcement
4. check that a file (inode) can be mmaped only by one process at a time
5. make sure that sections that read + modify mmap metadata are protected by spinlocks (otherwise timer interrpupts _might_ cause very subtle bugs)
6. make sure to check test cases:
    1. closing files call munmap
    2. exiting closes files
    3. mmap fails if two processes try to mmap the same file
    4. common case: mmap writes to correct offset, writeback persists (check with cat)

## cow
1. just make sure it doesnt crash the system (this basically already tests your implementation QUITE a bit since literally everything is initially COWd and all kallocs and frees go through refcnt functions)
2. dont forget to initialize the refcnt array in kernel startup code (to 1 or zero remember kinit calls freerange that calls kfree)
3. when forking frees (# of free pages in the system) shouldnt change by much

## *VERY IMPORTANT*
1. DELETE DEBUGGING PRINT STATEMENTS (i suggest macroing out every single one to "comment them out" before submitting)
2. if you used debugging tools like my vmemlayout syscall, be sure to delete it otherwise it will be a bad look for everyone included...
3. look through the QnA board for all major assignment spec clarifications!!! for example: the mmap should fail if a process tries to mmap a file that is mmaped by another

# general troubleshooting tips
1. if you made custom files, be sure to increase the FSSIZE in param.h to something bigger and to register the file in the Makefile under target fs.img
2. to check if stuff was written to disk after the process was closed u can use *cat [file]* to check its contents
3. if you are getting panic "acquire" then u should check if ptable lock is held when doing disk I/O (those try to acquire the ptable lock)
