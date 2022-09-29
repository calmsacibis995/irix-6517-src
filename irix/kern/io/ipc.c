/* Copyright 1986, Silicon Graphics Inc., Mountain View, CA. */
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)kern-port:io/ipc.c	10.3"*/
#ident	"$Revision: 3.29 $"
#include "sys/types.h"
#include "sys/param.h"
#include "sys/errno.h"
#include "sys/kipc.h"
#include "sys/ipc.h"
#include "sys/sat.h"
#include "sys/mac_label.h"
#include "sys/capability.h"

/*
**	Common IPC routines.
*/

/*
**	Check message, semaphore, or shared memory access permissions.
**
**	This routine verifies the requested access permission for the current
**	process.  Super-user is always given permission.  Otherwise, the
**	appropriate bits are checked corresponding to owner, group, or
**	everyone.  Zero is returned on success.  On failure, return EACCES.
**	The arguments must be set up as follows:
**		p - Pointer to permission structure to verify
**		mode - Desired access permissions
*/

int
ipcaccess(struct ipc_perm	*p,
	  struct cred		*cred,
	  ushort		mode)
{
	ushort	sat_mode = mode;
	int error;

	if (cred->cr_uid != p->uid && cred->cr_uid != p->cuid) {
		mode >>= 3;
		if (!groupmember(p->gid, cred) && !groupmember(p->cgid, cred))
			mode >>= 3;
	}
	error = (mode & p->mode || _CAP_CRABLE(cred, CAP_DAC_OVERRIDE)) ?
	    0 : EACCES;
	_SAT_SVIPC_ACCESS(NULL, sat_mode,
	    SAT_DAC | (error ? SAT_FAILURE : SAT_SUCCESS), error);
	return(error);
}

/*
**	Get message, semaphore, or shared memory structure.
**
**	This routine searches for a matching key based on the given flags
**	and returns a pointer to the appropriate entry.  A structure is
**	allocated if the key doesn't exist and the flags call for it.
**	The arguments must be set up as follows:
**		key - Key to be used
**		flag - Creation flags and access modes
**		ipcfcn - Function that returns ptrs to appropriate
**			ipc id structure and associated semaphore
**		size - sizeof(facility structure)
**		status - Pointer to status word: set on successful completion
**			only:	0 => existing entry found
**				1 => new entry created
**	Ipcget returns the appropriate errno if it fails, or 0 if it succeeds.
*/

int
ipcget(	key_t		key,
	register int	flag,
	register int	(*ipcfcn)(int, struct ipc_perm **, mutex_t **),
	register int	*status,
	struct	 cred	*cred,
	register int	*indxp)
{
	struct ipc_perm			*base;	/* ptr to perm array entry */
	register struct ipc_perm	*a;	/* ptr to available entry */
	mutex_t				*lockp;	/* ptr to perm mutex */
	mutex_t				*slockp;/* ptr to mutex for available entry */
	register int			i;	/* loop control */
	int				indx;
	int				error;

	if (key == IPC_PRIVATE) {
		for (i = 0; (*ipcfcn)(i, &base, &lockp); i++) {
			if (base->mode & IPC_ALLOC)
				continue;
			mutex_lock(lockp, PZERO);
			if (base->mode & IPC_ALLOC) {
				mutex_unlock(lockp);
				continue;
			}
			indx = i;
			goto init;
		}
		return ENOSPC;
	}
tryagain:
	for (i = 0, a = NULL; (*ipcfcn)(i, &base, &lockp); i++) {
		if (base->mode & IPC_ALLOC) {
			if (base->key == key) {
				mutex_lock(lockp, PZERO);
				if (base->key != key) {
					mutex_unlock(lockp);
					goto tryagain;
				}
				if ((flag & (IPC_CREAT | IPC_EXCL)) ==
				    (IPC_CREAT | IPC_EXCL)) {
					error = EEXIST;
					goto ipc_badget;
				}
				if ((flag & 0777) & ~base->mode) {
					error = EACCES;
					goto ipc_badget;
				}
				*status = 0;
				*indxp = i;
				return 0;
			}
			continue;
		}
		if (a == NULL) {
			a = base;
			slockp = lockp;
			indx = i;
		}
	}
	if (!(flag & IPC_CREAT))
		return ENOENT;
	if (a == NULL)
		return ENOSPC;
	base = a;
	lockp = slockp;
	mutex_lock(lockp, PZERO);
	/* make sure two processors have not grabbed the same slot */
	if (base->mode & IPC_ALLOC) {
		mutex_unlock(lockp);
		goto tryagain;
	}
init:
	*status = 1;
	base->mode = IPC_ALLOC | (flag & 0777);
	base->key = key;
	base->cuid = base->uid = cred->cr_uid;
	base->cgid = base->gid = cred->cr_gid;
	*indxp = indx;
	return 0;
ipc_badget:
	mutex_unlock(lockp);
	return error;
}

void
ipc_init(
	struct ipc_perm *perm,
	key_t		key,
	register int	flag,
	struct	 cred	*cred)
{
	perm->mode = IPC_ALLOC | (flag & 0777);
	perm->key = key;
	perm->cuid = perm->uid = cred->cr_uid;
	perm->cgid = perm->gid = cred->cr_gid;
}

#if _MIPS_SIM == _ABI64
void
irix5_to_ipc_perm(
	struct irix5_ipc_perm *i5_perm,
	struct ipc_perm *perm)
{
	perm->uid = i5_perm->uid;
	perm->gid = i5_perm->gid;
	perm->cuid = i5_perm->cuid;
	perm->cgid = i5_perm->cgid;
	perm->mode = i5_perm->mode;
	perm->seq = i5_perm->seq;
	perm->key = i5_perm->key;
}

void
ipc_perm_to_irix5(
	struct ipc_perm *perm,
	struct irix5_ipc_perm *i5_perm)
{
	i5_perm->uid = perm->uid;
	i5_perm->gid = perm->gid;
	i5_perm->cuid = perm->cuid;
	i5_perm->cgid = perm->cgid;
	i5_perm->mode = perm->mode;
	i5_perm->seq = perm->seq;
	i5_perm->key = perm->key;
}
#endif	/* _ABI64 */
