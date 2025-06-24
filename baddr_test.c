#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define SECTOR 512
#define NSECT  20

int
main(void)
{
    char buf[SECTOR];
    int  fd, i;
    uint blk;

    // Fill buffer with recognisable data
    for (i = 0; i < SECTOR; i++)
        buf[i] = (i & 0xff);

    // Create / open test file
    fd = open("baddr.file", O_CREATE | O_RDWR);
    if (fd < 0)
    {
        printf(2, "baddr_test: open failed\n");
        exit();
    }

    // sequentially write NSECT sectors => will allocate
    // DIRECT blocks 0..NDIRECT-1
    // then a few SINGLE-INDIRECT blocks
    for (i = 0; i < NSECT; i++)
    {
        if (write(fd, buf, SECTOR) != SECTOR)
        {
            printf(2, "baddr_test: write error at sector %d\n", i);
            exit();
        }
    }

    printf(1, "---- user-space mapping via baddr() ----\n");
    for (i = 0; i < NSECT; i++)
    {
        blk = baddr(fd, i * SECTOR);
        printf(1, "logical %d -> block %d\n", i, blk);
    }

    close(fd);
    printf(1, "baddr_test: finished OK\n");
    exit();
}