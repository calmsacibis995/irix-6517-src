#ident "$Header: "

#include <stdio.h>
#include <sys/types.h>
#include <sys/pcb.h>
#include <klib/klib.h>

/*
 * alloc_sframe() -- Allocate a stack frame record
 */
sframe_t *
alloc_sframe(trace_t *trace)
{
	sframe_t *f;

	if (!(f = (sframe_t *)kl_alloc_block(sizeof (sframe_t), K_TEMP))) {
		return((sframe_t *)NULL);
	}
	f->level = trace->nframes + 1;
	return(f);
}

/*
 * free_sframes() -- Free all stack frames allocated to a trace record.
 */
void
free_sframes(trace_t *t)
{
	sframe_t *sf;

	t->nframes = 0;
	sf = t->frame;
	while(t->frame) {
		sf = (sframe_t *)dequeue((element_t **)&t->frame);
		if (sf->srcfile) {
			kl_free_block((k_ptr_t)sf->srcfile);
		}
		kl_free_block((k_ptr_t)sf);
	}
}

/*
 * alloc_trace_rec() -- Allocate stack trace header
 */
trace_t *
alloc_trace_rec()
{
	trace_t *t;

	t = (trace_t *)kl_alloc_block(sizeof(trace_t), K_TEMP);
	return(t);
}

/*
 * free_trace_rec() -- Free memory associated with stack trace header
 */
void
free_trace_rec(trace_t *t)
{
	int i;

	if (t->ktp) {
		kl_free_block(t->ktp);
	}
	if (t->ktp2) {
		kl_free_block(t->ktp2);
	}
	if (t->exp) {
		kl_free_block(t->exp);
	}
	if (t->pdap) {
		kl_free_block(t->pdap);
	}
	for (i = 0; i < STACK_SEGMENTS; i++) {
		if (t->stack[i].ptr) {
			kl_free_block((k_ptr_t)t->stack[i].ptr);
		}
	}
	free_sframes(t);
	kl_free_block((k_ptr_t)t);
}

/*
 * clean_trace_rec() -- Clean up stack trace record without releasing 
 * 						any of the allocated memory (except sframes).
 */
void
clean_trace_rec(trace_t *t)
{
	int i;

	t->flags = 0;
	t->kthread = 0;
	if (t->ktp) {
		kl_free_block(t->ktp);
		t->ktp = 0;
	}
	t->kthread2 = 0;
	if (t->ktp2) {
		kl_free_block(t->ktp2);
		t->ktp2 = 0;
	}
	t->exception = 0;
	if (t->exp) {
		kl_free_block(t->exp);
		t->exp = 0;
	}
	t->u_eframe = 0;
	if (t->pdap) {
		kl_free_block(t->pdap);
		t->pdap = (k_ptr_t)NULL;
	}
	t->stackcnt = 0;
	t->intstack = 0;
	t->bootstack = 0;
	for (i = 0; i < STACK_SEGMENTS; i++) {
		if (t->stack[i].ptr) {
			t->stack[i].type = 0;
			t->stack[i].size = 0;
			t->stack[i].addr = (kaddr_t)NULL;
			kl_free_block((k_ptr_t)t->stack[i].ptr);
			t->stack[i].ptr = (uint *)NULL;
		}
	}
	t->nframes = 0;
	free_sframes(t);
}

/*
 * setup_trace_rec()
 *
 *   Set up a trace record and initialize all the relevant fields
 *   depending on the value of the flag paramater.
 *
 *   1 == KTHREAD_FLAG
 *
 *   This option is used to setup a trace record for any kind of a
 *   kthread. The kthread structure contains information on what type
 *   of thread environment it supports. (XXX: For now, only PROC and
 *   KTHREAD environments are supported). It is assumed that the
 *   kthread pointer actually points to a data buffer containing the
 *   full type specific structure (e.g., proc struct). Such a pointer
 *   is returned from the kl_get_kthread() routine. (XXX: For now, the
 *   kthread struct is always part of the proc structure). A check is
 *   made to see if the process is running on a cpu. If it is, we need
 *   to get the pad_s struct for the cpu. The kthread stack (kernel
 *   stack), plus the intstack and bootstack if the process is running
 *   on a cpu, are then loaded into buffers and placed in the trace
 *   record.
 *
 *   2 == KTHREAD2_FLAG
 *
 *   This is a special option. It is for that rare case when we have to
 *   have more than one kthread mapped into the trace_rec. This option
 *   will only work when the trace_rec passed in has already been set up.
 *
 *   3 == CPU_FLAG
 *	
 *   The ptr paramater points to a data block containing a pda_s
 *   struct. Get the intstack and bootstack, load them into buffers,
 *   and link them into the trace record. Check and see if a
 *   process/kthread is running on this cpu. If one is, load the
 *   kthread into the trace record.
 *
 *   4 == RAW_FLAG
 *  
 *   The addr paramater contains the virtual address for the start of
 *   a stack. There is no way to tell if this address is a
 *   kernel/kthread stack or if it is from one of the cpus (intstack
 *   or bootstack). We will load it anyway and assume that the size of
 *   the stack is one page. (XXX: We might like to cycle through the
 *   cpus looking to see if the stack address is a bootstack or
 *   intstack. Also, we could walk through the process/kthreads and
 *   see if there is a mapping for it).
 *
 *   If an error occurs, the global structure error gets loaded with 
 *   the appropriate error related information and '1' is returned. For 
 *   all flag values except RAW_FLAG, we need to map ptr into the 
 *   appropriate trace rec field even when we fail. That will ensure 
 *   that the data block will be freed up by free_trace_rec() or 
 *   clean_trace_rec().
 *
 */
int
setup_trace_rec(kaddr_t addr, kstruct_t *s, int flag, trace_t *trace)
{
	int type, size, stkflg = -1;
	char stat;
	cpuid_t cpuid;
	kaddr_t kthread, upage, curkthread, saved_kthread;

	kl_reset_error();

	if (flag == KTHREAD_FLAG) {

		trace->ktp = s->ptr;
		trace->kthread = s->addr;

		/* Make sure the kthread is valid 
		 */
		if (!IS_UTHREAD(trace->ktp) && !IS_XTHREAD(trace->ktp) &&
				!IS_STHREAD(trace->ktp)) {
			if (DEBUG(KLDC_TRACE, 1)) {
				fprintf(KL_ERRORFP, "setup_trace_rec: kthread at 0x%llx "
					"is not valid!\n", trace->kthread);
			}
			KL_SET_ERROR_NVAL(KLE_BAD_KTHREAD, trace->kthread, 2);
			return(1);
		}

		/* If the kthread is KT_UTHREAD type, then get the exception 
		 * structure as well (that's where the u_eframe is stored).
		 */
		if (IS_UTHREAD(trace->ktp)) {
			trace->exception = 
				kl_kaddr(trace->ktp, "uthread_s", "ut_exception");
			trace->exp = kl_alloc_block(EXCEPTION_SIZE, K_TEMP);
			kl_get_block(trace->exception, EXCEPTION_SIZE, 
							trace->exp, "exception");
			if (KL_ERROR) {
				KL_SET_ERROR_NVAL(KLE_BAD_KTHREAD, trace->kthread, 2);
				return(1);
			}
			trace->u_eframe = 
				trace->exception + kl_member_offset("exception", "u_eframe");
		}

		/* If the kthread is running on a cpu, get the pda_s struct,
		 * along with the address of the intstack and bootstack.
		 */
		if ((cpuid = kl_uint(trace->ktp, 
										"kthread", "k_sonproc", 0)) != -1) {
			trace->pdap = kl_alloc_block(PDA_S_SIZE, K_TEMP);
			kl_get_pda_s((kaddr_t)cpuid, trace->pdap);
			if (KL_ERROR) {
				return(1);
			}
			trace->intstack = kl_kaddr(trace->pdap, "pda_s", "p_intstack");
			trace->bootstack = 
				kl_kaddr(trace->pdap, "pda_s", "p_bootstack");
		}
	}
	else if (flag == KTHREAD2_FLAG) {

		/* XXX -- Not sure about the following code if there aren't any
		 * ithreads. Have to look into this...
		 */
		trace->ktp2 = s->ptr;
		trace->kthread2 = s->addr;

		/* Make sure the kthread is valid 
		 */
		if (!IS_UTHREAD(trace->ktp2) && !IS_XTHREAD(trace->ktp2) &&
				!IS_STHREAD(trace->ktp2)) {
			if (DEBUG(KLDC_TRACE, 1)) {
				fprintf(KL_ERRORFP, "setup_trace_rec: kthread at 0x%llx "
					"is not valid!\n", trace->kthread2);
			}
			KL_SET_ERROR_NVAL(KLE_BAD_KTHREAD, trace->kthread2, 2);
			return(1);
		}

		/* If the kthread is KT_UTHREAD type, then get the exception 
		 * structure as well (that's where the u_eframe is stored).
		 * There shouldn't already be one there... 
		 */
		if (IS_UTHREAD(trace->ktp2)) {
			trace->exception = 
				kl_kaddr(trace->ktp2, "uthread_s", "ut_exception");
			trace->exp = kl_alloc_block(EXCEPTION_SIZE, K_TEMP);
			kl_get_block(trace->exception, EXCEPTION_SIZE, 
							trace->exp, "exception");
			if (KL_ERROR) {
				KL_SET_ERROR_NVAL(KLE_BAD_KTHREAD, trace->kthread2, 2);
				return(1);
			}
			trace->u_eframe = 
				trace->exception + kl_member_offset("exception", "u_eframe");
		}

		/* The kthread SHOULDN't be running on a cpu. If it, then 
		 * return with an error.
		 */
		if ((cpuid = kl_uint(trace->ktp2, 
										"kthread", "k_sonproc", 0)) != -1) {
			KL_SET_ERROR_NVAL(KLE_BAD_KTHREAD, trace->kthread2, 2);
			return(1);
		}
	}
	else if (flag == CPU_FLAG) {

		trace->pdap = s->ptr;
		trace->intstack = kl_kaddr(trace->pdap, "pda_s", "p_intstack");
		trace->bootstack = kl_kaddr(trace->pdap, "pda_s", "p_bootstack");
		stkflg = kl_uint(trace->pdap, "pda_s", "p_kstackflag", 0);

		/* Check to see if there is a kthread running on this CPU.
		 */
		cpuid = kl_uint(trace->pdap, "pda_s", "p_cpuid", 0);
		curkthread = kl_kaddr(trace->pdap, "pda_s", "p_curkthread");
		if (curkthread) {

			trace->ktp = kl_get_kthread(curkthread, 0);
			trace->kthread = curkthread;
			if (KL_ERROR) {
				/* It's possible that with an NMI dump that the
				 * kthread pointer in the pda_s struct is bad. So, 
				 * we will set a flag in the trace_rec indicating 
				 * this and forge ahead. It's the only way to
				 * get a stack trace on the S_INTSTACK or S_BOOTSTACK.
				 */
				if (IS_NMI) {
					trace->flags |= TF_BAD_KTHREAD;
					if (trace->ktp) {
						kl_free_block(trace->ktp);
					}
					trace->ktp = 0;
					trace->kthread = 0;
				}
				else {
					return(1);
				}
			}
			else {

				/* If the kthread is KT_UTHREAD type, then get the exception 
				 * structure as well (that's where the u_eframe is stored).
				 */
				if (IS_UTHREAD(trace->ktp)) {
					trace->exception = kl_kaddr(trace->ktp, 
							"uthread_s", "ut_exception");
					trace->exp = kl_alloc_block(EXCEPTION_SIZE, K_TEMP);
					kl_get_block(trace->exception, EXCEPTION_SIZE, 
									trace->exp, "exception");
					if (KL_ERROR) {
						KL_SET_ERROR_NVAL(KLE_BAD_KTHREAD, trace->kthread, 2);
						return(1);
					}
					trace->u_eframe = trace->exception + 
								kl_member_offset("exception", "u_eframe");
				}
			}
		}
	}
	else if (flag == RAW_FLAG) {

		add_stack(addr, S_RAW_STACK, stack_size(S_RAW_STACK, trace), trace);
#if 0
		/* Currently, we don't support trace_rec setups for 
		 * non kthread/proc/pda stacks. For now, just return 
		 * an error ...
		 */
		KL_SET_ERROR(KLE_NOT_IMPLEMENTED);
		return(1);
#endif
	}
	else {
		KL_SET_ERROR(KLE_UNKNOWN_ERROR);
		return(1);
	}

	/* Now to add all valid stacks... Note that in the case of KTHRAD2_FLAG,
	 * we only add that stack.
	 */
	 
	/* First, check to see if this is a CPU trace
	 */
	if (trace->pdap && !(trace->flags & TF_TRACEREC_VALID)) {

		/* Add the bootstack
		 */
		size = stack_size(S_BOOTSTACK, trace);
		if (add_stack(trace->bootstack, S_BOOTSTACK, size, trace) == -1) {
			return(1);
		}

		/* Add the intstack
		 */
		size = stack_size(S_INTSTACK, trace);
		if (add_stack(trace->intstack, S_INTSTACK, size, trace) == -1) {
			return(1);
		}
	}

	/* Add the kthread stack if there is one
	 */
	if (trace->ktp && !(trace->flags & TF_TRACEREC_VALID)) {

		/* Get the stack address from the kthread 
		 */
		addr = kthread_stack(trace, &size);
		if (KL_ERROR) {
			return(1);
		}

		/* Determine what type of stack this is
		 */
		if (IS_UTHREAD(trace->ktp)) {
			if (addr == K_KERNELSTACK) {
				type = S_KERNELSTACK;
			}
			else if (addr == K_KEXTSTACK) {
				type = S_KEXTSTACK;
			}
		}
		else if (IS_XTHREAD(trace->ktp)) {
			type = S_XTHREADSTACK;
		}
		else if (IS_STHREAD(trace->ktp)) {
			type = S_STHREADSTACK;
		}

		/* We have to set defkthread equal to kthread so that address
		 * translation will be done properly.
		 */
		saved_kthread = K_DEFKTHREAD;
		kl_set_defkthread(trace->kthread);

		/* Add the stack to the trace record.
		 */
		if (add_stack(addr, type, size, trace) == -1) {
			return(1);
		}

		/* Now set defkthread back 
		 */
		kl_set_defkthread(saved_kthread);
	}

	/* Add the second kthread stack 
	 */
	if (flag == KTHREAD2_FLAG) {

		/* Get the stack address from the kthread 
		 */
		addr = kl_kthread_stack(trace->kthread2, &size);
		if (KL_ERROR) {
			return(1);
		}

		/* Determine what type of kthread stack this is
		 */
		if (IS_UTHREAD(trace->ktp2)) {
			if (addr == K_KERNELSTACK) {
				type = S_KERNELSTACK;
			}
			else if (addr == K_KEXTSTACK) {
				type = S_KEXTSTACK;
			}
		}
		else if (IS_XTHREAD(trace->ktp2)) {
			type = S_XTHREADSTACK;
		}
		else if (IS_STHREAD(trace->ktp2)) {
			type = S_STHREADSTACK;
		}

		/* We have to set defkthread equal to kthread2 so that address
		 * translation will be done properly.
		 */
		saved_kthread = K_DEFKTHREAD;
		kl_set_defkthread(trace->kthread2);

		/* Add the stack to the trace record.
		 */
		if (add_stack(addr, type, size, trace) == -1) {
			return(1);
		}

		/* Now set defkthread back 
		 */
		kl_set_defkthread(saved_kthread);

		trace->flags |= TF_KTHREAD2_VALID;
		return(0);
	}
	trace->flags = TF_TRACEREC_VALID;
	return(0);
}

/*
 * setup_kthread_trace_rec()
 */
int
setup_kthread_trace_rec(kaddr_t value, int mode, trace_t *trace)
{
	k_ptr_t ktp;
	kstruct_t *ksp;
	kaddr_t kthread;

	kl_reset_error();

	if (mode != 2) {
		KL_SET_ERROR_NVAL(KLE_BAD_KTHREAD, value, mode);
		return(1);
	}
	kthread = value;

	ktp = kl_get_kthread(kthread, 0);
	if (KL_ERROR) {
		return(1);
	}

	ksp = (kstruct_t *)kl_alloc_block(sizeof(kstruct_t), K_TEMP);
	ksp->addr = kthread;
	ksp->ptr = ktp;
	setup_trace_rec((kaddr_t)0, ksp, KTHREAD_FLAG, trace);
	kl_free_block((k_ptr_t)ksp);
	if (KL_ERROR) {
		return(1);
	}
	else {
		return(0);
	}
}

/*
 * setup_cpu_trace_rec()
 *
 *   Value can be a cpuid or it can be a pointer to a pda_s strudt.
 *   The kl_get_pda_s() routine determins which type of value it is.
 */
int
setup_cpu_trace_rec(kaddr_t value, trace_t *trace)
{
	k_ptr_t pdap;
	cpuid_t cpuid;
	kstruct_t *ksp;
	kaddr_t pda;

	kl_reset_error();

	pdap = kl_alloc_block(PDA_S_SIZE, K_TEMP);
	ksp = (kstruct_t *)kl_alloc_block(sizeof(kstruct_t), K_TEMP);

	kl_get_pda_s(value, pdap);
	if (KL_ERROR) {
		kl_free_block(pdap);
		kl_free_block((k_ptr_t)ksp);
		return(1);
	}

	cpuid = kl_uint(pdap, "pda_s", "p_cpuid", 0);
	ksp->addr = kl_pda_s_addr(cpuid);
	ksp->ptr = pdap;
	setup_trace_rec((kaddr_t)0, ksp, CPU_FLAG, trace);
	kl_free_block((k_ptr_t)ksp);
	if (KL_ERROR) {
		return(1);
	}
	else {
		return(0);
	}
}

/*
 * stack_index()
 * 
 *   Check to see if we have a stack record allocated that contains
 *   addr. Return the stack record index if we do. Return -1 if addr
 *   is not a valid stack address. This routine assumes that the trace
 *   record has been fully initialized before entering.
 */
int
stack_index(kaddr_t addr, trace_t *trace)
{
	int i;
	kaddr_t saddr;

	if (DEBUG(KLDC_TRACE, 1) || DEBUG(KLDC_FUNCTRACE, 3)) {
		fprintf(KL_ERRORFP, "stack_index: addr=0x%llx, trace=0x%x\n", 
			addr, trace);
	}

	kl_reset_error();

	kl_is_valid_kaddr(addr, trace->ktp, WORD_ALIGN_FLAG);
	if (KL_ERROR) {
		if (trace->flags & TF_KTHREAD2_VALID) {
			kl_reset_error();
			kl_is_valid_kaddr(addr, trace->ktp2, WORD_ALIGN_FLAG);
		}
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_SP;
			return(-1);
		}
	}

	/* Cycle through all the stacks in trace record and see if addr
	 * falls inside one of them.
	 */
	for (i = 0; i < trace->stackcnt; i++) {
		if ((addr >= trace->stack[i].addr) &&
				(addr < trace->stack[i].addr + trace->stack[i].size)) {
			return(i);
		}
	}
	KL_SET_ERROR_NVAL(KLE_BAD_SP, addr, 2);
	return (-1);
}

/*
 * add_stack()
 * 
 *   Allocate space for a new stack and load it into the next
 *   available slot in the trace record stack array. Return the stack
 *   record index upon success. Return -1 if an error occurs.
 */
int
add_stack(kaddr_t addr, int type, int size, trace_t *trace)
{
	int curstack;
	k_ptr_t ptr;

	if (DEBUG(KLDC_TRACE, 1) || DEBUG(KLDC_FUNCTRACE, 3)) {
		fprintf(KL_ERRORFP, "add_stack: addr=0x%llx, type=%d, "
				"size=%d, trace=0x%x\n", addr, type, size, trace);
	}

	kl_reset_error();

	/* Allocate space for the stack.
	 */
	ptr = kl_alloc_block(size, K_TEMP);

	/* Get the contents of stack and load it into the newly allocated
	 * buffer
	 */
	kl_get_block(addr, size, ptr, "stack");
	if (KL_ERROR) {
		if (KL_ERROR == KLE_INVALID_KERNELSTACK) {

			k_uint_t e = KL_ERROR;

			/* XXX - This doesn't work anymore (kthread != proc)
			 */
			if (kl_uint(trace->ktp2, "proc", "p_stat", 0) == 3) {
				KL_SET_ERROR_NVAL(e|KLE_DEFUNCT_PROCESS, addr, 2);
			}
			else {
				KL_SET_ERROR_NVAL(e, addr, 2);
			}
		}
		else {
			KL_SET_ERROR_NVAL(KL_ERROR|KLE_BAD_SADDR, addr, 2);
		}
		kl_free_block(ptr);
		return(-1);
	}

	/* Bump the stack counter
	 */
	curstack = trace->stackcnt;
	trace->stackcnt++;

	/* Fill in the stack record detail.
	 */
	trace->stack[curstack].ptr = ptr;
	trace->stack[curstack].size = size;
	trace->stack[curstack].type = type;
	trace->stack[curstack].addr = addr;
	return (curstack);
}

/*
 * stack_type()
 *
 *   Given the virtual stack address addr, check to see which stack it
 *   is from. A stack type will be returned ONLY IF a proper virtual
 *   to physical mapping is possible (e.g., the kthread must be of
 *   type KT_UTHREAD if the stack type is S_KERNELSTACK). Note that it is 
 *   expected that all relevent fields in the trace struct (kthread, 
 *   pdap, etc.) will be filled in upon entry to this routine. 
 *   Consequently, if the stack isn't already loaded in, we have
 *   to return an error.
 */
int
stack_type(kaddr_t addr, trace_t *trace)
{
	int i, type = 0;
	kaddr_t kstack;

	kl_reset_error();

	/* First, cycle through any stacks that have already been
	 * allocated to see if there is a match.
	 */
	for (i = 0; i < trace->stackcnt; i++) {
		if ((addr >= trace->stack[i].addr) && 
					(addr < trace->stack[i].addr + trace->stack[i].size)) {
			return(trace->stack[i].type);
		}
		else if (trace->stack[i].addr == K_KERNELSTACK) {
			if ((addr >= K_KEXTSTACK) && (addr < K_KERNELSTACK)) {
				/* There isn't any page mapped for the kextstack page.
				 * just return the kernelstack as the type.
				 */
				return(trace->stack[i].type);
			}
		}

	}
	KL_SET_ERROR_NVAL(KLE_STACK_NOT_FOUND, addr, 2);
	return(0);
}

/*
 * stack_size() 
 *
 *   Return the size of a stack based on its type. Use type to locate
 *   the proper stack. The trace record should be fully set up before
 *   calling this routine. If no stack could be found, then return a
 *   size of zero.
 */
int
stack_size(int type, trace_t *trace)
{
	int i, size = 0;
	kaddr_t saddr;

	kl_reset_error();

	/* First, cycle through all stacks contained in the trace 
	 * record and see if the one we are looking for is currently 
	 * mapped. We can't use this approach if there is more than
	 * one kthread mapped in.
	 */
	if (!(trace->flags & TF_KTHREAD2_VALID)) {
		for (i = 0; i < trace->stackcnt; i++) {
			if (type == trace->stack[i].type) {
				return(trace->stack[i].size);
			}
		}
	}

	/* If we reach this point, most likely it was from a call in the 
	 * setup_trace_rec() routine. Use a basic set of rules to determine
	 * the size of the stack.
	 */
	switch (type) {

		case S_KERNELSTACK :
			size = K_PAGESZ;
			break;

		case S_KEXTSTACK :
			size = 2 * K_PAGESZ;
			break;

		case S_XTHREADSTACK :
		case S_STHREADSTACK :
			/* XXX -- we need to handle the two kthread case!
			 */
			if (trace->ktp) {
				size = KL_UINT(trace->ktp, "kthread", "k_stacksize");
			}
			break;

		case S_INTSTACK :
			if (trace->pdap) {
				kaddr_t lastframe;

				lastframe = kl_kaddr(trace->pdap, "pda_s", "p_intlastframe");
				size = (lastframe + EFRAME_S_SIZE) - trace->intstack;
			}
			break;

		case S_BOOTSTACK :
			if (trace->pdap) {
				size = K_PAGESZ - (trace->bootstack & (K_PAGESZ - 1));
			}
			break;

		case S_RAW_STACK :
			size = K_PAGESZ;
			break;

		default:
			size = 0;
			break;
	}
	return(size);
}

/*
 * stack_addr()
 *
 *   Given a virtual stack address (addr), check to see which stack it
 *   is from. Return the start address of that stack. Note that a
 *   stack address will be returned ONLY IF a proper virtual to
 *   physical mapping is possible. Also note that it is assumed that
 *   all relevent fields in the trace struct (kthread, pdap, etc.) are 
 *   filled in appropriately.
 */
kaddr_t
stack_addr(kaddr_t addr, trace_t *trace)
{
	int type, size;
	kaddr_t saddr;

	kl_reset_error();

	type = stack_type(addr, trace);
	if (KL_ERROR) {
		return(0);
	}
	
	/* Return the stack address based on the stack type.
	 */
	switch (type) {

		case S_XTHREADSTACK :
		case S_STHREADSTACK :
			saddr = kl_kthread_stack(trace->kthread, &size);
			if (trace->flags & TF_KTHREAD2_VALID) {
				/* We have to make sure this is the right stack address;
				 */
				if ((addr > saddr) && (addr <= (saddr + size))) {
					break;
				}
				saddr = kl_kthread_stack(trace->kthread2, &size);
			}
			break;

		case S_KERNELSTACK:
			saddr = K_KERNELSTACK;
			break;

		case S_KEXTSTACK :
			saddr = K_KEXTSTACK;
			break;

		case S_BOOTSTACK :
			saddr = trace->bootstack;
			break;

		case S_INTSTACK :
			saddr = trace->intstack;
			break;

		case S_UNKNOWN_STACK:
			saddr = addr & (1 - (K_PAGESZ - 1));
			break;

		case S_RAW_STACK: /* just take what the user gave */
			saddr = trace->stack[stack_index(addr, trace)].addr;
			break;

		default :
			saddr = 0;
			break;
	}
	if (saddr == 0) {
		KL_SET_ERROR_NVAL(KLE_STACK_NOT_FOUND|KLE_BAD_SP, saddr, 2);
	}

	if (PTRSZ64) {
		return (saddr);
	}
	else {
		return (saddr & 0xffffffff);
	}
}

/*
 * kthread_stack()
 *
 *  This routine returns the address of the kthread stack (which
 *  can be a process kernelstack or it can be a stack for an
 *  sthread). If a pointer to a size variable is passed in, 
 *  the size of the stack is returned as well. Special 
 *  considerations are taken when the kernel is configured to
 *  support stack extension pages (for processes only). Since the
 *  extended page address is always mapped in (along with a
 *  double page stack size), we have to determine if the
 *  extension page has actually been allocated before we return
 *  it (XXX Need to check on this). Otherwise, the call to kl_virtop()
 *  will fail. If it is allocated, then we return the stack
 *  extension address. Otherwise, we return the kernelstack (and
 *  reduce the size by half).
 */
kaddr_t
kthread_stack(trace_t *trace, int *size)
{
	kaddr_t saddr;

	if (DEBUG(KLDC_TRACE, 1) || DEBUG(KLDC_FUNCTRACE, 3)) {
		fprintf(KL_ERRORFP, "kthread_stack: trace=0x%x, size=0x%x\n",
			trace, size);
	}

	kl_reset_error();

	if (!trace->ktp) {
		KL_SET_ERROR(KLE_NO_KTHREAD);
		return(0);
	}

	saddr = kl_kaddr(trace->ktp, "kthread", "k_stack");

	/* If stack extensions are enabled (usually only with 32-bit 
	 * kernels) we have to check and see if an extension page is 
	 * mapped in for the current process. If a page is mapped in,
	 * then use it, otherwise use the regular stack page.
	 */
	if (K_EXTUSIZE) {
		if (saddr == K_KEXTSTACK) {
			if (!IS_UTHREAD(trace->ktp)) {
				if (IS_XTHREAD(trace->ktp)) {
					KL_SET_ERROR(KLE_IS_XTHREAD);
				}
				else if (IS_STHREAD(trace->ktp)) {
					KL_SET_ERROR(KLE_IS_STHREAD);
				}
				return(0);
			}
			kl_virtop(saddr, trace->ktp);
			if (KL_ERROR) {
				saddr = K_KERNELSTACK;
				if (size) {
					*size = stack_size(S_KERNELSTACK, trace);
				}
			}
			else if (size) {
				*size = stack_size(S_KEXTSTACK, trace);
			}
			return(saddr);
		}
	}
	
	/* We either have the kernelstack (on a system that does not
	 * support extension stack pages) -- or an sthread stack. Just 
	 * get the size from the kthread struct.
	 */
	if (size) {
		*size = KL_UINT(trace->ktp, "kthread", "k_stacksize");
	}
	return(saddr);
}

/*
 * is_kthreadstack()
 */
int
is_kthreadstack(kaddr_t addr, trace_t *trace)
{
	int size;
	kaddr_t ktstack;

	kl_reset_error();

	/* Just in case...
	 */
	if (!trace->ktp) {
		KL_SET_ERROR(KLE_NO_KTHREAD);
		return(0);
	}

	ktstack = kl_kaddr(trace->ktp, "kthread", "k_stack");
	size = KL_UINT(trace->ktp, "kthread", "k_stacksize");
	if ((addr >= ktstack) && (addr < (ktstack + size))) {
		return(1);
	}
	else {
		KL_SET_ERROR_NVAL(KLE_BAD_KERNELSTACK, addr, 2);
		return(0);
	}
}

/*
 * _get_kthread_stack()
 * 
 *   A wrapper for kthread_stack() that allows routines outside the
 *   trace module to get kthread stack addresses (they don't need to
 *   know about trace structs).
 */
kaddr_t
_get_kthread_stack(kaddr_t kthread, int *size) 
{
	kaddr_t addr;
	k_ptr_t ktp;
	kstruct_t *ksp;
	trace_t *trace;

	kl_reset_error();

	if (*size) {
		*size = 0;
	}

	trace = alloc_trace_rec();
	ksp = (kstruct_t*)kl_alloc_block(sizeof(kstruct_t), K_TEMP);

	ktp = kl_get_kthread(kthread, 0);
	if (KL_ERROR) {
		kl_free_block((k_ptr_t)ksp);
		free_trace_rec(trace);
		return((kaddr_t)NULL);
	}
	ksp->addr = kthread;
	ksp->ptr = ktp;
	setup_trace_rec((kaddr_t)0, ksp, KTHREAD_FLAG, trace);
	kl_free_block((k_ptr_t)ksp);
	if (KL_ERROR) {
		free_trace_rec(trace);
		return((kaddr_t)NULL);
	}
	addr = kthread_stack(trace, size);
	free_trace_rec(trace);
	return(addr);
}

/* 
 * get_frame_size() 
 *
 *   Search the size of func (or first 40 instructions) looking for
 *   the instruction that decrements the stack pointer. The amount of
 *   the decrement is the size of stack frame. If this is a LEAF
 *   function, there will be no stack frame size (return 0).
 *
 *   Note that it is possible for part of a function to use the 
 *   exception frame/SP from the calling function and part that 
 *   allocates space in the stack. We have to be careful not to
 *   go beyond addr when determining this. To search the entire 
 *   function for frame_size regardless of PC, flag must be non-zero.
 */
short
get_frame_size(kaddr_t addr, int flag)
{
	int i, off, size;
	uint instr;
	kaddr_t func_addr;

	func_addr = kl_funcaddr(addr);
	if (KL_ERROR) {
		return(-1);
	}

	size = kl_funcsize(func_addr); 
	if (KL_ERROR) {
		/* This should never really happen, but just in case...
		 */
		if (DEBUG(KLDC_TRACE, 1)) {
			fprintf(KL_ERRORFP, "get_frame_size: func_addr=0x%llx, size=%d\n", 
				func_addr, size);
		}
		size = 40;
	}
	else {
		/* Convert size to number of instructions. If flag is non-zero,
		 * use size returned by kl_funcsize() regardless of PC value.
		 * Otherwise, adjust size to go only upto and including PC.
		 */
		if (flag == 0) {
			size = (addr - func_addr);
		}
		size = size / 4;
	}

	for(i = 0; i < size; i++) {
		kl_get_block((func_addr + (i * 4)), 4, &instr, "instr");
		if (PTRSZ64) {
			int op;
			op =(instr & 0xfc000000) >> 26;
			if (op) {
				if ((op == 25) && (((instr & 0x3e00000) >> 21) == 29) &&
							(((instr & 0x1f0000) >> 16) == 29)) {
					off = (short)(instr & 0xffff);
					return(abs(off));
				}
			}
			else {
				op = instr & 0x3f;
				if (((op == 45) || (op == 47)) && 
							(((instr & 0x3e00000) >> 21) == 29) &&
							(((instr & 0xf800) >> 11) == 29)) {

					uint last_instr;
					int rt;

					/* We have to back up one instruction and get the
					 * imediate value that was loaded into rt ($at).
					 * That's should be our offset.
					 */
					if (i) {
						rt = (instr & 0x1f0000) >> 16;
						kl_get_block((func_addr + ((i - 1) * 4)), 4, 
								&last_instr, "instr");
						if (((last_instr & 0x1f0000) >> 16) == rt) {
							off = (short)(last_instr & 0xffff);
							return(abs(off));
						}
					}
				}
				else if ((op == 47) && (((instr & 0x3e00000) >> 21) == 29) &&
							(((instr & 0xf800) >> 11) == 29)) {

					uint last_instr;
					int rt;

					/* We have to back up one instruction and get the
					 * imediate value that was loaded into rt ($at).
					 * That's should be our offset.
					 */
					if (i) {
						rt = (instr & 0x1f0000) >> 16;
						kl_get_block(func_addr + ((i - 1) * 4), 4, 
								&last_instr, "instr");
						if (((last_instr & 0x1f0000) >> 16) == rt) {
							off = (short)(last_instr & 0xffff);
							return(abs(off));
						}
					}
				}
			}
		}
		else {
			if ((instr & 0xffff0000) == 0x27bd0000) {
				off = (short)(instr & 0xffff);
				return(abs(off));
			}
		}
	}
	return(0);
}

/* 
 * get_ra_offset() 
 *
 *   Walk through the instructions in a function looking for the
 *   instruction that stores the return address. If the instruction is
 *   a store word (sw), return the ra offset. if the instruction is a
 *   store double (sd), return the ra_offset + 4 (even though icrash
 *   looks at 64 bit kernels, it lives in a 32 bit world).
 *
 *   Note that it is possible for part of a function to use the 
 *   exception frame/SP from the calling function and part that 
 *   allocates space in the stack. We have to be careful not to
 *   go beyond addr when determining this. To search the entire 
 *   function for ra_offset regardless of PC, flag must be non-zero.
 */
int
get_ra_offset(kaddr_t addr, int flag)
{
	int i, size;
	uint instr;
	kaddr_t func_addr;

	kl_reset_error();

	func_addr = kl_funcaddr(addr);
	if (KL_ERROR) {
		KL_SET_ERROR_NVAL(KLE_NO_RA, addr, 2);
		return(-1);
	}

	size = kl_funcsize(addr);
	if (KL_ERROR) {
		/* This should never really happen, but just in case...
		 */
		if (DEBUG(KLDC_TRACE, 1)) {
			fprintf(KL_ERRORFP, "get_ra_offset: func_addr = 0x%llx, size=%d\n", 
				func_addr, size);
		}
		size = 100;
	}
	else {
		/* Convert size to number of instructions. If flag is non-zero,
		 * use size returned by kl_funcsize() regardless of PC value.
		 * Otherwise, adjust size to go only upto and including PC.
		 */
		if (flag == 0) {
			size = (addr - func_addr);
		}
		size = size / 4;
	}

	for(i = 0; i < size; i++) {
		kl_get_block((func_addr + (i * 4)), 4, &instr, "instr");
		if (PTRSZ64) {
			if ((instr & 0xffff0000) == 0xffbf0000) {
				return(instr & 0xffff);
			}
		}
		else {
			if ((instr & 0xffff0000) == 0xafbf0000) {
				return(instr & 0xffff);
			}
			else if ((instr & 0xffff0000) == 0xffbf0000) {
				return((instr & 0xffff) + 4);
			}
		}
	}
	KL_SET_ERROR_NVAL(KLE_BAD_FUNCADDR, addr, 2);
	return(-1);
}

/* 
 * find_functions() 
 *
 *   Search through a block of code looking for instructions that 
 *   decrement the stack pointer. Between an instruction that
 *   decrements a stack frame pointer and the next such function,
 *   we will consider a "function." We have to do this because 
 *   many loadable module functions do not contain a symbol table 
 *   entry.
 */
function_rec_t *
find_functions(kaddr_t start_addr, int bytes)
{
	int i, frame_size, off, size;
	uint instr;
	kaddr_t addr;
	function_rec_t *func_list, *frp;

	size = (bytes / 4) + 1;
	addr = start_addr;

	frp = func_list = (function_rec_t*)NULL;

	for(i = 0; i < size; i++) {
		kl_get_block((addr + (i * 4)), 4, &instr, "instr");
		if (PTRSZ64) {
			int op;

			if (op = (instr & 0xfc000000) >> 26) {
				off = 0;
				if ((op == 25) && (((instr & 0x3e00000) >> 21) == 29) &&
							(((instr & 0x1f0000) >> 16) == 29)) {
					off = (short)(instr & 0xffff);

				}

				/* Check to see if the offset is less than zero. If
				 * it is then it means that we are at the start of
				 * a new function (or block of text?) If the offset
				 * is greater than zero, it may mean that we have
				 * reached the end of the function. We need to check
				 * and see if we are looking at the last instruction
				 * in the sequence.
				 */
				if (off < 0) {
					if (frp) {
						/* Finish off the current function
						 */
						frp->func_size = (addr + (i * 4)) - frp->lowpc;
						frp->next = (function_rec_t*)
							kl_alloc_block(sizeof(function_rec_t), K_TEMP);
						frp = frp->next;
					}
					else {
						func_list = (function_rec_t*)
							kl_alloc_block(sizeof(function_rec_t), K_TEMP);
						frp = func_list;
					}
					frp->lowpc = addr + (i * 4);
					frp->frame_size = abs(off);
				}
				else if (off > 0) {
					if (i == (size - 1)) {
						/* We are at the end of the instruction
						 * block. We can figure that we are at 
						 * the end of the current function.
						 *
						 * XXX - not sure how to handle the case
						 * where the incrementing of the stack pointer
						 * occurs before the last instruction in 
						 * the block.
						 */
					}
				}
			}
		}
		else {
			if ((instr & 0xffff0000) == 0x27bd0000) {
				off = (short)(instr & 0xffff);
			}
		}
	}
	return(func_list);
}


/*
 * is_exception()
 */
int
is_exception(char *funcname)
{
	if (!strncmp(funcname, "VEC", 3) || 
			!strcmp(funcname, "systrap") ||
			!strcmp(funcname, "tfp_special_int") ||
			!strcmp(funcname, "exception_ip12") || 
			!strncmp(funcname, "elocore_exl", 11)) {
		return(1);
	}
	return(0);
}

#define RETURN_TRACE(trace, flags) \
			if (flags & (K_ALL|K_FULL)) { \
				return (trace->nframes); \
			} \
			else { \
				return(0); \
			}

/* 
 * find_trace()
 * 
 *   Given a starting pc (spc), starting stack pointer (ssp), and
 *   stack page address, check to see if a valid trace is possible. A
 *   trace is considered valid if no errors are encountered (bad PC,
 *   bad SP, etc.) Certain errors are tolorated however. For example,
 *   if the current stack frame is an exception frame (e.g., VEC_*),
 *   go ahead and return success -- even if PC and SP obtained from
 *   the exception frame are bad (a partial trace is better than no
 *   trace)..
 *
 *   Return zero if no valid trace was found. Otherwise, return the
 *   number of frames found. If the K_ALL flag is set, return the
 *   number of good frames that were found.
 *
 *   Parameters:
 *
 *   spc			starting program counter 
 *   ssp			starting stack pointer 
 *   check_pc		if non-NULL, check to see if check_pc/check_sp 
 *   check_sp		are a sub-trace of trace beginning with spc/ssp 
 *   trace		    structure containing all trace related info (frames,
 * 				    pages, page/frame counts, etc.
 *   flags
 */
int
find_trace(kaddr_t spc, kaddr_t ssp, kaddr_t check_pc, kaddr_t check_sp, 
		   trace_t *trace, int flags)
{
	int i, j, line_no, frame_size, ra_offset;
	int curstkidx, exception = 0;
	kaddr_t s, sp, pc, ra, saddr, upage, func_addr, curkthread;
	uint *sbp, *asp;
	char *srcfile, *funcname;
	sframe_t *curframe;

	if (DEBUG(KLDC_TRACE, 1) || DEBUG(KLDC_FUNCTRACE, 3)) {
		fprintf(KL_ERRORFP, "find_trace: spc=0x%llx, ssp=0x%llx\n"
			"             check_pc=0x%llx, check_sp=0x%llx\n", 
			spc, ssp, check_pc, check_sp);
		fprintf(KL_ERRORFP, "             trace=0x%x, flags=0x%x\n", 
			trace, flags);
	}

	pc = spc;
	sp = ssp;
	curstkidx = stack_index(sp, trace);
	if (KL_ERROR) {
		return(0);
	}
	sbp = trace->stack[curstkidx].ptr;
	saddr = trace->stack[curstkidx].addr;

	/* Build the trace
	 */
	while(pc) {

		/* Make sure that we have the correct stack. It's possible that
		 * an exception frame in one stack contains an sp from another 
		 * stack.
		 */
		if (trace->stack[curstkidx].addr != saddr) {
			sbp = trace->stack[curstkidx].ptr;
			saddr = trace->stack[curstkidx].addr;
			asp = (uint*)((uint)sbp + ((sp - trace->stack[curstkidx].addr)));
		}

		/* If the current stack is the bootstack and this is the
		 * last frame, then just return (it's possible for the pc 
		 * to be non-null).
		 */
		if ((saddr == trace->bootstack) && 
			(sp == kl_kaddr(trace->pdap, "pda_s", "p_bootlastframe"))) {
			return(trace->nframes);
		}

		/* Allocate space for a stack frame rec
		 */
		curframe = alloc_sframe(trace);

		/* Get function start address (and make sure pc is valid text). 
		 */
		func_addr = kl_funcaddr(pc);
		if (KL_ERROR || (pc & 3)) {
			/* We have to reset the global error structure or we
			 * wont be able to print out any trace fragments.
			 */
			kl_reset_error();
			curframe->error = KLE_BAD_PC;
			UPDATE_FRAME(0, pc, 0, 0, 0, 0, 0, 0);
			RETURN_TRACE(trace, flags);
		}

		/* Check to see if check_pc/check_sp points to a sub-trace 
		 * of spc/ssp. If it does then don't return a trace (unless K_ALL).
		 * Make sure we free the curframe block since we wont be linking
		 * it in to the trace rec. 
		 */
		if (check_pc && ((pc == check_pc) && (sp == check_sp))) {
			kl_free_block((k_ptr_t)curframe);
			RETURN_TRACE(trace, flags);
		}

		/* Determine source filename, funcname and line number,
		 * stackframe size and RA offset.
		 */
		funcname = kl_funcname(pc);
		if (!KL_ERROR) {

			srcfile = kl_srcfile(pc);
			line_no = kl_lineno(pc);
			if (frame_size = get_frame_size(pc, 0)) {

				/* Get the offset in the stack frame to the return address.
				 * If there isn't one, most likely it is because the function
				 * does not make a call to any other function (the RA doesn't
				 * have to be saved). There's not much we can do, so just
				 * return an error (this should only happen on the first
				 * level of a trace). 
				 */
				if ((ra_offset = get_ra_offset(pc, 0)) == -1) {
					kl_free_block((k_ptr_t)curframe);
					RETURN_TRACE(trace, flags);
				}
			}

			if (DEBUG(KLDC_TRACE, 2)) {
				int funcsize;

				funcsize = kl_funcsize(func_addr);
				fprintf(KL_ERRORFP, "find_trace: funcname=%s, line_no=%d, "
					"funcsize=%d\n", funcname, line_no, funcsize);
				fprintf(KL_ERRORFP, "            frame_size=%d, ra_offset=%d\n",
					frame_size, ra_offset);
			}

			/* Determine the start address of the stack which SP is from
			 */
			s = stack_addr(sp, trace);
			if (KL_ERROR) {
				if (DEBUG(KLDC_TRACE, 1)) {
					fprintf(KL_ERRORFP, "\nfind_trace: SP 0x%llx is not "
						"valid\n", sp);
				}
				return(0);
			}

			/* Now check to make sure it is part of the current stack 
			 * segment.
			 */
			if (s == trace->stack[curstkidx].addr) {
				asp = (uint*)((uint)sbp + 
						((sp - trace->stack[curstkidx].addr)));
			}
			else {
				if (DEBUG(KLDC_TRACE, 1)) {
					fprintf(KL_ERRORFP, "\nfind_trace: SP 0x%llx is not from "
						"stack 0x%llx\n", sp, trace->stack[curstkidx].addr);
				}
				KL_SET_ERROR_NVAL(KLE_BAD_SP, s, 2);
				return(0);
			}

			if (DEBUG(KLDC_TRACE, 2)) {
				fprintf(KL_ERRORFP, "->find_trace: %s: pc=0x%llx, sp=0x%llx, "
				"asp=0x%x (0x%llx)\n", 
				funcname, pc, sp, asp, *(kaddr_t*)asp);
			}

			/* Check to see if the current stack function is an
			 * exception. If it is, make an attempt to locate the
			 * exception frame (on the current stack, a new stack,
			 * or in the ublock.
			 */
			if (exception = is_exception(funcname)) {

				UPDATE_FRAME(funcname, pc, ra, sp, asp, 
							 srcfile, line_no, frame_size);

				/* Try and locate an exception frame. If the eframe
				 * is on another stack, the new stack page will be
				 * mapped into trace_rec and curframe will be adjusted.
				 */
				if (find_eframe(trace, flags)) {
					return(trace->nframes);
				}

				/* Check to see if the exception frame is in the ublock.
				 * If it is, just return (we're at the end of the trace).
				 */
				if (curframe->ep == trace->u_eframe) {
					return(trace->nframes);
				}

				/* Set the actual stack pointer (asp), real stack pointer
				 * current stack page, etc from curframe (in case any of
				 * them have changed).
				 */
				curstkidx = curframe->ptr;
				asp = curframe->eframe;
				ssp = curframe->ep;
				sbp = trace->stack[curstkidx].ptr;
				saddr = trace->stack[curstkidx].addr;

				/* Get the PC, RA, and SP from the exception frame.
				 */
				pc = *(kaddr_t*)((uint)asp + 
					(kl_member_offset("eframe_s", "ef_epc")));
				ra = *(kaddr_t*)((uint)asp + 
					(kl_member_offset("eframe_s", "ef_ra")));
				sp = *(kaddr_t*)((uint)asp + 
					(kl_member_offset("eframe_s", "ef_sp")));
				if (PTRSZ32) {
					pc &= 0xffffffff;
					ra &= 0xffffffff;
					sp &= 0xffffffff;
				}

				if (DEBUG(KLDC_TRACE, 1)) {
					fprintf(KL_ERRORFP, "->find_trace: asp=0x%x, ssp=0x%llx, "
						"curstkidx=%d\n", asp, ssp, curstkidx);
					fprintf(KL_ERRORFP, "              pc=0x%llx, ra=0x%llx, "
						"sp=0x%llx\n", pc, ra, sp);
				}

				/* make sure sp is within current stack page
				 */
				if ((sp < ssp) || (sp >= (trace->stack[curstkidx].addr + 
							trace->stack[curstkidx].size))) {

					/* Make sure the stack pointer is from some other 
					 * stack already in the trace record (e.g., the 
					 * bootstack). If it is, then continue on (spage, 
					 * ssp, etc. will be adjusted at the top of the
					 * loop).
					 */
					if ((curstkidx = stack_index(sp, trace)) == -1) {
						curframe = alloc_sframe(trace);
						curframe->error = KLE_BAD_SP;
						UPDATE_FRAME(0, 0, 0, sp, 0, 0, 0, 0);

						/* Return success even though we have a bad SP. It's
						 * possible that defkthread was not set properly and 
						 * this is really a good trace.
						 */
						return(trace->nframes);
					}
				}

				/* make sure pc is valid text 
				 */
				if (!IS_TEXT(pc) || (pc & 3)) {
					pc = ra - 8;

					/* make sure the new PC is valid
					 */
					if (!IS_TEXT(pc) || (pc & 3)) {
						curframe = alloc_sframe(trace);
						curframe->error = KLE_BAD_PC;
						UPDATE_FRAME(0, pc, 0, 0, 0, 0, 0, 0);

						/* Return success even though we have a bad PC. 
						 * It's possible that defkthread was not set 
						 * properly and this is really a good trace.
						 */
						return(trace->nframes);
					}
				}
				else {

					/* If current function doesn't have a stack frame,
					 * or if the RA is not saved in the exception frame,
					 * capture an sframe_rec for the function and then 
					 * go on, using the RA to generate the next PC.
					 */
					frame_size = get_frame_size(pc, 0); 
					ra_offset = get_ra_offset(pc, 0);

					if (!frame_size || (ra_offset == -1)) {

						/* Determine the funcname, source file and line 
						 * number for pc 
						 */
						funcname = kl_funcname(pc);
						srcfile = kl_srcfile(pc);
						line_no = kl_lineno(pc);

						if (DEBUG(KLDC_TRACE, 2)) {
							fprintf(KL_ERRORFP, "find_trace: sp=0x%llx, "
								"pc=0x%llx, ra=0x%llx, frame_size=%d\n", 
								sp, pc, ra, frame_size);
						}
						curframe = alloc_sframe(trace);
						UPDATE_FRAME(funcname, pc, ra, sp, asp, 
									srcfile, line_no, 0);
						pc = ra - 8;
						sp = sp + frame_size;
					}
				}
				exception = 0;
			} 
			else {
				if (frame_size) {
					/* Get the RA from the frame 
					 */
					ra = kl_kaddr_val((k_ptr_t)((uint)asp + ra_offset));
					UPDATE_FRAME(funcname, pc, ra, sp, asp, 
							 srcfile, line_no, frame_size);
					if (ra) {
						sp += frame_size;
						pc = ra - 8;
					}
					else {
						pc = 0;
					}
				}
				else {
					ra = 0;
					UPDATE_FRAME(funcname, pc, ra, sp, asp, 
							 srcfile, line_no, frame_size);
					pc = 0;
				}
			}
		}
		else {
			curframe->error = KLE_BAD_PC;
			UPDATE_FRAME(0, pc, 0, 0, 0, 0, 0, 0);
			RETURN_TRACE(trace, flags);
		}
	}
	return(trace->nframes);
}


/* 
 * find_trace2() -- Try and find a valid trace using pc, ra, sp, and saddr
 *
 *   This routine is particularily usefule when pc is from a LEAF
 *   function. In such cases, special handling is necessary to
 *   generate a comlete backtrace. This routine is primarily for
 *   genereting a backtrace from an exception frame or from nmi
 *   register dumps. This routine does some testing of pc and then
 *   hands the values off to find_trace(), which does the real work.
 *   Unlike with find_trace(), find_trace2() returns zero (0) on
 *   success and one (1) if an error occurs.
 */
int
find_trace2(kaddr_t pc, kaddr_t ra, kaddr_t sp, kaddr_t saddr, 
					trace_t *trace, int flags)
{
	int type, line_no, curstkidx = -1, frame_size = 0, ra_offset = 0;
	char *srcfile, *funcname;
	uint *asp;
	sframe_t *curframe;

	if (DEBUG(KLDC_TRACE, 1) || DEBUG(KLDC_FUNCTRACE, 3)) {
		fprintf(KL_ERRORFP, "find_trace2: pc=0x%llx, ra=0x%llx, sp=0x%llx, "
			"saddr=0x%llx\n", pc, ra, sp, saddr);
		fprintf(KL_ERRORFP, "             trace=0x%x, flags=0x%x\n", 
			trace, flags);
	}

	/* Make sure that sp and spage is valid and that SP is from that stack.
	 */
	if (!(type = stack_type(saddr, trace))) {
		if (DEBUG(KLDC_TRACE, 1)) {
			fprintf(KL_ERRORFP, "find_trace2: 0x%llx is not a valid stack.",
				saddr);
		}
		return(1);
	}

	if (type != stack_type(sp, trace)) {
		if (DEBUG(KLDC_TRACE, 1)) {
			fprintf(KL_ERRORFP, "find_trace2: sp 0x%llx is not from stack "
						"0x%llx\n", sp, saddr);
		}
		KL_SET_ERROR_NVAL(KLE_BAD_SP, sp, 2);
		return(1);
	}

	curstkidx = stack_index(saddr, trace);
	if (KL_ERROR) {
		return(1);
	}

	/* Check to see if PC is valid text and from a LEAF function or
	 * there is no saved RA in the stack frame (which would be the case
	 * if the function makes no calls to other functions). If it is, 
	 * capture information about the stack frame and then use RA to 
	 * compute the new PC.
	 */
	if (frame_size = get_frame_size(pc, 0)) {
		ra_offset = get_ra_offset(pc, 0);
		asp = (uint*)((uint)trace->stack[curstkidx].ptr + 
					(sp - trace->stack[curstkidx].addr));
	}
	else {
		asp = (uint *)NULL;
	}

	if (IS_TEXT(pc) && (!frame_size || (ra_offset == -1))) {

		/* Determine the funcname, source file and line number for PC
		 */
		funcname = kl_funcname(pc);
		srcfile = kl_srcfile(pc);
		line_no = kl_lineno(pc);

		/* Capture stackframe info for PC then use RA to compute new PC
		 */
		curframe = alloc_sframe(trace);
		UPDATE_FRAME(funcname, pc, ra, sp, asp, srcfile, line_no, frame_size);
		pc = ra - 8;
		if (frame_size) {
			sp += frame_size;
		}
	}
	else if (!IS_TEXT(pc) || (pc & 3)) {

		kaddr_t newpc;

		/* If the PC is bad, try using the RA to compute a PC. If both 
		 * PC and RA are invalid then just return.
		 */
		newpc = ra - 8;
		if (!IS_TEXT(newpc) || (newpc & 3) || get_frame_size(newpc, 0)) {
			if (DEBUG(KLDC_TRACE, 1)) {
				fprintf(KL_ERRORFP, "Could not dump trace. Both PC and RA are "
					"invalid! PC=0x%llx, RA=0x%llx\n", pc, ra);
			}
			KL_SET_ERROR_NVAL(KLE_BAD_PC, newpc, 2);
			return(1);
		}
		pc = newpc;
	}

	if (find_trace(pc, sp, (kaddr_t)0, (kaddr_t)0, trace, (flags | K_ALL))) {
		return(0);
	}
	return(1);
}

/*
 * is_eframe()
 *
 *   Return 1 if eframe appears to be valid. Return 0 if it is not valid.
 */
int
is_eframe(kaddr_t ef, trace_t *trace)
{
	k_ptr_t efp;
	kaddr_t pc, ra, sp, saddr;

	/* Get the address of the stack the ef is from
	 */
	saddr = stack_addr(ef, trace);
	if (KL_ERROR) {
		return(0);
	}

	efp = kl_alloc_block(EFRAME_S_SIZE, K_TEMP);

	kl_get_block(ef, EFRAME_S_SIZE, efp, "eframe_s");
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_EFRAME_S;
		return(0);
	}

	/* Get the PC, RA and SP from the exception frame.
	*/
	sp = *(kaddr_t*)((uint)efp + (kl_member_offset("eframe_s", "ef_sp")));
	pc = *(kaddr_t*)((uint)efp + (kl_member_offset("eframe_s", "ef_epc")));
	ra = *(kaddr_t*)((uint)efp + (kl_member_offset("eframe_s", "ef_ra")));
	if (PTRSZ32) {
		pc &= 0xffffffff;
		ra &= 0xffffffff;
		sp &= 0xffffffff;
	}
	kl_free_block(efp);

	/* Check to see if the SP, PC, or RA are NULL. If any one of them
	 * is, then it most likely means that efp is not a valid eframe 
	 * pointer.
	 */
	if (!pc || !ra || !sp) {
		return(0);
	}
	if (stack_addr(sp, trace) != saddr) {
		return(0);
	}
	return(1);
}

/*
 * find_eframe() 
 * 
 *   locate the pointer to the eframe for current frame level. Check
 *   and see if the current stack pointer points to an exception frame
 *   (which would be the a0 value saved by the next function). If it
 *   does, do the following:
 *
 *   o Check to see if the eframe pointer is the current SP. If it is,
 *     update the current sframe rec and return.
 *
 *   o Otherwise, if the current stack is the interrupt stack, check
 *     to see if the eframe pointer is from the kernel stack. If it
 *     is, update the eframe record and load the kernel stack page.
 *     Then return.
 *
 *   o If the eframe pointer is located in the ublock, update the
 *     eframe rec and return.
 *
 *   If the current stack pointer does NOT point to an exception frame
 *   It is NULL or is not a valid kernel address, or does not belong
 *   in the current or interrupt stacks or is not in the ublock) do a
 *   brute force in the subsequent stack frames -- looking for a
 *   pointer that will work.
 */
int
find_eframe(trace_t *trace, int flags)
{
	int i, size, intstksz, found, ktstkidx = -1;
	int eflag = -1, curstkflg = -1, curstack = -1;
	uint *sbp, *asp, *s;
	kaddr_t ep = 0, sp, upage, saddr, lastframe, ktstack; 

	sframe_t *curframe;

	/* Initialize some variables
	 */
	curframe = trace->frame->prev;	
	asp = curframe->asp;			
	curstack = curframe->ptr;		
	saddr = trace->stack[curstack].addr; 

	/* Determine which stack we currently are on (S_KERNELSTACK/S_KEXTSTACK, 
	 * S_INTSTACK, S_STHREADSTACK, S_XTHREADSTACK, or S_UNKNOWN_STACK). 
	 * The only time the current stack should be S_UNKNOWN_STACK is when an 
	 * strace is done an address that is NOT an interrupt or bootstack for a 
	 * CPU (for example when someone converts the kernelstack address
	 * into a K0 address).
	 */
	if (!(curstkflg = stack_type(saddr, trace))) {
		return(1);
	}

	switch (curstkflg) {

		case S_KEXTSTACK :
		case S_KERNELSTACK :
			lastframe = K_KERNELSTACK + (K_PAGESZ - 144);
			break;

		case S_INTSTACK :
			lastframe = kl_kaddr(trace->pdap, "pda_s", "p_intlastframe");
			break;

		case S_BOOTSTACK :
			lastframe = kl_kaddr(trace->pdap, "pda_s", "p_bootlastframe");
			break;

		case S_XTHREADSTACK :
		case S_STHREADSTACK :
			lastframe = 0;
			break;

		case S_RAW_STACK :
			KL_SET_ERROR(KLE_NOT_IMPLEMENTED);
			return(1);
	}

	if (curstkflg == S_INTSTACK) {

		/* If we are at the end of the S_INTSTACK, then we have to 
		 * see if we should switch to some other stack and continue 
		 * our backtrace. It depends on weather or not a kthread was 
		 * running on this CPU (we ARE on the S_INTSTACK).
		 */
		if (trace->frame->prev->sp == lastframe) {
			if (trace->kthread) {
				/* We need to see if there is an eframe pointer in the
				 * running thread's kthread struct. If there is, then
				 * use that as the efrme pointer (we will have to 
				 * switch stacks too).
				 */
				if (ep = kl_kaddr(trace->ktp, "kthread", "k_eframe")) {
					/* Set the ep address and the eframe pointer in the 
					 * current sframe record.
					 */
					curstack = stack_index(ep, trace);
					saddr = trace->stack[curstack].addr;
					curframe->eframe = 
						(uint*)((uint)trace->stack[curstack].ptr + 
							(ep - saddr));
					curframe->ep = ep;
					curframe->ptr = curstack;
					return(0);
				}
				else {
					/* If this is a uthread, point to u_eframe in 
					 * exception struct.
					 */
					if (curframe->ep = trace->u_eframe) {
						curframe->eframe = (k_ptr_t)((uint)trace->exp +
									kl_member_offset("exception", "u_eframe"));
						curframe->ra = 0;
						return(0);
					}
					else {
						/* Return 1 to terminate the backtrace (there
						 * wasn't any error).
						 */
						return(1);
					}
				}
			}
			else {
				/* Like above, we have to return 1 to terminate the
				 * backtrace (there is no error).
				 */
				return(1);
			}
		}
	}

	/* If we don't know what the current stack page is, we really can't
	 * determine where the eframe pointer is. We need to mark curframe
	 * with an error and return.
	 */ 
	if (curstkflg == S_UNKNOWN_STACK) {
		KL_SET_ERROR(KLE_NO_EFRAME);
		return(1);
	}

	/* First we need to check to see if the current stack pointer
	 * points to the exception frame.
	 */
	sp = kl_reg(asp, "eframe_s", "ef_sp");
	if (PTRSZ32) {
		sp = (sp & 0xffffffff);
	}
	if (sp == (kaddr_t)(curframe->sp + EFRAME_S_SIZE)) {
		curframe->eframe = (k_ptr_t)asp;
		curframe->ep = curframe->sp;
		return(0);
	}

	/* Check to see if the current stack pointer points to the pointer
	 * to the exception frame. It's necessary to perform some sanity 
	 * checking as it's entirly possible for ep to be non-zero yet not 
	 * a valid eframe pointer.
	 */
	ep = kl_kaddr_val(asp);
	if ((ep && (ep > (K_KUBASE + K_KUSIZE))) || 
				!strcmp(curframe->funcname, "systrap") ||
				!strcmp(curframe->funcname, "tfp_special_int")) {

		if (ep == curframe->sp) {

			/* The current stack pointer is a pointer to itself. Normally,
			 * this means the current SP is the start of an exception frame.
			 * Double check to make sure this is really the case.
			 */
			sp = kl_reg(asp, "eframe_s", "ef_sp");
			if (PTRSZ32) {
				sp = (sp & 0xffffffff);
			}
			if (sp == (kaddr_t)(curframe->sp + EFRAME_S_SIZE)) {
				curframe->eframe = (k_ptr_t)asp;
				return(0);
			}
			else {
				ep = 0;
			}
		}
		else if ((ep < K_KERNELSTACK) && 
					(ep >= (K_KERNELSTACK - K_PAGESZ))) {

			/* eframe pointer is is in kernelstack
			 */
			if ((curstkflg == S_INTSTACK) && (curframe->sp == lastframe)) {
				eflag = S_KERNELSTACK; 
			}
			else {
				ep = 0;
			}
		}
		else if ((ep == trace->u_eframe) || 
					!strcmp(curframe->funcname, "systrap") ||
					!strcmp(curframe->funcname, "tfp_special_int")) {

			/* The eframe is in the ublock
			 */
			if (curstkflg == S_BOOTSTACK) {
				ep = 0;
			}
			else if (curframe->sp != lastframe) {
				ep = 0;
			}
			else {
				eflag = U_EFRAME;
				if (!ep) {
					ep = trace->u_eframe;
				}
			}
		}
		else if (trace->pdap) {
			if ((ep >= trace->intstack) && 
							(ep < (trace->intstack + K_PAGESZ))) {
				eflag = S_INTSTACK;
			}	
			else if ((ep >= trace->bootstack) && 
						(ep < (trace->bootstack + K_PAGESZ))) {
				eflag = S_BOOTSTACK;
			}
			else {
				ep = 0;
			}
		}
		else {

			/* This isn't an exception frame, set ep equal to zero to
			 * force a brute force search.
			 */
			ep = 0;
		}
	}
	else {
		ep = 0;
	}

	/* If the exception frame pointer was not found, use a brute force
	 * approach to look for one in the next stack frame. Perform some 
	 * sanity checks on any potentially valid addresses found to ensure 
	 * that an exception frame is actually there.
	 */
	if (!ep) {

		s = curframe->prev->asp;
		size = curframe->prev->frame_size / K_NBPW;

		for (i = 0; i < size; i++) {
			ep = kl_kaddr_val(s);

			if (ep == trace->u_eframe) {

				/* The address is for the eframe in the ublock
				 */
				if (DEBUG(KLDC_TRACE, 1)) {
					fprintf(KL_ERRORFP, "find_eframe: EP 0x%llx is in the "
						"ublock\n", ep);
				}	
				if (curstkflg == S_BOOTSTACK) {
					ep = 0;
				}
				else if (curframe->sp != lastframe) {
					ep = 0;
				}
				else {
					eflag = U_EFRAME;
					break;
				}
			}
			else if (trace->ktp && 
					(IS_UTHREAD(trace->ktp) && IS_KERNELSTACK(ep))) {

				kaddr_t kstkpg;

				/* The address is from the kernel stack
				 */
				if (DEBUG(KLDC_TRACE, 1)) {
					fprintf(KL_ERRORFP, "find_eframe: EP 0x%llx is from the"
						"kernelstack\n", ep);
				}

				/* Get the index for the kernelstack page for the current 
				 * process (if there is one -- defkthread if there isn't).
				 */
				kstkpg = stack_index(K_KERNELSTACK, trace);
				if (KL_ERROR) {
					curframe->error = KLE_BAD_SADDR;
					return(1);
				}

				if (curstkflg == S_BOOTSTACK) {
					ep = 0;
				}
				else if (curstkflg == S_INTSTACK) {
					if (curframe->sp != lastframe) {
						ep = 0;
					}
				}
				else if (curstkflg == S_KERNELSTACK) {

					k_ptr_t eptr;

					if (curframe->sp == lastframe) {
						ep = 0;
					}
					else {
						eptr = (k_ptr_t*)((uint)trace->stack[kstkpg].ptr + 
							(ep - K_KERNELSTACK));
						if (kl_reg(eptr, "eframe_s", "ef_sp") != 
								(kaddr_t)(ep + EFRAME_S_SIZE)) {
							ep = 0;
						}
					}
				}

				if (ep) {
					is_eframe(ep, trace);
					if (KL_ERROR) {
						curframe->error = KLE_BAD_EFRAME_S;
					}
					curstack = kstkpg;
					eflag = S_KERNELSTACK;
					break;
				}
			}
			else if (trace->ktp && !IS_UTHREAD(trace->ktp)) {

				/* This is not a process (e.g., no u_area). Check to see 
				 * if ep is from the kthread stack (NOT the kernelstack). 
				 * First, check to see if the kthread stack is mapped in 
				 * the trace struct.
				 */
				if (ktstkidx == -1) {
					for (i = 0; i < trace->stackcnt; i++) {
						if ((trace->stack[i].type == S_XTHREADSTACK) ||
								(trace->stack[i].type == S_STHREADSTACK)) {
							ktstkidx = i;
							ktstack = trace->stack[i].addr;
							break;
						}
					}
					if (i == trace->stackcnt) {
						/* We didn't find it, return an error
						 */
						KL_SET_ERROR(KLE_BAD_KERNELSTACK);
						return(1);
					}
				}

				/* Now, check to see if ep is on the kthread stack
				 */
				if ((ep >= ktstack) && 
						(ep < (ktstack + trace->stack[ktstkidx].size))) {

					if (curstkflg == S_BOOTSTACK) {
						ep = 0;
					}
					else if (curstkflg == S_INTSTACK) {
						if (curframe->sp != lastframe) {
							ep = 0;
						}
					}
				}
				else {
					ep = 0;
				}

				if (ep) {
					curstack = ktstkidx;
					if (IS_XTHREAD(trace->ktp)) {
						eflag = S_XTHREADSTACK;
					}
					else if (IS_STHREAD(trace->ktp)) {
						eflag = S_STHREADSTACK;
					}
					break;
				}
			}
			else if (trace->pdap) {

				if ((ep >= trace->intstack) && 
					(ep < (trace->intstack + stack_size(S_INTSTACK, trace)))) {

					/* Address is from the CPU intstack 
					 */
					if (curstkflg == S_INTSTACK) {
						eflag = S_INTSTACK;
						break;
					}
					else {
						ep = 0;
					}
				}
				else if ((ep >= trace->bootstack) && 
					(ep < (trace->bootstack + 
							stack_size(S_BOOTSTACK, trace)))) {

					/* If address is from the CPU bootstack  
					 */
					if (curstkflg == S_INTSTACK) {
						eflag = S_BOOTSTACK;
						break;
					}
					else {
						ep = 0;
					}
				}
				else {
					ep = 0;
				}
			}
			else if ((ep >= trace->stack[curstack].addr) &&
						(ep < (trace->stack[curstack].addr + 
						trace->stack[curstack].size))) {

				/* The address is from the current stack page 
				 */
				if (DEBUG(KLDC_TRACE, 1)) {
					fprintf(KL_ERRORFP, "find_eframe: EP 0x%llx is from the "
						"current stack page (0x%llx)\n", 
						ep, trace->stack[curstack].addr);
				}
				eflag = S_UNKNOWN_STACK;
				break;
			}
			s += (K_NBPW / 4);
		}

		if (!ep) {

			/* If we didn't find a potential eframe address, just
			 * assume that it's in the current stack and forge ahead.
			 * Before we do that though, we should check to make sure
			 * that asp is eight-byte aligned (register values are
			 * all eight bytes in length -- this prevents a SIGBUS
			 * later on).
			 */
			if ((uint)asp % 8) {
				curframe->error = KLE_BAD_EFRAME_S;
				return(1);
			}
			curframe->eframe = (k_ptr_t)asp;
			curframe->ep = curframe->sp;
			return(0);
		}
	}

	switch (eflag) {

		case U_EFRAME: /* eframe in ublock */
			
				/* Make sure the kthread is of type KT_UTHREAD
				 */
				if (!IS_UTHREAD(trace->ktp)) {
					if (DEBUG(KLDC_TRACE, 1)) {
						fprintf(KL_ERRORFP, "find_eframe: No process set!\n");
					}
					KL_SET_ERROR(KLE_KTHREAD_TYPE(trace->ktp));
					curframe->error = KLE_KTHREAD_TYPE(trace->ktp);
					return(1);
				}

				/* Point eframe to u_eframe in exception struct
				 */
				curframe->ep = trace->u_eframe;
				curframe->eframe = (k_ptr_t)((uint)trace->exp + 
							kl_member_offset("exception", "u_eframe"));
				curframe->ra = 0;
				return(0);

		case S_KERNELSTACK: /* eframe is on kernelstack */

				curstack = stack_index(K_KERNELSTACK, trace);
				if (KL_ERROR) {
					curframe->error = KLE_BAD_SADDR;
					return(1);
				}
				break;

		case S_INTSTACK: /* eframe is on interrupt stack */

				curstack = stack_index(trace->intstack, trace);
				if (KL_ERROR) {
					curframe->error = KLE_BAD_SADDR;
					return(1);
				}
				break;

		case S_BOOTSTACK: /* eframe is on bootstack */

				curstack = stack_index(trace->bootstack, trace);
				if (KL_ERROR) {
					curframe->error = KLE_BAD_SADDR;
					return(1);
				}
				break;

		case S_XTHREADSTACK: /* kernel xthread stack */
		case S_STHREADSTACK: /* kernel sthread stack */
				break;


		default: /* Unknown error */
				
				KL_SET_ERROR(KLE_UNKNOWN_ERROR);
				curframe->error = KLE_UNKNOWN_ERROR;
				return(1);
	}
				
	/* Set the ep address and the eframe pointer in the current 
	 * sframe record.
	 */
	saddr = trace->stack[curstack].addr;
	curframe->eframe = (uint*)((uint)trace->stack[curstack].ptr + (ep - saddr));
	curframe->ep = ep;
	curframe->ptr = curstack;
	return(0);
}

#ifdef INCLUDE_REGINFO
/*
 * check_instr()
 */
int
check_instr(uint instr, sframe_t *fp)
{
	int i, rd, rt, rs, sa, base, offset;

	if (PTRSZ64) {
		switch ((instr & 0xfc000000) >> 26) {
			case 0x23:
			case 0x37:
				/* lw rt, offset(base) 
				 * 
				 * We have to check and see if this instruction modifys
				 * any of the A-regs or S-regs before their value has 
				 * been saved. 
				 */
				rt = ((instr & 0x1f0000) >> 16);
				if ((IS_AREG(rt) || IS_SREG(rt)) &&
								(fp->regs[rt].state == REGVAL_UNKNOWN)) {
					fp->regs[rt].state = REGVAL_BAD;
				}
				break;

			case 0x3f:
				/* sd rt, offset(base) 
				 */

				rt = ((instr & 0x1f0000) >> 16);
				base = ((instr & 0x3e00000) >> 21);
				offset = (instr & 0xffff);
				if ((base == 29) && 
						(IS_SREG(rt) || IS_AREG(rt) || (rt == 31)) && 
										fp->regs[rt].state == REGVAL_UNKNOWN) {
					fp->regs[rt].state = REGVAL_VALID;
					kl_get_block((fp->sp + offset), 8, 
								&fp->regs[rt].value, "reg_value");

					/* If this was an A-reg that was saved, check the
					 * other A-regs to see if one of them had been stuffed
					 * into this register (just like an S-reg).
					 */
					if (IS_AREG(rt)) {
						for (i = 4; i < 12; i++) {
							if (i == rt) {
								continue;
							}
							if ((fp->regs[i].state == REGVAL_SAVEREG) &&
										(fp->regs[i].savereg == rt)) {
								fp->regs[i].state = REGVAL_VALID;
								fp->regs[i].value = fp->regs[rt].value;
								break;
							}
						}
					}
				}
				break;

			case 0x0: /* SPECIAL */

				/* We now have to determine which special instruction 
				 * this is.
				 */
				switch (instr & 0x3f) {
					case 0x0:
						/* sll rd, rt, sa (sll rd, rt, 0)
						 * 
						 * Save a place holder for these values in the 
						 * current frame. The REAL value will be determined
						 * later on.
						 */

						rd = ((instr & 0xf800) >> 11);
						rt = ((instr & 0x1f0000) >> 16);
						sa = ((instr & 0x7c0) >> 6);

						if (!sa && IS_AREG(rt) && 
									(fp->regs[rt].state == REGVAL_UNKNOWN)) {
							fp->regs[rt].state = REGVAL_SAVEREG;
							fp->regs[rt].savereg = rd;
						}
						else if ((IS_AREG(rd) || IS_SREG(rd)) &&
									(fp->regs[rd].state == REGVAL_UNKNOWN)) {
							fp->regs[rd].state = REGVAL_BAD;

						}
						break;

					case 0x25:
						/* move rd, rs (or rd, rs, 0)
						 * 
						 * Save a place holder for these values in the 
						 * current frame. The REAL value may be determined
						 * later on.
						 */

						rs = ((instr & 0x3e00000) >> 21);
						rd = ((instr & 0xf800) >> 11);

						if (IS_AREG(rs) && 
									(fp->regs[rs].state == REGVAL_UNKNOWN)) {
							fp->regs[rs].state = REGVAL_SAVEREG;
							fp->regs[rs].savereg = rd;
						}
						else if ((IS_AREG(rd) || IS_SREG(rd)) &&
									(fp->regs[rd].state == REGVAL_UNKNOWN)) {
							fp->regs[rd].state = REGVAL_BAD;

						}
						break;

					default:
						break;
				}
				break;

			default:
				break;
		}
	}
	else {
		/* Need to test this out on 32-bit systems
		 */
		return(-1);
	}
	return(0);
}

/*
 * get_eframe_regs()
 */
void
get_eframe_regs(reg_rec_t *regs, k_ptr_t eframe)
{
	/* Save the A-regs
	 */
	regs[4].state = REGVAL_VALID;
	regs[4].value = *(kaddr_t*)((uint)eframe + 
		(kl_member_offset("eframe_s", "ef_a0")));
	regs[5].state = REGVAL_VALID;
	regs[5].value = *(kaddr_t*)((uint)eframe + 
		(kl_member_offset("eframe_s", "ef_a1")));
	regs[6].state = REGVAL_VALID;
	regs[6].value = *(kaddr_t*)((uint)eframe + 
		(kl_member_offset("eframe_s", "ef_a2")));
	regs[7].state = REGVAL_VALID;
	regs[7].value = *(kaddr_t*)((uint)eframe + 
		(kl_member_offset("eframe_s", "ef_a3")));
	regs[8].state = REGVAL_VALID;
	regs[8].value = *(kaddr_t*)((uint)eframe + 
		(kl_member_offset("eframe_s", "ef_a4")));
	regs[9].state = REGVAL_VALID;
	regs[9].value = *(kaddr_t*)((uint)eframe + 
		(kl_member_offset("eframe_s", "ef_a5")));
	regs[10].state = REGVAL_VALID;
	regs[10].value = *(kaddr_t*)((uint)eframe + 
		(kl_member_offset("eframe_s", "ef_a6")));
	regs[11].state = REGVAL_VALID;
	regs[11].value = *(kaddr_t*)((uint)eframe + 
		(kl_member_offset("eframe_s", "ef_a7")));

	/* Save the S-regs
	 */
	regs[16].state = REGVAL_VALID;
	regs[16].value = *(kaddr_t*)((uint)eframe + 
		(kl_member_offset("eframe_s", "ef_s0")));
	regs[17].state = REGVAL_VALID;
	regs[17].value = *(kaddr_t*)((uint)eframe + 
		(kl_member_offset("eframe_s", "ef_s1")));
	regs[18].state = REGVAL_VALID;
	regs[18].value = *(kaddr_t*)((uint)eframe + 
		(kl_member_offset("eframe_s", "ef_s2")));
	regs[19].state = REGVAL_VALID;
	regs[19].value = *(kaddr_t*)((uint)eframe + 
		(kl_member_offset("eframe_s", "ef_s3")));
	regs[20].state = REGVAL_VALID;
	regs[20].value = *(kaddr_t*)((uint)eframe + 
		(kl_member_offset("eframe_s", "ef_s4")));
	regs[21].state = REGVAL_VALID;
	regs[21].value = *(kaddr_t*)((uint)eframe + 
		(kl_member_offset("eframe_s", "ef_s5")));
	regs[22].state = REGVAL_VALID;
	regs[22].value = *(kaddr_t*)((uint)eframe + 
		(kl_member_offset("eframe_s", "ef_s6")));
	regs[23].state = REGVAL_VALID;
	regs[23].value = *(kaddr_t*)((uint)eframe + 
		(kl_member_offset("eframe_s", "ef_s7")));
	regs[30].state = REGVAL_VALID;
	regs[30].value = *(kaddr_t*)((uint)eframe + 
		(kl_member_offset("eframe_s", "ef_fp")));

	/* Save the RA
	 */
	regs[31].state = REGVAL_VALID;
	regs[31].value  = *(kaddr_t*)((uint)eframe + 
		(kl_member_offset("eframe_s", "ef_ra")));
}

/*
 * get_machregs()
 */
void
get_machregs(reg_rec_t *regs, uint64 *machregsp)
{
	regs[16].value = ((uint64*)machregsp)[0]; /* S0 */
	regs[16].state = REGVAL_VALID;
	regs[17].value = ((uint64*)machregsp)[1]; /* S1 */
	regs[17].state = REGVAL_VALID;
	regs[18].value = ((uint64*)machregsp)[1]; /* S2 */
	regs[18].state = REGVAL_VALID;
	regs[19].value = ((uint64*)machregsp)[3]; /* S3 */
	regs[19].state = REGVAL_VALID;
	regs[20].value = ((uint64*)machregsp)[4]; /* S4 */
	regs[20].state = REGVAL_VALID;
	regs[21].value = ((uint64*)machregsp)[5]; /* S5 */
	regs[21].state = REGVAL_VALID;
	regs[22].value = ((uint64*)machregsp)[6]; /* S6 */
	regs[22].state = REGVAL_VALID;
	regs[23].value = ((uint64*)machregsp)[7]; /* S7 */
	regs[23].state = REGVAL_VALID;
}

/* 
 * get_reg_values() 
 *
 *   Walk through the stack, frame-by-frame, and determine as many 
 *   of the saved register values as possible.
 */
int
get_reg_values(trace_t *trace)
{
	int i, size, func_size, areg, sreg;
	uint instr;
	sframe_t *fp, *tfp1, *tfp2;
	k_ptr_t machregsp = 0;
	kaddr_t func_addr, value;

	kl_reset_error();

	/* First thing we have to do is get the starting register values
	 * if at all possible. If the backtrace begins with an eframe,
	 * then it's possible to get all register values. Otherwise
	 * we can only get the values for the S-regs (saved in the 
	 * k_regs field of the kthread, the pda_s struct, or dumpregs).
	 */
	if (trace->pdap) {
		/* This backtrace originates on a CPU. We need to check and 
		 * see if this is the panicking CPU. If it is, then get the 
		 * S-reg values from dumpregs. Otherwise, get them from the 
		 * pda_s struct.
		 */
		if (kl_uint(trace->pdap, "pda_s", "p_panicregs_valid", 0)) {
			machregsp = (uint64*)((uint)trace->pdap +
								kl_member_offset("pda_s", "p_panicregs_tbl"));
			get_machregs(trace->regs, machregsp);
		}
		else {
			get_machregs(trace->regs, (uint64*)K_DUMPREGS);
		}
	}
	else if (trace->ktp) {
		uint64 *eframe;

		if (kl_kaddr(trace->ktp, "kthread", "k_eframe")) {
			eframe = (uint64*)((uint)trace->ktp + 
				kl_member_offset("kthread", "k_regs"));
			get_eframe_regs(trace->regs, eframe);
		}
		else {
			get_machregs(trace->regs, (uint64*)trace->ktp);
		}
	}

	/* Get the last (newest) frame on the stack. For the first pass,
	 * we are going to walk back up the stack.
	 */
	fp = trace->frame;
	do {
		if (fp->eframe) {
			get_eframe_regs(fp->regs, fp->eframe);
			fp = fp->next;
			continue;
		}
		func_addr = kl_funcaddr(fp->pc);
		if (KL_ERROR) {
			KL_SET_ERROR_NVAL(KLE_NO_RA, fp->pc, 2);
			return(-1);
		}

		/* Set funcsize so that we only walk the code upto the PC
		 * for this frame.
		 */
		func_size = kl_funcsize(fp->pc);
		if (KL_ERROR) {
			/* This should never really happen, but just in case...
			 */
			if (DEBUG(KLDC_TRACE, 1)) {
				fprintf(KL_ERRORFP, "set_regvalues: func_addr = 0x%llx, "
					"size=%d\n", func_addr, size);
			}
			size = 20;
		}
		else {
			/* Convert size to number of instructions (upto pc + 1 to
			 * allow for jump delay slot).
			 */
			size = ((fp->pc - func_addr) / 4) + 1;
			if (size > func_size) {
				size = func_size / 4;
			}
		}

		for(i = 0; i < size; i++) {
			kl_get_block((func_addr + (i * 4)), 4, &instr, "instr");
			check_instr(instr, fp);
		}
		fp = fp->next;
	} while (fp != trace->frame);

	/* We now have to fill in the S-reg values from the regs array
	 * in trace for the first (newest) frame in the backtrace. This
	 * must be done before we begin pass 2 below.
	 */
	fp = trace->frame;
	for (i = 0; i < 32; i++) {
		if (!IS_AREG(i) && !IS_SREG(i)) {
			continue;
		}
		if ((fp->regs[i].state == REGVAL_UNKNOWN) && 
							(trace->regs[i].state == REGVAL_VALID)) {
			fp->regs[i].state = REGVAL_VALID;
			fp->regs[i].value = trace->regs[i].value;
		}
	}

	/* Pass 2. Go from the oldest frame to the newest. Look for A-regs
	 * that have been saved in S-regs. When one is found, walk ahead
	 * and look for where the S-reg might have been stuffed in a stack
	 * frame. If we find one, drag the saved value back to the current
	 * frame to fill in the A-reg value (fill in all S-values between
	 * there and here has well.
	 */
	 fp = trace->frame->prev;
	 do {
		for (i = 0; i < 32; i++) {
			if (fp->regs[i].state == REGVAL_SAVEREG) {
				tfp1 = fp->prev;
				areg = i;
				sreg = fp->regs[i].savereg;
				while (tfp1 != trace->frame->prev) {
					if (tfp1->regs[sreg].state == REGVAL_VALID) {
						value = tfp1->regs[sreg].value;
						tfp2 = tfp1->next;
						while (tfp2 != fp) {
							tfp2->regs[sreg].state = REGVAL_VALID;
							tfp2->regs[sreg].value = value;
							tfp2 = tfp2->next;
						}
						fp->regs[areg].state = REGVAL_VALID;
						fp->regs[areg].value = value;
						break;
					}
					tfp1 = tfp1->prev;
				}
			}
		}
		fp = fp->prev;
	 } while (fp != trace->frame);

#ifdef NOT
	 /* Now for the last pass. Walk from the bottom (newest) frame in the
	  * stack to the top of the stack. Fill in all the S-regs and A-regs 
	  * that haven't been identified so far -- if possible.
	  * 
	  * XXX - Still working on this...
	  */
	 fp = trace->frame->prev;
	 do {
		for (i = 0; i < 32; i++) {
			if (IS_AREG(i) || IS_SREG(i)) {
				if (fp->regs[i].state != REGVAL_VALID) {
					tfp1 = fp->prev;
					areg = i;
					sreg = fp->regs[i].savereg;
					while (tfp1 != trace->frame) {
						if (tfp1->regs[sreg].state == REGVAL_VALID) {
							value = tfp1->regs[sreg].value;
							tfp2 = tfp1->next;
							while (tfp2 != fp) {
								tfp2->regs[sreg].state = REGVAL_VALID;
								tfp2->regs[sreg].value = value;
								tfp2 = tfp2->next;
							}
							fp->regs[areg].state = REGVAL_VALID;
							fp->regs[areg].value = value;
							break;
						}
						tfp1 = tfp1->prev;
					}
				}
			}
		}
		fp = fp->prev;
	 } while (fp != trace->frame);
#endif
	return(0);
}


/*
 * print_reg_values()
 */
void
print_reg_values(sframe_t *fp, FILE *ofp)
{
	int i, count;

	fprintf(ofp, "\n");
	for (i = 0; i < 32; i++) {
		switch(fp->regs[i].state) {
			case REGVAL_VALID:
				fprintf(ofp, "r%d=0x%llx ", i, fp->regs[i].value);
				if (fp->regs[i].savereg) {
					fprintf(ofp, "(r%d-->r%d)\n", i, fp->regs[i].savereg);
				}
				else {
					fprintf(ofp, "\n");
				}
				break;

			case REGVAL_SAVEREG:
				fprintf(ofp, "r%d-->r%d\n", i, fp->regs[i].savereg);
				break;

			case REGVAL_BAD:
				fprintf(ofp, "r%d is BAD!\n", i);
				break;

#ifdef NOT
			case REGVAL_UNKNOWN:
				fprintf(ofp, "r%d is UNKNOWN!\n", i);
				break;
#endif
		}
		count++;
	}
	fprintf(ofp, "\n");
}
#endif /* INCLUDE_REGINFO */

/*
 * cpu_trace()
 */
int
cpu_trace(int cpuid, int flags, FILE *ofp)
{
	trace_t *trace;
	kstruct_t *ksp;
	k_ptr_t pdap;

	pdap = kl_alloc_block(PDA_S_SIZE, K_TEMP);
	kl_get_pda_s((kaddr_t)cpuid, pdap);
	if (KL_ERROR) {
		kl_free_block(pdap);
		return(1);
	}
	trace = alloc_trace_rec();
	ksp = (kstruct_t*)kl_alloc_block(sizeof(kstruct_t), K_TEMP);
	ksp->addr = kl_pda_s_addr(cpuid);
	ksp->ptr = pdap;
	setup_trace_rec((kaddr_t)0, ksp, CPU_FLAG, trace);
	kl_free_block((k_ptr_t)ksp);
	if (KL_ERROR) {
		if (!(flags & K_SUPRSERR)) {
			print_trace_error(trace, ofp);
		}
		free_trace_rec(trace);
		return(1);
	}
	if (!ACTIVE) {

		/* Even if there was an error, we want to print out the
		 * portion of the trace we were able to get.
		 */
		if (get_cpu_trace(cpuid, trace) && !trace->nframes) {
			if (!(flags & K_SUPRSERR)) {
				print_trace_error(trace, ofp);
			}
			free_trace_rec(trace);
			return(1);
		}
		print_trace(trace, flags, ofp);
	}
	else if (!(flags & K_SUPRSERR)) {
		fprintf(ofp, "Process is currently running on CPU %d.\n\n", cpuid);
	}
	free_trace_rec(trace);
	return(0);
}

/*
 * get_cpu_trace()
 */
int
get_cpu_trace(int cpuid, trace_t *trace)
{
	int size, stkflg, panicregs_valid = 0; 
	kaddr_t curkthread, epc, pc, ra, sp, saddr, saved_kthread = 0;
	k_ptr_t machregsp;

	kl_reset_error();

	/* Make sure this is a core dump. We can't do a CPU trace on
	 * a live system.
	 */
	if (ACTIVE) {
		KL_SET_ERROR(KLE_ACTIVE);
		return(1);
	}

	/* Based on the current status of the CPU, determine what the 
	 * address of the current stack is.
	 */
	stkflg = kl_uint(trace->pdap, "pda_s", "p_kstackflag", 0);
	if (stkflg == S_KERNELSTACK) {
		saddr = kthread_stack(trace, (int *)NULL);
	}
	else if (stkflg == S_INTSTACK) {
		saddr = trace->intstack;
	}
	else if (stkflg == S_BOOTSTACK) {
		saddr = trace->bootstack;
	}

	/* Make sure that defkthread is set to the kthread running on 
	 * cpuid (if there is one).
	 */
	if (K_DEFKTHREAD != trace->kthread) {
		saved_kthread = K_DEFKTHREAD;
		kl_set_defkthread(trace->kthread);
	}

	/* Check to see if the dump was initiated by an NMI...If it was,
	 * use kl_NMI_saveregs to get the starting epc, pc, sp, and ra.
	 */
	if (IS_NMI) {

		if (kl_NMI_saveregs(cpuid, &epc, &pc, &sp, &ra) == -1) {
			if (saved_kthread) {
				kl_set_defkthread(saved_kthread);
			}
			KL_SET_ERROR(KLE_NO_TRACE);
			return(1);
		}

		/* Make sure that saddr is set correctly. It's possible that
		 * there is a bad kthread running on the CPU (because this is
		 * an NMI dump). Regardless of the current setting of saddr,
		 * set it again based on the contents of sp.
		 */
		saddr = stack_addr(sp, trace);
		if (!find_trace2(epc, ra, sp, saddr, trace, K_ALL)) {

			/* We found a trace
			 */
			if (saved_kthread) {
				kl_set_defkthread(saved_kthread);
			}
			return(0);
		}

		/* No trace was found. 
		 */
		if (saved_kthread) {
			kl_set_defkthread(saved_kthread);
		}
		KL_SET_ERROR(KLE_NO_TRACE);
		return(1);
	}

	/* Check to see if p_panicregs_valid in the pda_s struct is non-NULL. 
	 * If it is, use the values stored in p_panicregs_tbl to generate the 
	 * trace. Otherwise, use the values stored in dumpregs. In both cases, 
	 * PC is element 10, SP is element 8.
	 */
	if (kl_uint(trace->pdap, "pda_s", "p_panicregs_valid", 0)) {
		machregsp = (k_ptr_t)((uint)trace->pdap + 
				kl_member_offset("pda_s", "p_panicregs_tbl"));
		panicregs_valid = 1;
	}
	else {
		machregsp = K_DUMPREGS;
	}
	pc = ((kaddr_t*)machregsp)[10];
	sp = ((kaddr_t*)machregsp)[8];

	if (PTRSZ32) {
		pc &= 0xffffffff;
		sp &= 0xffffffff;
	}

	/* If sp is from the kernel/kthread stack, make sure that defkthread is
	 * set. Otherwise, make sure that sp comes from an intstack or bootstack
	 * AND make sure it is from the one pointed to by trace->pdap. In certain 
	 * rare situations, it's possible that the p_panicregs_tbl will not be 
	 * valid for more than one CPU in a dump. If we don't check, we could use 
	 * dumpregs for the wrong CPU.
	 */
	if (IS_KERNELSTACK(sp) || is_kthreadstack(sp, trace)) {
		if (!K_DEFKTHREAD) {
			if (saved_kthread) {
				kl_set_defkthread(saved_kthread);
			}
			KL_SET_ERROR_NVAL(KLE_BAD_SP, sp, 2);
			return(1);
		}
	}
	else if (!trace->pdap) {
		/* This shouldn'n happen, but just in case...
		 */
		if (saved_kthread) { 
			kl_set_defkthread(saved_kthread);
		}
		KL_SET_ERROR(KLE_NO_PDA);
		return(1);
	}
	else {
		saddr = 0;
		if ((sp >= trace->intstack) && 
			(sp < trace->intstack + stack_size(S_INTSTACK, trace))) {
			saddr = trace->intstack;
		}
		else if ((sp >= trace->bootstack) && 
			(sp < trace->bootstack + stack_size(S_BOOTSTACK, trace))) {
			saddr = trace->bootstack;
		}
		if (!saddr) {
			if (saved_kthread) { 
				kl_set_defkthread(saved_kthread);
			}
			if (panicregs_valid) {
				KL_SET_ERROR_NVAL(KLE_BAD_SADDR, sp, 2);
			}
			else {
				KL_SET_ERROR(KLE_NO_TRACE);
			}
			return(1);
		}
	}

	/* Just in case, make sure the PC is valid.
	 */
	if (!IS_TEXT(pc) || (pc & 3)) {
		if (DEBUG(KLDC_TRACE, 1)) {
			fprintf(KL_ERRORFP, "PC is invalid (pc=0x%llx)\n", pc);
		}
		if (saved_kthread) { 
			kl_set_defkthread(saved_kthread);
		}
		KL_SET_ERROR_NVAL(KLE_BAD_PC, pc, 2);
		return(1);
	}

	/* Find a stack trace
	 */
	trace->nframes = find_trace(pc, sp, (kaddr_t)0, (kaddr_t)0, trace, K_ALL);
	if (trace->nframes == 0) {
		if (DEBUG(KLDC_TRACE, 1)) {
			fprintf(KL_ERRORFP, "get_cpu_trace: Not a valid trace! "
					"(pc=0x%llx, sp=0x%llx, saddr=0x%llx\n", pc, sp, saddr);
		}
		if (saved_kthread) { 
			kl_set_defkthread(saved_kthread);
		}
		KL_SET_ERROR(KLE_NO_TRACE);
		return(1);
	}

	if (saved_kthread) { 
		kl_set_defkthread(saved_kthread);
	}
	return(0);
}

#define TRACE_TABLE_SIZE 100

#define NEXT_WORD(wordp, sp) \
			BUMP_WORDP(wordp); \
			if (PTRSZ64) { \
				sp += 8; \
			} \
			else { \
				sp += 4; \
			}

struct t {
	kaddr_t pc;
	kaddr_t sp;
};

/* 
 * print_valid_traces()
 *
 *   Parameters:
 *
 *   saddr		Address of the stack we are searching in
 *   stack		Structure containing all stack related info and buffers
 *   level		Minimum number of frames for valid stack
 *   flags
 *   ofp
 *
 */
int
print_valid_traces(kaddr_t saddr, 
		trace_t *trace, int level, int flags, FILE *ofp)
{
	int i = 0, j;
	int stkidx, frame_size, ra_offset, valid_trace, sframe_count;
	kaddr_t sp, rsp, pc, jpc; 
	uint instr, *wordp;
	char *funcname;
	struct t *trace_table;
	sframe_t *sframe_hold;

	if (DEBUG(KLDC_TRACE, 1) || DEBUG(KLDC_FUNCTRACE, 3)) {
		fprintf(KL_ERRORFP, "print_valid_traces: trace=0x%x, level=%d\n", 
				trace, level);
	}

	/* Set the kernel stack pointer (sp) and word pointer (wordp) at
	 * the start of the stack page -- unless stack page and ublock are
	 * in the same page. Then set sp and saddr just beyond the 
	 * ublock.
	 */
	stkidx = stack_index(saddr, trace);
	if (KL_ERROR) {
		return(1);
	}

	trace_table = 
		(struct t*)kl_alloc_block(TRACE_TABLE_SIZE * sizeof(struct t), K_TEMP);

	wordp = trace->stack[stkidx].ptr;
	sp = saddr;

	while(wordp < trace->stack[stkidx].ptr + (trace->stack[stkidx].size/4)) {

		if (pc = kl_kaddr_val(wordp)) {

			/* determine the real PC (subtract 8 from the potential RA).
			 */
			pc -= 8;

			/* check to see if the pc is a valid code address 
			 */
			if (!IS_TEXT(pc) || (pc & 3)) {
				NEXT_WORD(wordp, sp);
				continue;
			}

			/* get the instruction pc points to 
			 */
			kl_get_block(pc, 4, &instr, "instr");

			/* Go to the next word in the stack if instr isn't a 
			 * jump or jump and link 
			 */
			if (!((instr >> 26) == 2) && !((instr >> 26) == 3)) {
				NEXT_WORD(wordp, sp);
				continue;
			}

			/* Determine the jump pc address 
			 */
			jpc = ((instr & 0x3ffffff) << 2) | (pc >> 28 << 28);

			/* check to see if jpc is a valid code address 
			 */
			if (!IS_TEXT(jpc) || (jpc & 3)) {
				NEXT_WORD(wordp, sp);
				continue;
			}

			/* Determine size of the jpc stack frame 
			 */
			if (frame_size = get_frame_size(jpc, 1)) {

				/* Determine offset to ra in stack frame 
				 */
				ra_offset = get_ra_offset(jpc, 1);

				if (DEBUG(KLDC_TRACE, 2)) {
					fprintf(KL_ERRORFP, "print_valid_traces: ra_offset=%d\n", 
						ra_offset);
				}
			}
			else {
				/* jpc doesn't appear to be a LEAF function (it doesn't
				 * have a stack frame). We'll make a try to see if it
				 * jumps to another function (e.g., psema --> _psema). 
				 * If it does, we'll use the jumpped to function instead 
				 * to compute the real SP.
				 */
				funcname = kl_funcname(jpc);

				if (DEBUG(KLDC_TRACE, 1)) {
					fprintf(KL_ERRORFP, "print_valid_traces: name=%s " 
						"does not have a stack frame!\n", funcname);
				}

				/* get the instruction jpc points to 
				 */
				kl_get_block(jpc, 4, &instr, "instr");

				/* if instr isn't a jump or jump and link use the
				 * current sp as the real sp.
				 */
				if (!((instr >> 26) == 2) && !((instr >> 26) == 3)) {
					ra_offset = 0;
					goto TRY_ANYWAY;
				}

				/* Disassemble the new jpc instruciton 
				 */
				jpc = ((instr & 0x3ffffff) << 2) | (pc >> 28 << 28);

				/* Check to see if jpc is a valid code address.
				 */
				if (!IS_TEXT(jpc) || (jpc & 3)) {
					ra_offset = 0;
					goto TRY_ANYWAY;
				}

				/* Determine size of the new jpc stack frame 
				 */
				frame_size = get_frame_size(jpc, 1);

				if (frame_size) {
					/* Determine offset to ra in stack frame 
					 */
					ra_offset = get_ra_offset(jpc, 1);
				}
				else {
					ra_offset = 0;
					goto TRY_ANYWAY;
				}
			}
TRY_ANYWAY:
			/* Determine the real stack pointer for pc 
			 */
			if (ra_offset == -1) {
				sp = sp + frame_size;
			}
			else {
				rsp = sp - ra_offset + frame_size;
			}

			if (DEBUG(KLDC_TRACE, 1)) {

				fprintf(KL_ERRORFP, 
					"print_valid_traces: sp=0x%llx, rsp=0x%llx\n", sp, rsp);

				fprintf(KL_ERRORFP, 
					"        pc=0x%llx (%s), jpc=0x%llx (%s)\n", 
					pc, kl_funcname(pc), jpc, kl_funcname(jpc));

				fprintf(KL_ERRORFP, 
					"        frame_size=%d, ra_offset=%d\n", 
					frame_size, ra_offset);
			}

			if (find_trace(pc, rsp, (kaddr_t)0, (kaddr_t)0, 
						trace, flags) >= level) {

				/* Check to see if the current trace is a sub-trace 
				 * of one we've already found.
				 */
				j = 0;
				valid_trace = 1;

				/* Hold the valid frame records while we check to see
				 * if this is a sub-trace.
				 */
				sframe_hold = trace->frame;
				sframe_count = trace->nframes;
				trace->frame = (sframe_t*)NULL;
				trace->nframes = 0;

				while (trace_table[j].pc) {
					if (!find_trace(trace_table[j].pc, trace_table[j].sp, 
								pc, rsp, trace, flags)) {
						free_sframes(trace);
						valid_trace = 0;
						break;
					}
					free_sframes(trace);
					j++;
				}
				trace->frame = sframe_hold;
				trace->nframes = sframe_count;

				/* print the trace if it is valid and unique -- or if
				 * the K_ALL flag was set.
				 */
				if (valid_trace) {
					fprintf(ofp, "\n");
					fprintf(ofp, "PC=0x%llx, SP=0x%llx\n", pc, rsp);
					fprintf(ofp, "========================================="
						"=======================\n");
					print_trace(trace, flags, ofp);
					fprintf(ofp, "========================================="
						"=======================\n");
					trace_table[i].pc = pc;
					trace_table[i].sp = rsp;
					if (++i == TRACE_TABLE_SIZE) {
						fprintf(KL_ERRORFP, "There are too many stack traces "
							"for trace_table (max = 100)!\n");
						goto ALL_DONE;
					}
				}
			}
			NEXT_WORD(wordp, sp);
			free_sframes(trace);
		}
		else {
			if (DEBUG(KLDC_TRACE, 2)) {
				fprintf(KL_ERRORFP, "--print_valid_traces: wordp=0x%x, "
					"*wordp=0x%llx, sp=0x%llx\n", 
					wordp, kl_kaddr_val(wordp), sp);
			}
			NEXT_WORD(wordp, sp);
		}
	}

ALL_DONE:

	/* Make sure and clear any error that might be set (after all
	 * we are likely to generate some with random tries at stack
	 * traces).
	 */
	kl_reset_error();
	kl_free_block((k_ptr_t)trace_table);
	return(0);
}

/*
 * do_list()
 *
 *   Output a list of all valid code addresses contained in a stack
 *   along with their function name, line number, and source file name.
 */
int
do_list(kaddr_t saddr, trace_t *trace, FILE *ofp)
{
	int stkidx, ifd, line_no;
	char *srcfile, *funcname;
	uint *wordp;
	kaddr_t addr;

	if (DEBUG(KLDC_TRACE, 1) || DEBUG(KLDC_FUNCTRACE, 3)) {
		fprintf(KL_ERRORFP, "do_list: trace=0x%x\n", trace);
	}

	stkidx = stack_index(saddr, trace);
	if (KL_ERROR) {
		return(1);
	}

	wordp = trace->stack[stkidx].ptr;
	while(wordp < (trace->stack[stkidx].ptr + trace->stack[stkidx].size/4)) {
		if (addr =  kl_kaddr_val(wordp)) {

			/* check to see if this is a valid code address 
			 */
			if (funcname = kl_funcname(addr)) {

				/* Determine the source file name and line number 
				 */
				srcfile = kl_srcfile(addr);
				line_no = kl_lineno(addr);
				fprintf(ofp, "0x%llx -- 0x%llx\n    (%s:%d \"%s\")\n", 
					(trace->stack[stkidx].addr + ((uint)wordp - 
						(uint)trace->stack[stkidx].ptr)), 
						addr, funcname, line_no, srcfile);
				/* Make sure and free the memory block that holds 
				 * the source filename.
				 */
				if (srcfile) {
					kl_free_block((k_ptr_t)srcfile);
				}
			}
			BUMP_WORDP(wordp);
		}
		else {
			BUMP_WORDP(wordp);
		}
	}
	return(0);
}

/* 
 * eframe_trace()
 */
int
eframe_trace(kaddr_t ef, trace_t *trace, int flags, FILE *ofp)
{
	kaddr_t addr, pc, ra, sp, saddr, defk_saved = 0;
	kstruct_t *ksp;
	k_ptr_t efp, ktp;

	efp = kl_alloc_block(EFRAME_S_SIZE, K_TEMP);

	if ((trace->flags & TF_TRACEREC_VALID) && trace->kthread != K_DEFKTHREAD) {
		defk_saved = K_DEFKTHREAD;
		kl_set_defkthread(trace->kthread);
	}
	kl_get_struct(ef, EFRAME_S_SIZE, efp, "eframe_s");
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_EFRAME_S;
		print_trace_error(trace, ofp);
		kl_free_block(efp);
		if (defk_saved) {
			kl_set_defkthread(defk_saved);
		}
		return(1);
	}

	/* Print out the eframe regardless of weather or not we can
	 * find a valid trace.
	 */
	if (!(flags & K_INDENT) && !(trace->flags & TF_SUPPRESS_HEADER)) {
		trace_banner(ofp);
	}
	if (!(trace->flags & TF_SUPPRESS_HEADER)) {
		fprintf(ofp, "EXCEPTION FRAME FOR 0x%llx:\n\n", ef);
	}
	print_eframe_s((kaddr_t)NULL, efp, flags, ofp);

	/* Get the PC, RA and SP from the exception frame. 
	 */
	sp = *(kaddr_t*)((uint)efp + (kl_member_offset("eframe_s", "ef_sp")));
	pc = *(kaddr_t*)((uint)efp + (kl_member_offset("eframe_s", "ef_epc")));
	ra = *(kaddr_t*)((uint)efp + (kl_member_offset("eframe_s", "ef_ra")));
	if (PTRSZ32) {
		pc &= 0xffffffff;
		ra &= 0xffffffff;
		sp &= 0xffffffff;
	}

	if (DEBUG(KLDC_TRACE, 2)) {
		fprintf(KL_ERRORFP, "eframe_trace: pc=0x%llx, ra=0x%llx, sp=0x%llx\n", 
			pc, ra, sp);
	}

	ksp = (kstruct_t*)kl_alloc_block(sizeof(kstruct_t), K_TEMP);

	/* If the trace record has not been setup already, we need to do
	 * that here.
	 */
	if (!(trace->flags & TF_TRACEREC_VALID)) {
		if (IS_KERNELSTACK(sp)) {
			if (K_DEFKTHREAD) {
				ktp = kl_get_kthread(K_DEFKTHREAD, 0);
				if (!KL_ERROR) {
					if (IS_UTHREAD(ktp)) {
						ksp->addr = K_DEFKTHREAD;
						ksp->ptr = ktp;
						setup_trace_rec((kaddr_t)0, ksp, KTHREAD_FLAG, trace);
						kl_free_block((k_ptr_t)ksp);
					}
					else {
						KL_SET_ERROR_NVAL(KLE_KTHREAD_TYPE(ktp), 
							K_DEFKTHREAD, 2);
						kl_free_block(ktp);
					}
				}
			}
			else {
				KL_SET_ERROR(KLE_NO_DEFKTHREAD);
			}
		}
		else if (K_DEFKTHREAD) {
			ktp = kl_get_kthread(K_DEFKTHREAD, 0);
			if (!KL_ERROR) {
				ksp->addr = K_DEFKTHREAD;
				ksp->ptr = ktp;
				setup_trace_rec((kaddr_t)0, ksp, KTHREAD_FLAG, trace);
				kl_free_block((k_ptr_t)ksp);
				if (KL_ERROR) {
					KL_SET_ERROR_NVAL(KLE_KTHREAD_TYPE(ktp), 
						K_DEFKTHREAD, 2);
					kl_free_block(ktp);
				}
			}
		}
		else {
			k_ptr_t pdap;
			cpuid_t cpuid;

			/* Check to see which cpu intstack/bootstack this sp might
			 * be from. 
			 */
			pdap = kl_sp_to_pdap(sp, (k_ptr_t)NULL);
			if (pdap) {
				cpuid = kl_uint(pdap, "pda_s", "p_cpuid", 0);
				ksp->addr = kl_pda_s_addr(cpuid);
				ksp->ptr = pdap;
				setup_trace_rec((kaddr_t)0, ksp, CPU_FLAG, trace);
				kl_free_block((k_ptr_t)ksp);
			}
			else if (!KL_ERROR) {
				KL_SET_ERROR_NVAL(KLE_BAD_SP, sp, 2);
			}
		}
	}

	if (!KL_ERROR) {

		/* Before we try and find a trace, check and see if the eframe
		 * pointer is the same as u_eframe in the trace_rec. If it is,
		 * it means the uthread took an interrupt while executing user code.
		 * consequently there wont be any traces to find. In that case, just
		 * return...
		 */
		if (ef == trace->u_eframe) {
			if (!(flags & K_INDENT) && !(trace->flags & TF_SUPPRESS_HEADER)) {
				trace_banner(ofp);
			}
			kl_free_block(efp);
			if (defk_saved) {
				kl_set_defkthread(defk_saved);
			}
			return(0);
		}
	
		saddr = stack_addr(sp, trace);
		if (!KL_ERROR) {
			find_trace2(pc, ra, sp, saddr, trace, (flags | K_ALL));
		}
	}

	if (KL_ERROR && !trace->nframes) {
		print_trace_error(trace, ofp);
	}
	else {
		print_trace(trace, flags, ofp);
	}

	if (!(flags & K_INDENT) && !(trace->flags & TF_SUPPRESS_HEADER)) {
		trace_banner(ofp);
	}
	kl_free_block(efp);
	if (defk_saved) {
		kl_set_defkthread(defk_saved);
	}
	return(0);
}

/*
 * proc_trace()
 */
int
proc_trace(trace_t *trace, int flags, FILE *ofp)
{
	int i, stkflg = -1, stacksize;
	pid_t p_pid;
	cpuid_t cpuid;
	char p_stat;
	k_ptr_t pdap, kt_name;
	kaddr_t p, pda;
	kaddr_t pc, sp, saddr, upage, curkthread; 

	if (DEBUG(KLDC_TRACE, 1) || DEBUG(KLDC_FUNCTRACE, 3)) {
		fprintf(KL_ERRORFP, "proc_trace: trace=0x%x, flags=%d\n", trace, flags);
	}

	p_pid = KL_INT(trace->ktp, "proc", "p_pid"); 
	if (IS_UTHREAD(trace->ktp)) {
		fprintf (ofp, "STACK TRACE FOR UTHREAD 0x%llx ", trace->kthread);
		fprintf (ofp, "(%s, PID=%d):\n\n", 
			kl_kthread_name(trace->ktp), p_pid);
	}
	else {
		if (kt_name = kl_kthread_name(trace->ktp)) {
			fprintf (ofp, "STACK TRACE FOR KTHREAD 0x%llx ", trace->kthread);
			fprintf (ofp, "(%s, PID=%d):\n\n", kt_name, p_pid);
			kl_free_block(kt_name);
		}
		else {
			fprintf (ofp, "(PID=%d):\n\n", p_pid);
		}
	}

	/* Determine if kthread is currently running on a CPU (by 
	 * checking p_sonproc). If it isn't, then it's fairly straight 
	 * forward to get a stack trace (just grab the PC and SP from 
	 * the pcb_args in the ublock and use the process kernel stack 
	 * page). If it IS, then get a cpu trace -- IF this is a core
	 * dump (you can't get a cpu trace on a live system).
	 */
	cpuid = kl_uint(trace->ktp, "kthread", "k_sonproc", 0);
	if (cpuid != -1) { 

		/* Process is running on a CPU 
		 */
		if (!ACTIVE) {
			if (get_cpu_trace(cpuid, trace)) {
				return(1);
			}
			print_trace(trace, flags, ofp);
		}
		else {
			fprintf(ofp, "Process is currently running on CPU %d.\n\n", cpuid);
		}
		return(0);
	}
	else {
		saddr = kthread_stack(trace, &stacksize);
		pc = ((kaddr_t*)trace->ktp)[10];
		sp = ((kaddr_t*)trace->ktp)[8];
		if (PTRSZ32) {
			pc &= 0xffffffff;
			sp &= 0xffffffff;
		}
	}

	/* Just in case, make sure the PC is valid code and that SP is from
	 * saddr.
	 */
	if (!IS_TEXT(pc) || (pc & 3)) {
		if (DEBUG(KLDC_TRACE, 1)) {
			fprintf(KL_ERRORFP, "PC is invalid (pc=0x%llx\n", pc);
		}
		KL_SET_ERROR_NVAL(KLE_BAD_PC, pc, 2);
		return(1);
	}
	if (((sp < saddr) || (sp >= (saddr + stacksize)))) {
		if (DEBUG(KLDC_TRACE, 1)) {
			fprintf(KL_ERRORFP, "SP is invalid (sp=0x%llx)\n", sp);
		}
		KL_SET_ERROR_NVAL(KLE_BAD_SP, sp, 2);
		return(1);
	}

	/* Go get a stack trace...even one that ends with an error.
	 */
	trace->nframes = find_trace(pc, sp, (kaddr_t)0, 
				(kaddr_t)0, trace, (flags | K_ALL));

	if (trace->nframes == 0) {
		if (DEBUG(KLDC_TRACE, 1)) {
			fprintf(KL_ERRORFP, "proc_trace: Not a valid trace! "
					"(pc=0x%llx, sp=0x%llx, saddr=0x%llx\n", 
							pc, sp, saddr);
		}
		if (!KL_ERROR) {
			KL_SET_ERROR(KLE_NO_TRACE);
		}
		return(1);
	}
	print_trace(trace, flags, ofp);
	return(0);
}

/*
 * kthread_trace()
 */
int
kthread_trace(trace_t *trace, int flags, FILE *ofp)
{
	int i, stacksize;
	pid_t p_pid;
	k_uint_t k_id;
	cpuid_t cpuid;
	char p_stat;
	k_ptr_t kt_name;
	kaddr_t p, pc, sp, saddr, upage, curkthread, eframe;

	kl_reset_error();

	if (DEBUG(KLDC_FUNCTRACE, 3)) {
		fprintf(KL_ERRORFP, "kthread_trace: trace=0x%x, flags=%d\n", 
			trace, flags);
	}

	/* Determine what type of kthread this is and print the appropriate
	 * trace header.
	 */
	if (IS_UTHREAD(trace->ktp)) {
		p_pid = kl_uthread_to_pid(trace->ktp); 
		fprintf (ofp, "STACK TRACE FOR UTHREAD 0x%llx ", trace->kthread);
		fprintf (ofp, "(%s, PID=%d):\n\n", 
			kl_kthread_name(trace->ktp), p_pid);
	}
	else if (IS_STHREAD(trace->ktp)) {
			fprintf (ofp, "STACK TRACE FOR STHREAD 0x%llx ", trace->kthread);
			fprintf (ofp, "(%s):\n\n", kl_kthread_name(trace->ktp));
	}
	else if (IS_XTHREAD(trace->ktp)) {
			fprintf (ofp, "STACK TRACE FOR XTHREAD 0x%llx ", trace->kthread);
			fprintf (ofp, "(%s):\n\n", kl_kthread_name(trace->ktp));
	}

	/* Determine if the kthread is currently running on a CPU (by 
	 * checking k_sonproc). If it isn't, then it's fairly straight 
	 * forward to get a stack trace (just check for an eframe pointer
	 * in the kthread struct or grab the PC and SP from the k_regs[]).
	 * If it the kthread was running on a CPU, then get a cpu trace. 
	 * Note that it isn't possible to get a cpu trace on a live system.
	 */
	cpuid = kl_uint(trace->ktp, "kthread", "k_sonproc", 0);
	if (cpuid != -1) { 

		/* kthread is running on a CPU. If it's not running, then 
		 * issue an error message to that effect and return.
		 */
		if (trace->kthread == kl_get_curkthread(cpuid)) {
			if (!ACTIVE) {
				get_cpu_trace(cpuid, trace);

				/* If there was an error, go ahead and print out the part
				 * of the trace we were able to get (if any).
				 */
				if (KL_ERROR && !trace->nframes) {
					return(1);
				}
				print_trace(trace, flags, ofp);
			}
			else {
				fprintf(ofp, "Kthread is currently running on "
					"CPU %d.\n\n", cpuid);
			}
			return(0);
		}
	}
	
	saddr = kthread_stack(trace, &stacksize);

	/* We have to check and see if there is an eframe pointer in
	 * the kthread struct. If there is, then we can use eframe_trace()
	 * to generate the stack trace). 
	 */
	if (eframe = kl_kaddr(trace->ktp, "kthread", "k_eframe")) {
		trace->flags |= TF_SUPPRESS_HEADER;
		return(eframe_trace(eframe, trace, flags, ofp));
	}

	if (IS_XTHREAD(trace->ktp)) {
		/* Determine if the xthread is using its stack. If it's not
		 * then return with an error.
		 */
		if (KL_UINT(trace->ktp, "kthread", "k_flags") & 0x400) {
			KL_SET_ERROR(KLE_NO_TRACE);
			return(1);
		}
	}
	pc = ((kaddr_t*)trace->ktp)[10];
	sp = ((kaddr_t*)trace->ktp)[8];
	if (PTRSZ32) {
		pc &= 0xffffffff;
		sp &= 0xffffffff;
	}

	/* Just in case, make sure the PC is valid code and that SP is from
	 * saddr.
	 */
	if (!IS_TEXT(pc) || (pc & 3)) {
		if (DEBUG(KLDC_TRACE, 1)) {
			fprintf(KL_ERRORFP, "PC is invalid (pc=0x%llx\n", pc);
		}
		KL_SET_ERROR_NVAL(KLE_BAD_PC, pc, 2);
		return(1);
	}
	if (((sp < saddr) || (sp >= (saddr + stacksize)))) {
		if (DEBUG(KLDC_TRACE, 1)) {
			fprintf(KL_ERRORFP, "SP is invalid (sp=0x%llx)\n", sp);
		}
		KL_SET_ERROR_NVAL(KLE_BAD_SP, sp, 2);
		return(1);
	}

	/* Go get a stack trace...even one that ends with an error.
	 */
	trace->nframes = find_trace(pc, sp, (kaddr_t)0, 
				(kaddr_t)0, trace, (flags | K_ALL));

	if (trace->nframes == 0) {
		if (DEBUG(KLDC_TRACE, 1)) {
			fprintf(KL_ERRORFP, "kthread_trace: Not a valid trace! "
					"(pc=0x%llx, sp=0x%llx, saddr=0x%llx\n", 
							pc, sp, saddr);
		}
		if (!KL_ERROR) {
			KL_SET_ERROR(KLE_NO_TRACE);
		}
		return(1);
	}
	print_trace(trace, flags, ofp);
	return(0);
}

/*
 * trace_banner()
 */
void
trace_banner(FILE *ofp)
{
	fprintf(ofp, "========================================"
				 "=======================================\n");
}

/* 
 * print_trace()
 */
int
print_trace(trace_t *trace, int flags, FILE *ofp)
{
	int i, j;
#ifdef INCLUDE_REGINFO
	int args;
#endif
	sframe_t *sf;

	/* If there is no trace, make sure error is set to something and 
	 * return (the print_trace_error() routine will print out an error 
	 * message).
	 */
	if (!(sf = trace->frame)) {
		if (!KL_ERROR) {
			KL_SET_ERROR(KLE_NO_TRACE);
		}
		return(1);
	}

#ifdef INCLUDE_REGINFO
	if (trace->flags & TF_GET_REGVALS) {
		get_reg_values(trace);
	}
#endif

	for (i = 0; i < trace->nframes; i++, sf = sf->next) {

		/* If the sframe_rec contains frame information, print out the 
		 * stack frame -- EVEN when there has been an error.
		 */
		if (sf->funcname) {

			fprintf(ofp, "%2d %s[", sf->level, sf->funcname);
			if (sf->srcfile) {
				fprintf(ofp, "%s: %d, 0x%llx]\n", 
					sf->srcfile, sf->line_no, sf->pc);
			}
			else { 
				fprintf(ofp, "???: %d, 0x%llx]\n", sf->line_no, sf->pc);
			}
			
			/* Check to see if this stack frame is really in a stack from
			 * another kthread (e.g., trace->kthread2). If it is, we need 
			 * to put a message in the middle of the backtrace to indicate 
			 * this.
			 */
			if (sf->flag & TF_KTHREAD2_VALID) {
				int p_pid;

				if (IS_UTHREAD(trace->ktp2)) {
					p_pid = kl_uthread_to_pid(trace->ktp2);
					fprintf (ofp, "\nSTACK TRACE CONTINUES ON UTHREAD "
						"0x%llx ", trace->kthread2);
					fprintf (ofp, "(%s, PID=%d):\n\n",
						kl_kthread_name(trace->ktp2), p_pid);
				}
				else if (IS_STHREAD(trace->ktp2)) {
						fprintf (ofp, "\nSTACK TRACE CONTINUES ON STHREAD "
							"0x%llx ", trace->kthread2);
						fprintf (ofp, "(%s):\n\n", 
							kl_kthread_name(trace->ktp2));
				}
				else if (IS_XTHREAD(trace->ktp2)) {
						fprintf (ofp, "\nSTACK TRACE CONTINUES ON XTHREAD "
							"0x%llx ", trace->kthread2);
						fprintf (ofp, "(%s):\n\n", 
							kl_kthread_name(trace->ktp2));
				}
			}
#ifdef INCLUDE_REGINFO
			args = 0;
			for (j = 4; j < 12; j++) {
				if (sf->regs[j].state == REGVAL_VALID) {
					if (args == 0) {
						fprintf(ofp, "    (0x%llx", sf->regs[j].value);
					}
					else {
						fprintf(ofp, ",0x%llx", sf->regs[j].value);
					}
					args++;
				}
				else if (sf->regs[j].state == REGVAL_UNKNOWN) {
					continue;
				}
				else {
					break;
				}
			}
			if (args) {
				fprintf(ofp, ")\n");
			}
#endif

			if (sf->eframe) {
				print_eframe_s((kaddr_t)NULL, sf->eframe, flags, ofp);

				/* If the eframe points to the ublock, just print the
				 * eframe and return.
				 */
				if (sf->ep == trace->u_eframe) {
					return(0);
				}
			}

#ifdef INCLUDE_REGINFO
			if (trace->flags & TF_GET_REGVALS) {
				print_reg_values(sf, ofp);
			}
#endif /* INCLUDE_REGINFO */

			if (flags & K_FULL) {
				fprintf(ofp, "\n   RA=0x%llx, SP=0x%llx, FRAME SIZE=%d\n",
						sf->ra, sf->sp, sf->frame_size);

				fprintf(ofp, "\n");

				if (sf->frame_size) {
					dump_stack_frame(trace, sf, ofp);
				}
			}
		}

		/* If no error occurred, just return. Otherwise, print an error
		 * message.
		 */
		if (sf->error == KLE_BAD_PC) {
			fprintf(ofp, "Trace stops because bad PC 0x%llx found\n", sf->pc);
			return(1);
		}
		else if (sf->error == KLE_BAD_SP) {
			fprintf(ofp, "Trace stops because bad SP 0x%llx found\n", sf->sp);
			return(1);
		}
		else if (sf->error == KLE_BAD_EP) {
			fprintf(ofp, "Trace stops because eframe could not be found\n");
			return(1);
		}
		else if (sf->error == KLE_BAD_PROC) {
			fprintf(ofp, "Trace stops because proc was invalid or not "
				"specified\n");
			return(1);
		}
		else if (sf->error == KLE_UNKNOWN_ERROR) {
			fprintf(ofp, "Trace stops because of an unknown error\n");
			return(1);
		}
	}
	return(0);
}

/* 
 * dump_stack_frame()
 */
void
dump_stack_frame(trace_t *trace, sframe_t *curframe, FILE *ofp)
{
	int i, first_time = 1;
	kaddr_t sp; 
	uint *asp;

	sp = curframe->sp;
	asp = curframe->asp;

	for (i = 0; i < curframe->frame_size / K_NBPW; i++) {
		if (!(i % ((K_NBPW == 8) ? 2 : 4))) {
			if (first_time) {
				first_time = 0;
				if (PTRSZ64) {
					fprintf(ofp, "   %llx: %016llx  ", 
						sp, kl_kaddr_val(asp));
					asp += 2;
				}
				else {
					fprintf(ofp, "   %llx: %08x  ", sp, *asp++);
				}
			}
			else {
				fprintf(ofp, "\n   %llx: ", sp);
				if (PTRSZ64) {
					fprintf(ofp, "%016llx  ", kl_kaddr_val(asp));
					asp += 2;
				}
				else {
					fprintf(ofp, "%08x  ", *asp++);
				}
			}
			sp += 16;
		}
		else  {
			if (PTRSZ64) {
				fprintf(ofp, "%016llx  ", kl_kaddr_val(asp));
				asp += 2;
			}
			else {
				fprintf(ofp, "%08x  ", *asp++);
			}
		}
	}
	if (curframe->frame_size) {
		fprintf(ofp, "\n\n");
	}
}

/*
 * print_trace_error()
 */
void
print_trace_error(trace_t *trace, FILE *ofp)
{
	if (DEBUG(KLDC_GLOBAL, 1)) {
		fprintf(ofp, "Could not find trace...\n");
	}
	kl_print_error();
}

/*
 * print_cpu_status()
 * 
 *   Print out information about a specific CPU. This routine is
 *   called from the report module. It is in the trace module because
 *   it is so closely tied to the trace data structures and function
 *   calls.
 */
int
print_cpu_status(int cpuid, FILE *ofp)
{
	int i;
	int stkidx = -1, stkflg = -1, ktp_alloced = 0;
	k_ptr_t up, pdap, ktp;
	kstruct_t *ksp;
	trace_t *trace;
	sframe_t *frame;

	pdap = kl_alloc_block(PDA_S_SIZE, K_TEMP);

	kl_get_pda_s((kaddr_t)cpuid, pdap);
	if (KL_ERROR) {
		kl_free_block(pdap);
		return(1);
	}

	trace = alloc_trace_rec();
	ksp = (kstruct_t*)kl_alloc_block(sizeof(kstruct_t), K_TEMP);
	ksp->addr = kl_pda_s_addr(cpuid);
	ksp->ptr = pdap;
	setup_trace_rec((kaddr_t)0, ksp, CPU_FLAG, trace);
	kl_free_block((k_ptr_t)ksp);
	if (KL_ERROR) {
		fprintf(ofp, "  CPU %d ERROR: ", cpuid);
		print_trace_error(trace, ofp);
		free_trace_rec(trace);
		return(1);
	}

	/* Get a stack trace for cpuid
	 */
	if (!get_cpu_trace(cpuid, trace) && !KL_ERROR) {

		/* Point at the first (the most recent) stack level
		 */
		if (frame = trace->frame) {

			if (strcmp(frame->funcname, "panicspin") == 0) {

				/* This CPU should be on the interrupt stack -- spinning 
				 * waiting for the dump to complete. It most likely was not
				 * servicing an interrupt when the panic occured. We need 
				 * to backtrace to the VEC_intr() to see which stack was
				 * active when the interrupt occurred.
				 */

				for (;;) {

					frame = frame->next;
					if (strcmp(frame->funcname, "VEC_int") == 0) {
						if (frame->next == trace->frame) {

							/* The VEC_int frame is the last frame in the
							 * trace. If there is a kthread running on this
							 * CPU, we have to determine what type. If it's
							 * a uthread, it means that the interrupt
							 * occurred while the application was in USER
							 * mode. If it was any other type of thread,
							 * key the stack type to it. If no kthread was
							 * running, it means we interrupted out of an
							 * idle state.
							 */
							if (trace->kthread) {
								if (IS_UTHREAD(trace->ktp)) {
									stkflg = 0;
								}
								else if (IS_XTHREAD(trace->ktp)) {
									stkflg = S_XTHREADSTACK;
								}
								else if (IS_STHREAD(trace->ktp)) {
									stkflg = S_STHREADSTACK;
								}
							}
							else {
								stkflg = S_BOOTSTACK;
							}
						}
						else {
							stkidx = frame->next->ptr;	
						}
						break;
					}
					if (frame->next == trace->frame) {
						break;
					}
				}
			}
			else {

				/* Assume this is the panicing CPU or it's an NMI dump. In 
				 * either case, the first (most recent) frame should be in 
				 * the stack that was active when the dump occurred.
				 */
				int s;
				kaddr_t pda;

				pdap = kl_alloc_block(PDA_S_SIZE, K_TEMP);
				kl_get_pda_s((kaddr_t)cpuid, pdap);

				stkidx = frame->ptr;
				s = kl_uint(pdap, "pda_s", "p_kstackflag", 0);
				if (trace->stack[stkidx].type != s) {
					stkflg = s;
					stkidx = -1;
				}
				kl_free_block(pdap);
			}
		}

		if (stkidx != -1) {
			stkflg = trace->stack[stkidx].type;
		}
	}

	if (KL_ERROR || ((stkidx == -1) && (stkflg == -1))) {

		/* For some reason (an error) we were not able to get a valid
		 * stack trace. So, we'll just have to make a best guess based
		 * on the stack flag in the CPU's pda_s struct.
		 */

		kaddr_t pda, curkthread;

		pdap = kl_alloc_block(PDA_S_SIZE, K_TEMP);
		kl_get_pda_s((kaddr_t)cpuid, pdap);

		stkflg = kl_uint(pdap, "pda_s", "p_kstackflag", 0);
		if (curkthread = kl_kaddr(pdap, "pda_s", "p_curkthread")) {
			if (ktp = kl_get_kthread(curkthread, 0)) {
				ktp_alloced++;
			}
		}
	}
	else {
		ktp = trace->ktp;
	}

	if (stkflg == S_KERNELSTACK) {
		/* HACK! With certain NMI dumps, it's possible for
		 * the kthread running on a cpu to be bad (all NULLs
		 * or not even in the dump). We can't use the below
		 * macros or it will cause a SEGV. Just print an error
		 * message instead.
		 */
		if (!ktp) {
			fprintf(KL_ERRORFP, "  CPU %d ERROR: kthread (0x%llx) on CPU %d "
				"is bad!\n", 
				cpuid, 
				kl_kaddr(pdap, "pda_s", "p_curkthread"), cpuid);
			if (trace) {
				free_trace_rec(trace);
			}
			return(0);
		}
		if (IS_XTHREAD(ktp)) {
			stkflg = S_XTHREADSTACK;
		}
		else if (IS_STHREAD(ktp)) {
			stkflg = S_STHREADSTACK;
		}
	}

	fprintf(ofp, "  ");
	switch (stkflg) {

		case S_USERSTACK:
			fprintf(ofp,
				"CPU %d was in user mode running the command "
				"'%s'\n", cpuid, kl_kthread_name(ktp));
			break;

		case S_KERNELSTACK:
			fprintf(ofp,
				"CPU %d was in kernel mode running the command "
				"'%s'\n", cpuid, kl_kthread_name(ktp));
			break;

		case S_INTSTACK:
			fprintf(ofp, "CPU %d was servicing an interrupt\n", cpuid);
			break;

		case S_BOOTSTACK:
			fprintf(ofp, "CPU %d was idle\n", cpuid);
			break;

		case S_STHREADSTACK: {

			k_ptr_t kt_name;

			fprintf(ofp, "CPU %d was in kernel mode running an "
				"sthread ", cpuid);
			if (kt_name = kl_kthread_name(ktp)) {
				fprintf(ofp, "named '%s'\n", kt_name);
			}
			else {
				fprintf(ofp, "\n");
			}
			break;
		}

		case S_XTHREADSTACK: {

			k_ptr_t kt_name;

			fprintf(ofp, "CPU %d was in kernel mode running an "
				"xthread ", cpuid);
			if (kt_name = kl_kthread_name(ktp)) {
				fprintf(ofp, "named '%s'\n", kt_name);
			}
			else {
				fprintf(ofp, "\n");
			}
			break;
		}

		default :
			fprintf(ofp, "CPU %d -- invalid stack!\n", cpuid);
	}

	/* Free buffers...
	 */
	if (ktp_alloced) {
		kl_free_block(ktp);
	}
	if (trace) {
		free_trace_rec(trace);
	}
	return(0);
}
