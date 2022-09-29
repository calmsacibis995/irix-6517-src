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

#ifndef _FS_PATHNAME_H	/* wrapper symbol for kernel use */
#define _FS_PATHNAME_H	/* subject to change without notice */

#ifdef __cplusplus
extern "C" {
#endif

/*#ident	"@(#)uts-3b2:fs/pathname.h	1.2"*/
#ident	"$Revision: 1.5 $"

/*
 * Pathname structure.
 * System calls that operate on path names gather the path name
 * from the system call into this structure and reduce it by
 * peeling off translated components.  If a symbolic link is
 * encountered the new path name to be translated is also
 * assembled in this structure.
 *
 * By convention pn_buf is not changed once it's been set to point
 * to the underlying storage; routines which manipulate the pathname
 * do so by changing pn_path and pn_pathlen.  pn_pathlen is redundant
 * since the path name is null-terminated, but is provided to make
 * some computations faster.
 */

#include <sys/types.h>
#include <sys/uio.h>

typedef struct pathname {
	char	*pn_buf;		/* underlying storage */
	char	*pn_path;		/* remaining pathname */
	u_int	pn_pathlen;		/* remaining length */
	u_long	pn_hash;		/* last component's hash */
	u_short	pn_complen;		/* last component length */
	u_short	pn_flags;		/* flags, see below */
} pathname_t;

#define PN_STRIP	0	/* Strip next component from pn */
#define PN_PEEK		1	/* Only peek at next component of pn */
#define pn_peekcomponent(pnp, comp) pn_getcomponent(pnp, comp, PN_PEEK)
#define pn_stripcomponent(pnp, comp) pn_getcomponent(pnp, comp, PN_STRIP)

#define pn_peekchar(pnp)	((pnp)->pn_pathlen > 0 ? *((pnp)->pn_path) : 0)
#define pn_pathleft(pnp)	((pnp)->pn_pathlen)

/* XXXbe replace these with precomputed flags in struct pathname */
#define ISDOT(nm)	((nm)[0] == '.' && (nm)[1] == '\0')
#define ISDOTDOT(nm)	((nm)[0] == '.' && (nm)[1] == '.' && (nm)[2] == '\0')

/* pathname flags */
#define PN_ISDOT	0x1
#define PN_ISDOTDOT	0x2

struct vnode;
struct cred;
enum uio_seg;
extern void	pn_alloc(pathname_t *);		/* allocate pathname buffer */
extern int	pn_get(char *, enum uio_seg, pathname_t *);
extern int	pn_set(pathname_t *, char *);
extern int	pn_insert(pathname_t *, pathname_t *);
extern int	pn_getsymlink(struct vnode *, pathname_t *, struct cred *);
extern int	pn_getpathattr(struct vnode *, char *, pathname_t *pnp, struct cred *);
extern u_long	pn_hash(char *, int);
extern int	pn_getcomponent(pathname_t *, char *, int);
extern void	pn_setcomponent(pathname_t *, char *, int);
extern void	pn_setlast(pathname_t *);	/* isolate last component */
extern void	pn_skipslash(pathname_t *);	/* skip over slashes */
extern void	pn_fixslash(pathname_t *);	/* eliminate trailing slashes */
extern void	pn_free(pathname_t *);		/* free pathname buffer */

#ifdef __cplusplus
}
#endif

#endif	/* _FS_PATHNAME_H */
