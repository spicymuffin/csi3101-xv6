#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"
#include "fcntl.h"

int main(void)
{
	// #7 - heap demand paging
	int *ptr, after_malloc_frees, init_frees, after_frees;

	init_frees = frees();
	printf(1, "=== Check initial frees : %d ===\n", init_frees);

	ptr = malloc(40960);

	after_malloc_frees = frees();
    	printf(1, "=== Check frees after malloc: %d ===\n", after_malloc_frees);

    	*ptr = 10;

	after_frees = frees();
	printf(1, "=== Check frees after paging heap : %d ===\n", after_frees);

	if(init_frees - after_frees == 1) {
		printf(1, "==== Heap demand paging success ====\n");
	} else {
		printf(1, "**** Heap demand paging failure ****\n");
	}

	exit();
}
