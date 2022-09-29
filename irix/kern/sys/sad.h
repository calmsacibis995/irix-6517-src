/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _IO_SAD_SAD_H	/* wrapper symbol for kernel use */
#define _IO_SAD_SAD_H	/* subject to change without notice */

#ident	"@(#)uts-3b2:io/sad/sad.h	1.6"

/*
 * Streams Administrative Driver
 */

#ifdef _KERNEL
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/stream.h>
#endif /* _KERNEL */

/*
 *  ioctl defines
 */
#define	SADIOC		('D'<<8)
#define SAD_SAP		(SADIOC|01)	/* set autopush */
#define SAD_GAP		(SADIOC|02)	/* get autopush */
#define SAD_VML		(SADIOC|03)	/* validate module list */

/*
 * Device naming and numbering conventions.
 */
#define USERDEV "/dev/sad/user"
#define ADMINDEV "/dev/sad/admin"

#define USRMIN 0
#define ADMMIN 1

/*
 * The maximum modules you can push on a stream using
 * the autopush feature.  This should be less than NSTRPUSH.
 */
#define MAXAPUSH	8

/*
 * autopush info common to user and kernel
 */
struct apcommon {
	uint	apc_cmd;		/* command (see below) */
	long	apc_major;		/* major # of device */
	long	apc_minor;		/* minor # of device */
	long	apc_lastminor;		/* last minor for range */
	uint	apc_npush;		/* number of modules to push */
};

/*
 * ap_cmd: various flavors of autopush
 */
#define SAP_CLEAR	0		/* remove configuration list */
#define SAP_ONE		1		/* configure one minor device */
#define SAP_RANGE	2		/* configure range of minor devices */
#define SAP_ALL		3		/* configure all minor devices */

/*
 * format for autopush ioctls
 */
struct strapush {
	struct apcommon sap_common;				/* see above */
	char		sap_list[MAXAPUSH][FMNAMESZ + 1];	/* module list */
};

#define sap_cmd		sap_common.apc_cmd
#define sap_major	sap_common.apc_major
#define sap_minor	sap_common.apc_minor
#define sap_lastminor	sap_common.apc_lastminor
#define sap_npush	sap_common.apc_npush

#ifdef _KERNEL

#if _MIPS_SIM == _ABI64
/*
 * autopush info common to kernel for 32 bit ABIs
 */
struct apcommon32 {
	app32_uint_t	apc_cmd;		/* command (see below) */
	app32_long_t	apc_major;		/* major # of device */
	app32_long_t	apc_minor;		/* minor # of device */
	app32_long_t	apc_lastminor;		/* last minor for range */
	app32_uint_t	apc_npush;		/* number of modules to push */
};

/*
 * format for autopush ioctls
 */
struct strapush32 {
	struct apcommon32 sap_common;
	char		  sap_list[MAXAPUSH][FMNAMESZ + 1];
};

#endif

/*
 * state values for ioctls
 */
#define GETSTRUCT	1L
#define GETRESULT	2L
#define GETLIST		3L

struct saddev {
	queue_t	*sa_qp;		/* pointer to read queue */
	caddr_t	 sa_addr;	/* saved address for copyout */
	int	 sa_flags;	/* see below */
};

/*
 * values for saddev flags field.
 */
#define SADPRIV		0x01

/*
 * Module Autopush Cache
 */
struct autopush {
	struct autopush	*ap_nextp;		/* next on list */
	int		 ap_flags;		/* see below */
	struct apcommon  ap_common;		/* see above */
	ushort		 ap_list[MAXAPUSH];	/* list of modules to push */
						/* (indices into fmodsw array) */
};

/*
 * The command issued by the user ultimately becomes
 * the type of the autopush entry.  Therefore, occurrences of
 * "type" in the code refer to an existing autopush entry.
 * Occurrences of "cmd" in the code refer to the command the
 * user is currently trying to complete.  types and cmds take
 * on the same values.
 */
#define ap_type		ap_common.apc_cmd
#define ap_major	ap_common.apc_major
#define ap_minor	ap_common.apc_minor
#define ap_lastminor	ap_common.apc_lastminor
#define ap_npush	ap_common.apc_npush

/*
 * autopush flag values
 */
#define APFREE	0x00	/* free */
#define APUSED	0x01	/* used */
#define APHASH	0x02	/* on hash list */

/*
 * hash function for cache
 */
#define strphash(maj)	strpcache[(((int)maj)&strpmask)]
extern struct autopush *strpcache[];	/* autopush hash list, from sad.cf */
extern int strpmask;			/* hash function mask, from sad.cf */

#endif /* _KERNEL */

#endif	/* _IO_SAD_SAD_H */
