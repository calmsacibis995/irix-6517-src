#ident "$Revision: 1.12 $"

#include <sys/param.h>
#ifdef SIM
#define _KERNEL	1
#endif
#include <sys/buf.h>
#ifdef SIM
#undef _KERNEL
#endif
#include <sys/vnode.h>
#include <sys/debug.h>
#ifndef SIM
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#endif
#include "xfs_macros.h"
#include "xfs_types.h"
#include "xfs_inum.h"
#include "xfs_log.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_mount.h"

#ifdef SIM
#include "sim.h"
#endif


/*
 * This is called to determine if there are any pending
 * async transactions for the given file system.
 * Just return a value based on the value
 * of the list pointer in the xfs_mount structure.
 */
int
xfs_trans_any_async(struct xfs_mount *mp)
{
	return (mp->m_async_trans != NULL);
}


/*
 * Add the given transaction to the list of asynchronous
 * transactions to be committed.
 */
void
xfs_trans_add_async(xfs_trans_t *tp)
{
	int			s;
	struct xfs_mount	*mp;

	mp = tp->t_mountp;
	s = mutex_spinlock(&mp->m_async_lock);
	tp->t_forw = mp->m_async_trans;
	mp->m_async_trans = tp;
	mutex_spinunlock(&mp->m_async_lock, s);
}

/*
 * Return the list of asynchronous transactions hung off
 * of the given mount structure. If there is nothing on
 * the list, return NULL.
 */
xfs_trans_t *
xfs_trans_get_async(struct xfs_mount *mp)
{
	int			s;
	xfs_trans_t		*async_list;

	/*
	 * If the list is NULL, return without bothering
	 * with the lock.
	 */
	if (mp->m_async_trans == NULL) {
		return (NULL);
	}

	/*
	 * Pull off the entire list and return it.
	 * If it's NULL, we return NULL which is fine.
	 */
	s = mutex_spinlock(&mp->m_async_lock);
	async_list = mp->m_async_trans;
	mp->m_async_trans = NULL;
	mutex_spinunlock(&mp->m_async_lock, s);

	return (async_list);
}


