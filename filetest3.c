#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

// TEST 3
// write in child, but file was closed in main


void child_thread(void *arg)
{
    int *argint = arg;


    printf(1, "child_thread: writing to file\n");
    int wc = write(*argint, "ilovesofia", 10);
    printf(1, "child_thread first write, writecode=%d\n", wc);
    sleep(300);
    wc = write(*argint, "hello hello hello", 18);
    printf(1, "child_thread second write, writecode=%d\n", wc);
    printf(1, "child_thread returning\n");
    return;
}

void get_padded_hex_addr(uint addr, char *buf)
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

int main(int argc, char **argv)
{
    printf(1, "frees before: %d\n", frees());
    int tid;

    int fd1 = open("README", O_RDWR);

    printf(1, "main: child_thread spawned, fd=%d\n", fd1);
    tid = thread_create(child_thread, &fd1);

    sleep(150);

    close(fd1);
    thread_join(tid);
    printf(1, "main: child_thread joined\n");
    
    char buf[128];

    // workaround to reset readpos
    fd1 = open("README", O_RDWR);

    read(fd1, buf, sizeof(buf));
    write(1, buf, sizeof(buf));
    write(1, "\n", 1);

    printf(1, "frees after: %d\n", frees());
    exit();
}
