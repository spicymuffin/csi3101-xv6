#include "types.h"
#include "stat.h"
#include "user.h"
#include "user.h"
#include "param.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"

void test_lzallc()
{
    int pid;
    //   int pid2;

    printf(1, "====start of test 0: fork====\n");
    printf(1, "[child was spawned]\n");
    pid = fork();
    if (pid == 0)
    {
        printf(1, "[child started execution]\n");
        printf(1, "[child is sleeping for 10 ticks]\n");
        sleep(10);
        printf(1, "[child is exiting]\n");
        exit();
    }
    else
    {
        printf(1, "[parent started execution]\n");
        printf(1, "[parent is waiting for child]\n");
        wait();
        printf(1, "[parent is terminating]\n");
    }
    printf(1, "====end of test 0: fork====\n");

    printf(1, "\n\n\n");

    printf(1, "====start of test 1: a====\n");
    
    printf(1, "====end of test 1: a====\n");
}

int main(int argc, char **argv)
{
    printf(1, "====testing lzallc====\n");
    test_lzallc();
    printf(1, "====finished testing====\n");
    exit();
}