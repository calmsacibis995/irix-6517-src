#ifndef __DI_GROUP_H__
#define __DI_GROUP_H__
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

/*
 * Database-specific ssdi interface ---> Number and position of database
 * interface routines. All srcs for passwd should have the following
 * routines.
 */

#define DI_GETGRBYNAME		0
#define DI_GETGRBYGID		1
#define DI_SETGRENT		2
#define DI_GETGRENT		3
#define DI_ENDGRENT		4
#define DI_GETGRBYMEMBER	5
#define DI_NGRFUNCS		6

typedef struct group *(*DI_GETGRBYNAME_SIG)(const char *);
typedef struct group *(*DI_GETGRBYGID_SIG)(gid_t);
typedef void	      (*DI_SETGRENT_SIG)(void);
typedef struct group *(*DI_GETGRENT_SIG)(void);
typedef void	      (*DI_ENDGRENT_SIG)(void);
typedef int	      (*DI_GETGRBYMEMBER_SIG)(const char *, gid_t[]);

#ifdef __cplusplus
}
#endif
#endif /* !__DI_GROUP_H__ */
