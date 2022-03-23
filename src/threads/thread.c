#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
/* To keep track of timer ticks*/

#include "devices/timer.h"

/*For using fixed point mathematics to get accuracy*/

#include "threads/fixed-point.h"

#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/*A waiting queue of threads sleeping for a fixed amount of time*/
static struct list sleeping_threads;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

/*Function prototypes added*/

/*For task-1*/
void thread_sleep(int64_t) ;
void thread_wakeup(void) ;

/*For MLFQS scheduler*/
void update_load_avg(void) ;
void update_thread_recent_cpu (struct thread*) ;
void update_all_recent_cpu (struct thread*) ;
void update_thread_priority(struct thread*) ;
void change_thread_priority (struct thread *t, int new_priority);
void update_priority (struct thread *t, int old_priority, int new_priority);
struct thread* get_max_thread (struct list *list);
void update_all_priorities (void); 

/*For Priority Donation*/
void update_donated_priority (struct thread *t, int old_priority, int new_priority);
void decrement_donated_priorities (struct thread *t, struct list *donars);



/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);
  
  /*Initialising sleeping_threads queue*/
  list_init(&sleeping_threads) ;
  
  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
  
  /*Initialising load average to 0 during system boot*/
  
  load_avg=0 ;
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();
  /* Implementing parts for advanced scheduler*/
  if(thread_mlfqs)
  {
      if(t!=idle_thread)
      {
          t->recent_cpu = add_int_fixed_point(t->recent_cpu , 1) ;
      }
      
      int64_t ticks = timer_ticks() ;
      
      /*  updating load average, all threads recent_cpu time and priorites after a second has passed */
      if(ticks%(get_timer_frequency())==0) 
      {
          update_load_avg() ;
          update_all_recent_cpu() ;
          update_all_priority() ;
      }
      
      if(ticks%4 == 0)
      {
          update_thread_priority(t) ;
      }
  }
  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}


void
update_load_avg()
{
    int ready_threads = list_size(&ready_list) ;
    
    int first_term=0 ;
    first_term = int_to_fixed_point(59) ;
    first_term = divide_int_fixed_point(first_term,60) ;
    first_term = multiply_fixed_point(first_term,load_avg) ;
    
    int second_term=0 ;
    second_term = int_to_fixed_point(ready_threads) ;
    second_term = divide_int_fixed_point(first_term,60) ;
    
    load_avg = add_fixed_point(first_term,second_term) ;
    
    return  ;
}

void 
update_thread_recent_cpu (struct thread* t)
{
    int first_term_numerator = multiply_int_fixed_point(load_avg , 2) ;
    int first_term_denominator = add_int_fixed_point( first_term_numerator , 1) ;
    int first_term = divide_fixed_point(first_term_numerator , first_term_denominator) ;
    int first_term = multiply_fixed_point(first_term , t->recent_cpu) ;
    int second_term = t->nice ;
    t->recent_cpu = add_fixed_point(first_term , second_term) ;
    return ;
}

void
update_all_recent_cpu (struct thread* t)
{
    
    struct list_elem* ele ;
    
    for(ele = list_begin(&all_list) ; list_next(ele) != NULL ; ele = list_next(ele)  )
    {
        struct thread* t = list_entry( ele , struct thread , allelem ) ;
        update_thread_recent_cpu(t) ;
    }
    
    return  ;
}

void 
update_thread_priority(struct thread* t)
{
    int first_term = int_to_fixed_point(t->pre_computed_priority);
    int second_term = divide_int_fixed_point(t->recent_cpu , 4) ;
    int priority = subtract_fixed_point(first_term , second_term) ;
    t->max_priority = fixed_point_to_int(priority) ;
    
    return  ;
}

void 
update_all_priority()
{
    struct list_elem* ele ;
    
    for(ele = list_begin(&all_list) ; list_next(ele) != NULL ; ele = list_next(ele)  )
    {
        struct thread* t = list_entry( ele , struct thread , allelem ) ;
        update_thread_priority (t) ;
    }
    
    return  ;
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;
  
  enum intr_level old_level ;
  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();
  
  /*disabling interrupts as we are working on kernel*/
  
  old_level = intr_disable() ;
  
  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;
  
  /*enabling interrupts*/
  intr_set_level(old_level) ;
  
  /* Add to run queue. */
  thread_unblock (t);
  
  if(t->max_priority > thread_current()->max_priority)
  thread_yield() ;
  
  return tid;
}


/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  list_push_back (&ready_list, &t->elem);
  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/*Implementing first task function*/

bool
compare_threads_wakeup_time (const struct list_elem *e_one, const struct list_elem *e_two , void *aux)
{
    struct thread *t_one = list_entry (e_one, struct thread, elem);
    struct thread *t_two = list_entry (e_two, struct thread, elem);
    return t_one->wakeup_time < t_two->wakeup_time;
}

void
thread_sleep(int64_t ticks)
{
    int64_t start = timer_ticks ();
    ASSERT (intr_get_level () == INTR_ON);
	enum intr_level old_level = intr_disable ();

    struct thread *current_thread = thread_current ();
    current_thread->wakeup_time = start + ticks;
	list_insert_ordered (&sleeping_threads, &current_thread->elem, &compare_threads_wakeup_time, NULL);
	thread_block ();
	intr_set_level (old_level);
}

void
thread_wakeup ()
{
    int64_t time_now = timer_ticks ();
	ASSERT (intr_get_level () == INTR_OFF);
	
	if (list_empty (&sleeping_threads))
	   return;
	
    struct list_elem  *e ;
    while (!list_empty (&sleeping_threads)) 
	{
	    e = list_begin (&sleeping_threads);
	    struct thread *t = list_entry (e, struct thread, elem);
	    if (t->wakeup_time <= time_now)
	    {
	      list_pop_front (&sleeping_threads);
	      thread_unblock (t);
	    }
	    else break;
	}
	return ;
}
/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread) 
    list_push_back (&ready_list, &cur->elem);
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{
  thread_current ()->priority = new_priority;
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice) 
{
  ASSERT(thread_mlfqs) ;
  
  struct thread* t ;
  t=thread_current() ;
  t->nice = nice ;
  t->pre_computed_priority = PRI_MAX - 2*nice ;
  update_thread_priority(t) ;
  thread_yield() ;
  
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  ASSERT(thread_mlfqs) ;
  
  return thread_current()->nice  ;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{
  ASSERT(thread_mlfqs) ;
  
  int current_load_avg = multiply_int_fixed_point(load_avg,100) ;
  return nearest_int(current_load_avg) ;
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
  ASSERT(thread_mlfqs) ;
  
  int recent_cpu_time = multiply_int_fixed_point(thread_current->recent_cpu,100) ;
  return nearest_int(recent_cpu_time) ;
}


/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  enum intr_level old_level;

  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;
  t->magic = THREAD_MAGIC;
  
  /*initialization of added properties of thread*/
  t->waiting_lock = NULL ;
  memset(t->donated_priorities,0,sizeof(t->donated_priorities)) ;
  t->max_priority = priority ;
  t->donated_priorities[priority]++ ;
  t->pre_computed_priority = PRI_MAX - 2 * (t->nice) ;
  lock_init(&t->priority_lock) ;
  set_recent_cpu_and_nice(t) ;
  
  old_level = intr_disable ();
  list_push_back (&all_list, &t->allelem);
  intr_set_level (old_level);
}

void 
set_recent_cpu_and_nice (thread* t)
{
    if(t!=initial_thread)
    {
        t->recent_cpu = thread_current()->recent_cpu ;
        t->nice = thread_current()->nice ;
    }
}

struct thread*
get_max_thread(struct list* list)
{
    int cur_max_priority = PRI_MIN ;
    struct list_elem* e = list_begin(list) ;
    struct thread* t = list_entry(e,struct thread,elem) ;
    while(e != list_end(list))
    {
        thread* temp = list_entry (e, struct thread, elem);
        if(temp->max_priority > cur_max_priority)
        {
            cur_max_priority = temp->max_priority ;
            t=temp ;
        }
        
        e = list_next(e) ;
    }
    return t ;
    
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* A function that return max_priority thread*/
struct thread *
get_max_thread (struct list *list)
{
  int max_priority = PRI_MIN;

  // loop though list
  struct list_elem *e = list_begin (list);
  struct thread *t = list_entry (e, struct thread, elem);
  while (e != list_end (list))
  {
    struct thread *tmp_thread = list_entry (e, struct thread, elem);
    if (tmp_thread->max_priority > max_priority)
    {
      max_priority = tmp_thread->max_priority;
      t = tmp_thread;
    }
    e = list_next(e);
  }
  return t;
}
/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void) 
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}
/* Priority donations functions*/

void 
update_max_priority(thread* t)
{
    int cur_max_priority= t->max_priority ;
    
    for(int i=cur_max_priority ;i<PRI_MAX;i++)
    {
        if(t->donated_priority[i]>0 )
        t->max_priority = i;
    }
}
/* Here the concept of locks is used.
We firstly acquire the priority_lock of the thread because if some other thread tries to change the priority of the thread t at same time while the lock 
is acquired ,It wont be able to do so and hence values are not compromised.  
The function recursively finds the thread which holds an independent lock and donates the current thread priority 
to it. So that lock gets released and there is no starvation.
*/

void 
update_priority(thread* t,int cur_priority,int new_priority)
{
    lock_acquire(&t->priority_lock) ;
    t->donated_priorities[cur_priority]-- ;
    t->donated_priorities[new_priority]++ ;
    
    int prev_max_priority = t->max_priority ;
    update_max_priority(t);
    
    if(t->waiting_lock != NULL && t->max_priority > prev_max_priority)
    {
        update_priority(t->waiting_lock->holder, prev_max_priority, t->max_priority);
    }
    lock_release(&t->priority_lock) ;
}

/*If we follow priority scheduler firstly for each changing priority we call change_thread_priority*/
void 
change_thread_priority(thread* t , int new_priority)
{
    int cur_priority = t->priority ;
    t->priority = new_priority ;
    update_priority(t,cur_priority,new_priority) ;
}

void 
donate_priority(thread* t, int new_priority)
{
    ASSERT(new_priority<=PRI_MAX);
    t->donated_priorities[new_priority]++ ;
    
    if(t->waiting_lock !=NULL && t->max_priority <new_priority)
    {
        update_priority(t->waiting_lock->holder, t->max_priority, new_priority) ;
    }
    
    if(t->max_priority < new_priority)
    t->max_priority = new_priority ;
}

/* This function will let go the priorities which were obtained from other threads and deletes it*/
void 
decrement_donated_priorities (thread* t,struct list* doners)
{
    struct list_elem* doner = list_begin(doners) ;
    
    while(doner != list_end(doners))
    {
        thread* t_donar = list_entry(doner ,struct thread, elem) ;
        t->donated_priorities[t_doner->max_priority]-- ;
        doner = list_next(doner) ;
    }
}

void 
remove_donated_priorities (thread* t, struct list* doners)
{
    ASSERT(!thread_mlfqs) ;
    decrement_donated_priorities(t,doners) ;
    update_max_priority(t) ;
}
/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);
