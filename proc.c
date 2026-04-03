#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

// CPS3250
// initialize global variables
int sched_policy = 1; // 0: default round robin; 1: multi-level feedback queue
int sched_trace_enabled = 1;
int running_threshold = 2;
int waiting_threshold = 4;

/*
 * Course: CPS3250
 *
 * Structure: mlfq
 * ---------------
 * mlfq is a ptable-like structure
 * includes two level queues: rr and wr
 *
*/
struct {
  struct proc *round_robin_queue[NPROC];
  struct proc *wait_ranking_queue[NPROC];
  int rr_process_count; // the number of processes in round_robin_queue
  int wr_process_count; // the number of processes in wait_ranking_queue
} mlfq;

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
  // CPS 3250 
  // initialize queue counters
  mlfq.rr_process_count = 0;
  mlfq.wr_process_count = 0;
}

/*
 * Course: CPS3250
 *
 * Function: set_rr_binding
 * ------------------------
 * change is_pinned_to_rr_queue of a process [pid]
 *
 * 0: bind
 * 1: unbind
 *
*/
int set_rr_binding(int pid, int pinned)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) // traverse ptable
  {
    if(p->pid == pid) // find out the process whose pid is input [pid]
    {
      p->is_pinned_to_rr_queue = pinned; // change its is_pinned_to_rr_queue
      return 0;
    }
  }

  return -1;
}

/*
 * Course: CPS3250
 *
 * Function: enqueue_rr
 * --------------------
 * enqueue a process to the end of round_robin_queue
 * rr_process_count+1
 *
*/
void enqueue_rr(struct proc *p)
{
  mlfq.round_robin_queue[mlfq.rr_process_count] = p;
  mlfq.rr_process_count++;
}

/*
 * Course: CPS3250
 *
 * Function: enqueue_wr
 * --------------------
 * enqueue a process to the end of wait_ranking_queue
 * wr_process_count+1
 *
*/
void enqueue_wr(struct proc *p)
{
  mlfq.wait_ranking_queue[mlfq.wr_process_count] = p;
  mlfq.wr_process_count++;
}

/*
 * Course: CPS3250
 *
 * Function: dequeue_rr
 * --------------------
 * dequeue a process from the round_robin_queue
 *
*/
void dequeue_rr(struct proc *p)
{
  for(int i = 0; i < mlfq.rr_process_count; i++) // traverse rr_queue
  {
    if(mlfq.round_robin_queue[i]->pid == p->pid) // find out the process [pid]
    {
      for(int j = i; j < mlfq.rr_process_count; j++) // remove the process from queue
      {
        mlfq.round_robin_queue[j] = mlfq.round_robin_queue[j+1]; // shift subsequent processes forward to fill the gap
      }
      mlfq.round_robin_queue[mlfq.rr_process_count] = 0; // reset the end of queue
      mlfq.rr_process_count--;

      p->running_counter = 0; // reset running_counter
      p->waiting_counter = 0; // reset waiting_counter

      break;
    }
  }
}

/*
 * Course: CPS3250
 *
 * Function: dequeue_wr
 * --------------------
 * dequeue a process from the wait_ranking_queue
 *
*/
void dequeue_wr(struct proc *p)
{
  for(int i = 0; i < mlfq.wr_process_count; i++) // traverse wr_queue
  {
    if(mlfq.wait_ranking_queue[i]->pid == p->pid) // find out the process [pid]
    {
      for(int j = i; j < mlfq.wr_process_count; j++) // remove the process from queue
      {
        mlfq.wait_ranking_queue[j] = mlfq.wait_ranking_queue[j+1]; // shift subsequent processes forward to fill the gap
      }
      mlfq.wait_ranking_queue[mlfq.wr_process_count] = 0; // reset the end of queue
      mlfq.wr_process_count--;

      p->running_counter = 0; // reset running_counter
      p->waiting_counter = 0; // reset waiting_counter

      break;
    }
  }
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;
  release(&ptable.lock);
  return 0;

found:
  // CPS3250
  // process enqueues rr_queue when it enters the system
  if(sched_policy == 1)
  {
    enqueue_rr(p);
  }

  p->state = EMBRYO;
  p->pid = nextpid++;
  // CPS3250
  // initialize the new process states
  p->is_pinned_to_rr_queue = 1;
  p->running_counter = 0;
  p->waiting_counter = 0;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;
  
  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;
  
  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];
  
  p = allocproc();
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  
  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  safestrcpy(np->name, proc->name, sizeof(proc->name));
 
  pid = np->pid;

  // lock to force the compiler to emit the np->state write last.
  acquire(&ptable.lock);
  np->state = RUNNABLE;
  release(&ptable.lock);
  
  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        // CPS3250
        // clear process from queue after completing
        dequeue_wr(p);
        dequeue_rr(p);

        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}

/*
 * Course: CPS3250
 *
 * Function: update_running_counter_for_demotion
 * ---------------------------------------------
 * update the chosen process running_counter
 * perform demotion if needed
 *
*/
void
update_running_counter_for_demotion(struct proc *p)
{
  p->running_counter++;

  if((p->running_counter >= running_threshold) && (p->pid != 1) && (p->pid != 2) && (p->is_pinned_to_rr_queue != 0))
  {
    dequeue_rr(p);
    enqueue_wr(p);
  }
}

/*
 * Course: CPS3250
 *
 * Function: update_waiting_counter_for_promotion
 * ----------------------------------------------
 * update all the other processes waiting_counter in wr_queue
 * perform promotion if needed
 *
*/
void
update_waiting_counter_for_promotion(struct proc *p)
{
  struct proc *q; // local temp variable to hold a process

  for(int i = 0; i < mlfq.wr_process_count; i++) // traverse wait_ranking_queue
  {
    q = mlfq.wait_ranking_queue[i]; // get the current process

    if (q->pid == p->pid) // check if the current process is the same as the argument process
    {
      continue; // skip the following code, back to for
    }

    q->waiting_counter++; // increment the waiting counter for this process

    //check if waiting_counter meets threshold or is pinned
    if (q->waiting_counter >= waiting_threshold || q->is_pinned_to_rr_queue == 0)
    {
      dequeue_wr(q);
      enqueue_rr(q);
    }
  }
}

/*
 * Course: CPS3250
 *
 * Function: find_index_of_max_wait
 * --------------------------------
 * find out the process with max waiting counter in wr_queue
 * return its index
 *
*/
int find_index_of_max_wait()
{
  // TODO: implement this function
  // steps break down:
    // traverse wait_ranking_queue
    // compare every process's waiting_counter
    // find out the one with the max waiting_counter
    // return its index in the wait_ranking_queue
    // or return -1 (not found)
  return -1;
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
/*
 * Course: CPS3250
 *
 * Function: sched1
 * ----------------
 * xv6 default rr scheduler
 *
*/
int
sched1(void)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){ // traverse ptable
      if(p->state != RUNNABLE) // choose a runnable process
        continue;

      // switch to the chosen process
      // switch back when completing a time slice
      proc = p;
      switchuvm(p);
      p->state = RUNNING;
      swtch(&cpu->scheduler, proc->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
  }

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){ // check runnable
      if(p->state == RUNNABLE)
	      return 1;
  }

  return 0;
}

/*
 * Course: CPS3250
 *
 * Function: sched2
 * ----------------
 * mlfq: rr + wr
 *
*/
int
sched2(void)
{
  struct proc *p;
  int rr_found = 0; // found flag

  // round_robin_queue
  for(int i = 0; i < mlfq.rr_process_count; i++) // traverse rr_queue
  {
    p = mlfq.round_robin_queue[i];

    if(p->state != RUNNABLE) // choose a runnable process
    {
      continue;
    }

    rr_found = 1;

    // switch to the chosen process
    // switch back when completing a time slice
    proc = p;
    switchuvm(p);
    p->state = RUNNING;
    swtch(&cpu->scheduler, proc->context);
    switchkvm();

    update_running_counter_for_demotion(p);
    update_waiting_counter_for_promotion(p);

    proc = 0;
  }

  // wait_ranking_queue
  if(rr_found == 0) // enter wr_queue only when no runnable process in rr_queue
  {
    int index_of_max_wait = find_index_of_max_wait();

    if(index_of_max_wait >= 0)
    {
      struct proc *q;
      q = mlfq.wait_ranking_queue[index_of_max_wait];

      proc = q;
      switchuvm(q);
      q->state = RUNNING;
      swtch(&cpu->scheduler, proc->context);
      switchkvm();

      update_waiting_counter_for_promotion(q);

      proc = 0;
    }
  }

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){ // check runnable
      if(p->state == RUNNABLE)
        return 1;
  }

  return 0;
}



/*
 * Course: CPS3250
 *
 * Function: scheduler
 * -------------------
 * original scheduler function is disassembled
 * to support two different schedulings: default rr and mlfq
 * default rr is put into sched1()
 *
*/
void
scheduler(void)
{
//  struct proc *p;
  int runnable_flag = 0;

  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    // CPS3250
    // switch between default rr and mlfq
    // based on variable sched_policy
    if(sched_policy == 0){
      runnable_flag = sched1();
    }else{
      runnable_flag = sched2();
    }
    release(&ptable.lock);

    if (runnable_flag == 0){
        halt();
    }
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  // CPS3250
  if (sched_trace_enabled && proc && proc->pid != 1 && (proc->pid != 2))
   	cprintf("[%d]", proc->pid);

  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  proc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot 
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }
  
  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  proc->chan = chan;
  proc->state = SLEEPING;
  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];
  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
