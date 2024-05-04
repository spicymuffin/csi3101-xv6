#include "types.h"
#include "stat.h"
#include "user.h"


void mlfq_test()
{
  int pid;

  printf(1, "====start of test 0: testing essence of round robin====\n");
  printf(1, "[child is spawned]\n");
  pid = fork();
  if (pid == 0) {
    printf(1, "[child started execution]\n");
    ps();
    printf(1, "[child is performing an intensive task...]\n");
    
    for (int a = 0; a < 150; a++){
      volatile unsigned int short_sum = 1;
      unsigned int j;
      for (j=0; j<2000000; j++) {
        short_sum *= j;
      }
      printf(1, "c");
    }

    printf(1, "\n");

    printf(1, "[child has finished the intensive task]\n");
    ps();

    printf(1, "[child is exiting]\n");
    exit();


  } else {
    printf(1, "[parent started execution]\n");
    ps();
    printf(1, "[parent is performing an intensive task...]\n");
    
    for (int a = 0; a < 150; a++){
      volatile unsigned int short_sum = 1;
      unsigned int j;
      for (j=0; j<2000000; j++) {
        short_sum *= j;
      }
      printf(1, "p");
    }

    printf(1, "\n");

    printf(1, "[parent has finished the intensive task]\n");
    ps();

    printf(1, "[parent is waiting]\n");
    wait();
    printf(1, "[parent is terminating]\n");
  }
  printf(1, "====end of test 0: testing essence of round robin====\n");



  printf(1, "\n\n\n");



  printf(1, "====start of test 1: time slice expiration====\n");
  printf(1, "[child is spawned]\n");
  pid = fork();
  if (pid == 0) {
    printf(1, "[child started execution]\n");
    ps();
    printf(1, "[child is doing performing an intensive task...]\n");
    
    for (int a = 0; a < 8; a++){
      volatile unsigned int short_sum = 1;
      unsigned int j;
      for (j=0; j<2000000; j++) {
        short_sum *= j;
      }
      ps();
    }

    printf(1, "\n");

    printf(1, "[child has finished the intensive task]\n");
    ps();

    printf(1, "[child is exiting]\n");
    exit();
  } else {
    printf(1, "[parent started execution]\n");
    printf(1, "[parent is waiting]\n");
    wait();
    printf(1, "[parent is terminating]\n");
  }
  printf(1, "====start of test 1: time slice expiration====\n");



  printf(1, "\n\n\n");



  printf(1, "====start of test 2: yield time slice expiration prevention====\n");
  printf(1, "[child is spawned]\n");
  pid = fork();
  if (pid == 0) {
  printf(1, "[child started execution]\n");
  ps();
  printf(1, "[child is performing an intensive task...]\n");
  
  for (int a = 0; a < 8; a++){
    volatile unsigned int short_sum = 1;
    unsigned int j;
    for (j=0; j<2000000; j++) {
      short_sum *= j;
    }
    ps();
    yield();
  }

  printf(1, "\n");

  printf(1, "[child has finished the intensive task]\n");
  ps();

  printf(1, "[child is exiting]\n");
  exit();
  } else {
    printf(1, "[parent started execution]\n");
    printf(1, "[parent is waiting]\n");
    wait();
    printf(1, "[parent is terminating]\n");
  }

  printf(1, "====end of test 2: yield time slice expiration prevention====\n");


  
  printf(1, "\n\n\n");



  printf(1, "====start of test 3: testing processes with different priorities====\n");
  printf(1, "[child is spawned]\n");
  pid = fork();
  if (pid == 0) {
    printf(1, "[child started execution]\n");
    ps();
    printf(1, "[child is performing an intensive task...]\n");
    
    for (int a = 0; a < 50; a++){
      volatile unsigned int short_sum = 1;
      unsigned int j;
      for (j=0; j<2000000; j++) {
        short_sum *= j;
      }
      printf(1, "c");
      yield();
    }

    printf(1, "\n");

    printf(1, "[child has finished the intensive task]\n");
    ps();

    printf(1, "[child is exiting]\n");
    exit();
  } else {
    printf(1, "[parent started execution]\n");
    ps();
    printf(1, "[parent is performing an intensive task...]\n");
    
    for (int a = 0; a < 50; a++){
      volatile unsigned int short_sum = 1;
      unsigned int j;
      for (j=0; j<2000000; j++) {
        short_sum *= j;
      }
      printf(1, "p");
    }

    printf(1, "\n");

    printf(1, "[parent has finished the intensive task]\n");
    ps();
    printf(1, "[parent is waiting]\n");
    wait();
    printf(1, "[parent is terminating]\n");
  }

  printf(1, "====end of test 3: testing processes with different priorities====\n");
}

int main(int argc, char **argv)
{
  printf(1, "====Testing MLFQ Scheduler====\n");
  mlfq_test();
  printf(1, "====Finished Testing====\n");
  exit();
}
