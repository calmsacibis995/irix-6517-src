/*
 * cell_ucopy.c
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident $Id: ucopy.c,v 1.9 1997/09/08 14:35:11 henseler Exp $

#include <sys/types.h>
#include <ksys/cell.h>
#include <sys/immu.h>
#include <sys/page.h>
#include <sys/cmn_err.h>
#include <sys/ddi.h>
#include <sys/errno.h>
#include <sys/idbgentry.h>
#include <sys/sema.h>
#include <ksys/cell/service.h>
#include "invk_ucopy_stubs.h"
#include "I_ucopy_stubs.h"
#include <sys/uthread.h>

#include <ksys/cell/subsysid.h>

/* ARGSUSED */
int
I_ucopy_copyin(
	caddr_t	src,
	char	**dst,
	size_t	*dst_len,
	void	**bdesc)
{
	int error=0;

	ASSERT(*dst_len != 0);
	*dst = kmem_alloc(*dst_len, KM_SLEEP);
	if (IS_KUSEG(src))
		error = copyin(src, *dst, *dst_len);
	else
		bcopy(src, *dst, *dst_len);
	return(error);
} 

/* ARGSUSED */
void
I_ucopy_copyin_done(
	char	*dst,
	size_t	dst_len,
	void	*bdesc)
{
	kmem_free(dst, dst_len);
}

int
do_ucopy_copyin(
	caddr_t	src,
	caddr_t	dst,
	size_t	len)
{
	int error;
#if 517986
	int	npgs = numpages(src,len);
	size_t	xpgs, nb;
#endif

	ASSERT(dst != NULL);
	ASSERT(len != 0);
#if 517986
	while (npgs) {
		if (npgs > 10) {
			xpgs = 10;
			nb = ctob(xpgs);
		} else {
			xpgs = npgs;
			nb = len;
		}
		npgs -= xpgs;
		error = invk_ucopy_copyin(src, &dst, &nb);
		if (error)
			return(error);
		src += nb;
		dst += nb;
		len -= nb;
	}
	ASSERT(len == 0);
#else
	error = invk_ucopy_copyin(src, &dst, &len);
#endif
	return(error);
}

int
I_ucopy_copyout(
	char	*src,
	size_t	len,
	caddr_t	dst)
{
	int error=0;

	if (IS_KUSEG(dst))
		error = copyout(src, dst, len);
	else
		bcopy(src, dst, len);
	return(error);
} 

int
do_ucopy_copyout(
	caddr_t	src,
	caddr_t	dst,
	size_t	len)
{
	int error;
#if 517986
	int	npgs = numpages(src,len);
	int	xpgs, nb;
#endif

#if 517986
	while (npgs) {
		if (npgs > 10) {
			xpgs = 10;
			nb = ctob(xpgs);
		} else {
			xpgs = npgs;
			nb = len;
		}
		npgs -= xpgs;
		error = invk_ucopy_copyout(src, nb, dst);
		if (error)
			return(error);
		src += nb;
		dst += nb;
		len -= nb;
	}
	ASSERT(len == 0);
#else
	error = invk_ucopy_copyout(src, len, dst);
#endif
	return(error);
}

int
do_ucopy_zero(
	caddr_t	dst,
	size_t	len)
{
	int error;

	error = invk_ucopy_zero(dst, len);
	return(error);
}

int
I_ucopy_zero(
	caddr_t	dst,
	size_t	len)
{
	int error=0;

	if (IS_KUSEG(dst))
		error = uzero(dst, len);
	else
		bzero(dst, len);
	return(error);
}

void
ucopy_init(void)
{
        mesg_handler_register(ucopy_msg_dispatcher, UCOPY_SUBSYSID);
}

void
get_current_flid(flid_t *flidp)
{
	/*REFERENCED*/
	trans_t	*tr;

	if ((tr = mesg_thread_info()) == NULL) {
		ASSERT(curuthread);
		*flidp = curuthread->ut_flid;
	} else {
		/* REFERENCED */
		int	msgerr;

		msgerr = invk_ucopy_get_flid(flidp);
		ASSERT(!msgerr);
	}
}

int
I_ucopy_get_flid(flid_t *flidp)
{
	ASSERT(curuthread);
	*flidp = curuthread->ut_flid;
	return(0);
}

