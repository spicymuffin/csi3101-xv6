#include "types.h"
#include "stat.h"
#include "user.h"

void mlfq_long()
{
  int long_start1, long_end1, long_start2, long_end2, long_start3, long_end3;
  int short_start, short_end;
  int med_start, med_end;
  int io_start, io_end;
  int pid1, pid2, pid3, pid4, pid5, pid6;

  // Long-running process1
  printf(1, "creating long1\n");
  long_start1 = uptime();
  pid1 = fork();
  if (pid1 == 0)
  {
    volatile unsigned int sum = 0;
    unsigned int i;
    for (i = 0; i < 500000000; i++)
    {
      sum += i;
      if (i == 300000000)
      {
        printf(1, "creating medium\n");
        med_start = uptime();
        pid4 = fork();
        if (pid4 == 0)
        {
          // medium-process
          volatile unsigned int sum = 1;
          unsigned int a;
          for (a = 0; a < 40000000; a++)
          {
            sum *= a;
          }
          med_end = uptime();
          printf(1, "Medium: %d\n", med_end - med_start);
          exit();
        }

        printf(1, "creating short\n");
        short_start = uptime();
        pid5 = fork();
        if (pid5 == 0)
        {
          // short-process
          volatile unsigned sum = 1;
          unsigned int b;
          for (b = 0; b < 20000000; b++)
          {
            sum *= b;
          }
          short_end = uptime();
          printf(1, "Short: %d\n", short_end - short_start);

          printf(1, "creating interactive\n");
          io_start = uptime();
          pid6 = fork();
          if (pid6 == 0)
          {
            // interactive process
            for (int c = 0; c < 5; c++)
            {
              sleep(2);
            }
            io_end = uptime();
            printf(1, "IO: %d\n", io_end - io_start);
            exit();
          }

          wait();
          exit();
        }
      }
    }
    long_end1 = uptime();
    printf(1, "Long1: %d\n", long_end1 - long_start1);
    wait();
    wait();
    exit();
  }

  // Long-running process2
  printf(1, "creating long3\n");
  long_start2 = uptime();
  pid2 = fork();
  if (pid2 == 0)
  {
    volatile unsigned int sum = 0;
    unsigned int j;
    for (j = 0; j < 200000000; j++)
    {
      sum += j;
    }
    long_end2 = uptime();
    printf(1, "Long2: %d\n", long_end2 - long_start2);
    exit();
  }

  // Long-running process3
  printf(1, "creating long3\n");
  long_start3 = uptime();
  pid3 = fork();
  if (pid3 == 0)
  {
    volatile unsigned int sum = 0;
    unsigned int k;
    for (k = 0; k < 100000000; k++)
    {
      sum += k;
    }
    long_end3 = uptime();
    printf(1, "Long3: %d\n", long_end3 - long_start3);
    exit();
  }

  wait();
  wait();
  wait();
}

int main(int argc, char **argv)
{
  printf(1, "====Testing MLFQ Scheduler====\n");
  mlfq_long();
  printf(1, "====Finished Testing====\n");
  exit();
}
