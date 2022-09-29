/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994-1995 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.1 $"

#include <sys/errno.h>

#include <sys/types.h>
#include <sys/pda.h>
#include <sys/sema.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/ddi.h>	/* ZZZ  - for delay */
#include <sys/debug.h>
#include <sys/atomic_ops.h>
#include <sys/klstat.h>

/*---------------------------------------------------*/
/*              klstat.c                             */
/*---------------------------------------------------*/

static lstat_control_t	lstat_control;
#pragma fill_symbol (lstat_control, 128)

extern long		*kernel_magic;

static ushort	lstat_make_dir_entry(void *, void *);


/*
 * lstat_lookup
 * Given a RA, locate the directory entry for the lock.
 */
static __inline ushort	
lstat_lookup(
	void	*lock_ptr,
	void	*caller_ra)
{
	ushort			index;
	lstat_directory_entry_t	*dirp;

	index = lstat_control.hashtab[DIRHASH(caller_ra)];
	dirp = lstat_control.dir;
	while (dirp[index].caller_ra != caller_ra) {
		#pragma mips_frequency_hint NEVER
		if (index == 0) {
			#pragma mips_frequency_hint NEVER
			return(lstat_make_dir_entry(lock_ptr, caller_ra));
		}
		index = dirp[index].next_stat_index;
	}

	if (dirp[index].lock_ptr != NULL && 
			dirp[index].lock_ptr != lock_ptr)  {
		#pragma mips_frequency_hint NEVER
		dirp[index].lock_ptr = NULL;
	}

	return(index);
}


/*
 * lstat_make_dir_entry
 * Called to add a new lock to the lock directory.
 */
static ushort	
lstat_make_dir_entry(
	void	*lock_ptr, 			
	void	*caller_ra)
{
	lstat_directory_entry_t	*dirp;
	int			s;
	ushort			index, hindex;

	s = spl7();
	while (compare_and_swap_int(&lstat_control.directory_lock, 0, 1) == 0) {
		#pragma mips_frequency_hint NEVER
		splx(s);
		us_delay(5);
		s = spl7();
	}

	hindex = DIRHASH(caller_ra);
	index = lstat_control.hashtab[hindex];
	dirp = lstat_control.dir;
	while (index && dirp[index].caller_ra != caller_ra)
		index = dirp[index].next_stat_index;

	if (index == 0 && lstat_control.next_free_dir_index < LSTAT_MAX_STAT_INDEX) {
		index = lstat_control.next_free_dir_index++;
		lstat_control.dir[index].caller_ra = caller_ra;
		lstat_control.dir[index].lock_ptr = lock_ptr;
		lstat_control.dir[index].next_stat_index = lstat_control.hashtab[hindex];
		lstat_control.hashtab[hindex] = index;
	}

	lstat_control.directory_lock = 0;
	splx(s);
	return(index);

}




void*
lstat_update (
	void	*lock_ptr,
	void	*caller_ra,
	int	action,
	void	*return_value)
{
	ushort	index;

	ASSERT(action < LSTAT_ACT_MAX_VALUES);

	if (lstat_control.state == 0) {
		#pragma mips_frequency_hint FREQUENT
		return(return_value);
	}

	index = lstat_lookup(lock_ptr, caller_ra);

	(*lstat_control.counts[cpuid()])[index].count[action]++;

	return(return_value);


}

void*
lstat_update_spin (
	void	*lock_ptr,
	void	*caller_ra,
	int	ticks,
	void	*return_value)
{
	ushort	index;
	int	cpu;

	if (lstat_control.state == 0) {
		#pragma mips_frequency_hint FREQUENT
		return(return_value);
	}

	index = lstat_lookup(lock_ptr, caller_ra);

	cpu = cpuid();
	(*lstat_control.counts[cpu])[index].count[ticks?LSTAT_ACT_SPIN:LSTAT_ACT_NO_WAIT]++;
	(*lstat_control.counts[cpu])[index].ticks += ticks;

	return(return_value);


}

void*
lstat_update_time (
	void	*lock_ptr,
	void	*caller_ra,
	int	action,
	void	*return_value,
	uint32_t ticks)
{
	ushort	index;
	int	cpu;

	ASSERT(action < LSTAT_ACT_MAX_VALUES);

	if (lstat_control.state == 0) {
		#pragma mips_frequency_hint FREQUENT
		return(return_value);
	}

	index = lstat_lookup(lock_ptr, caller_ra);

	cpu = cpuid();
	(*lstat_control.counts[cpu])[index].count[action]++;
	(*lstat_control.counts[cpu])[index].ticks += ticks;

	return(return_value);


}

int
lstat_user_command(
	int	fc,
	void	*arg)
{
	lstat_user_request_t	req;
	int			cpu, error = 0;
	int			dirsize, countsize, hashsize;
	char			*p;

	dirsize = LSTAT_MAX_STAT_INDEX * sizeof(lstat_directory_entry_t);
	countsize = sizeof(lstat_cpu_counts_t);
	hashsize = (1 + LSTAT_HASH_TABLE_SIZE) * sizeof(ushort);

	if (arg && copyin(arg, &req, sizeof(req)))
		return EFAULT;

	mutex_lock(&lstat_control.control_lock, PZERO);

	switch (fc) {
	case LSTAT_ON:
		if (lstat_control.state) {
			error = EBUSY;
			goto done;
		}
		lstat_control.directory_lock = 0;
		lstat_control.next_free_dir_index = 1;		/* 0 is for overflows */

		/*
		 * !!! For now, the arrays are not deallocated when turning
		 * stats off. To do so, would required splhi in the update routines
		 * & this is too much overhead. May rewrite update routines in
		 * assembly lang - revisit then.
		 */
		if (!lstat_control.hashtab) {
			lstat_control.hashtab = kmem_alloc(hashsize, KM_SLEEP);
			lstat_control.dir = kmem_alloc(dirsize, KM_SLEEP);
			for (cpu = 0; cpu<maxcpus; cpu++) {
				if (pdaindr[cpu].CpuId != cpu)
					continue;
				lstat_control.counts[cpu] = kmem_alloc_node_hint(
						countsize, 0, pdaindr[cpu].pda->p_nodeid);
				if (!lstat_control.counts[cpu]) {
					while (--cpu >= 0) {
						kern_free(lstat_control.counts[cpu]);
						error = ENOSPC;
						goto done;
					}
				}
			}
		}

		bzero(lstat_control.hashtab, hashsize);
		bzero(lstat_control.dir, dirsize);
		for (cpu = 0; cpu<maxcpus; cpu++)
			if (pdaindr[cpu].CpuId == cpu)
				bzero(lstat_control.counts[cpu], countsize);

		lstat_control.enabled_lbolt =  lbolt;
		__synchronize();		/* prevent code reordering */
		lstat_control.state = 1;
		break;
	case LSTAT_OFF:
		if (!lstat_control.state) {
			error = EBUSY;
			goto done;
		}
		lstat_control.state = 0;

#ifdef notyet
		/*
		 * Dont free the tables yet.
		 */
		delay(1000);
		kmem_free(lstat_control.hashtab, hashsize);
		kmem_free(lstat_control.dir, dirsize);
		for (cpu = 0; cpu<maxcpus; cpu++)
			if (lstat_control.counts[cpu])
				kmem_free(lstat_control.counts[cpu], countsize);
#endif
		break;
	case LSTAT_READ:
		if (!lstat_control.state) {
			error = ENOMSG;
			goto done;
		}
		for (p=(char*)req.cpu_counts_ptr, cpu = 0; cpu<maxcpus; cpu++, p += countsize) {
			if (pdaindr[cpu].CpuId != cpu) {
				if (uzero(p, countsize))
					error = EFAULT;
			} else {
				if (copyout(lstat_control.counts[cpu], p, countsize))
					error = EFAULT;
			}
		}
		if (copyout(lstat_control.dir, req.directory_ptr, dirsize))
			error = EFAULT;
		req.current_time = time;
		req.enabled_lbolt = lstat_control.enabled_lbolt;
		req.current_lbolt = lbolt;
		req.next_free_dir_index = lstat_control.next_free_dir_index;
		break;
	case LSTAT_STAT:
		req.kernel_magic_addr = &kernel_magic;
		req.kernel_end_addr= kernel_magic;
		req.enabled_lbolt = lstat_control.enabled_lbolt;
		req.maxcpus = maxcpus;
		query_cyclecntr(&req.cycleval);
		req.lstat_is_enabled = lstat_control.state;
		break;
	default:
		error = EINVAL;
	}



done:
	mutex_unlock(&lstat_control.control_lock);

	if (arg && copyout(&req, arg, sizeof(req)))
		return EFAULT;

	return error;
}

