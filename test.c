#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

void secondary_thread(void* arg)
{
	int* argint = arg;
  int wc;
  for(int i = 0; i < 512; i++){
    wc = write(*argint, "aaaaaaaaaaaaaaa\n", 16);
    printf(1, "secondary thread write, wc=%d\n", wc);
  }

	return;
}

int main(int argc, char** argv)
{
	int tid;

  int fd1 = open("README", O_RDWR);

	tid = thread_create(secondary_thread, &fd1);
  int wc;
  for(int i = 0; i < 512; i++){
    wc = write(fd1, "bbbbbbbbbbbbbbb\n", 16);
    printf(1, "main thread write,      wc=%d\n", wc);
  }

  thread_join(tid);

  close(fd1);

	printf(1, "main thread exiting\n");

	exit();
}

