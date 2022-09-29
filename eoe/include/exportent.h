#ifndef __EXPORTENT_H__
#define __EXPORTENT_H__
#ident "$Revision: 1.9 $"
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
/*	@(#)exportent.h	1.1 88/03/15 4.0NFSSRC SMI
 *
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 *  1.5 88/02/07 (C) 1986 SMI
 */
#include <stdio.h>

/*
 * Exported file system table, see exportent(3)
 */ 

#define TABFILE "/etc/xtab"		/* where the table is kept */

/*
 * Options keywords
 */
#define ACCESS_OPT	"access"	/* machines that can mount fs */
#define ROOT_OPT	"root"		/* machines with root access of fs */
#define RO_OPT		"ro"		/* export read-only */
#define RW_OPT		"rw"		/* export read-mostly */
#define ANON_OPT	"anon"		/* uid for anonymous requests */
#define	NOHIDE_OPT	"nohide"	/* visible from upper exported fs */
#define	WSYNC_OPT	"wsync"		/* write synchronously to disk */
#define	B32CLNT_OPT	"32bitclients"	/* mask off high 32 bits in cookie3 */
#define	NOXATTR_OPT	"noxattr"	/* only trusted clients may access
					   extended attributes */

struct exportent {
	char *xent_dirname;	/* directory (or file) to export */
	char *xent_options;	/* options, as above */
};

#ifdef __cplusplus
extern "C" {
#endif

extern FILE *setexportent(void);
extern void endexportent(FILE *);
extern int remexportent(FILE *, char *);
extern int addexportent(FILE *, char *, char *);
extern char *getexportopt(struct exportent *, char *);
extern struct exportent *getexportent(FILE *);

#ifdef __cplusplus
}
#endif
#endif /* !__EXPORTENT_H__ */
