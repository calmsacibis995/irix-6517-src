/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 * 
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 * 
 * 		Copyright Notice 
 * 
 * Notice of copyright on this source code product does not indicate 
 * publication.
 * 
 * 	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 * 	          All rights reserved.
 *  
 */

#ifndef _KSYS_CRED_H	/* wrapper symbol for kernel use */
#define _KSYS_CRED_H	/* subject to change without notice */

#ident	"$Revision: 1.6 $"

#include <sys/cred.h>		/* REQUIRED */

#ifdef NOTYET

These declarations should all get moved out of sys/cred.h,
but there's lots of files that would have to get changed.

extern int ngroups_max;
extern struct cred *sys_cred;

struct proc;
extern void cred_init(void);
extern ushort crhold(cred_t *);
extern void crfree(cred_t *);
extern cred_t *crcopy(struct proc *);
extern cred_t *crdup(cred_t *);
extern cred_t *crgetcred(void);
extern size_t crgetsize(void);
extern int groupmember(gid_t, cred_t *);
extern int crsuser(cred_t *);
extern cred_t *get_current_cred(void);
extern void set_current_cred(cred_t *);
#endif

/*
 * Process credentials and locking.
 *
 * Each process has a pointer to its credential structure.  In addition,
 * each process thread (uthread) has a cached copy of the credential
 * structure pointer.
 *
 * If a process gets a new cred pointer, the uthreads get the UT_UPDCRED
 * bit set in their update-fields, and each uthread will sync-up with
 * the process cred pointer when next entering or leaving the kernel.
 *
 * Must hold process credential lock in access mode if dereferencing
 * process's credential pointer; must hold the lock in update mode if
 * changing the pointer or any fields in the credential structure.
 *
 * Must hold the lock in update mode when updating the current uthread's
 * credential pointer to the (changed) process pointer.
 *
 * N.B.: the only thread that can dereference or change the cached
 * uthread credential structure is the uthread itself.  Since only
 * the owning uthread can change the structure, it does not require
 * any locking to dereference the pointer.
 */

/*
 * Get process credential structure in update or access mode.
 */
extern	cred_t *pcred_lock(struct proc *);
extern	cred_t *pcred_access(struct proc *);

/*
 * Unlock process credential structure from update or access mode.
 */
#define	pcred_unlock(p)	mrunlock(&(p)->p_credlock)
#define	pcred_unaccess(p) mraccunlock(&(p)->p_credlock)

/*
 * Check if process credential structure is locked for update or access mode.
 */
#define	pcred_islocked(p)	mrislocked_update(&(p)->p_credlock)
#define	pcred_isaccess(p)	mrislocked_access(&(p)->p_credlock)


/*
 * Unlock process credential structure, but first push cred-update
 * request to all the uthreads in the named process.
 */
#define	pcred_push(p) \
	{ setpsync(&(p)->p_proxy, UT_UPDCRED); mrunlock(&(p)->p_credlock); }

#if defined(CELL)

extern cred_t *creds_crget(void);

#define CREDID_NULLID	(-1)
#define	CREDID_SYSCRED	(-2)
/*
 * Some handy macros
 */
#define	CRED_GETID(credp)	((credp)? cred_getid(credp) : CREDID_NULLID)
#define	CREDID_GETCRED(credid)	(((credid) == CREDID_NULLID)? 	\
					NULL : credid_getcred(credid))
/*
 * Manage mappings between cred structs and cred ids
 */
extern	void credid_lookup_remove(cred_t *);
extern	cred_t *credid_getcred(credid_t);
extern	credid_t cred_getid(cred_t *);
#endif
#endif	/* _KSYS_CRED_H */
