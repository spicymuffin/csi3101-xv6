#include "types.h"
#include "stat.h"
#include "user.h"


void mlfq_test()
{
  volatile unsigned int sum = 0;
  unsigned int i;
  int pid1, pid2;
  int long_start, long_end, short_start, short_end, inter_start, inter_end;

  // Long-running process
  long_start = uptime();

  for (i=0; i<200000000; i++) {
    sum += i;
    if (i == 80000000) {
      short_start = uptime();
      pid1 = fork();
      if (pid1 == 0) {
        // Short-running process
        volatile unsigned int short_sum = 1;
        unsigned int j;
        for (j=0; j<20000000; j++) {
          short_sum *= j;
        }
        short_end = uptime();
        printf(1, "Short process turnaround time: %d\n",  short_end - short_start);
        exit();
      } 

    } else if (i == 160000000) {
      inter_start = uptime();
      pid2 = fork();
      if (pid2 == 0) {
        // Interactive process
        for (int k=1; k<5; k++) {
	        sleep(1);
        }
        inter_end = uptime();
        printf(1, "Interactive process turnaround time: %d\n", inter_end - inter_start);
        exit();
      }

    }
    
  }

  long_end = uptime();
  printf(1, "Long process turnaround time: %d\n", long_end - long_start);
  wait();
  wait();
}

int main(int argc, char **argv)
{
  printf(1, "====Testing MLFQ Scheduler====\n");
  mlfq_test();
  printf(1, "====Finished Testing====\n");
  exit();
}
