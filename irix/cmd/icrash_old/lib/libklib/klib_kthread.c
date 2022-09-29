#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libklib/RCS/klib_kthread.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"
#define _KERNEL  1
#include <sys/types.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <stdio.h>
#include <errno.h>
#include "klib.h"
#include "klib_extern.h"

/* 
 * kl_set_defkthread()
 */
int
kl_set_defkthread(klib_t *klp, kaddr_t kthread) 
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
		ktp = kl_get_kthread(klp, kthread, 0);
		if (KL_ERROR) {
			return(1);
		}
	}
	K_DEFKTHREAD(klp) = kthread;
	if (ktp) {
		klib_free_block(klp, ktp);		
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
kl_get_curkthread(klib_t *klp, int cpuid)
{
	kaddr_t curkthread;
	k_ptr_t pdap, ktp;

	if (DEBUG(KLDC_FUNCTRACE, 3)) {
		fprintf(KL_ERRORFP, "kl_get_curkthread: cpuid=%d\n", cpuid);
	}

	kl_reset_error();

	if (cpuid < 0 || cpuid >= K_MAXCPUS(klp)) {
		KL_SET_ERROR_NVAL(KLE_BAD_CPUID, cpuid, 0);
		return ((kaddr_t)NULL);
	}

	/* Get the pda_s pointer for this cpu 
	 */
	pdap = klib_alloc_block(klp, PDA_S_SIZE(klp), K_TEMP);
	kl_get_pda_s(klp, (kaddr_t)cpuid, pdap);
	if (KL_ERROR) {
		klib_free_block(klp, pdap);
		return ((kaddr_t)NULL);
	}

	/* Get the pointer for the currently running kthread (if there is 
	 * one) and stuff it into curkthread. If there is no currently
	 * running kthread, just return NULL (not an ERROR).
	 */
	curkthread = kl_kaddr(klp, pdap, "pda_s", "p_curkthread");
	klib_free_block(klp, pdap);
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
kl_kthread_addr(klib_t *klp, k_ptr_t ktp)
{
	k_ptr_t tp = (k_ptr_t)NULL;
	kaddr_t ka;

	/* Get the next kthread on the linked list and then get the prev
	 * kthread pointer (the pointer we want) from there. This is ugly,
	 * but it's the only way to get the address when all we have is
	 * a pointer to a klib buffer containing a kthread.
	 */
	ka = kl_kaddr(klp, ktp, "kthread", "k_flink");		
	tp = klib_alloc_block(klp, KTHREAD_SIZE(klp), K_TEMP);
	kl_get_struct(klp, ka, KTHREAD_SIZE(klp), tp, "kthread");
	if (KL_ERROR) {
		KL_SET_ERROR(KLE_BAD_KTHREAD);
		klib_free_block(klp, tp);
		return((kaddr_t)NULL);
	}
	ka = kl_kaddr(klp, tp, "kthread", "k_blink");
	klib_free_block(klp, tp);
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
kl_kthread_name(klib_t *klp, k_ptr_t ktp) 
{
	kaddr_t proc, ui;
	k_ptr_t procp, up;
	static char kt_name[256];

	kl_reset_error();

	if (IS_UTHREAD(klp, ktp)) {
		if (proc = kl_kaddr(klp, ktp , "uthread_s", "ut_proc")) {
			ui = proc + kl_member_offset(klp, "proc", "p_ui");
			up = klib_alloc_block(klp, kl_struct_len(klp, "user_info"), K_TEMP);
			kl_get_struct(klp, ui, 
				kl_struct_len(klp, "user_info"), up, "user_info"); 
			strcpy(kt_name, CHAR(klp, up, "user_info", "u_comm"));
			klib_free_block(klp, up);
		}
		else {
			return((k_ptr_t)NULL);
		}
	}
#ifdef ANON_ITHREADS
	else if (IS_ITHREAD(klp, ktp)) {
		strcpy(kt_name, CHAR(klp, ktp, "ithread_s", "it_name"));
	}
#endif
	else if (IS_XTHREAD(klp, ktp)) {
		strcpy(kt_name, CHAR(klp, ktp, "xthread_s", "xt_name"));
	}
	else if (IS_STHREAD(klp, ktp)) {
		strcpy(kt_name, CHAR(klp, ktp, "sthread_s", "st_name"));
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
kl_kthread_type(klib_t *klp, k_ptr_t ktp) 
{
	int type;

	type = KL_UINT(klp, ktp, "kthread", "k_type");
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
kl_kthread_stack(klib_t *klp, kaddr_t kthread, int *size) 
{
	kaddr_t saddr;
	k_ptr_t ktp;

	kl_reset_error();

	if (*size) {
		*size = 0;
	}

	ktp = kl_get_kthread(klp, kthread, 0);
	if (KL_ERROR) {
		return((kaddr_t)NULL);
	}

    saddr = kl_kaddr(klp, ktp, "kthread", "k_stack");
	if (KL_ERROR) {
		klib_free_block(klp, ktp);
		return((kaddr_t)NULL);
	}

    /* If stack extensions are enabled (usually only with 32-bit
     * kernels) we have to check and see if an extension page is
     * mapped in for the current process. If a page is mapped in,
     * then use it, otherwise use the regular stack page.
     */
    if (K_EXTUSIZE(klp)) {
        if (saddr == K_KEXTSTACK(klp)) {
            if (!IS_UTHREAD(klp, ktp)) {
                if (IS_XTHREAD(klp, ktp)) {
                    KL_SET_ERROR(KLE_IS_XTHREAD);
                }
#ifdef ANON_ITHREADS
                else if (IS_ITHREAD(klp, ktp)) {
                    KL_SET_ERROR(KLE_IS_ITHREAD);
                }
#endif
                else if (IS_STHREAD(klp, ktp)) {
                    KL_SET_ERROR(KLE_IS_STHREAD);
                }
				klib_free_block(klp, ktp);
                return(0);
            }
            kl_virtop(klp, saddr, ktp);
            if (KL_ERROR) {

                saddr = K_KERNELSTACK(klp);
                if (size) {
                    *size = K_PAGESZ(klp);
                }
            }
            else if (size) {
                *size = (2 * K_PAGESZ(klp));
            }
			klib_free_block(klp, ktp);
            return(saddr);
        }
    }

    /* We either have the kernelstack (on a system that does not
     * support extension stack pages) -- or an sthread or xthread 
	 * stack. Just get the size from the kthread struct.
     */
    if (size) {
        *size = KL_UINT(klp, ktp, "kthread", "k_stacksize");
    }
	klib_free_block(klp, ktp);
	return(saddr);
}
