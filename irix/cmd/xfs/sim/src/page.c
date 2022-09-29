 /**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1993 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.12 $"

#define _KERNEL	1
#include <sys/types.h>
#undef _KERNEL
#include <sys/immu.h>
#include <sys/vnode.h>
#include <sys/pfdat.h>

pfd_t	*pfdat;		/* ptr to the global array of pfdat structures */
char	*pages;		/* the memory described by the pfdats */

/*
 * Renounce vnode pages [first, last].
 */
/* ARGSUSED */
void
vnode_tosspages(register vnode_t *vp,
	register off_t	first,
	register off_t	last)
{
}

/*
 * Flush delayed writes for vp,
 * then invalidate remaining used pages and unhash cached ones.
 *
 * returns: nothing
 */
/* ARGSUSED */
void
vnode_flushinval_pages(
	register struct vnode *vp,
	register off_t	start,
	register off_t	end)
{
}

/*
 * Flush delayed writes for vp.
 *
 * returns: 0 or an errno
 */
/* ARGSUSED */
int
pflushvp(register struct vnode *vp, off_t filesize, int bflags)
{
	return 0;
}

/*
 * Return the address of the memory corresponding to the
 * given pfdat.
 */
/* ARGSUSED */
caddr_t
page_mapin(pfd_t	*pfd,
	   int		flag,
	   int		flag2)
{
	return (pages + (NBPP * (pfd - pfdat)));
} 

/* ARGSUSED */
void
page_mapout(caddr_t addr)
{
	return;
}
