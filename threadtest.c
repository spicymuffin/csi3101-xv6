#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"

void thread_main(void* arg)
{
	int* argint = arg;
	printf(1, "hello child thread argument:%d\n", *argint);
	sleep(300);
	printf(1, "hello child thread argument:%d\n", *argint);
	printf(1, "hello child thread byebye\n");
	return;
}

int main(int argc, char** argv)
{
	int id = 10;
	int tid;
	tid = thread_create(thread_main, &id);

	printf(1, "hello main thread\n");

	sleep(10);

	id = 228;

	thread_join(tid);

	printf(1, "hello main thread joined\n");

	exit();
}

