#include<init.h>
#include<lib.h>
#include<context.h>
#include<memory.h>

#define l4_mask 0x0000ff8000000000
#define l3_mask 0x0000007fc0000000
#define l2_mask 0x000000003fe00000
#define l1_mask 0x00000000001ff000
#define pfn_mask 0x00000ffffffff000

#define l4_offset 39
#define l3_offset 30
#define l2_offset 21
#define l1_offset 12
#define p_offset 0
#define rw_offset 1
#define us_offset 2
#define nl_add_offset 12
#define pfn_offset 12


u32 check_valid(u64 param1, struct exec_context *current){
	u64 l4 = (param1 & l4_mask) >> l4_offset;
	u64 l3 = (param1 & l3_mask) >> l3_offset;
	u64 l2 = (param1 & l2_mask) >> l2_offset;
	u64 l1 = (param1 & l1_mask) >> l1_offset;

	u32 l4_pfn = current->pgd;
	u32 l1_valid = 0;
	u64* l4_vaddr = (u64*)osmap(l4_pfn);
	if( *(l4_vaddr + l4) & 1 ){
		u32 l3_pfn = ( *(l4_vaddr + l4) & pfn_mask ) >> 12;
		u64* l3_vaddr = (u64*)osmap(l3_pfn);
		if( *(l3_vaddr + l3) & 1 ){
			u32 l2_pfn = ( *(l3_vaddr + l3) & pfn_mask ) >> 12;
			u64* l2_vaddr = (u64*)osmap(l2_pfn);
			if( *(l2_vaddr + l2) & 1 ){
				u32 l1_pfn = ( *(l2_vaddr + l2) & pfn_mask ) >> 12;
				u64* l1_vaddr = (u64*)osmap(l1_pfn);
				l1_valid = ( *(l1_vaddr + l1) & 1 );
			}
		}
	}
	return l1_valid;
}

/*System Call handler*/
long do_syscall(int syscall, u64 param1, u64 param2, u64 param3, u64 param4)
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
															; //  Labels can only be followed by statements, and declarations do not count as statements in C
															
															if(param2>1024)
																return -1;

															if(check_valid(param1,current) && check_valid(param1+param2,current)){
																printf("%s\n",param1);
																return param2;
															}
															else
																return -1;
															
					case SYSCALL_EXPAND:
														 {
																	if(param2 == MAP_RD){
														 					// printf("Next free: [%x]\n", current->mms[MM_SEG_RODATA].next_free);
																			unsigned long new_next = param1*(1<<12) + current->mms[MM_SEG_RODATA].next_free;
																			if( new_next <= current->mms[MM_SEG_RODATA].end){
																				current->mms[MM_SEG_RODATA].next_free = new_next;
																				// printf("Updated: %x\n", current->mms[MM_SEG_RODATA].next_free);
																				return current->mms[MM_SEG_RODATA].next_free - param1*(1<<12);
																			}
																			else
																				return (u64)NULL;
																		}
														
																	else if(param2 == MAP_WR){
																			// printf("Next free: [%x]\n", current->mms[MM_SEG_DATA].next_free);
																			unsigned long new_next = param1*(1<<12) + current->mms[MM_SEG_DATA].next_free;
																			// printf("New next after expanding: %x\n", new_next);
																			// printf("Data end addr%x\n", );
																			if( new_next <= current->mms[MM_SEG_DATA].end){
																				current->mms[MM_SEG_DATA].next_free = new_next; 
																				// printf("Updated: %x\n", current->mms[MM_SEG_DATA].next_free);
																				return current->mms[MM_SEG_DATA].next_free - param1*(1<<12);
																			}
																	else
																			return (u64)NULL;
																	}
																	
														 }
					case SYSCALL_SHRINK:
														 {  
																 if(param2 == MAP_RD){
																	unsigned long new_next = current->mms[MM_SEG_RODATA].next_free - param1*(1<<12);
																	if( new_next >= current->mms[MM_SEG_RODATA].start){
																		// Now free the pages
																		for(int i=0; i<param1; i++){
																			u64 vaddr = current->mms[MM_SEG_RODATA].next_free - (1<<12);
																			current->mms[MM_SEG_RODATA].next_free -= (1<<12);

																			u64 l4 = (vaddr & l4_mask) >> l4_offset;
																			u64 l3 = (vaddr & l3_mask) >> l3_offset;
																			u64 l2 = (vaddr & l2_mask) >> l2_offset;
																			u64 l1 = (vaddr & l1_mask) >> l1_offset;

																			u32 l4_pfn = current->pgd;
																			u64* l4_vaddr = (u64*)osmap(l4_pfn);
																			if(*(l4_vaddr + l4) & 0x1){
																				u32 l3_pfn = ( *(l4_vaddr + l4) & pfn_mask ) >> 12;
																				u64* l3_vaddr = (u64*)osmap(l3_pfn);
																				if(*(l3_vaddr + l3) & 0x1){
																					u32 l2_pfn = ( *(l3_vaddr + l3) & pfn_mask ) >> 12;
																					u64* l2_vaddr = (u64*)osmap(l2_pfn);
																					if(*(l2_vaddr + l2) & 0x1){
																						u32 l1_pfn = ( *(l2_vaddr + l2) & pfn_mask ) >> 12;
																						u64* l1_vaddr = (u64*)osmap(l1_pfn);
																						if(*(l1_vaddr+l1) & 0x1){
																							u32 data_pfn = ( *(l1_vaddr + l1) & pfn_mask ) >> 12;
																							*(l1_vaddr+l1) = 0;
																							os_pfn_free(USER_REG,data_pfn); 
																						}
																					}
																				}
																			}
																		}
																		// Flushing TLB
																		u64 cr3;
																		asm volatile  ("mov %%cr3, %0;"
                																		: "=r" (cr3));
                										asm volatile  ("mov %0, %%cr3;"
                																		: :"r"(cr3));
																				
																		return current->mms[MM_SEG_RODATA].next_free;
																	}
																	else
																		return (u64)NULL;
																}
														
																	else if(param2 == MAP_WR){
																			unsigned long new_next = current->mms[MM_SEG_DATA].next_free - param1*(1<<12);
																			// printf("New next after shrinking: %x\n", new_next);
																			if( new_next >= current->mms[MM_SEG_DATA].start){
																				for(int i=0; i<param1; i++){
																					u64 vaddr = current->mms[MM_SEG_DATA].next_free - (1<<12);
																					current->mms[MM_SEG_DATA].next_free -= (1<<12);

																					u64 l4 = (vaddr & l4_mask) >> l4_offset;
																					u64 l3 = (vaddr & l3_mask) >> l3_offset;
																					u64 l2 = (vaddr & l2_mask) >> l2_offset;
																					u64 l1 = (vaddr & l1_mask) >> l1_offset;

																					u32 l4_pfn = current->pgd;
																					u64* l4_vaddr = (u64*)osmap(l4_pfn);
																					if(*(l4_vaddr + l4) & 0x1){
																						u32 l3_pfn = ( *(l4_vaddr + l4) & pfn_mask ) >> 12;
																						u64* l3_vaddr = (u64*)osmap(l3_pfn);
																						if(*(l3_vaddr + l3) & 0x1){
																							u32 l2_pfn = ( *(l3_vaddr + l3) & pfn_mask ) >> 12;
																							u64* l2_vaddr = (u64*)osmap(l2_pfn);
																							if(*(l2_vaddr + l2) & 0x1){
																								u32 l1_pfn = ( *(l2_vaddr + l2) & pfn_mask ) >> 12;
																								u64* l1_vaddr = (u64*)osmap(l1_pfn);
																								if(*(l1_vaddr+l1) & 0x1){
																									u32 data_pfn = ( *(l1_vaddr + l1) & pfn_mask ) >> 12;
																									*(l1_vaddr+l1) = 0;
																									os_pfn_free(USER_REG,data_pfn); 
																								}
																							}
																						}
																					}
																				}

																				// Flushing TLB
																				u64 cr3;
																				asm volatile  ("mov %%cr3, %0;"
                    																		: "=r" (cr3));
                    										asm volatile  ("mov %0, %%cr3;"
                    																		: :"r"(cr3));
																				return current->mms[MM_SEG_DATA].next_free;
																			}
																			else
																				return (u64)NULL;
																		}
																	
														 }
														 
					default:
									return -1;
																
		}
		return 0;   /*GCC shut up!*/
}


extern int handle_div_by_zero(void)
{
		u64 rip,rip2;
		asm volatile( "mov 8(%%rbp), %0;" 
			: "=r" (rip) );
		printf("[GemOS] Div-by-zero detected at [%x]\n", rip);
		do_exit();
		return 0;
}

u32 checkVA(u64 cr2, struct exec_context *current){
	int seg=0;
	// Data = 1
	// Rodata =2
	// Stack = 3
	if(cr2 >= current->mms[MM_SEG_DATA].start && cr2 < current->mms[MM_SEG_DATA].end){
		// printf("%s\n","Data fault" );
		seg = 1;
	}
	if(cr2 >= current->mms[MM_SEG_RODATA].start && cr2 < current->mms[MM_SEG_RODATA].end){
		// printf("%s\n","ROData fault" );
		seg = 2;
	}
	if(cr2 < current->mms[MM_SEG_STACK].end-1 && cr2 >= current->mms[MM_SEG_STACK].start){
		// printf("%s\n","Stack fault" );
		seg = 3;
	}
	return seg;
}

void allocate_page(u64 cr2, struct exec_context *current, u32 seg){

	u64 l4 = (cr2 & l4_mask) >> l4_offset;
	u64 l3 = (cr2 & l3_mask) >> l3_offset;
	u64 l2 = (cr2 & l2_mask) >> l2_offset;
	u64 l1 = (cr2 & l1_mask) >> l1_offset;

	u32 l4_pfn = current->pgd;
	u64* l4_vaddr = (u64*)osmap(l4_pfn);
	u32 l3_pfn, l2_pfn, l1_pfn;
	u64* l3_vaddr;
	u64* l2_vaddr;
	u64* l1_vaddr;

	u32 rw_bit = 1;
	if(seg == 2)
		rw_bit = 0;

	if((*(l4_vaddr+l4)&0x1) == 0){
		l3_pfn = os_pfn_alloc(OS_PT_REG);

		*(l4_vaddr + l4) |= ( l3_pfn << nl_add_offset);
		*(l4_vaddr + l4) |= ( 1 << p_offset);
		*(l4_vaddr + l4) |= ( rw_bit << rw_offset);
		*(l4_vaddr + l4) |= ( 1 << us_offset);

		l3_vaddr = (u64*)osmap(l3_pfn);
		for(int i=0;i<512;i++)
			*(l3_vaddr + i) = 0x0;
	}
	else{
		l3_pfn = ( *(l4_vaddr + l4) & pfn_mask ) >> 12;
		l3_vaddr = (u64*)osmap(l3_pfn);
	}

	if((*(l3_vaddr+l3)&0x1) == 0){
		l2_pfn = os_pfn_alloc(OS_PT_REG);

		*(l3_vaddr + l3) |= ( l2_pfn << nl_add_offset);
		*(l3_vaddr + l3) |= ( 1 << p_offset);
		*(l3_vaddr + l3) |= ( rw_bit << rw_offset);
		*(l3_vaddr + l3) |= ( 1 << us_offset);

		l2_vaddr = (u64*)osmap(l2_pfn);
		for(int i=0;i<512;i++)
			*(l2_vaddr + i) = 0x0;
	}
	else{
		l2_pfn = ( *(l3_vaddr + l3) & pfn_mask ) >> 12;
		l2_vaddr = (u64*)osmap(l2_pfn);
	}

	if((*(l2_vaddr+l2)&0x1) == 0){
		l1_pfn = os_pfn_alloc(OS_PT_REG);

		*(l2_vaddr + l2) |= ( l1_pfn << nl_add_offset);
		*(l2_vaddr + l2) |= ( 1 << p_offset);
		*(l2_vaddr + l2) |= ( rw_bit << rw_offset);
		*(l2_vaddr + l2) |= ( 1 << us_offset);

		l1_vaddr = (u64*)osmap(l1_pfn);
		for(int i=0;i<512;i++)
			*(l1_vaddr + i) = 0x0;
	}
	else{
		l1_pfn = ( *(l2_vaddr + l2) & pfn_mask ) >> 12;
		l1_vaddr = (u64*)osmap(l1_pfn);
	}

	if((*(l1_vaddr+l1)&0x1) == 0){

		u32 data_pfn = os_pfn_alloc(USER_REG);
		u64* data_vaddr = (u64*)osmap(data_pfn);

		*(l1_vaddr + l1) |= ( data_pfn << nl_add_offset);

		*(l1_vaddr + l1) |= ( data_pfn << nl_add_offset);
		*(l1_vaddr + l1) |= ( 1 << p_offset);
		*(l1_vaddr + l1) |= ( rw_bit << rw_offset);
		*(l1_vaddr + l1) |= ( 1 << us_offset);

		data_vaddr = (u64*)osmap(data_pfn);
		for(int i=0;i<512;i++)
			*(data_vaddr + i) = 0x0;
		// printf("Data PFN: %x\n",data_pfn );
		// printf("%s\n","Page allocated successfully" );
	}
}

void print_err(u64 rip, u64 cr2, u64 err){
	printf("[INVALID Page Fault] RIP [%x] | VA [%x] | Error [%x]\n", rip,cr2,err);
	return;
}

extern int handle_page_fault(void)
{
		
		asm volatile  
    ( "push %r8;" 
    	"push %r9;"
    	"push %r10;" 
    	"push %r11;" 
    	"push %r12;" 
    	"push %r13;" 
    	"push %r14;"
      "push %r15;" 
      "push %rsi;" 
      "push %rdi;"
      "push %rax;" 
      "push %rbx;" 
      "push %rcx;" 
      "push %rdx;" 
     );

    u64 cr2,rsp,rbp;
		struct exec_context *current = get_current_ctx();
    
    asm volatile  ("mov %%rsp, %0;"
                    : "=r" (rsp)); 
    // These values are never changed in due course of time
    asm volatile  ("mov %%cr2, %0;"
                    : "=r" (cr2));   
    asm volatile  ("mov %%rbp, %0;"
                    : "=r" (rbp));

    // Storing in case of error printing
		u64 rip,err;
		asm volatile("mov 16(%%rbp),%0;" 
			: "=r"(rip));
		asm volatile("mov 8(%%rbp),%0;"
			: "=r"(err));


		int seg = checkVA(cr2,current);
		int set = err & 0x1;
		int rw = err & 0x2;
		int cpl = err & 0x4;
		// printf("%s\n","Page fault occured" );
		// print_err(rip,cr2,err);
		if(set == 1){
			printf("Protection Fault\n");
			print_err(rip,cr2,err);
			do_exit();
		}

		if(seg==0){
			// printf("Stack end: %x\n", current->mms[MM_SEG_STACK].end-1);
			// printf("Stack nextfree: %x\n", current->mms[MM_SEG_STACK].next_free);
			// printf("Stack Start: %x\n", current->mms[MM_SEG_STACK].start);
			printf("Address not in any segment\n");
			print_err(rip,cr2,err);
			do_exit();
		}

		else{
			if(seg==1){
				if(cr2 >= current->mms[MM_SEG_DATA].start && cr2 < current->mms[MM_SEG_DATA].next_free){
					allocate_page(cr2,current,1);
				}
				else{
					printf("Address not in any within range in data\n");
					print_err(rip,cr2,err);
					do_exit();
				}
			}
			else if(seg==2){
				if(rw == 1){
					printf("Invalid write access\n");
					print_err(rip,cr2,err);
					do_exit();
				}
				else if(cr2 >= current->mms[MM_SEG_RODATA].start && cr2 < current->mms[MM_SEG_RODATA].next_free){
					allocate_page(cr2,current,2);
				}
				else{
					printf("Address not in any within range in data\n");
					print_err(rip,cr2,err);
					do_exit();
				}
			}
			else if(seg==3){
				if(cr2 <= current->mms[MM_SEG_STACK].end-1 && cr2 >= current->mms[MM_SEG_STACK].start)
					allocate_page(cr2,current,3);
			}
		}


		asm volatile  (
    "mov %0, %%rsp;"
    "pop %%rdx;"
    "pop %%rcx;"
    "pop %%rbx;"
    "pop %%rax;"
    "pop %%rdi;"
    "pop %%rsi;"
    "pop %%r15;"
    "pop %%r14;"
    "pop %%r13;"
    "pop %%r12;"
    "pop %%r11;"
    "pop %%r10;"
    "pop %%r9;"
    "pop %%r8;"
    "mov %%rbp, %%rsp;"
    "mov (%%rbp),%%rbp;"
    "add $16, %%rsp;"
    "iretq"
    : : "r" (rsp)   
    );

		return 0;
}
