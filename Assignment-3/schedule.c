#include <context.h>
#include <memory.h>
#include <schedule.h>
#include <idt.h>
#include <apic.h>
#include <lib.h>
static struct exec_context *new_ctx;
static u64 numticks;

static void save_current_context()
{
  /*Your code goes in here*/ 
} 

static void schedule_context(struct exec_context *next)
{  
  /*Your code goes in here. get_current_ctx() still returns the old context*/
 struct exec_context *current = get_current_ctx();
 printf("schedluing: old pid = %d  new pid  = %d\n", current->pid, next->pid); /*XXX: Don't remove*/
 new_ctx = next;
/*These two lines must be executed*/
 // Better run them later
 // set_tss_stack_ptr(next);
 // set_current_ctx(next);
 return;
}

static struct exec_context *pick_new_ctx(struct exec_context *list)
{
  /*Your code goes in here*/
  struct exec_context *current = get_current_ctx(); 
  int i = current->pid + 1;
  while(i != current->pid){
    // If current PID was zero
    if(i==0){
      i++; continue;
    }
    if(i==MAX_PROCESSES){
      i=0;  continue;
    }
    if(list[i].state == RUNNING || list[i].state == READY )
      return (list+i);
    i++;
  }

  if(list[current->pid].state == RUNNING || list[current->pid].state == READY){
      return (list+(current->pid));
  }
  return list;
}

static void schedule()
{ 
 struct exec_context *current = get_current_ctx(); 
 struct exec_context *list = get_ctx_list();
 new_ctx = pick_new_ctx(list);
 schedule_context(new_ctx);     
}


static void do_sleep_and_alarm_account()
{
 /*All processes in sleep() must decrement their sleep count*/ 
  // Decrease ticks and ready the process
}



/*The five functions above are just a template. You may change the signatures as you wish*/
void handle_timer_tick()
{
 /*
   This is the timer interrupt handler. 
   You should account timer ticks for alarm and sleep
   and invoke schedule
 */
  // Save registers
    asm volatile  
  ( 
    "push %r8;"   "push %r9;"   "push %r10;"  "push %r11;"  "push %r12;" 
    "push %r13;"    "push %r14;"    "push %r15;"    "push %rsi;"    "push %rdi;"
    "push %rax;"    "push %rbx;"    "push %rcx;"    "push %rdx;" 
   );

  u64 rsp,rbp;
  
  asm volatile  ("mov %%rsp, %0;" 
    : "=r" (rsp));  
  asm volatile  ("mov %%rbp, %0;" 
    : "=r" (rbp));

  u64* ustackp = (u64*)((u64*)rbp +4) ;
  u64* urip = (u64*)((u64*)rbp +1) ;


  printf("Got a tick. #ticks = %u\n", ++numticks);   /*XXX Do not modify this line*/

  struct exec_context *current = get_current_ctx(); 
  new_ctx = current;

  struct exec_context *list = get_ctx_list();

  if(current->ticks_to_alarm > 0 ){
    // Decrement timer count
    current->ticks_to_alarm = current->ticks_to_alarm - 1;

    if(current->ticks_to_alarm == 0 && current->sighandlers[SIGALRM]>0){
      current->ticks_to_alarm = current->alarm_config_time;
      invoke_sync_signal(SIGALRM,ustackp,urip);
        /*acknowledge the interrupt, next interrupt */
      ack_irq();
      // Restore registers
      asm volatile  (
      "mov %0, %%rsp;"  "pop %%rdx;"  "pop %%rcx;"  "pop %%rbx;"  "pop %%rax;"
      "pop %%rdi;"    "pop %%rsi;"    "pop %%r15;"    "pop %%r14;"    "pop %%r13;"
      "pop %%r12;"    "pop %%r11;"    "pop %%r10;"    "pop %%r9;"     "pop %%r8;"
      : : "r" (rsp)   
      );
      asm volatile("mov %%rbp, %%rsp;"
               "pop %%rbp;"
               "iretq;"
               :::"memory");
    }
    
  }
  
  // printf("%x\n", (*(list+1)).pid);
  for(int i=1;i<MAX_PROCESSES;i++){
    if((*(list+i)).ticks_to_sleep > 0){
      (*(list+i)).ticks_to_sleep = (*(list+i)).ticks_to_sleep - 1;
      if( (*(list+i)).ticks_to_sleep == 0 ){
        // Make ready this process
        // printf("%s\n","Ready some process" );
        (*(list+i)).state = READY;
      }
    } 
  }

  // This sets the global new_ctx
  new_ctx = pick_new_ctx(list);
  schedule_context(new_ctx); 

  if(new_ctx->pid == current->pid){
    ack_irq();
      // Restore registers
      asm volatile  (
      "mov %0, %%rsp;"  "pop %%rdx;"  "pop %%rcx;"  "pop %%rbx;"  "pop %%rax;"
      "pop %%rdi;"    "pop %%rsi;"    "pop %%r15;"    "pop %%r14;"    "pop %%r13;"
      "pop %%r12;"    "pop %%r11;"    "pop %%r10;"    "pop %%r9;"     "pop %%r8;"
      : : "r" (rsp)   
      );
      asm volatile("mov %%rbp, %%rsp;"
               "pop %%rbp;"
               "iretq;"
               :::"memory");
  }

  u64 *nrbp = (u64*)rbp;
  u64 *nrsp = (u64*)rsp;

  current->regs.rbp = *(nrbp);
  current->regs.entry_rip = *(nrbp+1);
  current->regs. entry_cs = *(nrbp+2);
  current->regs.entry_rflags = *(nrbp+3);
  current->regs.entry_rsp = *(nrbp+4);
  current->regs.entry_ss = *(nrbp+5);

  current->regs.r8 = *(nrsp+13);
  current->regs.r9 = *(nrsp+12);      
  current->regs.r10 = *(nrsp+11);
  current->regs.r11 = *(nrsp+10);
  current->regs.r12 = *(nrsp+9);
  current->regs.r13 = *(nrsp+8);
  current->regs.r14 = *(nrsp+7);
  current->regs.r15 = *(nrsp+6);

  current->regs.rsi = *(nrsp+5);
  current->regs.rdi = *(nrsp+4);

  current->regs.rax = *(nrsp+3);
  current->regs.rbx = *(nrsp+2);
  current->regs.rcx = *(nrsp+1);
  current->regs.rdx = *(nrsp);

  // printf("%x\n", new_ctx->pid);

  current->state = READY;
  new_ctx->state = RUNNING;

  *(nrbp) = new_ctx->regs.rbp;
  *(nrbp+1) = new_ctx->regs.entry_rip;
  *(nrbp+2) = new_ctx->regs.entry_cs;
  *(nrbp+3) = new_ctx->regs.entry_rflags;
  *(nrbp+4) = new_ctx->regs.entry_rsp;
  *(nrbp+5) = new_ctx->regs.entry_ss;

  *(nrsp+1) = new_ctx->regs.r8;
  *(nrsp+2) = new_ctx->regs.r9;
  *(nrsp+3) = new_ctx->regs.r10;
  *(nrsp+4) = new_ctx->regs.r11;
  *(nrsp+5) = new_ctx->regs.r12;
  *(nrsp+6) = new_ctx->regs.r13;
  *(nrsp+7) = new_ctx->regs.r14;
  *(nrsp+8) = new_ctx->regs.r15;

  *(nrsp+9) = new_ctx->regs.rsi;
  *(nrsp+10) = new_ctx->regs.rdi;

  // *(nrsp+11) = new_ctx->regs.rax;
  *(nrsp+11) = new_ctx->regs.rbx;
  *(nrsp+12) = new_ctx->regs.rcx;
  *(nrsp+13) = new_ctx->regs.rdx;

  set_tss_stack_ptr(new_ctx);
  set_current_ctx(new_ctx);
  /*acknowledge the interrupt, next interrupt */
  ack_irq();
  asm volatile  (
  "mov %0, %%rsp;"  "pop %%rdx;"  "pop %%rcx;"  "pop %%rbx;"  "pop %%rax;"
  "pop %%rdi;"    "pop %%rsi;"    "pop %%r15;"    "pop %%r14;"    "pop %%r13;"
  "pop %%r12;"    "pop %%r11;"    "pop %%r10;"    "pop %%r9;"     "pop %%r8;"
  : : "r" (rsp)   
  );
  asm volatile("mov %%rbp, %%rsp;"
           "pop %%rbp;"
           "iretq;"
           :::"memory");
  
}
void do_exit()
{
  /*You may need to invoke the scheduler from here if there are
    other processes except swapper in the system. Make sure you make 
    the status of the current process to UNUSED before scheduling 
    the next process. If the only process alive in system is swapper, 
    invoke do_cleanup() to shutdown gem5 (by crashing it, huh!)
    */
    u64 rbp;
    asm volatile  ("mov %%rbp, %0" : "=r" (rbp));  
    rbp = *((u64 *)rbp);
    struct exec_context *current = get_current_ctx();
    struct exec_context *list = get_ctx_list(); 
    // Keeping in mind the condition above
    current->state = UNUSED;
    os_pfn_free(OS_PT_REG,current->os_stack_pfn);

    int isunused=0;
    int i=1;
    // Iterate and check if process some process in UNUSED state
    while(i<MAX_PROCESSES){
    if(i==current->pid){
      i++;
      continue;
    }
    else if(list[i].state != UNUSED){
      isunused++;
    }
    i++;
  }

  if(isunused == 0){
    do_cleanup();
    return;
  }
  else{
    struct exec_context *list = get_ctx_list();
    new_ctx = pick_new_ctx(list);
    schedule_context(new_ctx); 
    new_ctx->state = RUNNING;
    set_tss_stack_ptr(new_ctx);
    set_current_ctx(new_ctx);

    u64 *nrbp = (u64*)rbp;
    *(nrbp+1) = new_ctx->regs.rax;
    *(nrbp+2) = new_ctx->regs.r15;
    *(nrbp+3) = new_ctx->regs.r14;
    *(nrbp+4) = new_ctx->regs.r13;
    *(nrbp+5) = new_ctx->regs.r12;
    *(nrbp+6) = new_ctx->regs.r11;
    *(nrbp+7) = new_ctx->regs.r10;
    *(nrbp+8) = new_ctx->regs.r9;
    *(nrbp+9) = new_ctx->regs.r8;

    *(nrbp+10) = new_ctx->regs.rbp;
    *(nrbp+11) = new_ctx->regs.rdi;
    *(nrbp+12) = new_ctx->regs.rsi;
    *(nrbp+13) = new_ctx->regs.rdx;
    *(nrbp+14) = new_ctx->regs.rcx;
    *(nrbp+15) = new_ctx->regs.rbx;

    *(nrbp+16) = new_ctx->regs.entry_rip;
    *(nrbp+17) = new_ctx->regs.entry_cs;
    *(nrbp+18) = new_ctx->regs.entry_rflags;
    *(nrbp+19) = new_ctx->regs.entry_rsp;
    *(nrbp+20) = new_ctx->regs.entry_ss;

    rbp = rbp+8;
    asm volatile  (
      "mov %0, %%rsp;"
      "pop %%rax;"
      "pop %%r15;"
      "pop %%r14;"
      "pop %%r13;"
      "pop %%r12;"
      "pop %%r11;"
      "pop %%r10;"
      "pop %%r9;"
      "pop %%r8;"
      "pop %%rbp;"
      "pop %%rdi;"
      "pop %%rsi;"
      "pop %%rdx;"
      "pop %%rcx;"
      "pop %%rbx;"
      "iretq"     
      :: "r" (rbp)      
      :"memory" 
      );
    
  }

    /*Call this conditionally, see comments above*/
}

/*system call handler for sleep*/
long do_sleep(u32 ticks)
{
  u64 *rbp;
  asm volatile  ("mov %%rbp, %0;"
          : "=r" (rbp));
  u64* sys_rbp = (u64*)(*rbp);
  // printf("%x\n",rbp );
  // printf("%x\n", sys_rbp);

  struct exec_context *current = get_current_ctx();
  // Load swapper process
  struct exec_context *swapper = get_ctx_by_pid(0);

  current->ticks_to_sleep = ticks;
  if(current->ticks_to_sleep > 0)
    current->state = WAITING;

  current->regs.r15 = *(sys_rbp + 2 );
  current->regs.r14 = *(sys_rbp + 3 );
  current->regs.r13 = *(sys_rbp + 4 );
  current->regs.r12 = *(sys_rbp + 5 );
  current->regs.r11 = *(sys_rbp + 6 );
  current->regs.r10 = *(sys_rbp + 7 );
  current->regs.r9 = *(sys_rbp + 8 );
  current->regs.r8 = *(sys_rbp + 9 );
  current->regs.rbp = *(sys_rbp + 10 );

  current->regs.rdi = *(sys_rbp + 11 );
  current->regs.rsi = *(sys_rbp + 12 );
  current->regs.rdx = *(sys_rbp + 13 );
  current->regs.rcx = *(sys_rbp + 14 );
  current->regs.rbx = *(sys_rbp + 15 );

  current->regs.entry_rip = *(sys_rbp + 16 );
  current->regs.entry_cs = *(sys_rbp + 17 );
  current->regs.entry_rflags = *(sys_rbp + 18 );
  current->regs.entry_rsp = *(sys_rbp + 19 );
  current->regs.entry_ss = *(sys_rbp + 20 );


  swapper->state = RUNNING;
  *(sys_rbp+2) = swapper->regs.r15;
  *(sys_rbp+3) = swapper->regs.r14;
  *(sys_rbp+4) = swapper->regs.r13;
  *(sys_rbp+5) = swapper->regs.r12;
  *(sys_rbp+6) = swapper->regs.r11;
  *(sys_rbp+7) = swapper->regs.r10;
  *(sys_rbp+8) = swapper->regs.r9;
  *(sys_rbp+9) = swapper->regs.r8;
  *(sys_rbp+10) = swapper->regs.rbp;

  *(sys_rbp+11) = swapper->regs.rdi;
  *(sys_rbp+12) = swapper->regs.rsi;
  *(sys_rbp+13) = swapper->regs.rdx;
  *(sys_rbp+14) = swapper->regs.rcx;
  *(sys_rbp+15) = swapper->regs.rbx;

  *(sys_rbp+16) = swapper->regs.entry_rip;
  *(sys_rbp+17) = swapper->regs.entry_cs;
  *(sys_rbp+18) = swapper->regs.entry_rflags;
  *(sys_rbp+19) = swapper->regs.entry_rsp;
  *(sys_rbp+20) = swapper->regs.entry_ss;

  set_tss_stack_ptr(swapper);
  set_current_ctx(swapper);
  return 1;
}

/*
  system call handler for clone, create thread like 
  execution contexts
*/
long do_clone(void *th_func, void *user_stack)
{
    u64 rbp;
    asm volatile  ("mov %%rbp, %0" : "=r" (rbp));
    rbp = *((u64 *)rbp);

    u64 r_flags = *(((u64*)rbp)+18);
 
    struct  exec_context * cloned_context = get_new_ctx();
    struct exec_context *current = get_current_ctx();

    cloned_context->os_stack_pfn = os_pfn_alloc(OS_PT_REG);

    int i=0;
    while(current->name[i]!='\0'){
      cloned_context->name[i] = current->name[i];
      i++;
    }
    cloned_context->name[i] = '\0';

    u64 name_len=0;
    while(cloned_context->name[name_len]!='\0')
      name_len++;

    if(cloned_context->pid>9){
      int first = (cloned_context->pid)/10;
      int second = (cloned_context->pid)%10;
      cloned_context->name[name_len++]='0'+ first;
      cloned_context->name[name_len++]='0'+ second;
      cloned_context->name[name_len]='\0';
    }  
    else{
      int first = cloned_context->pid;
      cloned_context->name[name_len++]='0'+first;
      cloned_context->name[name_len]='\0';
    }

    // Copied from parent
    cloned_context->type = current->type;

    cloned_context->used_mem = current->used_mem;

    cloned_context->pgd = current->pgd;

    for(int i=0;i<MAX_MM_SEGS;i++)
      cloned_context->mms[i] = current->mms[i];

    for(int i=0;i<MAX_SIGNALS;i++)
      cloned_context->sighandlers[i] = current->sighandlers[i];

    // Initialized
    cloned_context->os_rsp = (u64)osmap(cloned_context->os_stack_pfn);
    cloned_context->state = READY;
    cloned_context->pending_signal_bitmap = 0;

    cloned_context->regs.entry_rip = (u64) th_func; 
    cloned_context->regs.rbp = (u64)user_stack;
    cloned_context->regs.entry_rsp = (u64) user_stack;
    cloned_context->regs.entry_rflags = r_flags;
    cloned_context->regs.entry_ss = 0x2b;
    cloned_context->regs.entry_cs = 0x23;
    return 0;
}


long invoke_sync_signal(int signo, u64 *ustackp, u64 *urip)
{
   /*If signal handler is registered, manipulate user stack and RIP to execute signal handler*/
   /*ustackp and urip are pointers to user RSP and user RIP in the exception/interrupt stack*/
   printf("Called signal with ustackp=%x urip=%x\n", *ustackp, *urip);
   /*Default behavior is exit( ) if sighandler is not registered for SIGFPE or SIGSEGV.
  Ignore for SIGALRM*/

  struct exec_context *current = get_current_ctx();
  // Will check registered condition later
  if(current->sighandlers[signo] > 0){
    // Handle signal else exit
    u64 **stackp = (u64**)ustackp;
    *stackp = *stackp - 1;
    **stackp = *urip;
    *urip = (u64)current->sighandlers[signo];
    return 0;
  }


  if(signo != SIGALRM)
    do_exit();
}

/*system call handler for signal, to register a handler*/
long do_signal(int signo, unsigned long handler)
{
  struct exec_context *current = get_current_ctx();
  // printf("register %x\n", handler);
  current->sighandlers[signo] = (void *)handler;
  return 0;
}

/*system call handler for alarm*/
long do_alarm(u32 ticks)
{
  struct exec_context *current = get_current_ctx();
  current->alarm_config_time = ticks;
  current->ticks_to_alarm = ticks;
  return 0;
}