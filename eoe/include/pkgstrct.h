#ifndef __PKGSTRCT_H__
#define __PKGSTRCT_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.4 $"
/*
*
* Copyright 1992, Silicon Graphics, Inc.
* All Rights Reserved.
*
* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
* the contents of this file may not be disclosed to third parties, copied or
* duplicated in any form, in whole or in part, without the prior written
* permission of Silicon Graphics, Inc.
*
* RESTRICTED RIGHTS LEGEND:
* Use, duplication or disclosure by the Government is subject to restrictions
* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
* and Computer Software clause at DFARS 252.227-7013, and/or in similar or
* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
* rights reserved under the Copyright Laws of the United States.
*/
/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#define CLSSIZ	12
#define PKGSIZ	14
#define ATRSIZ	14
#define PRIVSIZ 255

#define BADFTYPE	'?'
#define BADMODE		0
#define BADOWNER	"?"
#define BADGROUP	"?"
#define	BADMAC		0
#define	BADPRIV		"?"
#define BADID		0
#define BADMAJOR	0
#define BADMINOR	0
#define BADCLASS	"none"
#define BADINPUT	1 /* not EOF */
#define BADCONT		(-1L)
	
extern char	*errstr;
	
struct ainfo {
	char	*local;
	long	mode;
	char	owner[ATRSIZ+1];
	char	group[ATRSIZ+1];
	major_t	major;
	minor_t	minor;
};

struct cinfo {
	long	cksum;
	long	size;
	long	modtime;
};

struct pinfo {
	char	status;
	char	pkg[PKGSIZ+1];
	char	editflag;
	char	aclass[ATRSIZ+1];
	struct pinfo	
		*next;
};

struct cfent {
	short	volno;
	char	ftype;
	char	class[CLSSIZ+1];
	char	*path;
	struct ainfo ainfo;
	struct cinfo cinfo;
	short	npkgs;
	short	quoted;
	struct pinfo	
		*pinfo;
};

/* averify() & cverify() error codes */
#define	VE_EXIST	0x0001
#define	VE_FTYPE	0x0002
#define	VE_ATTR		0x0004
#define	VE_CONT		0x0008
#define	VE_FAIL		0x0010
#define VE_TIME		0x0020
#ifdef __cplusplus
}
#endif
#endif /* !__PKGSTRCT_H__ */
