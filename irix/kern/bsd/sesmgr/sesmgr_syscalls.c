/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 *	SGI Session Manager
 */

#ident	"$Revision: 1.3 $"

#include <sys/types.h>
#include <sys/sat.h>
#include <sys/errno.h>
#include <sys/sesmgr.h>
#include <sys/t6api_private.h>

/*ARGSUSED*/
int
sgi_sesmgr(struct syssesmgra *uap, rval_t *rvp)
{
	int error = 0;
	int sesmgr_id = (uap->cmd >> 16) & 0xffff;

	/* record command for sat */
	/* _SAT_SET_SUBSYSNUM(curprocp,uap->cmd); */ /* SAT_XXX */

	/* Check if this session manger installed */
	if (!(sesmgr_enabled && sesmgr_id))
		return ENOSYS;

	switch (sesmgr_id) {

	case SESMGR_TSIX_ID:
		return sesmgr_t6_syscall(uap, rvp);
	case SESMGR_SATMP_ID:
		return sesmgr_satmp_syscall(uap, rvp);
	default:
		return ENOSYS;
	}
	/*NOTREACHED*/
	return error;
}
