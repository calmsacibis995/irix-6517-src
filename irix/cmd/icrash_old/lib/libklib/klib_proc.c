#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libklib/RCS/klib_proc.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"
#define _KERNEL  1
#include <sys/types.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <stdio.h>
#include <errno.h>
#include "klib.h"
#include "klib_extern.h"

/*
 * kl_proc_to_kthread()
 *
 * Returns the first uthread associated with a given proc struct. It
 * also loads the number of threads associated with that proc into
 * the kthread_cnt parameter.
 */
kaddr_t
kl_proc_to_kthread(klib_t *klp, k_ptr_t p, int *kthread_cnt)
{
	kaddr_t kthread;

	kl_reset_error();

	*kthread_cnt = 0;

	/* Just in case
	 */
	if (!p) {
		KL_SET_ERROR_CVAL(KLE_BAD_KTHREAD, p);
		return((kaddr_t)NULL);
	}

	*kthread_cnt = kl_uint(klp, p, "proc_proxy_s", "prxy_nthreads", 0);
	if (*kthread_cnt) {
		kthread = kl_kaddr(klp, p, "proc_proxy_s", "prxy_threads");
		return(kthread);
	}
	else {
		return((kaddr_t)NULL);
	}
}

/*
 * kl_uthread_to_pid()
 */
int
kl_uthread_to_pid(klib_t *klp, k_ptr_t u)
{
	int pid = -1;
	kaddr_t proc;
	k_ptr_t p;

	proc = kl_kaddr(klp, u, "uthread_s", "ut_proc");
	if (p = kl_get_proc(klp, proc, 2, 0)) {
		pid = KL_INT(klp, p, "proc", "p_pid");
		klib_free_block(klp, p);
	}
	return(pid);
}

/*
 * kl_pid_to_proc()
 */
kaddr_t
kl_pid_to_proc(klib_t *klp, int pid) 
{
	kaddr_t pid_slot, pid_entry, vproc, proc, bhv;
	k_ptr_t psp, pep, vprocp;
	
	if (DEBUG(KLDC_PROC, 2)) {
		fprintf(KL_ERRORFP, "kl_pid_to_proc: klp=0x%x, pid=0x%d\n", klp, pid);
	}

	kl_reset_error();

	psp = klib_alloc_block(klp, PID_SLOT_SIZE(klp), K_TEMP);
	pep = klib_alloc_block(klp, PID_ENTRY_SIZE(klp), K_TEMP);
	pid_slot = K_PID_SLOT(klp, pid);

	while (pid_slot) {
		
		kl_get_struct(klp, pid_slot, PID_SLOT_SIZE(klp), psp, "pid_slot");
		if (KL_ERROR) {
			KL_SET_ERROR_NVAL(KLE_BAD_PID_SLOT, pid, 0);	
			klib_free_block(klp, psp);
			klib_free_block(klp, pep);
			return((kaddr_t)NULL);
		}

		/* Get the pointer to the pid_entry struct
		 */
		if (!(pid_entry = kl_kaddr(klp, psp, "pid_slot", "ps_chain"))) {
			KL_SET_ERROR_NVAL(KLE_BAD_PID_ENTRY, pid, 0);   
			klib_free_block(klp, psp);
			klib_free_block(klp, pep);
			return((kaddr_t)NULL);
		}

		/* Now get the pid_entry struct
		 */
		kl_get_struct(klp, pid_entry, PID_ENTRY_SIZE(klp), pep, "pid_entry");
		if (KL_ERROR) {
			KL_SET_ERROR_NVAL(KLE_BAD_PID_ENTRY, pid, 0);   
			klib_free_block(klp, psp);
			klib_free_block(klp, pep);
			return((kaddr_t)NULL);
		}

		/* If the current pid_slot is for the pid we are searching for, 
		 * check to see if the pid is currently in use. If it is then 
		 * determine the address of the proc struct and return it. 
		 * Otherwise, walk to the next pid_slot struct on the chain.
		 */
		if (pid == KL_INT(klp, psp, "pid_slot", "ps_pid")) {
			vproc = kl_kaddr(klp, pep, "pid_entry", "pe_vproc");
			vprocp = klib_alloc_block(klp, VPROC_SIZE(klp), K_TEMP);
			kl_get_struct(klp, vproc, VPROC_SIZE(klp), vprocp, "vproc");
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_VPROC;
				klib_free_block(klp, psp);
				klib_free_block(klp, pep);
				klib_free_block(klp, vprocp);
				return((kaddr_t)NULL);
			}
			bhv = kl_kaddr(klp, vprocp, "vproc", "vp_bhvh");
			proc = kl_bhv_pdata(klp, bhv);
			klib_free_block(klp, psp);
			klib_free_block(klp, pep);
			klib_free_block(klp, vprocp);
			return(proc);
		}
		else {
			pid_slot = kl_kaddr(klp, pep, "pid_entry", "pe_next");
		}
	}
	klib_free_block(klp, psp);
	klib_free_block(klp, pep);
	return((kaddr_t)NULL);
}
