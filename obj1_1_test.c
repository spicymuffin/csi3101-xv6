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
	int file1_size = 1024 * 3;
	int fd1;
	char *mmaped_area1;
	char *test_string = "----- Hello, OS, Virtual Memory! -----";
	char *mid_string = "----- Hmmmm, OS, Virtual Memory! -----";

	fd1 = open(filename1, O_CREATE | O_RDWR);

	mmaped_area1 = (char *)mmap(fd1, 0, file1_size, MAP_PROT_READ | MAP_PROT_WRITE);

	// #1 - mmaped file read and strncmp
	if(strncmp(mmaped_area1, test_string, strlen(test_string)) == 0) {
		printf(1, "=== String comparison test success ===\n");
	} else {
		printf(1, "*** String comparison test failed ***\n");
	}

	for (int i = 0; i < strlen(mid_string); i++){
		mmaped_area1[i] = mid_string[i];
	}

	// #2 - munmap() => dirty mmap'ed area written back
	munmap(mmaped_area1, file1_size);
	
	char buf_m_read[strlen(mid_string)];

	if(read(fd1, buf_m_read, sizeof(buf_m_read)) == sizeof(buf_m_read)) {
		if(strncmp(buf_m_read, mid_string, strlen(mid_string)) == 0) {
			printf(1, "==== Written back test success (2) ====\n");
		} else {
			printf(1, "**** Written back test failed (2) ****\n");
		}
	} else {
		printf(1, "*** fileread failed (2) ***\n");
	}
	
	close(fd1);

	exit();
}
