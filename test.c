#include "user.h"
#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"

// <---------------util---------------->
void mmap_print(uint start_addr, int len)
{
    int j;
    for (j = 0; j < len; j++)
    {
        // a very scuffed way to print a single character
        char *ptr = (char *)(start_addr + j);
        char c[2];
        c[0] = *ptr;
        c[1] = '\0';
        printf(1, "%s", c);
    }
    printf(1, "\n");

    printf(1, "chars outputted: %d\n", j);
}

void mmap_nullify(uint start_addr, int len)
{
    int j;
    for (j = 0; j < len; j++)
    {
        // a very scuffed way to modify a single character
        char *ptr = (char *)(start_addr + j);
        *ptr = '\0';
    }

    printf(1, "chars overwritten: %d\n", j);
}

void mmap_write_at_addr(uint start_addr, const char* write) {
    int j; 
    int length = strlen(write);
    for (j = 0; j < length; j++) {
        // a very scuffed way to modify a single character
        char *ptr = (char *)(start_addr + j);
        *ptr = write[j];
    }

    printf(1, "wrote %s to mmap\n", write);
}
// <---------------util---------------->

int main(int argc, char *argv[])
{
    struct stat st1;
    struct stat st2;

    int fd1;
    int fd2;

    int i = 1;

    printf(1, "========== executing test ==========\n");

    printf(1, "free page frames: %d\n", frees());

    if ((fd1 = open(argv[i], 0)) < 0)
    {
        printf(1, "test: cannot open %s\n", argv[i]);
        exit();
    }
    if ((fd2 = open(argv[i + 1], 0)) < 0)
    {
        printf(1, "test: cannot open %s\n", argv[i + 1]);
        exit();
    }

    if (stat(argv[i], &st1) < 0)
    {
        printf(1, "test: cannot retrieve statistics for %s\n", argv[i]);
        close(fd1);
        exit();
    }

    if (stat(argv[i + 1], &st2) < 0)
    {
        printf(1, "test: cannot retrieve statistics for %s\n", argv[i + 1]);
        close(fd2);
        exit();
    }

    printf(1, "file length1 is %d\n", st1.size);
    printf(1, "file length2 is %d\n", st2.size);
    int offset1 = 2;
    int length1 = st1.size - offset1;
    int offset2 = 0;
    int length2 = st2.size - offset2 - 40000;

    int addr1 = (int)mmap(fd1, offset1, length1, MAP_PROT_WRITE | MAP_PROT_READ);
    vmemlayout();
    int addr2 = (int)mmap(fd2, offset2, length2, MAP_PROT_WRITE | MAP_PROT_READ);
    vmemlayout();

    if (addr1 == -1)
    {
        printf(1, "test: mmap1 failed\n");
        close(fd2);
        close(fd1);
        exit();
    }

    // print contents of mmaps
    printf(1, "contents of mmap1:\n");
    mmap_print(addr1, length1);

    vmemlayout();

    if (addr2 == -1)
    {
        printf(1, "test: mmap2 failed\n");
        close(fd1);
        close(fd2);
        exit();
    }

    printf(1, "contents of mmap2:\n");
    mmap_print(addr2, length2);

    mmap_write_at_addr(addr2 + 123, "ilovesofia");

    // the vmemlayout() SC had bugs so i tested
    // char* suspect = (char*)0x7FFFC000;
    // *suspect = 1;

    vmemlayout();

    printf(1, "free page frames: %d\n", frees());

    printf(1, "test: closing file1\n");
    //close(fd1);
    printf(1, "test: closing file2\n");
    //close(fd2);

    exit();
}