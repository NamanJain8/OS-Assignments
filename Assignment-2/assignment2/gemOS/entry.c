#include<init.h>
#include<lib.h>
#include<context.h>


/*System Call handler*/
int do_syscall(int syscall, u64 param1, u64 param2, u64 param3, u64 param4)
{
    struct exec_context *current = get_current_ctx();
    printf("[GemOS] System call invoked. syscall no  = %d\n", syscall);
    switch(syscall)
    {
          case SYSCALL_EXIT:
                              printf("[GemOS] exit code = %d\n", (int) param1);
                              do_exit();
                              break;
          case SYSCALL_GETPID:
                              printf("[GemOS] getpid called for process %s, with pid = %d\n", current->name, current->id);
                              return current->id;      
          case SYSCALL_WRITE:
                             {  
                                     /*Your code goes here*/
                             }
          case SYSCALL_EXPAND:
                             {  
                                     /*Your code goes here*/
                             }
          case SYSCALL_SHRINK:
                             {  
                                     /*Your code goes here*/
                             }
                             
          default:
                              return -1;
                                
    }
    return 0;   /*GCC shut up!*/
}

extern int handle_div_by_zero(void)
{
    /*Your code goes in here*/
    printf("Div-by-zero handler: unimplemented!\n");
    return 0;
}

extern int handle_page_fault(void)
{
    /*Your code goes in here*/
    printf("page fault handler: unimplemented!\n");
    return 0;
}
