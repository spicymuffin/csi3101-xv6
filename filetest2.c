#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

// TEST 2
// main thread exits before child thread, child thread writes to file


void child_thread(void *arg)
{
    int *argint = arg;

    sleep(30);

    printf(1, "child_thread: writing to file\n");
    int wc = write(*argint, "ilovesofia", 10);
    printf(1, "child_thread: closing file, writecode=%d\n", wc);
    close(*argint);

    printf(1, "frees after (in child): %d\n", frees());
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

    int fd1 = open("README", O_RDWR);
    
    printf(1, "main: child_thread spawned\n");
    thread_create(child_thread, &fd1);

    printf(1, "main: exiting\n");
    printf(1, "frees after: %d\n", frees());
    exit();

}
