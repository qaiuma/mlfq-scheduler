/*
 * Course: CPS3250
 *
 * File: test3.c
 * Description: test demotion and promotion
 *
 * Comment:
 * set running threshold and waiting threshold to reasonable number
 * process B is forced to ALWAYS stay in RR queue
 * at the beginning
 * output: ......ABABABAB......BBBBBBB...... (A has demoted)
 * only B runs since it has higher priority
 * A accumulates its waiting_counter in wait_ranking_queue
 * when exceeding the waiting threshold, A moves back (promotes) to RR queue
 * output: ......ABABABABAB......
 *
 * overall output: ...ABABAB....BBBBBB....ABABABAB.....BBBBBB.....
 * the output should be the [pid]s of the processes
 *
*/

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

void
loop()
{
  int i;
  int j=0;
  for(i=0;i<1000000;i++)
    j=j+1;
}

void
forktest(void)
{
  int pid1;
  int ret;
  int fds1[2];

  ret = set_running_threshold(10);
  if (ret < 0)
  {
    printf(1, "cannot set running threshold\n");
    exit();
  }
  ret = set_waiting_threshold(20);
    if (ret < 0)
  {
    printf(1, "cannot set waiting threshold\n");
    exit();
  }

  ret = pipe(fds1);
  if ( ret < 0)
  {
    printf(1, "cannot create a pipe\n");
    exit();
  }

  pid1 = fork();
  if(pid1 < 0)
    return;
    
  if(pid1 == 0){
      int i;
      char buf[256];
      // block here
      close(fds1[1]);
      read(fds1[0], buf, 1);
      printf(1, "\n start process A [%d]\n", getpid());

      for (i=0;i<100;i++)
      {
        //printf(1, "Program C[%d] %d\n", getpid(), i);
	    loop();
      }
  }
  else
  {
      int i;
      close(fds1[0]);
      write(fds1[1],"Done", 5);
      printf(1, "\n start process B [%d]\n", getpid());

      // B process always in queue 0
      set_rr_binding(getpid(), 0);

      for (i=0;i<100;i++)
      {
        //printf(1, "Program A[%d] %d\n", getpid(), i);
        loop();
      }
      wait();
  }
}

int
main(void)
{
  forktest();
  exit();
}