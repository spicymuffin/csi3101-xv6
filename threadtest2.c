#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"

volatile int s = 0;
int l = 0;

void child_thread(void* arg)
{
	int i;
	for (i=0; i<10000; ++i) {
		int x;
		mutex_lock(&l);
		x=s;
		x+=1;
		s=x;
		mutex_unlock(&l);
	}
	return;
}

int main(int argc, char** argv)
{
	int tid1, tid2;
	tid1 = thread_create(child_thread, 0);
	tid2 = thread_create(child_thread, 0);

	thread_join(tid1);
	thread_join(tid2);

	printf(1, "s=%d s should be 20000\n", s);

	exit();
}

