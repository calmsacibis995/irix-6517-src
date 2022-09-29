#ident "$Header: "

#define _KERNEL  1
#include <sys/types.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <stdio.h>
#include <errno.h>
#include <klib/klib.h>

/*
 * kl_proc_to_kthread()
 *
 * Returns the first uthread associated with a given proc struct. It
 * also loads the number of threads associated with that proc into
 * the kthread_cnt parameter.
 */
kaddr_t
kl_proc_to_kthread(k_ptr_t p, int *kthread_cnt)
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

	*kthread_cnt = kl_uint(p, "proc_proxy_s", "prxy_nthreads", 0);
	if (*kthread_cnt) {
		kthread = kl_kaddr(p, "proc_proxy_s", "prxy_threads");
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
kl_uthread_to_pid(k_ptr_t u)
{
	int pid = -1;
	kaddr_t proc;
	k_ptr_t p;

	proc = kl_kaddr(u, "uthread_s", "ut_proc");
	if (p = kl_get_proc(proc, 2, 0)) {
		pid = KL_INT(p, "proc", "p_pid");
		kl_free_block(p);
	}
	return(pid);
}

/*
 * kl_pid_to_proc()
 */
kaddr_t
kl_pid_to_proc(int pid) 
{
	kaddr_t pid_slot, pid_entry, vproc, proc, bhv;
	k_ptr_t psp, pep, vprocp;
	
	if (DEBUG(KLDC_PROC, 2)) {
		fprintf(KL_ERRORFP, "kl_pid_to_proc: pid=0x%d\n", pid);
	}

	kl_reset_error();

	psp = kl_alloc_block(PID_SLOT_SIZE, K_TEMP);
	pep = kl_alloc_block(PID_ENTRY_SIZE, K_TEMP);
	pid_slot = K_PID_SLOT(pid);

	while (pid_slot) {
		
		kl_get_struct(pid_slot, PID_SLOT_SIZE, psp, "pid_slot");
		if (KL_ERROR) {
			kl_free_block(psp);
			kl_free_block(pep);
			KL_SET_ERROR_NVAL(KLE_BAD_PID_SLOT, pid, 0);	
			return((kaddr_t)NULL);
		}

		/* Get the pointer to the pid_entry struct
		 */
		if (!(pid_entry = kl_kaddr(psp, "pid_slot", "ps_chain"))) {
			kl_free_block(psp);
			kl_free_block(pep);
			KL_SET_ERROR_NVAL(KLE_BAD_PID_ENTRY, pid, 0);   
			return((kaddr_t)NULL);
		}

		/* Now get the pid_entry struct
		 */
		kl_get_struct(pid_entry, PID_ENTRY_SIZE, pep, "pid_entry");
		if (KL_ERROR) {
			kl_free_block(psp);
			kl_free_block(pep);
			KL_SET_ERROR_NVAL(KLE_BAD_PID_ENTRY, pid, 0);   
			return((kaddr_t)NULL);
		}

		/* If the current pid_slot is for the pid we are searching for, 
		 * check to see if the pid is currently in use. If it is then 
		 * determine the address of the proc struct and return it. 
		 * Otherwise, walk to the next pid_slot struct on the chain.
		 */
		if (pid == KL_INT(psp, "pid_slot", "ps_pid")) {
			vproc = kl_kaddr(pep, "pid_entry", "pe_vproc");
			vprocp = kl_alloc_block(VPROC_SIZE, K_TEMP);
			kl_get_struct(vproc, VPROC_SIZE, vprocp, "vproc");
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_VPROC;
				kl_free_block(psp);
				kl_free_block(pep);
				kl_free_block(vprocp);
				return((kaddr_t)NULL);
			}
			bhv = kl_kaddr(vprocp, "vproc", "vp_bhvh");
			proc = kl_bhv_pdata(bhv);
			kl_free_block(psp);
			kl_free_block(pep);
			kl_free_block(vprocp);
			return(proc);
		}
		else {
			pid_slot = kl_kaddr(pep, "pid_entry", "pe_next");
		}
	}
	kl_free_block(psp);
	kl_free_block(pep);
	return((kaddr_t)NULL);
}

/*
 * kl_get_proc() -- Get a proc struct
 * 
 *   This routine returns a pointer to a proc struct for a valid
 *   process (unless the K_ALL flag is set). If the process is not 
 *   valid or an error occurred, a NULL value is returned (error 
 *   will be set to the appropriate value).
 */
k_ptr_t
kl_get_proc(kaddr_t value, int mode, int flags)
{
	kaddr_t proc, pdata;
	k_ptr_t procp;

	if (DEBUG(KLDC_FUNCTRACE, 2)) {
		if (mode != 2) {
			fprintf(KL_ERRORFP, "kl_get_proc: value=%lld, mode=%d, ",
				value, mode);
		} 
		else {
			fprintf(KL_ERRORFP, "kl_get_proc: value=0x%llx, mode=%d, ",
				value, mode);
		}
		fprintf(KL_ERRORFP, "flags=0x%x\n", flags);
	}

	kl_reset_error();

	/* Just in case... 
	 */
	if ((mode == 2) && !value) {
		KL_SET_ERROR_NVAL(KLE_BAD_PROC, value, mode);
		return ((k_ptr_t)NULL);
	}

	if (mode == 0) {
		KL_SET_ERROR_NVAL(KLE_BAD_PROC, value, mode);
		return((k_ptr_t)NULL);
	}
	else if (mode == 1) {
		proc = kl_pid_to_proc((int)value);
	}
	else {
		proc = value;
	}

	/* Allocate a block for a proc struct. Make sure and check to
	 * see if a K_PERM block needs to be allocated.
	 */
	if (flags & K_PERM) {
		procp = kl_alloc_block(PROC_SIZE, K_PERM);
	}
	else {
		procp = kl_alloc_block(PROC_SIZE, K_TEMP);
	}
	kl_get_struct(proc, PROC_SIZE, procp, "proc");
	if (KL_ERROR) {
		/* We need to make sure the error values reflect that this
		 * is a bad proc struct and that the proper value and
		 * mode are used (the values that generated the error in
		 * kl_get_struct() may not be the same).
		 */
		KL_ERROR |= KLE_BAD_PROC;
		KLIB_ERROR.e_nval = value;
		KLIB_ERROR.e_mode = mode;
		return((k_ptr_t)NULL);
	}

	/* Make sure this is actually a proc struct. The bd_pdata field
	 * of the p_bhv struct contains a pointer to the proc. If we 
	 * get a match, we can assume this is a valid proc struct. If
	 * not then return an error (unless the K_ALL flag is set).
	 */
	pdata = kl_bhv_pdata(proc + kl_member_offset("proc", "p_bhv")); 
	if ((proc != pdata) && !(flags & K_ALL)) {
		kl_free_block(procp);
		KL_SET_ERROR_NVAL(KLE_BAD_PROC, value, mode);
		return ((k_ptr_t)NULL);
	}

	/* Make sure the process is valid (active), or that the 
	 * K_ALL flag is set. 
	 */
	if (KL_INT(procp, "proc", "p_stat") || (flags & K_ALL)) {
		return (procp);
	}
	else {
		kl_free_block(procp);
		KL_SET_ERROR_NVAL(KLE_BAD_PROC, value, mode);
		return ((k_ptr_t)NULL);
	}
}
