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
 * 
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

#ifndef _PROC_CRED_H	/* wrapper symbol for kernel use */
#define _PROC_CRED_H	/* subject to change without notice */

#ident	"$Revision: 1.20 $"
/*
 * User credentials.  The size of the cr_groups[] array is configurable
 * but is the same (ngroups_max) for all cred structures; cr_ngroups
 * records the number of elements currently in use, not the array size.
 */

#include <sys/types.h>		/* REQUIRED */
#include <sys/capability.h>	/* REQUIRED for cap_set_t */

typedef struct cred {
	int	cr_ref;			/* reference count */
	ushort	cr_ngroups;		/* number of groups in cr_groups */
	uid_t	cr_uid;			/* effective user id */
	gid_t	cr_gid;			/* effective group id */
	uid_t	cr_ruid;		/* real user id */
	gid_t	cr_rgid;		/* real group id */
	uid_t	cr_suid;		/* "saved" user id (from exec) */
	gid_t	cr_sgid;		/* "saved" group id (from exec) */
	struct mac_label *cr_mac;	/* MAC label for B1 and beyond */
	cap_set_t cr_cap;		/* capability (privilege) sets */
#if CELL || CELL_PREPARE
	credid_t cr_id;			/* cred id */
#endif
	gid_t	cr_groups[1];		/* supplementary group list */
} cred_t;

#ifdef _KERNEL

extern struct cred *sys_cred;

struct proc;
extern void cred_init(void);
extern int crhold(cred_t *);
extern void crfree(cred_t *);
extern cred_t *crcopy(struct proc *);
extern cred_t *crdup(cred_t *);
extern cred_t *crgetcred(void);
extern size_t crgetsize(void);
extern int groupmember(gid_t, cred_t *);
extern int crsuser(cred_t *);
extern cred_t *get_current_cred(void);
extern void set_current_cred(cred_t *);
#ifdef CELL
extern void credid_flush_cache(cell_t);
#endif

#endif	/* _KERNEL */

#endif	/* _PROC_CRED_H */
