#include "types.h"
#include "user.h"

#define MAX 3072

int dummy[MAX] = {1,};

int main(void)
{
	int init_frees, final_frees;

	init_frees = frees();
	printf(1, "=== Initial frees : %d ===\n", init_frees);

	for(int i = 0; i < MAX; i += 1024) {
		dummy[i] = 1;
	}

	final_frees = frees();
	printf(1, "=== Final frees : %d ===\n", final_frees);

	// #11 - data demand paging
	if((init_frees - final_frees) == 2) {
		printf(1, "=== Data demand paging success ===\n");
	} else {
		printf(1, "*** Data demand paging failure ***\n");
	}

	exit();
 }

