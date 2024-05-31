#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"

volatile int s = 0;
int l[2] = {0};

void thread_main(void* arg)
{
	int i, a = (int)arg;
	for (i=0; i<10000; ++i) {
		int x;
		mutex_lock(&l[a]);
		x=s;
		x+=1;
		s=x;
		mutex_unlock(&l[a]);
	}
	return;
}

int main(int argc, char** argv)
{
	int pid, i, tid[10];

	pid = fork();

	for ( i=0; i < 10; ++i )
		tid[i] = thread_create(thread_main, (void*)(pid == 0));

	for ( i=0; i < 10; ++i )
		thread_join(tid[i]);
	
	if ( pid == 0 )
		printf(1, "s=%d s should be 100000\n", s);

	if ( pid != 0 ) {
		while (wait() != pid);
		printf(1, "s=%d s should be 100000\n", s);
	}

	exit();
}

