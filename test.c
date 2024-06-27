#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"

void thread_main(void* arg)
{
	int* argint = arg;
	printf(1, "hello child thread argument:%d\n", *argint);
	sleep(300);
	printf(1, "hello child thread byebye\n");
	return;
}

void 
get_padded_hex_addr(uint addr, char *buf)
{
  char map[16] = {
      '0',
      '1',
      '2',
      '3',
      '4',
      '5',
      '6',
      '7',
      '8',
      '9',
      'A',
      'B',
      'C',
      'D',
      'E',
      'F',
  };
  buf[0] = '0';
  buf[1] = 'x';
  buf[10] = '\0';
  for (int i = 9; i >= 2; i--)
  {
    buf[i] = map[addr % 16];
    addr = addr / 16;
  }
}

int main(int argc, char** argv)
{
	int id = 10;
	int tid;
	tid = thread_create(thread_main, &id);

	printf(1, "main thread started, tid of thread is: %d\n", tid);
	exit();
	printf(1, "hello main thread joined\n");

	exit();
}

