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
	// #4 - after munmap, accessing munmap'ed area cause a fault
	char *filename = "README";
	int file_size = 1024 * 3;
	int fd = open(filename, O_CREATE | O_RDWR);
	char *mmaped_area = (char *)mmap(fd, 0, file_size, MAP_PROT_READ | MAP_PROT_WRITE);

	char *test_address = mmaped_area + (1024 * 3) - 1;

	munmap(mmaped_area, 1024 * 3);

	printf(1, "=== Test accessng munmap'ed area ===\n");
	*test_address = 'X'; // must be failed
	printf(1, "@@@@ If you can see this printf, your code is failed @@@@\n");

	close(fd);

	exit();

}
