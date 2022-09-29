/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1997, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.1 $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/cred.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/kmem.h>
#include <sys/debug.h>

/*
int UMFSMaint(
    UMFSMaintOp             inOp,
    int64_t                 inCookie,
    size_t                  inSize,
    void*                   inParams,
    size_t                  outSize,
    void*                   outParams);
*/

struct umfscalla {
	sysarg_t	op;
	sysarg_t	cookie;
	sysarg_t	inlen;
	caddr_t		inbuf;
	sysarg_t	outlen;
	caddr_t		outbuf;
};

/* ARGSUSED */
int umfscall(struct umfscalla *uap, rval_t *rvp)
{
return nopkg();
}
