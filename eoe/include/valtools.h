#ifndef __VALTOOLS_H__
#define __VALTOOLS_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.3 $"
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


struct _choice_ {
	char *token;
	char *text;
	struct _choice_ *next;
};

struct _menu_ {
	char	*label;
	int	attr;
	short	longest;
	short	nchoices;
	struct _choice_ 
		*choice;
	char	**invis;
};

typedef struct _menu_ CKMENU;

#define P_ABSOLUTE	0x0001
#define P_RELATIVE	0x0002
#define P_EXIST		0x0004
#define P_NEXIST	0x0008
#define P_REG		0x0010
#define P_DIR		0x0020
#define P_BLK		0x0040
#define P_CHR		0x0080
#define P_NONZERO	0x0100
#define P_READ		0x0200
#define P_WRITE		0x0400
#define P_EXEC		0x0800
#define P_CREAT		0x1000

#define CKUNNUM		0x01
#define CKALPHA		0x02
#define CKONEFLAG	0x04
#ifdef __cplusplus
}
#endif
#endif /* !__VALTOOLS_H__ */
