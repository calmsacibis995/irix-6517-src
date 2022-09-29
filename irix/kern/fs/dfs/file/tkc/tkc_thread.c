/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tkc_thread.c,v 65.3 1998/03/23 16:27:21 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/* Copyright (C) 1994, 1989 Transarc Corporation - All rights reserved */

#include <tkc.h>

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkc/RCS/tkc_thread.c,v 65.3 1998/03/23 16:27:21 gwehrman Exp $")

/*
 * In some kernels (particularly SunOS), it is possible for some x-ops
 * to cause calls to o-ops (glued).  If an o-op called an x-op which in turn
 * caused a call to another o-op, the glue code in the two o-op functions might
 * create a problem.  To avoid this, the inner o-op checks to see whether
 * an outer o-op (or the PX) has already done token management.  The functions
 * in this module implement this checking.  They are called from the glue code
 * and from the PX.
 */

#ifdef AFS_SUNOS5_ENV

struct tkc_vn {
    struct tkc_vn *next;
    struct tkc_vn *pushed;
    char *proc;
    long flags;	
};
struct tkc_vn *tkc_vnatt = 0;
struct lock_data tkc_vnlock;

void gluevn_InitThreadFlags()
{
    lock_Init(&tkc_vnlock);
}

/* Change _SetThreadFlags and _Done to push and pop, since we might recurse
   through here. */


void gluevn_SetThreadFlags(flags)
    long flags; 
{
    register struct tkc_vn *gp, *ogp;
    register char *procp = (char *)osi_ThreadUnique();

    lock_ObtainWrite(&tkc_vnlock);
    for (ogp = tkc_vnatt; ogp; ogp = ogp->next) {
	if (procp == ogp->proc) {
	    break;
	}
    }
    gp = (struct tkc_vn *) tkc_AllocSpace();
    gp->proc = procp;
    gp->flags = flags;
    gp->pushed = NULL;
    if (ogp) {
	/* Rare case.  Connect gp as first ``pushed'' of ogp. */
	gp->next = ogp->pushed;
	ogp->pushed = gp;
    } else {
	gp->next = tkc_vnatt;
	tkc_vnatt = gp;
    }
    lock_ReleaseWrite(&tkc_vnlock);
}


void gluevn_Done()
{
    register struct tkc_vn *gp, **gpp, *rgp;

    lock_ObtainWrite(&tkc_vnlock);
    gpp = &tkc_vnatt;
    for (gp = *gpp; gp; gpp = &gp->next, gp = *gpp) {
	if ((char *)osi_ThreadUnique() == gp->proc) {
	    if (gp->pushed) {
		/* Get the top one off the list */
		rgp = gp;
		gp = rgp->pushed;
		rgp->pushed = gp->next;
	    } else {
		*gpp = gp->next;		/* Remove it */
	    }
	    tkc_FreeSpace((char *)gp);
	    lock_ReleaseWrite(&tkc_vnlock);
	    return;
	}
    }
    panic("bad gluevn_Done");
}


long gluevn_GetThreadFlags()
{
    register struct tkc_vn *gp;
    register long code = 0;

    lock_ObtainRead(&tkc_vnlock);
    for (gp = tkc_vnatt; gp; gp = gp->next) {
	if ((char *)osi_ThreadUnique() == gp->proc) {
	    if (gp->pushed) {
		/* Grab flags from most-recently-pushed block */
		code = gp->pushed->flags;
	    } else {
		code = gp->flags;
	    }
	    break;
	}
    }
    lock_ReleaseRead(&tkc_vnlock);
    return code;
}

#endif /* AFS_SUNOS5_ENV */
