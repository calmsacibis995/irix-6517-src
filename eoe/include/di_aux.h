#ifndef __DI_AUX_H__
#define __DI_AUX_H__
#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.2 $"

/*
 *
 * Copyright 1995, Silicon Graphics, Inc.
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

#include <stdio.h>	/* for FILE definition 	*/
#include <grp.h> 	/* for gid_t definition */

/*
 * Database-specific ssdi interface ---> Number and position of database
 * interface routines. All srcs for aux should have the following
 * routines.
 */

#define DI_INITAUXGROUP			0
#define DI_NAUXFUNCS			1

typedef int (*DI_INITAUXGROUP_SIG)(const char *, gid_t, FILE *);

int initauxgroup(const char *, gid_t, FILE *);

#ifdef __cplusplus
}
#endif
#endif /* !__DI_AUX_H__ */
