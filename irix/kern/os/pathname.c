/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
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

/*#ident	"@(#)uts-comm:fs/pathname.c	1.2"*/
#ident	"$Revision: 1.20 $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/cred.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/pathname.h>
#include <sys/sema.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/sat.h>

/*
 * Pathname utilities.
 *
 * In translating file names we copy each argument file
 * name into a pathname structure where we operate on it.
 * Each pathname structure can hold MAXPATHLEN characters
 * including a terminating null, and operations here support
 * allocating and freeing pathname structures, fetching
 * strings from user space, getting the next character from
 * a pathname, combining two pathnames (used in symbolic
 * link processing), and peeling off the first component
 * of a pathname.
 */

/*
 * Allocate contents of pathname structure.  Structure is typically
 * an automatic variable in calling routine for convenience.
 *
 * May sleep in the call to kmem_alloc() and so must not be called
 * from interrupt level.
 */
void
pn_alloc(pathname_t *pnp)
{
	pnp->pn_buf = (char *)kmem_zone_alloc(pn_zone, KM_SLEEP);
	pnp->pn_path = pnp->pn_buf;
	pnp->pn_pathlen = 0;
}

/*
 * Free pathname resources.
 */
void
pn_free(pathname_t *pnp)
{
	kmem_zone_free(pn_zone, pnp->pn_buf);
	pnp->pn_buf = 0;
}

/*
 * Pull a path name from user or kernel space.  Allocates storage (via
 * pn_alloc()) to hold it.
 */
int
pn_get(char *str, enum uio_seg seg, pathname_t *pnp)
{
	register int error;
	size_t pathlen;

	pn_alloc(pnp);
	error = ((seg == UIO_USERSPACE) ? copyinstr : copystr)
		(str, pnp->pn_path, MAXPATHLEN, &pathlen);
	if (error)
		pn_free(pnp);
	pnp->pn_pathlen = pathlen - 1;		/* don't count null byte */
	return error;
}

/*
 * Set path name to argument string.  Storage has already been allocated
 * and pn_buf points to it.
 *
 * On error, all fields except pn_buf will be undefined.
 */
int
pn_set(pathname_t *pnp, char *path)
{
	register int error;
	size_t pathlen;

	pnp->pn_path = pnp->pn_buf;
	error = copystr(path, pnp->pn_path, MAXPATHLEN, &pathlen);
	pnp->pn_pathlen = pathlen - 1;		/* don't count null byte */
	return error;
}

/*
 * Combine two argument path names by putting the second argument before
 * the first in the first's buffer, and freeing the second argument.
 * This isn't very general: it is designed specifically for symbolic
 * link processing.
 */
int
pn_insert(pathname_t *pnp, pathname_t *sympnp)
{

	if (pnp->pn_pathlen + sympnp->pn_pathlen >= MAXPATHLEN)
		return ENAMETOOLONG;
	ovbcopy(pnp->pn_path, pnp->pn_buf + sympnp->pn_pathlen,
	  pnp->pn_pathlen);
	bcopy(sympnp->pn_path, pnp->pn_buf, sympnp->pn_pathlen);
	pnp->pn_pathlen += sympnp->pn_pathlen;
	pnp->pn_buf[pnp->pn_pathlen] = '\0';
	pnp->pn_path = pnp->pn_buf;
	return 0;
}

int
pn_getsymlink(vnode_t *vp, pathname_t *pnp, cred_t *crp)
{
	struct iovec aiov;
	struct uio auio;
	register int error;

	aiov.iov_base = pnp->pn_buf;
	aiov.iov_len = MAXPATHLEN;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = 0;
	auio.uio_fmode = 0;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_resid = MAXPATHLEN;
	auio.uio_pmp = NULL;
	auio.uio_pio = 0;
	auio.uio_readiolog = 0;
	auio.uio_writeiolog = 0;
	auio.uio_pbuf = 0;
	VOP_READLINK(vp, &auio, crp, error);
	if (!error)
		pnp->pn_pathlen = MAXPATHLEN - auio.uio_resid;
	return error;
}


/*
 * When pathnames are stored in user-visible attributes, they can
 * be retrieved using pn_getpathattr.
 */
int
pn_getpathattr(vnode_t *vp, char *attr, pathname_t *pnp, cred_t *crp)
{
	register int error;
	int buf_size = MAXPATHLEN;

	VOP_ATTR_GET(vp, attr, pnp->pn_buf, &buf_size, 0, crp, error);
	if (error == 0)
		pnp->pn_pathlen = buf_size;

	return error;
}


/*
 * Compute the same hash function for name that pn_getcomponent would if
 * it were a component in a pathname.
 */
#define PN_HASH_ACCUM(hash,c)	((hash) = ((hash) << 1) ^ (c))

u_long
pn_hash(char *name, int len)
{
	u_long hash;

	for (hash = 0; --len >= 0; name++)
		PN_HASH_ACCUM(hash, *name);
	return hash;
}

/*
 * Get next component from a path name and leave in
 * buffer "component" which should have room for
 * MAXNAMELEN bytes (including a null terminator character).
 * If PN_PEEK is set in flags, just peek at the component,
 * i.e., don't strip it out of pnp.
 */
int
pn_getcomponent(pathname_t *pnp, char *component, int flags)
{
	register char *cp, *bp;
	register int l, n;
	register u_long hash;

	cp = pnp->pn_path;
	l = pnp->pn_pathlen;
	n = MAXNAMELEN - 1;
	hash = 0;
	while (l > 0 && *cp != '/') {
		if (--n < 0)
			return ENAMETOOLONG;
		PN_HASH_ACCUM(hash, *cp);
		*component++ = *cp++;
		--l;
	}
	if ((flags & PN_PEEK) == 0) {
		bp = pnp->pn_path;
		pnp->pn_path = cp;
		pnp->pn_pathlen = l;
		pnp->pn_hash = hash;
		pnp->pn_complen = n = cp - bp;
		if (n == 1 && *bp == '.')
			pnp->pn_flags = PN_ISDOT;
		else if (n == 2 && bp[0] == '.' && bp[1] == '.')
			pnp->pn_flags = PN_ISDOTDOT;
		else
			pnp->pn_flags = 0;
	}
	*component = 0;
	return 0;
}

/*
 * Skip over consecutive slashes in the path name.
 */
void
pn_skipslash(pathname_t *pnp)
{
	while (pnp->pn_pathlen > 0 && *pnp->pn_path == '/') {
		pnp->pn_path++;
		pnp->pn_pathlen--;
	}
}

/*
 * Sets pn_path to the last component in the pathname, updating
 * pn_pathlen.  If pathname is empty, or degenerate, leaves pn_path
 * pointing at NULL char.  The pathname is explicitly null-terminated
 * so that any trailing slashes are effectively removed.
 */
void
pn_setlast(pathname_t *pnp)
{
	register char *buf = pnp->pn_buf;
	register char *path = pnp->pn_path + pnp->pn_pathlen;
	register char *endpath;

	while (--path > buf && *path == '/')
		;
	endpath = ++path;
	while (--path > buf && *path != '/')
		;
	if (*path == '/')
		path++;
	*endpath = '\0';
	pnp->pn_path = path;
	pnp->pn_pathlen = endpath - path;
}

/*
 * Eliminate any trailing slashes in the pathname.
 */
void
pn_fixslash(pathname_t *pnp)
{
	register char *buf = pnp->pn_buf;
	register char *path = pnp->pn_path + pnp->pn_pathlen;

	while (--path > buf && *path == '/')
		;
	*++path = '\0';
	pnp->pn_pathlen = path - pnp->pn_path;
}

/*
 * Set up a single component in a pathname structure.
 */
void
pn_setcomponent(
	pathname_t	*pnp,
        char            *path,
        int             len)
{
        int             flags = 0;
	u_long          hash;

        pnp->pn_buf = path;
	pnp->pn_path = path;
        pnp->pn_pathlen = len;
        pnp->pn_complen = len;
	if (*path == '.') {
		if (len == 1) 
			flags = PN_ISDOT;
		else if (len == 2 && path[1] == '.')
			flags = PN_ISDOTDOT;
	}
	pnp->pn_flags = flags;
	for (hash = 0; --len >= 0; path++)
		PN_HASH_ACCUM(hash, *path);
	pnp->pn_hash = hash;
}
