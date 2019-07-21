#include <context.h>
#include <memory.h>
#include <lib.h>

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

void prepare_context_mm(struct exec_context *ctx)
{
	u64 code_start = ctx->mms[MM_SEG_CODE].start;
	u64 data_start = ctx->mms[MM_SEG_DATA].start;
	u64 stack_end = ctx->mms[MM_SEG_STACK].end;
	stack_end = stack_end - 1;

	// Allocate L4 page
	u32 l4_pfn = os_pfn_alloc(OS_PT_REG);
	u64* l4_vaddr = (u64*)osmap(l4_pfn);

	for(int i=0;i<512;i++)
		*(l4_vaddr + i) = 0x0;

	// Setting-up PGD for context
	ctx->pgd = l4_pfn;
	///////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////
	// STACK SEGMENT //
	///////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////

	// Get offset for level pages
	u64 stack_l4 = (stack_end & l4_mask) >> l4_offset;
	u64 stack_l3 = (stack_end & l3_mask) >> l3_offset;
	u64 stack_l2 = (stack_end & l2_mask) >> l2_offset;
	u64 stack_l1 = (stack_end & l1_mask) >> l1_offset;
 
	// Setting up L4 page table entries for stack
	*(l4_vaddr + (stack_l4)) |= ( 1 << p_offset);
	*(l4_vaddr + (stack_l4)) |= ( 1 << rw_offset);
	*(l4_vaddr + (stack_l4)) |= ( 1 << us_offset);

	u32 l3_pfn = os_pfn_alloc(OS_PT_REG);
	u64* l3_vaddr = (u64*)osmap(l3_pfn);
	for(int i=0;i<512;i++)
		*(l3_vaddr + i) = 0x0;
	*(l4_vaddr + (stack_l4)) |= ( l3_pfn << nl_add_offset);

	*(l3_vaddr + (stack_l3)) |= ( 1 << p_offset);
	*(l3_vaddr + (stack_l3)) |= ( 1 << rw_offset);
	*(l3_vaddr + (stack_l3)) |= ( 1 << us_offset);

	u32 l2_pfn = os_pfn_alloc(OS_PT_REG);
	u64* l2_vaddr = (u64*)osmap(l2_pfn);
	for(int i=0;i<512;i++)
		*(l2_vaddr + i) = 0x0;
	*(l3_vaddr + (stack_l3)) |= ( l2_pfn << nl_add_offset);

	*(l2_vaddr + (stack_l2)) |= ( 1 << p_offset);
	*(l2_vaddr + (stack_l2)) |= ( 1 << rw_offset);
	*(l2_vaddr + (stack_l2)) |= ( 1 << us_offset);

	u32 l1_pfn = os_pfn_alloc(OS_PT_REG);
	u64* l1_vaddr = (u64*)osmap(l1_pfn);
	for(int i=0;i<512;i++)
		*(l1_vaddr + i) = 0x0;
	*(l2_vaddr + (stack_l2)) |= ( l1_pfn << nl_add_offset);

	*(l1_vaddr + (stack_l1)) |= ( 1 << p_offset);
	*(l1_vaddr + (stack_l1)) |= ( 1 << rw_offset);
	*(l1_vaddr + (stack_l1)) |= ( 1 << us_offset);

	u32 data_pfn = os_pfn_alloc(USER_REG);
	u64* data_vaddr = (u64*)osmap(data_pfn);
	*(l1_vaddr + (stack_l1)) |= ( data_pfn << nl_add_offset);

	// printf("L4 PFN %x\n",l4_pfn );
	// printf("L4 vaddr %x\n",*l4_vaddr);
	// printf("L4 PTE %x\n",*(l4_vaddr + stack_l4));
	// printf("\n");
	// printf("L3 PFN %x\n",l3_pfn );
	// printf("L3 vaddr %x\n",*l3_vaddr);
	// printf("L3 PTE %x\n",*(l3_vaddr + stack_l3));
	// printf("\n");
	// printf("L2 PFN %x\n",l2_pfn );
	// printf("L2 vaddr %x\n",*l2_vaddr);
	// printf("L2 PTE %x\n",*(l2_vaddr + stack_l2));
	// printf("\n");
	// printf("L1 PFN %x\n",l1_pfn );
	// printf("L1 vaddr %x\n",*l1_vaddr);
	// printf("L1 PTE %x\n",*(l1_vaddr + stack_l1));
	// printf("\n");
	
	///////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////
	// CODE SEGMENT //
	///////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////
	
	// Get offset for level pages
	u64 code_l4 = (code_start & l4_mask) >> l4_offset;
	u64 code_l3 = (code_start & l3_mask) >> l3_offset;
	u64 code_l2 = (code_start & l2_mask) >> l2_offset;
	u64 code_l1 = (code_start & l1_mask) >> l1_offset;
 
	// Setting up L4 page table entries for code
	if((*(l4_vaddr + (code_l4)) & 1) != 1){
		*(l4_vaddr + (code_l4)) = 0x0;
		*(l4_vaddr + (code_l4)) |= ( 1 << p_offset);
		*(l4_vaddr + (code_l4)) |= ( 1 << rw_offset);
		*(l4_vaddr + (code_l4)) |= ( 1 << us_offset);
		l3_pfn = os_pfn_alloc(OS_PT_REG);
		l3_vaddr = (u64*)osmap(l3_pfn);
		for(int i=0;i<512;i++)
			*(l3_vaddr + i) = 0x0;
		*(l4_vaddr + (code_l4)) |= ( l3_pfn << nl_add_offset);	
	}
	
	if((*(l3_vaddr + (code_l3)) & 1) != 1){
		*(l3_vaddr + (code_l3)) |= ( 1 << p_offset);
		*(l3_vaddr + (code_l3)) |= ( 1 << rw_offset);
		*(l3_vaddr + (code_l3)) |= ( 1 << us_offset);

		l2_pfn = os_pfn_alloc(OS_PT_REG);
		l2_vaddr = (u64*)osmap(l2_pfn);
		for(int i=0;i<512;i++)
			*(l2_vaddr + i) = 0x0;
		*(l3_vaddr + (code_l3)) |= ( l2_pfn << nl_add_offset);
	}

	if((*(l2_vaddr + (code_l2)) & 1) != 1){
		*(l2_vaddr + (code_l2)) |= ( 1 << p_offset);
		*(l2_vaddr + (code_l2)) |= ( 1 << rw_offset);
		*(l2_vaddr + (code_l2)) |= ( 1 << us_offset);

		l1_pfn = os_pfn_alloc(OS_PT_REG);
		l1_vaddr = (u64*)osmap(l1_pfn);
		for(int i=0;i<512;i++)
			*(l1_vaddr + i) = 0x0;
		*(l2_vaddr + (code_l2)) |= ( l1_pfn << nl_add_offset);
	}

	if((*(l1_vaddr + (code_l1)) & 1) != 1){
		*(l1_vaddr + (code_l1)) |= ( 1 << p_offset);
		*(l1_vaddr + (code_l1)) |= ( 0 << rw_offset);
		*(l1_vaddr + (code_l1)) |= ( 1 << us_offset);

		data_pfn = os_pfn_alloc(USER_REG);
		data_vaddr = (u64*)osmap(data_pfn);
		for(int i=0;i<512;i++)
			*(data_vaddr + i) = 0x0;	
		*(l1_vaddr + (code_l1)) |= ( data_pfn << nl_add_offset);
	}

	//////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////
	// DATA SEGMENT ////////////////////////////
	//////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////

	// Get offset for level pages
	u64 data_l4 = (data_start & l4_mask) >> l4_offset;
	u64 data_l3 = (data_start & l3_mask) >> l3_offset;
	u64 data_l2 = (data_start & l2_mask) >> l2_offset;
	u64 data_l1 = (data_start & l1_mask) >> l1_offset;
 
	// Setting up L4 page table entries for data
	if((*(l4_vaddr + (data_l4)) & 1) != 1){
		*(l4_vaddr + (data_l4)) |= ( 1 << p_offset);
		*(l4_vaddr + (data_l4)) |= ( 1 << rw_offset);
		*(l4_vaddr + (data_l4)) |= ( 1 << us_offset);

		l3_pfn = os_pfn_alloc(OS_PT_REG);
		l3_vaddr = (u64*)osmap(l3_pfn);
		for(int i=0;i<512;i++)
			*(l3_vaddr + i) = 0x0;
		*(l4_vaddr + (data_l4)) |= ( l3_pfn << nl_add_offset);	
	}

	if((*(l3_vaddr + (data_l3)) & 1) != 1){
		*(l3_vaddr + (data_l3)) |= ( 1 << p_offset);
		*(l3_vaddr + (data_l3)) |= ( 1 << rw_offset);
		*(l3_vaddr + (data_l3)) |= ( 1 << us_offset);

		l2_pfn = os_pfn_alloc(OS_PT_REG);
		l2_vaddr = (u64*)osmap(l2_pfn);
		for(int i=0;i<512;i++)
			*(l2_vaddr + i) = 0x0;
		*(l3_vaddr + (data_l3)) |= ( l2_pfn << nl_add_offset);
	}

	if((*(l2_vaddr + (data_l2)) & 1) != 1){
		*(l2_vaddr + (data_l2)) |= ( 1 << p_offset);
		*(l2_vaddr + (data_l2)) |= ( 1 << rw_offset);
		*(l2_vaddr + (data_l2)) |= ( 1 << us_offset);

		l1_pfn = os_pfn_alloc(OS_PT_REG);
		l1_vaddr = (u64*)osmap(l1_pfn);
		for(int i=0;i<512;i++)
			*(l1_vaddr + i) = 0x0;
		*(l2_vaddr + (data_l2)) |= ( l1_pfn << nl_add_offset);
	}

	if((*(l1_vaddr + (data_l1)) & 1) != 1){
		*(l1_vaddr + (data_l1)) |= ( 1 << p_offset);
		*(l1_vaddr + (data_l1)) |= ( 1 << rw_offset);
		*(l1_vaddr + (data_l1)) |= ( 1 << us_offset);

		data_pfn = ctx->arg_pfn;
		data_vaddr = (u64*)osmap(data_pfn);
		*(l1_vaddr + (data_l1)) |= ( data_pfn << nl_add_offset);
	}

    return;
}

void insert(int arr[], int *s, u32 pfn){
	int flag=0;
	for(int i=0;i<*s;i++){
		if(arr[i]==pfn){
			flag=1;
			break;
		}
	}

	if(flag==0){
		arr[*s] = pfn;
		*s=*s+1;
	}
	return;
}

void cleanup_context_mm(struct exec_context *ctx)
{
	int idx = 0;
	// To store pfn
	int arr[25];      
	int data[25];
	int s1=0,s2=0,flag=0;
	
	u32 l4_pfn = ctx->pgd;
	arr[idx] = ctx->pgd;
	u64* l4_vaddr = (u64*)osmap(l4_pfn);

	u64 code_start = ctx->mms[MM_SEG_CODE].start;
	u64 data_start = ctx->mms[MM_SEG_DATA].start;
	u64 stack_end = ctx->mms[MM_SEG_STACK].end;
	stack_end = stack_end - 1;

	/////////////////////////////////////////////////` 
	// Get offset for level pages
	u64 stack_l4 = (stack_end & l4_mask) >> l4_offset;
	u64 stack_l3 = (stack_end & l3_mask) >> l3_offset;
	u64 stack_l2 = (stack_end & l2_mask) >> l2_offset;
	u64 stack_l1 = (stack_end & l1_mask) >> l1_offset;

	u32 l3_pfn = *(l4_vaddr+stack_l4) >> nl_add_offset;
	u64* l3_vaddr = (u64*)osmap(l3_pfn);
	insert(arr,&s1,l3_pfn);

	u32 l2_pfn = *(l3_vaddr+stack_l3) >> nl_add_offset;
	u64* l2_vaddr = (u64*)osmap(l2_pfn);
	insert(arr,&s1,l2_pfn);

	u32 l1_pfn = *(l2_vaddr+stack_l2) >> nl_add_offset;	
	u64* l1_vaddr = (u64*)osmap(l1_pfn);
	insert(arr,&s1,l1_pfn);

	u32 data_pfn = *(l1_vaddr+stack_l1) >> nl_add_offset;
	u64* data_vaddr = (u64*)osmap(data_pfn);
	insert(data,&s2,data_pfn);

	// Get offset for level pages
	u64 code_l4 = (code_start & l4_mask) >> l4_offset;
	u64 code_l3 = (code_start & l3_mask) >> l3_offset;
	u64 code_l2 = (code_start & l2_mask) >> l2_offset;
	u64 code_l1 = (code_start & l1_mask) >> l1_offset;

	l3_pfn = *(l4_vaddr+code_l4) >> nl_add_offset;
	l3_vaddr = (u64*)osmap(l3_pfn);
	insert(arr,&s1,l3_pfn);

	l2_pfn = *(l3_vaddr+code_l3) >> nl_add_offset;
	l2_vaddr = (u64*)osmap(l2_pfn);
	insert(arr,&s1,l2_pfn);

	l1_pfn = *(l2_vaddr+code_l2) >> nl_add_offset;	
	l1_vaddr = (u64*)osmap(l1_pfn);
	insert(arr,&s1,l1_pfn);

	data_pfn = *(l1_vaddr+code_l1) >> nl_add_offset;
	data_vaddr = (u64*)osmap(data_pfn);
	insert(data,&s2,data_pfn);

	// Get offset for level pages
	u64 data_l4 = (data_start & l4_mask) >> l4_offset;
	u64 data_l3 = (data_start & l3_mask) >> l3_offset;
	u64 data_l2 = (data_start & l2_mask) >> l2_offset;
	u64 data_l1 = (data_start & l1_mask) >> l1_offset;

	l3_pfn = *(l4_vaddr+data_l4) >> nl_add_offset;
	l3_vaddr = (u64*)osmap(l3_pfn);
	insert(arr,&s1,l3_pfn);

	l2_pfn = *(l3_vaddr+data_l3) >> nl_add_offset;
	l2_vaddr = (u64*)osmap(l2_pfn);
	insert(arr,&s1,l2_pfn);

	l1_pfn = *(l2_vaddr+data_l2) >> nl_add_offset;	
	l1_vaddr = (u64*)osmap(l1_pfn);
	insert(arr,&s1,l1_pfn);

	data_pfn = ctx->arg_pfn;
	data_vaddr = (u64*)osmap(data_pfn);
	insert(data,&s2,data_pfn);

	arr[s1++] = ctx->pgd;
	for(int i=0;i<s1;i++)
		os_pfn_free(OS_PT_REG,arr[i]);

	for(int i=0;i<s2;i++)
		os_pfn_free(USER_REG,data[i]);
	// printf("\n")
    return;
}