#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char* argv[])
{
    int bench_start = uptime();

    int pid1 = fork();
    if (pid1 == 0)
    {
        sched_setattr(5, 1);
        // psm();
        printf(1, "pid1: %d\n", getpid());
        int start = uptime();
        for (volatile int i = 0; i < 50000000; i++) {} // low weight
        // for (volatile int i = 0; i < 50000000; i++) {}
        // for (volatile int i = 0; i < 50000000; i++) {}
        // for (volatile int i = 0; i < 50000000; i++) {}
        // for (volatile int i = 0; i < 50000000; i++) {}
        // for (volatile int i = 0; i < 50000000; i++) {}
        // for (volatile int i = 0; i < 50000000; i++) {}
        // for (volatile int i = 0; i < 50000000; i++) {}
        // for (volatile int i = 0; i < 50000000; i++) {}

        int end = uptime();
        printf(1, "pid1 Time: %d\n", end - start);
        printf(1, "pid1 uptime: %d\n", uptime());
        exit();
    }
    int pid2 = fork();
    if (pid2 == 0)
    {
        sched_setattr(5, 5);
        // psm();
        printf(1, "pid2: %d\n", getpid());
        int start = uptime();
        for (volatile int i = 0; i < 50000000; i++) {} // high weight
        // for (volatile int i = 0; i < 50000000; i++) {}
        // for (volatile int i = 0; i < 50000000; i++) {}
        // for (volatile int i = 0; i < 50000000; i++) {}
        // for (volatile int i = 0; i < 50000000; i++) {}
        // for (volatile int i = 0; i < 50000000; i++) {}
        // for (volatile int i = 0; i < 50000000; i++) {}
        // for (volatile int i = 0; i < 50000000; i++) {}
        // for (volatile int i = 0; i < 50000000; i++) {}

        int end = uptime();
        printf(1, "pid2 Time: %d\n", end - start);
        printf(1, "pid2 uptime: %d\n", uptime());
        exit();
    }

    wait();
    wait();
    int bench_end = uptime();
    printf(1, "[FINISH] Time: %d\n", bench_end - bench_start);
    exit();
}
