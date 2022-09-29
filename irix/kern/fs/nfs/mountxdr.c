/*
 *
 * NFSSRC 3.2/4.3 for the VAX*
 * Copyright (C) 1987 Sun Microsystems, Inc.
 * 
 * (*)VAX is a trademark of Digital Equipment Corporation
 *
 */

#ifdef _KERNEL
#include "types.h"
#include "xdr.h"
#include "nfs.h"
#include "mount.h"
#else
#include <stdio.h>
#include <rpc/xdr.h>
#include <nfs/nfs.h>
#include <rpcsvc/mount.h>
#endif
#include <sys/time.h>
#include <sys/errno.h>

#define xdr_dev_t xdr_short

int
xdr_fhstatus(xdrs, fhsp)
	XDR *xdrs;
	struct fhstatus *fhsp;
{
	if (!xdr_int(xdrs, &fhsp->fhs_status))
		return (FALSE);
	if (fhsp->fhs_status == 0) {
		if (!xdr_fhandle(xdrs, &fhsp->fhs_fh))
			return (FALSE);
	}
	return (TRUE);
}

#ifndef _KERNEL
/*
 * The kernel uses nfs_xdr.c's version of xdr_fhandle.
 */
xdr_fhandle(xdrs, fhp)
	XDR *xdrs;
	fhandle_t *fhp;
{
	if (xdr_opaque(xdrs, fhp, NFS_FHSIZE)) {
		return (TRUE);
	}
	return (FALSE);
}
#endif

bool_t
xdr_path(xdrs, pathp)
	XDR *xdrs;
	char **pathp;
{
	if (xdr_string(xdrs, pathp, 1024)) {
		return(TRUE);
	}
	return(FALSE);
}

/* 
 * body of a mountlist
 */
bool_t
xdr_mountbody(xdrs, mlp)
	XDR *xdrs;
	struct mountlist *mlp;
{
	if (!xdr_path(xdrs, &mlp->ml_name))
		return FALSE;
	if (!xdr_path(xdrs, &mlp->ml_path))
		return FALSE;
	return(TRUE);
}

bool_t
xdr_mountlist(xdrs, mlp)
	register XDR *xdrs;
	register struct mountlist **mlp;
{
	/*
	 * more_elements is pre-computed in case the direction is
	 * XDR_ENCODE or XDR_FREE.  more_elements is overwritten by
	 * xdr_bool when the direction is XDR_DECODE.
	 */
	int more_elements;
	register int freeing = (xdrs->x_op == XDR_FREE);
	register struct mountlist **nxt;

	while (TRUE) {
		more_elements = (*mlp != NULL);
		if (! xdr_bool(xdrs, &more_elements))
			return (FALSE);
		if (! more_elements)
			return (TRUE);  /* we are done */
		/*
		 * the unfortunate side effect of non-recursion is that in
		 * the case of freeing we must remember the nxt object
		 * before we free the current object ...
		 */
		if (freeing)
			nxt = &((*mlp)->ml_nxt); 
		if (! xdr_reference(xdrs, (char **) mlp, 
			sizeof(struct mountlist), xdr_mountbody)) 
		{
			return (FALSE);
		}
		mlp = (freeing) ? nxt : &((*mlp)->ml_nxt);
	}
}

/*
 * Strange but true: the boolean that tells if another element
 * in the list is present has already been checked.  We handle the
 * body of this element then check on the next element.  YUK.
 */
bool_t
xdr_groups(xdrs, gr)
	register XDR *xdrs;
	register struct groups *gr;
{
	/*
	 * more_elements is pre-computed in case the direction is
	 * XDR_ENCODE or XDR_FREE.  more_elements is overwritten by
	 * xdr_bool when the direction is XDR_DECODE.
	 */
	int more_elements;

	if (! xdr_path(xdrs, &(gr->g_name)))
		return (FALSE);
	more_elements = (gr->g_next != NULL);
	if (! xdr_bool(xdrs, &more_elements))
		return (FALSE);
	if (! more_elements) {
		gr->g_next = NULL;
		return (TRUE);  /* we are done */
	}
	return (xdr_reference(xdrs, (char **) &(gr->g_next), 
		sizeof(struct groups), xdr_groups));
}

/* 
 * body of a exportlist
 */
bool_t
xdr_exportbody(xdrs, ex)
	XDR *xdrs;
	struct exports *ex;
{
	int more_elements;

	if (!xdr_path(xdrs, &ex->ex_name))
		return FALSE;
	more_elements = (ex->ex_groups != NULL);
	if (! xdr_bool(xdrs, &more_elements))
		return (FALSE);
	if (! more_elements) {
		ex->ex_groups = NULL;
		return (TRUE);  /* we are done */
	}
	if (! xdr_reference(xdrs, (char **) &(ex->ex_groups), 
		sizeof(struct groups), xdr_groups)) 
	{
		return (FALSE);
	}
	return(TRUE);
}


/*
 * Encodes the export list structure "exports" on the
 * wire as:
 * bool_t eol;
 * if (!eol) {
 * 	char *name;
 *	struct groups *groups;
 * }
 * where groups look like:
 * if (!eog) {
 *	char *gname;
 * }
 */
bool_t
xdr_exports(xdrs, exp)
	register XDR *xdrs;
	register struct exports **exp;
{
	/*
	 * more_elements is pre-computed in case the direction is
	 * XDR_ENCODE or XDR_FREE.  more_elements is overwritten by
	 * xdr_bool when the direction is XDR_DECODE.
	 */
	int more_elements;
	register int freeing = (xdrs->x_op == XDR_FREE);
	register struct exports **nxt;

	while (TRUE) {
		more_elements = (*exp != NULL);
		if (! xdr_bool(xdrs, &more_elements))
			return (FALSE);
		if (! more_elements)
			return (TRUE);  /* we are done */
		/*
		 * the unfortunate side effect of non-recursion is that in
		 * the case of freeing we must remember the nxt object
		 * before we free the current object ...
		 */
		if (freeing)
			nxt = &((*exp)->ex_next); 
		if (! xdr_reference(xdrs, (char **) exp, 
			sizeof(struct exports), xdr_exportbody)) 
		{
			return (FALSE);
		}
		exp = (freeing) ? nxt : &((*exp)->ex_next);
	}
}

/* 
 * NFSSRC 3.2/4.3 for the VAX*
 * Copyright (C) 1987 Sun Microsystems, Inc.
 * 
 * (*)VAX is a trademark of Digital Equipment Corporation
 */
#ifndef _KERNEL
/*
 * Copyright 1988 Silicon Graphics, Inc. All rights reserved.
 *
 * New SGI version of the mount protocol returns the SGI "nohide" option
 * as well as Sun-defined options and the exported directory name for the
 * MOUNTPROC_EXPORT and EXPORTALL remote procedures.
 *
 * The MOUNTPROC_EXPORTLIST procedure returns a list of exportent(3)
 * structures, so any future options can be acquired from a server's
 * /etc/exports file.
 */
#define xdr_groups_pointer(xdrs, grp) \
	xdr_pointer(xdrs, (char **) (grp), sizeof **(grp), xdr_groups)

bool_t
xdr_exportentry(XDR *xdrs, struct exportentry *ee)
{
	bool_t has_options;

	if (!xdr_path(xdrs, &ee->ee_dirname))
		return FALSE;
	has_options = (ee->ee_options != NULL);
	if (!xdr_bool(xdrs, &has_options))
		return FALSE;
	if (!has_options) {
		ee->ee_options = NULL;
		return TRUE;
	}
	return xdr_wrapstring(xdrs, &ee->ee_options);
}

bool_t
xdr_exportlist(XDR *xdrs, struct exportlist **elp)
{
	int freeing, more_elements;
	struct exportlist **next_elp;

	freeing = (xdrs->x_op == XDR_FREE);
	for (;;) {
		more_elements = (*elp != NULL);
		if (!xdr_bool(xdrs, &more_elements))
			return FALSE;
		if (!more_elements)
			return TRUE;
		if (freeing)
			next_elp = &(*elp)->el_next;
		if (!xdr_reference(xdrs, (char **) elp, sizeof **elp,
				   (xdrproc_t) xdr_exportentry)) {
			return FALSE;
		}
		elp = (freeing) ? next_elp : &(*elp)->el_next;
	}
}
#endif	/* !_KERNEL */
