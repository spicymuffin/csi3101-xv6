#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"
#include "fcntl.h"

extern int dummy1(void);
extern int dummy2(void);

int main(void)
{
	int f1, f2;

	f1 = frees();
 
	dummy2();

	f2 = frees();
	printf(1, "f1: %d f2: %d\n", f1, f2);

	// #10 - code demand paging
	if((f1 - f2) == 2) {
		printf(1, "=== Code demand paging success ===\n");
	} else {
		printf(1, "*** Code demand paging failure ***\n");
	}

	exit();
 }

