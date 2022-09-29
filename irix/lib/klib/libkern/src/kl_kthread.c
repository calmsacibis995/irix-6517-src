#ident "$Header: "

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>

/* 
 * kl_set_defkthread()
 */
int
kl_set_defkthread(kaddr_t kthread) 
{
	k_ptr_t ktp = 0;

	if (DEBUG(KLDC_FUNCTRACE, 4)) {
		fprintf(KL_ERRORFP, "kl_set_defkthread: kthread=0x%llx\n", kthread);
	}

	kl_reset_error();

	/* Get the kthread pointed to by kthread. If there is an error, 
	 * don't change the current defkthread setting. Just return. 
	 * Note that if kthread is NULL, it's not an error (it's OK to
	 * not have a defkthread).
	 */
	if (kthread) {
		ktp = kl_get_kthread(kthread, 0);
		if (KL_ERROR) {
			return(1);
		}
	}
	K_DEFKTHREAD = kthread;
	if (ktp) {
		kl_free_block(ktp);		
	}
	return(0);
}

/*
 * kl_get_curkthread() -- Gets kthread currently running on a cpu
 * 
 *   Upon success, returns the kernel address of the kthread running on
 *   cpuid. Zero will be returned in the event that no kthread was
 *   running on a cpu or there was an error (in which case klib_error
 *   will also be set).
 */
kaddr_t
kl_get_curkthread(int cpuid)
{
	kaddr_t curkthread;
	k_ptr_t pdap, ktp;

	if (DEBUG(KLDC_FUNCTRACE, 3)) {
		fprintf(KL_ERRORFP, "kl_get_curkthread: cpuid=%d\n", cpuid);
	}

	kl_reset_error();

	if (cpuid < 0 || cpuid >= K_MAXCPUS) {
		KL_SET_ERROR_NVAL(KLE_BAD_CPUID, cpuid, 0);
		return ((kaddr_t)NULL);
	}

	/* Get the pda_s pointer for this cpu 
	 */
	pdap = kl_alloc_block(PDA_S_SIZE, K_TEMP);
	kl_get_pda_s((kaddr_t)cpuid, pdap);
	if (KL_ERROR) {
		kl_free_block(pdap);
		return ((kaddr_t)NULL);
	}

	/* Get the pointer for the currently running kthread (if there is 
	 * one) and stuff it into curkthread. If there is no currently
	 * running kthread, just return NULL (not an ERROR).
	 */
	curkthread = kl_kaddr(pdap, "pda_s", "p_curkthread");
	kl_free_block(pdap);
	return (curkthread);
}

/*
 * kl_kthread_addr() 
 * 
 *  Returns the kernel address for a kthread struct. This routine
 *  depends upon the fact that the kthread is located at the start of
 *  the uthread_s, sthread_s, or xthread_s struct it points to. So, 
 *  we can use this pointer as the pointer to the kthread itself.
 *
 *  NOTE: In order for the above to work, we MUST have a buffer that
 *  contains not only the kthread, but the underlying struct (e.g., 
 *  uthread_s). Otherwise, we will exceed the buffer (and SEGV) when 
 *  we try to reference the underlying struct.
 */
kaddr_t 
kl_kthread_addr(k_ptr_t ktp)
{
	k_ptr_t tp = (k_ptr_t)NULL;
	kaddr_t ka;

	/* Get the next kthread on the linked list and then get the prev
	 * kthread pointer (the pointer we want) from there. This is ugly,
	 * but it's the only way to get the address when all we have is
	 * a pointer to a klib buffer containing a kthread.
	 */
	ka = kl_kaddr(ktp, "kthread", "k_flink");		
	tp = kl_alloc_block(KTHREAD_SIZE, K_TEMP);
	kl_get_struct(ka, KTHREAD_SIZE, tp, "kthread");
	if (KL_ERROR) {
		kl_free_block(tp);
		KL_SET_ERROR(KLE_BAD_KTHREAD);
		return((kaddr_t)NULL);
	}
	ka = kl_kaddr(tp, "kthread", "k_blink");
	kl_free_block(tp);
	return(ka);
}

/*
 * kl_kthread_name() -- Get the name of a kthread
 * 
 *   Returns a pointer to the name of a kthread (KT_UPROC or KT_STHREAD). 
 *   It shouldn't be necessary to allocate a memory block for the name 
 *   since the kthread pointer actually points to a memory block that 
 *   contains the entire struct for the type of kthread this is 
 *   (e.g., uthread_s). This struct should contain the name. If the 
 *   struct is bad, or it is really a dummy head to a linked list, then 
 *   just return a NULL pointer.
 */
k_ptr_t
kl_kthread_name(k_ptr_t ktp) 
{
	kaddr_t proc, ui;
	k_ptr_t procp, up;
	static char kt_name[256];

	kl_reset_error();

	if (IS_UTHREAD(ktp)) {
		if (proc = kl_kaddr(ktp , "uthread_s", "ut_proc")) {
			ui = proc + kl_member_offset("proc", "p_ui");
			up = kl_alloc_block(kl_struct_len("user_info"), K_TEMP);
			kl_get_struct(ui, kl_struct_len("user_info"), up, "user_info"); 
			strcpy(kt_name, CHAR(up, "user_info", "u_comm"));
			kl_free_block(up);
		}
		else {
			return((k_ptr_t)NULL);
		}
	}
	else if (IS_XTHREAD(ktp)) {
		strcpy(kt_name, CHAR(ktp, "xthread_s", "xt_name"));
	}
	else if (IS_STHREAD(ktp)) {
		strcpy(kt_name, CHAR(ktp, "sthread_s", "st_name"));
	}
	else {
		return((k_ptr_t)NULL);
	}
	return(kt_name);
}

/*
 * kl_kthread_type() -- return the kthread type
 *
 * This routine currently does no validy testing. It assumes that 
 * the kthread is valid. It assumes that the validity was checked
 * before calling this routine.
 */
int
kl_kthread_type(k_ptr_t ktp) 
{
	int type;

	type = KL_UINT(ktp, "kthread", "k_type");
	return(type);
}

/*
 * kl_kthread_stack()
 * 
 *   A wrapper for kthread_stack() that allows routines outside the
 *   trace module to get kthread stack addresses (they don't need to
 *   know about trace structs).
 */
kaddr_t
kl_kthread_stack(kaddr_t kthread, int *size) 
{
	kaddr_t saddr;
	k_ptr_t ktp;

	kl_reset_error();

	if (*size) {
		*size = 0;
	}

	ktp = kl_get_kthread(kthread, 0);
	if (KL_ERROR) {
		return((kaddr_t)NULL);
	}

	saddr = kl_kaddr(ktp, "kthread", "k_stack");
	if (KL_ERROR) {
		kl_free_block(ktp);
		return((kaddr_t)NULL);
	}

	/* If stack extensions are enabled (usually only with 32-bit
	 * kernels) we have to check and see if an extension page is
	 * mapped in for the current process. If a page is mapped in,
	 * then use it, otherwise use the regular stack page.
	 */
	if (K_EXTUSIZE) {
		if (saddr == K_KEXTSTACK) {
			if (!IS_UTHREAD(ktp)) {
				if (IS_XTHREAD(ktp)) {
					kl_free_block(ktp);
					KL_SET_ERROR(KLE_IS_XTHREAD);
				}
				else if (IS_STHREAD(ktp)) {
					kl_free_block(ktp);
					KL_SET_ERROR(KLE_IS_STHREAD);
				}
				return(0);
			}
			kl_virtop(saddr, ktp);
			if (KL_ERROR) {

				saddr = K_KERNELSTACK;
				if (size) {
					*size = K_PAGESZ;
				}
			}
			else if (size) {
				*size = (2 * K_PAGESZ);
			}
			kl_free_block(ktp);
			return(saddr);
		}
	}

	/* We either have the kernelstack (on a system that does not
	 * support extension stack pages) -- or an sthread or xthread 
	 * stack. Just get the size from the kthread struct.
	 */
	if (size) {
		*size = KL_UINT(ktp, "kthread", "k_stacksize");
	}
	kl_free_block(ktp);
	return(saddr);
}

/*
 * kl_get_kthread() -- Get a kthread struct
 * 
 *   This routine takes a kernel pointer to a kthread struct and returns 
 *   a pointer to a buffer containing the structure the kthread struct
 *   is part of. For example, if the kthread is part of a uthread_s struct,
 *   then the entire uthread_s struct is read into a buffer and a pointer 
 *   to that buffer is returned. If the kthread pointer is not valid or 
 *   some error occurred, a NULL pointer value will be returned and error 
 *   will be set to an appropriate value.
 */
k_ptr_t
kl_get_kthread(kaddr_t kthread, int flags)
{
	k_ptr_t ktp, typ;

	if (DEBUG(KLDC_FUNCTRACE, 2)) {
		fprintf(KL_ERRORFP, "get_kthread: kthread=0x%llx, flags=0x%x",
			kthread, flags);
	}

	/* Just in case... 
	 */
	if (!kthread) {
		KL_SET_ERROR_NVAL(KLE_BAD_KTHREAD, 0, 2);
		return ((k_ptr_t)NULL);
	}

	/* Allocate a buffer to hold a kthread struct. 
	 */
	ktp = kl_alloc_block(KTHREAD_SIZE, K_TEMP);

	/* Get the kthread structure
	 */
	kl_get_struct(kthread, KTHREAD_SIZE, ktp, "kthread"); 
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_KTHREAD;
		kl_free_block(ktp);
		return((k_ptr_t*)NULL);
	}

	/* Determine what type of kthread this is (KT_UTHREAD, KT_ITHREAD, or
	 * KT_STHREAD) and get a block with the appropriate structure loaded
	 * into it. If an error occurs, typ will be NULL and the global
	 * error value will be set. Just return typ.
	 */
	if (IS_UTHREAD(ktp)) {
		typ = kl_get_uthread_s(kthread, 2, flags);
	}
	else if (IS_XTHREAD(ktp)) {
		typ = kl_get_xthread_s(kthread, 2, flags);
	}
	else if (IS_STHREAD(ktp)) {
		typ = kl_get_sthread_s(kthread, 2, flags);
	}
	else {
		if (flags & K_ALL) {
			return(ktp);
		}
		else {
			kl_free_block(ktp);
			KL_SET_ERROR_NVAL(KLE_BAD_KTHREAD, kthread, 2);
			return ((k_ptr_t)NULL);
		}
	}

	/* Free the block holding the kthread and return the block holding the
	 * structure the kthread struct is part of.
	 */
	kl_free_block(ktp);
	return(typ);
}

/*
 * kl_get_sthread_s() -- Get a sthread_s struct
 * 
 *   This routine returns a pointer to a sthread_s struct for a valid
 *   sthread (unless the K_ALL flag is set). 
 */
k_ptr_t
kl_get_sthread_s(kaddr_t value, int mode, int flags)
{
	kaddr_t sthread;
	k_ptr_t stp;

	if (DEBUG(KLDC_FUNCTRACE, 2)) {
		if (mode != 2) {
			fprintf(KL_ERRORFP, "kl_get_sthread_s: value=%lld, mode=%d, ",
				value, mode);
		} 
		else {
			fprintf(KL_ERRORFP, "kl_get_sthread_s: value=0x%llx, mode=%d, ",
				value, mode);
		}
		fprintf(KL_ERRORFP, "flags=0x%x\n", flags);
	}

	kl_reset_error();

	/* Just in case... 
	 */
	if ((mode == 2) && !value) {
		KL_SET_ERROR_NVAL(KLE_BAD_STHREAD_S, value, mode);
		return ((k_ptr_t)NULL);
	}

	if (mode == 0) {
		KL_SET_ERROR_NVAL(KLE_BAD_STHREAD_S, value, mode);
		return((k_ptr_t)NULL);
	}
	else if (mode == 1) {
#ifdef XXX
		sthread = id_to_sthread((int)value);
#endif
	}
	else {
		sthread = value;
	}

	/* Allocate a block for a sthread_s struct. Make sure and check to
	 * see if a K_PERM block needs to be allocated.
	 */
	if (flags & K_PERM) {
		stp = kl_alloc_block(STHREAD_S_SIZE, K_PERM);
	}
	else {
		stp = kl_alloc_block(STHREAD_S_SIZE, K_TEMP);
	}
	kl_get_struct(sthread, STHREAD_S_SIZE, stp, "sthread_s");
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_STHREAD_S;
		return((k_ptr_t)NULL);
	}

	/* Make an attempt to verify that the sthread is valid. If
	 * it is valid, or if the K_ALL flag is set, return the
	 * pointer to the block containing the sthread_s struct.
	 * Otherwise return an error.
	 */
	if (IS_STHREAD(stp) || (flags & K_ALL)) {
		return (stp);
	}
	else {
		kl_free_block(stp);
		KL_SET_ERROR_NVAL(KLE_BAD_STHREAD_S, sthread, 2);
		return ((k_ptr_t)NULL);
	}
}

/*
 * kl_get_uthread_s() -- Get a uthread_s struct
 * 
 *   This routine returns a pointer to a uthread_s struct for a valid
 *   uthread (unless the K_ALL flag is set). 
 */
k_ptr_t
kl_get_uthread_s(kaddr_t value, int mode, int flags)
{
	kaddr_t uthread;
	k_ptr_t utp;

	if (DEBUG(KLDC_FUNCTRACE, 2)) {
		if (mode != 2) {
			fprintf(KL_ERRORFP, "kl_get_uthread_s: value=%lld, mode=%d, ",
				value, mode);
		} 
		else {
			fprintf(KL_ERRORFP, "kl_get_uthread_s: value=0x%llx, mode=%d, ",
				value, mode);
		}
		fprintf(KL_ERRORFP, "flags=0x%x\n", flags);
	}

	kl_reset_error();

	/* Just in case... 
	 */
	if ((mode == 2) && !value) {
		KL_SET_ERROR_NVAL(KLE_BAD_UTHREAD_S, value, mode);
		return ((k_ptr_t)NULL);
	}

	if (mode == 0) {
		KL_SET_ERROR_NVAL(KLE_BAD_UTHREAD_S, value, mode);
		return((k_ptr_t)NULL);
	}
	else if (mode == 1) {
#ifdef XXX
		uthread = id_to_uthread((int)value);
#endif
	}
	else {
		uthread = value;
	}

	/* Allocate a block for a uthread_s struct. Make sure and check to
	 * see if a K_PERM block needs to be allocated.
	 */
	if (flags & K_PERM) {
		utp = kl_alloc_block(UTHREAD_S_SIZE, K_PERM);
	}
	else {
		utp = kl_alloc_block(UTHREAD_S_SIZE, K_TEMP);
	}
	kl_get_struct(uthread, UTHREAD_S_SIZE, utp, "uthread_s");
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_UTHREAD_S;
		return((k_ptr_t)NULL);
	}

	/* Make an attempt to verify that the uthread is valid. If
	 * it is valid, or if the K_ALL flag is set, return the
	 * pointer to the block containing the uthread_s struct.
	 * Otherwise return an error.
	 */
	if (IS_UTHREAD(utp) || (flags & K_ALL)) {
		return (utp);
	}
	else {
		kl_free_block(utp);
		KL_SET_ERROR_NVAL(KLE_BAD_UTHREAD_S, uthread, 2);
		return ((k_ptr_t)NULL);
	}
}

/*
 * kl_get_xthread_s() -- Get a xthread_s struct
 * 
 *   This routine returns a pointer to a xthread_s struct for a valid
 *   xthread (unless the K_ALL flag is set). 
 */
k_ptr_t
kl_get_xthread_s(kaddr_t value, int mode, int flags)
{
	kaddr_t xthread;
	k_ptr_t xtp;

	if (DEBUG(KLDC_FUNCTRACE, 2)) {
		if (mode != 2) {
			fprintf(KL_ERRORFP, "kl_get_xthread_s: value=%lld, mode=%d, ",
				value, mode);
		} 
		else {
			fprintf(KL_ERRORFP, "kl_get_xthread_s: value=0x%llx, mode=%d, ",
				value, mode);
		}
		fprintf(KL_ERRORFP, "flags=0x%x\n", flags);
	}

	kl_reset_error();

	/* Just in case... 
	 */
	if ((mode == 2) && !value) {
		KL_SET_ERROR_NVAL(KLE_BAD_XTHREAD_S, value, mode);
		return ((k_ptr_t)NULL);
	}

	if (mode == 0) {
		KL_SET_ERROR_NVAL(KLE_BAD_XTHREAD_S, value, mode);
		return((k_ptr_t)NULL);
	}
	else if (mode == 1) {
#ifdef XXX
		xthread = id_to_xthread((int)value);
#endif
	}
	else {
		xthread = value;
	}

	/* Allocate a block for a xthread_s struct. Make sure and check to
	 * see if a K_PERM block needs to be allocated.
	 */
	if (flags & K_PERM) {
		xtp = kl_alloc_block(XTHREAD_S_SIZE, K_PERM);
	}
	else {
		xtp = kl_alloc_block(XTHREAD_S_SIZE, K_TEMP);
	}
	kl_get_struct(xthread, XTHREAD_S_SIZE, xtp, "xthread_s");
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_XTHREAD_S;
		return((k_ptr_t)NULL);
	}

	/* Make an attempt to verify that the xthread is valid. If
	 * it is valid, or if the K_ALL flag is set, return the
	 * pointer to the block containing the xthread_s struct.
	 * Otherwise return an error.
	 */
	if (IS_XTHREAD(xtp) || (flags & K_ALL)) {
		return (xtp);
	}
	else {
		kl_free_block(xtp);
		KL_SET_ERROR_NVAL(KLE_BAD_XTHREAD_S, xthread, 2);
		return ((k_ptr_t)NULL);
	}
}
