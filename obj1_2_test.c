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

int strncmp(const char *p, const char *q, uint n)
{
	while(n > 0 && *p && *p == *q)
		n--, p++, q++;
	if(n == 0)
		return 0;
	return (uchar)*p - (uchar)*q;
}

int main(void)
{
	char *filename1 = "README";
	int file1_size = 512;
	int fd1;
	char *mmaped_area1;
	char *new_string = "----- BBBye, OS, Virtual Memory! -----";

	fd1 = open(filename1, O_CREATE | O_RDWR);

	// #3 - no munmap() + close() => dirty mmap'ed area written back to disk
	mmaped_area1 = (char *)mmap(fd1, 0, file1_size, MAP_PROT_READ | MAP_PROT_WRITE);

	// modify file1 new contents
	for (int i = 0; i < strlen(new_string); i++){
		mmaped_area1[i] = new_string[i];
	}	

	close(fd1); // no munmap + close

	char buf_read[strlen(new_string)];

	fd1 = open(filename1, O_CREATE | O_RDWR);
	
	if(read(fd1, buf_read, sizeof(buf_read)) == sizeof(buf_read)) {
		if(strncmp(buf_read, new_string, strlen(new_string)) == 0) {
			printf(1, "==== Written back test success (3) ====\n");
		} else {
			printf(1, "**** Written back test failed (3) ****\n");
		}
	} else {
		printf(1, "*** fileread failed (3)***\n");
	}
	
	close(fd1);

	exit();
}
