/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.21 $"

#include "sys/types.h"
#include "sys/acct.h"
#include "sys/capability.h"
#include "sys/cmn_err.h"
#include "sys/cred.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/kabi.h"
#include "sys/proc.h"
#include "sys/sat.h"

/*
 * This function returns true if the passed credential is in any way
 * privileged. It does NO auditing.
 */
int
cap_able_any(cred_t *cred)
{
	cap_value_t cv;

	ASSERT(cred != NULL);
	cv = cred->cr_cap.cap_effective;

	/*
	 * If the credential has any capabilities return true.
	 */
	if (cv && (cv != CAP_NOT_A_CID))
		return 1;

	/*
	 * If Superuser is on and the euid is 0 return true.
	 */
	if (cap_enabled != CAP_SYS_NO_SUPERUSER && cred->cr_uid == 0)
		return 1;

	return 0;
}

int
cap_able_cred(cred_t *cred, cap_value_t cid)
{
	/*
	 * It would be silly to call this function with a NULL cred.
	 */
	ASSERT(cred != NULL);

	/*
	 * XXX:casey  SAT_SUSERCHK isn't the truth anymore.
	 * XXX:casey  SAT_SUSERPOSS isn't the truth anymore, either.
	 * XXX:jwag - sthreads don't have curproc ...
	 */

	_SAT_SET_SUFLAG(SAT_SUSERCHK);
	_SAT_SET_CAP(cid);

	/*
	 * If the capability is possessed return success.
	 */
	if (CAP_ID_ISSET(cid, cred->cr_cap.cap_effective)) {
		_SAT_SET_SUFLAG(SAT_CAPPOSS);
		return 1;
	}

	/*
	 * uid not-zero is always done now.
	 */
	if (cred->cr_uid)
		return 0;

	/*
	 * Check against the SuperUser policy.
	 * If uid 0 has no special power return in the negative.
	 */
	if (cap_enabled == CAP_SYS_NO_SUPERUSER)
		return 0;

	_SAT_SET_SUFLAG(SAT_SUSERPOSS);

	return 1;
}

int
cap_able(cap_value_t cid)
{
	/*
	 * This function requires user context. If you hit this
	 * ASSERT, there's a call to CAP_ABLE() which needs to be
	 * changed to use CAP_CRABLE() (or cap_able_cred())
	 */
	ASSERT(get_current_cred() != NULL);

	return cap_able_cred(get_current_cred(), cid);
}
