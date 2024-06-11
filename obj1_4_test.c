#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"
#include "fcntl.h"

#define MAP_PROT_READ   0x00000001

int main(void)
{
	// #5 - Try to write to RDONLY file
	int fd = open("grep", O_RDONLY);
	char *mmaped_area = (char*)mmap(fd, 0, 1024 * 16, MAP_PROT_READ);
	
	printf(1, "=== Test write to READONLY file ===\n");

	mmaped_area[0] = 'A'; // must be failed

	printf(1, "@@@@ If you can see this printf, your code is failed @@@@\n");

	close(fd);

	exit();
}
