#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"
#include "fcntl.h"

#define MAP_PROT_READ   0x00000001
#define MAP_PROT_WRITE  0x00000002

int main(void)
{
	// #8 - mmap demand paging
	int init_frees, after_mmap_frees, after_access_mmap_frees;

	init_frees = frees();
	printf(1, "==== Initial frees : %d ====\n", init_frees);

	int fd = open("README", O_RDWR);
	char *mmaped_area = (char*)mmap(fd, 0, 1024 * 4, MAP_PROT_READ | MAP_PROT_WRITE);
	
	after_mmap_frees = frees();
	printf(1, "==== After mmap frees : %d ====\n", after_mmap_frees);

	mmaped_area[0] = 'A';

	after_access_mmap_frees = frees();
	printf(1, "==== After access mmap frees : %d ====\n", after_access_mmap_frees);

	if(init_frees - after_mmap_frees == 0 && after_access_mmap_frees - init_frees != 0) {
		printf(1, "==== mmap demand paging success ====\n");
	} else {
		printf(1, "**** mmap demand paging failure ****\n");
	}

	close(fd);
	exit();
}
