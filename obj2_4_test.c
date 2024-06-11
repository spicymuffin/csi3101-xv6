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
	// #9 - if access invalid VA, must be terminated
	int fd = open("README", O_RDWR);
	char *mmaped_area = (char*)mmap(fd, 0, 1024 * 4, MAP_PROT_READ | MAP_PROT_WRITE);
	
	printf(1, "=== Test Access invlaid VA ===\n");
	mmaped_area[1024 * 8] = 'A';
		
	printf(1, "@@@@ If you can see this printf, your code is failed @@@@\n");

	close(fd);
	
	exit();
}
