#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char **argv)
{
    int psN = ps();
    int yieldN = yield();
    int niceN = nice(10);
    printf(1, "ps = %d; yield = %d; nice = %d\n", psN, yieldN, niceN);
    ps();
    nice(-3);
    ps();
    ps();
    yield();
    ps();
    exit();
}
