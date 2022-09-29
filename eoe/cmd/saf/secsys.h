/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SVC_SECSYS_H	/* wrapper symbol for kernel use */
#define _SVC_SECSYS_H	/* subject to change without notice */

#ident	"@(#)uts-comm:svc/secsys.h	1.7"

#ifdef	_KERNEL

#ifndef	_UTIL_TYPES_H
#include <util/types.h>	/* REQUIRED */
#endif

#else

#include <sys/types.h>	/* COMPATIBILITY */

#endif	/* _KERNEL */

/*
 * Commands for secsys system call.
 */

#define	ES_MACOPENLID	1	/* open LID file for kernel use */
#define	ES_MACSYSLID	2	/* assign LID to system processes */
#define	ES_MACROOTLID	3	/* assign LID to root fs root vnode */
#define	ES_PRVINFO	4	/* Get the privilege mechanism information */
#define	ES_PRVSETCNT	5	/* Get the privilege mechanism set count */
#define	ES_PRVSETS	6	/* Get the privilege mechanism sets */
#define	ES_MACADTLID	7	/* assign LID to audit daemon */
#define	ES_PRVID	8	/* Get the privileged ID number */
#define	ES_TPGETMAJOR	9	/* Get major device number of Trusted Path */


/*
 * Commands for secsys system call.
 */

#define	SA_EXEC		001	/* execute access */
#define	SA_WRITE	002	/* write access */
#define	SA_READ		004	/* read access */
#define	SA_SUBSIZE	010	/* get sub_attr size */

/*
 * Following is the attributes structure for an
 * object used by the secadvise() call.
 */

struct obj_attr {
	uid_t uid;
	gid_t gid;
	mode_t mode;
#ifdef sgi
	dev_t lid;
#else
	level_t lid;
#endif
	char filler[8];
};

/*
 * Following is the attributes structure for a
 * subject used by the secadvise() call.  Note
 * that this structure serves as a placeholder.
 */

struct sub_attr {
	char kernel_info[1];
};


#if defined(__STDC__) && !defined(_KERNEL)
int secsys(int, char *);
int secadvise(struct obj_attr *, int, struct sub_attr *);
#endif

#endif	/* _SVC_SECSYS_H */
