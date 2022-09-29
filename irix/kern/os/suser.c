/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1996 Silicon Graphics, Inc.		  *
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

#ident	"$Revision: 1.10 $"

#include <sys/types.h>
#include <ksys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/uthread.h>
#include <sys/capability.h>
#include <sys/sat.h>

/*
 * Test if the passed cred represents the super user.
 */
/* ARGSUSED */
int
cap_able_cred(cred_t *cred, cap_value_t cid)
{
	if (!curuthread)
		return cred->cr_uid == 0;

	_SAT_SET_SUFLAG(SAT_SUSERCHK);
	if (cred->cr_uid == 0) {
		_SAT_SET_SUFLAG(SAT_SUSERPOSS);
		return 1;
	}
	return 0;
}

/* ARGSUSED */
int
cap_able(cap_value_t cid)
{
	return cap_able_cred(get_current_cred(), cid);
}
