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

#ident	"$Revision: 1.49 $"

#include <sys/types.h>
#include <sys/acct.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/kthread.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/mac_label.h>
#include <sys/capability.h>
#include <sys/atomic_ops.h>
#include "os/proc/pproc_private.h"	/* XXX bogus */
#if defined(CELL)
#include <ksys/cred.h>
#endif

static zone_t *cred_zone;
STATIC size_t crsize = 0;	/* for performance only */

/*
 * Allocate a zeroed cred structure.
 */
#define crget()	(struct cred *)kmem_zone_zalloc(cred_zone, KM_SLEEP)

/*
 * Global system credential structure.
*/
struct	cred	*sys_cred;

/*
 * Initialize credentials data structures.
 */
void
cred_init(void)
{
	(void) crgetsize();	/* crsize set in crgetsize() */
	cred_zone = kmem_zone_init(crsize, "Credentials");
	sys_cred = crget();
	sys_cred->cr_ref = 1;
#ifdef CELL
	sys_cred->cr_id = CREDID_SYSCRED;
#endif
        /*
         * Initialize additional security attributes in the system
         * credentials.
         */
        sys_cred->cr_cap.cap_effective = CAP_ALL_ON;
        sys_cred->cr_cap.cap_inheritable = CAP_ALL_ON;
        sys_cred->cr_cap.cap_permitted = CAP_ALL_ON;
        _MAC_INIT_CRED();
}

/*
 * Note on crhold and crfree - the cr_ref field is atomically
 * bumped/decremented by atomic ops. 
 */

int
crhold(cred_t *cr)
{
	int ret;
	ASSERT(cr->cr_ref > 0);		/* catch bogus references */

	ret = atomicAddInt(&cr->cr_ref, 1);
	return(ret);
}

/*
 * Free a cred structure.  Return it to the freelist when the reference
 * count drops to 0.
 */
void
crfree(struct cred *cr)
{
	ASSERT(cr->cr_ref > 0);		/* catch bogus references */

	if (atomicAddInt(&cr->cr_ref, -1) == 0) {
#if defined(CELL)
		if (cr->cr_id != CREDID_NULLID) {
			credid_lookup_remove(cr);
		}
#endif
		kmem_zone_free(cred_zone, cr);
	}
}

/*
 * Copy a cred structure to a new one and free the old one.  Optimize
 * the case where the reference count is already 1 by just re-using
 * the same cred struct.
 * Caller must have process lock.
 */
struct cred *
crcopy(struct proc *p)
{
	struct cred *cr, *newcr;

	cr = p->p_cred;
	if (cr->cr_ref == 1) {
#if defined(CELL)
		if (cr->cr_id != CREDID_NULLID) {
			credid_lookup_remove(cr);
			cr->cr_id = CREDID_NULLID;
		}
#endif
		return cr;
	}

	ASSERT(cr->cr_ref > 1);

	p->p_cred = newcr = crget();
	bcopy(cr, newcr, crsize);
	newcr->cr_ref = 1;
#if defined(CELL)
	newcr->cr_id = CREDID_NULLID;
#endif

	crfree(cr);

	return newcr;
}

/*
 * Dup a cred struct to a new held one.
 */
struct cred *
crdup(struct cred *cr)
{
	register struct cred *newcr;

	newcr = crget();
	bcopy(cr, newcr, crsize);
	newcr->cr_ref = 1;
#if defined(CELL)
	newcr->cr_id = CREDID_NULLID;
#endif
	return newcr;
}

/*
 * compare two cred structs - return 0 if same or dups of each other
 */
int
crcmp(struct cred *cr1, struct cred *cr2)
{
	/* compare everything but  cred id and ref */
#if defined(CELL)
	char *crc1 = (char *)(cr1) + sizeof (cr1->cr_ref) + sizeof (cr1->cr_id);
	char *crc2 = (char *)(cr2) + sizeof (cr2->cr_ref) + sizeof (cr2->cr_id);
	return bcmp(crc1, crc2, crsize - sizeof (cr1->cr_ref) - sizeof (cr1->cr_id));
#else
	char *crc1 = (char *)(cr1) + sizeof (cr1->cr_ref);
	char *crc2 = (char *)(cr2) + sizeof (cr2->cr_ref);
	return bcmp(crc1, crc2, crsize - sizeof (cr1->cr_ref));
#endif
}

/*
 * Return the (held) credentials for the current running thread.
 */
struct cred *
crgetcred(void)
{
	cred_t *cr = get_current_cred();

	ASSERT(KT_CUR_ISUTHREAD());
	crhold(cr);
	return cr;
}

/*
 * Return the size of the credentials.
 *
 * Note that setting crsize here instead of crget() allows
 * crgetsize() to be called before a crget() call.
 * Further note that for performance, crsize is defined
 * as a static to the file.
 */
size_t
crgetsize(void)
{
	if (crsize == 0) {

		crsize = sizeof(struct cred) + sizeof(gid_t)*(ngroups_max-1);

		/* Align on the largest element of the struct. */
		crsize = (crsize + __builtin_alignof(struct cred) - 1) &
				 ~(__builtin_alignof(struct cred) - 1);
	}
	return(crsize);
}

/*
 * Determine whether the supplied group id is a member of the group
 * described by the supplied credentials.
 */
int
groupmember(gid_t gid, cred_t *cr)
{
	register gid_t *gp, *endgp;

	if (gid == cr->cr_gid)
		return 1;
	endgp = &cr->cr_groups[cr->cr_ngroups];
	for (gp = cr->cr_groups; gp < endgp; gp++)
		if (*gp == gid)
			return 1;
	return 0;
}

/*
 * This function is called to check whether the credentials set
 * "scrp" has permission to act on the credentials set of process targp.  
 * It enforces the permission requirements needed to send a signal to a process.
 * The same requirements are imposed by other system calls, however.
 *
 * Hold p_lck of the process being checked to prevent it from changing
 * its credentials while we're examining them.  This way we don't use
 * an old pointer if it does a crcopy in the middle here.
 */
int
hasprocperm(register proc_t *targp, register cred_t *scrp)
{
	register cred_t *tcrp;
	register int result = 0;

	mraccess(&targp->p_credlock);
	tcrp = targp->p_cred;

	if (!_MAC_ACCESS(tcrp->cr_mac, scrp, MACWRITE)) {
		result = (scrp->cr_uid  == tcrp->cr_ruid ||
			  scrp->cr_ruid == tcrp->cr_ruid ||
			  scrp->cr_uid  == tcrp->cr_suid ||
			  scrp->cr_ruid == tcrp->cr_suid ||
			  _CAP_CRABLE(scrp, CAP_KILL));
	}

	mrunlock(&targp->p_credlock);
	return result;
}

/*
 * get current execution credentials
 */
cred_t *
get_current_cred(void)
{
	return KTOP_GET_CURRENT_CRED();
}

void
set_current_cred(cred_t *cr)
{
	KTOP_SET_CURRENT_CRED(cr);
}

cred_t *
pcred_lock(proc_t *p)
{
	mrupdate(&p->p_credlock);
	return p->p_cred;
}

cred_t *
pcred_access(proc_t *p)
{
	mraccess(&p->p_credlock);
	return p->p_cred;
}

#ifdef CELL
cred_t *
creds_crget()
{
	return (crget());
}
#endif
