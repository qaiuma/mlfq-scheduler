/*
 * Course: CPS3250
 *
 * File: test2.c
 * Description: test wait_ranking_queue
 *
 * Comment:
 * set waiting threshold to a large number
 * so that the processes will not move back to RR queue after demotion
 * at the beginning, process C is forced to stay in RR queue
 * output: ......CCCCCCCCC...... (A and B have demoted)
 * when C is unbinded and demoted, A and B have accumulated their waiting_counter
 * output: ......ABABABAB...... (A and B have larger waiting_counter than C)
 * then after C accumulates its waiting_counter for a while
 * output: ......ABCABCABC...... (or similar patterns)
 *
 * overall output: .....CCCCCCCC....ABABABAB....ABCABCABC.....
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

  ret = set_running_threshold(2);
  if (ret < 0)
  {
    printf(1, "cannot set running threshold\n");
    exit();
  }
  ret = set_waiting_threshold(1000000);
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
    int pid2;
    int fds2[2];
    ret = pipe(fds2);

    ret = pipe(fds2);
    if (ret < 0)
    {
        printf(1, "cannot create the second pipe\n");
    }

    pid2 = fork();
    if(pid2 < 0)
      return;
    
    if(pid2 == 0){
      int i;
      char buf[256];
      close(fds1[0]);
      close(fds2[1]);
      //block here
      read(fds2[0], buf, 1);
      write(fds1[1], "Done", 5);
      printf(1, "\n start process B [%d]\n", getpid());

      for (i=0;i<100;i++)
      {
	    //printf(1, "Program B[%d] %d\n", getpid(), i);
        loop();
      }
    }
    else
    {
      int i;
      close(fds1[0]);
      close(fds1[1]);
      close(fds2[0]);
      write(fds2[1],"Done", 5);
      printf(1, "\n start process C [%d]\n", getpid());


      set_rr_binding(getpid(), 0);
      for (i=0;i<25;i++)
      {
        //printf(1, "Program A[%d] %d\n", getpid(), i);
        loop();
      }
      set_rr_binding(getpid(), 1);
      printf(1, "\n C prority changes\n");

      for (i=0;i<50;i++)
      {
        //printf(1, "Program A[%d] %d\n", getpid(), i);
        loop();
      }

      wait();
      wait();
    }
  }
}

int
main(void)
{
  forktest();
  exit();
}