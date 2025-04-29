#include "user.h"
#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"

int main(int argc, char* argv[])
{
    printf(1, "frees: %d\n", frees());
    exit();
    return 0;
}