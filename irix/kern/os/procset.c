/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.32 $"

#include <sys/types.h>
#include <ksys/vproc.h>
#include <sys/proc.h>
#include <ksys/vsession.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/procset.h>
#include <sys/debug.h>

/*
 * Check if a procset_t is valid.  Return zero or an errno.
 */
int
checkprocset(register procset_t *psp)
{
	switch (psp->p_lidtype) {
	case P_PID:
	case P_PPID:
	case P_PGID:
	case P_SID:
	case P_UID:
	case P_GID:
	case P_ALL:
		break;
	default:
		return EINVAL;
	}

	switch (psp->p_ridtype) {
	case P_PID:
	case P_PPID:
	case P_PGID:
	case P_SID:
	case P_UID:
	case P_GID:
	case P_ALL:
		break;
	default:
		return EINVAL;
	}

	switch (psp->p_op) {
	case POP_DIFF:
	case POP_AND:
	case POP_OR:
	case POP_XOR:
		break;
	default:
		return EINVAL;
	}

	return 0;
}

/*
 * procinset returns 1 if the process pointed to
 * by pp is in the process set specified by psp and is not in
 * the sys scheduling class - otherwise 0 is returned.
 *
 * This function expects to be called with a valid procset_t.
 * The set should be checked using checkprocset() before calling
 * this function.
 */
int
procinset(vproc_t *vpr, procset_t *psp)
{
	int loperand = 0;
	int roperand = 0;
	vp_get_attr_t attr;
	int flags;

	flags = VGATTR_PPID | VGATTR_PGID | VGATTR_SID |
		VGATTR_UID | VGATTR_GID;

	VPROC_GET_ATTR(vpr, flags, &attr);

	/*
	 * If process is in the sys class return 0.
	 */

	switch (psp->p_lidtype) {

	case P_PID:
		if (vpr->vp_pid == psp->p_lid)
			loperand++;
		break;

	case P_PPID:
		if (attr.va_ppid == psp->p_lid)
			loperand++;
		break;

	case P_PGID:
		if (attr.va_pgid == psp->p_lid)
			loperand++;
		break;

	case P_SID:
		if (attr.va_sid == psp->p_lid)
			loperand++;
		break;

	case P_UID:
		if (attr.va_uid == psp->p_lid)
			loperand++;
		break;

	case P_GID:
		if (attr.va_gid == psp->p_lid)
			loperand++;
		break;

	case P_ALL:
		loperand++;
		break;

	default:
		ASSERT(0);
		return 0;
	}

	switch (psp->p_ridtype) {

	case P_PID:
		if (vpr->vp_pid == psp->p_rid)
			roperand++;
		break;

	case P_PPID:
		if (attr.va_ppid == psp->p_rid)
			roperand++;
		break;

	case P_PGID:
		if (attr.va_pgid == psp->p_rid)
			roperand++;
		break;

	case P_SID:
		if (attr.va_sid == psp->p_rid)
			roperand++;
		break;

	case P_UID:
		if (attr.va_uid == psp->p_rid)
			roperand++;
		break;

	case P_GID:
		if (attr.va_gid == psp->p_rid)
			roperand++;
		break;

	case P_ALL:
		roperand++;
		break;

	default:
		ASSERT(0);
		return 0;
	}

	switch (psp->p_op) {
	
	case POP_DIFF:
		if (loperand && !roperand)
			return 1;
		else
			return 0;

	case POP_AND:
		if (loperand && roperand)
			return 1;
		else
			return 0;

	case POP_OR:
		if (loperand || roperand)
			return 1;
		else
			return 0;

	case POP_XOR:
		if ((loperand || roperand) && !(loperand && roperand))
			return 1;
		else
			return 0;

	default:
		ASSERT(0);
		return 0;
	}
}

id_t
getmyid(
	idtype_t idtype,
	vp_get_attr_t *attr)
{
	switch (idtype) {
	case P_PID:
		return current_pid();
	
	case P_PPID:
		return attr->va_ppid;

	case P_PGID:
		return attr->va_pgid;

	case P_SID:
		return attr->va_sid;

	case P_UID:
		return get_current_cred()->cr_uid;

	case P_GID:
		return get_current_cred()->cr_gid;

	case P_ALL:
		/*
		 * The value doesn't matter for P_ALL.
		 */
		return 0;

	default:
		return (id_t)-1;
	}
}
