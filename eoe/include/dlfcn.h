#ifndef __DLFCN_H__
#define __DLFCN_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.12 $"
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
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
#include <standards.h>
/* declarations used for dynamic linking support routines */
extern void *dlopen(const char *, int);
extern void *dlsym(void *, const char *);
extern int dlclose(void *);
extern char *dlerror(void);
#if _SGIAPI
extern void *sgidladd(const char *, int);
extern void *sgidlopen_version(const char *, int, const char *, int);
extern char *sgigetdsoversion(const char *);
#endif /* _SGIAPI */

/* valid values for mode argument to dlopen/sgidladd/sgidlopen_version */
#define RTLD_LAZY	1	/* lazy function call binding */
#define RTLD_NOW	2	/* immediate function call binding */
#define RTLD_GLOBAL	4	/* symbols in this dlopen'ed obj */
				/* are visible to other dlopen'ed objs */

#define RTLD_NOW_REPORT_ERROR	8	/* this is for sgidladd only -- */
					/* RTLD_NOW treated like 
					 *   RTLD_LAZY in sgidladd and
					 * RTLD_NOW_REPORT_ERROR
					 *   reports  all unresolved 
					 *   symbols including TEXT
					 */

#define RTLD_LOCAL      0       /* RTLD_LOCAL is the opposite of */
				/* RTLD_GLOBAL, and RTLD_LOCAL
			 	 * is the default in case 
				 * neither is specified.
			         * RTLD_LOCAL means that the symbols
			         * in the dlopen()ed DSO are not
				 * made globally visible.
			         */


#ifdef __cplusplus
}
#endif
#endif /* !__DLFCN_H__ */
