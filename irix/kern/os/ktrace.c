
/*
 * The functions defined here are only included in DEBUG kernels.
 * Ktrace_alloc is needed as a stub for non-debug kernels, for now.
 * It should really be in stubs/.
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/ktrace.h>
#include <sys/debug.h>
#include <sys/atomic_ops.h>

#ifndef DEBUG
/*ARGSUSED*/
ktrace_t *
ktrace_alloc(int nentries, int sleep)
{
	return NULL;
}
#else
/*
 * ktrace_alloc()
 *
 * Allocate a ktrace header and enough buffering for the given
 * number of entries.
 */
ktrace_t *
ktrace_alloc(int nentries, int sleep)
{
	ktrace_t	*ktp;
	ktrace_entry_t	*ktep;

	ktp = (ktrace_t*)kmem_alloc(sizeof(*ktp), sleep & KM_NOSLEEP);
	if (ktp == (ktrace_t*)NULL)
		return NULL;

	ktep = (ktrace_entry_t*)kmem_zalloc((nentries * sizeof(*ktep)),
					    sleep & KM_NOSLEEP);
	if (ktep)
		ktp->kt_entries = ktep;
	else {
		kmem_free(ktp, sizeof(*ktp));
		return NULL;
	}

	spinlock_init(&(ktp->kt_lock), "kt_lock");
	ktp->kt_nentries = nentries;
	ktp->kt_index = 0;
	ktp->kt_rollover = 0;

	return ktp;
}

/*
 * ktrace_free()
 *
 * Free up the ktrace header and buffer.  It is up to the caller
 * to ensure that no-one is referencing it.
 */
void
ktrace_free(ktrace_t *ktp)
{
	int	entries_size;

	if (ktp == (ktrace_t *)NULL)
		return;

	spinlock_destroy(&ktp->kt_lock);
	entries_size = ktp->kt_nentries * sizeof(ktrace_entry_t);
	kmem_free(ktp->kt_entries, entries_size);
	kmem_free(ktp, sizeof(ktrace_t));
}

/*
 * Enter the given values into the "next" entry in the trace buffer.
 * kt_index is always the index of the next entry to be filled.
 */
void
ktrace_enter(
	ktrace_t	*ktp,
	void		*val0,
	void		*val1,
	void		*val2,
	void		*val3,
	void		*val4,
	void		*val5,
	void		*val6,
	void		*val7,
	void		*val8,
	void		*val9,
	void		*val10,
	void		*val11,
	void		*val12,
	void		*val13,
	void		*val14,
	void		*val15)	     
{
	int		index;
	ktrace_entry_t	*ktep;

	ASSERT(ktp != NULL);

	/*
	 * Grab an entry by pushing the index up to the next one.
	 */
	index = atomicIncWithWrap(&ktp->kt_index, ktp->kt_nentries);
	if (!ktp->kt_rollover && index == ktp->kt_nentries - 1)
		ktp->kt_rollover = 1;
	ASSERT((index >= 0) && (index < ktp->kt_nentries));

	ktep = &(ktp->kt_entries[index]);
	ktep->val[0] = val0;
	ktep->val[1] = val1;
	ktep->val[2] = val2;
	ktep->val[3] = val3;
	ktep->val[4] = val4;
	ktep->val[5] = val5;
	ktep->val[6] = val6;
	ktep->val[7] = val7;
	ktep->val[8] = val8;
	ktep->val[9] = val9;
	ktep->val[10] = val10;
	ktep->val[11] = val11;
	ktep->val[12] = val12;
	ktep->val[13] = val13;
	ktep->val[14] = val14;
	ktep->val[15] = val15;

}

#include <sys/idbgentry.h>

static void
ktrace_print(
	ktrace_entry_t	*ktep)
{
	int		i;
	int		pad;

	qprintf("%s", ktep->val[0]);
	pad = 20 - strlen(ktep->val[0]);
	if (pad > 0)
		for (i = 1; i < (20 - strlen(ktep->val[0])); i++)
			qprintf(" ");
	qprintf("0x%x ", ktep->val[1]);
	for (i = 2; i < 16; i += 2) {
		if (ktep->val[i] == 0)
			break;
		qprintf("%s ", ktep->val[i]);
		if (((__psint_t)ktep->val[i+1]) < 0)
			prsymoff(ktep->val[i+1], NULL, " ");
		else
			qprintf("0x%x ", ktep->val[i+1]);
	}
	qprintf("\n");
}

void
ktrace_print_buffer(
	struct ktrace	*trbuffer,
	__psint_t	id,
	int		id_index,
	int		count)
{
	ktrace_entry_t	*ktep;
	ktrace_snap_t	kts;
	int		nentries;

	if (trbuffer == NULL)
		return;
	/*
	 * We want to dump nentries worth of trace info
	 */
	nentries = ktrace_nentries(trbuffer);
	if ((count <= 0) || (count > nentries))
		count = nentries;

	ktep = ktrace_first(trbuffer, &kts);
	if (count != nentries)
		ktep = ktrace_skip(trbuffer, nentries - count - 1, &kts);
	while (ktep != NULL) {
		if ((id_index < 0) || (ktep->val[id_index] == (void *)id))
			ktrace_print(ktep);
		ktep = ktrace_next(trbuffer, &kts);
	}
}

#endif /* DEBUG */
