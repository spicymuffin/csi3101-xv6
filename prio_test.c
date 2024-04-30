#include "types.h"
#include "stat.h"
#include "user.h"


void prio_test()
{ 
  int pid;
  
  printf(1, "====start of test 0: random processes====\n");
  printf(1, "[child was spawned]\n");
  pid = fork();
  if (pid == 0) {
    printf(1, "[child started execution]\n");
    ps();
    printf(1, "[child is setting prio to 4]\n");
    nice(2);
    printf(1, "[child started execution again]\n");
    ps();
    printf(1, "[child is exiting]\n");
    exit();
  } else {
    printf(1, "[parent started execution]\n");
    ps();
    printf(1, "[parent is setting prio to 3]\n");
    nice(1);
    printf(1, "[parent started execution again]\n");
    ps();
    printf(1, "[parent is waiting for child]\n");
    wait();
    printf(1, "[parent is terminating]\n");
  }
  printf(1, "====end of test 0: random processes====\n");
  // cleanup
  nice(-1);


  printf(1, "\n\n\n");


  printf(1, "====start of test 1: same priority processes====\n");
  printf(1, "[child is spawned]\n");
  pid = fork();
  if (pid == 0) {
    printf(1, "[child started execution]\n");
    printf(1, "[child is exiting]\n");
    exit();
  } else {
    printf(1, "[parent started execution]\n");
    ps();
    printf(1, "[parent is yielding]\n");
    yield();
    // child and parendt have same prio, but child has higher pid, so child
    // shouldnt have ran
    printf(1, "[parent started execution again, did child run in the meanwhile?]\n");
    printf(1, "[parent is setting prio to 4]\n");
    nice(2);
    printf(1, "[parent started execution again, child should've exited]\n");
    ps();
    printf(1, "[parent is waiting]\n");
    wait();
    printf(1, "[parent is terminating]\n");
  }
  printf(1, "====end of test 1: same priority processes====\n");
  // cleanup
  nice(-2);


  printf(1, "\n\n\n");

  printf(1, "====start of test 2: testing wakeup() preemptiveness====\n");
  printf(1, "[child is spawned]\n");
  pid = fork();
  if (pid == 0) {
    printf(1, "[child started execution]\n");
    printf(1, "[child is doing intensive task...]\n");
    
    for (int a = 0; a < 10; a++){
      printf(1, "0");
    }
    printf(1, "\n");

    printf(1, "[child is exiting]\n");
    exit();
  } else {
    printf(1, "[parent started execution]\n");
    ps();
    printf(1, "[parent is setting prio to 0]\n");
    nice(-2);
    ps();
    printf(1, "[parent is sleeping for 1 ticks]\n");
    sleep(1);
    printf(1, "[parent has woken up after sleep]\n");
    ps();
    printf(1, "[parent is waiting]\n");
    wait();
    printf(1, "[parent is terminating]\n");
  }
  printf(1, "====end of test 2: testing wakeup() preemptiveness====\n");
  // cleanup
  nice(2);
}



int main (int argc, char **argv)
{
  printf(1, "====testing priority scheduler====\n");
  prio_test();
  printf(1, "====finished testing====\n");
  exit();
}
