#ifndef __DI_PASSWD_H__
#define __DI_PASSWD_H__
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
#include <pwd.h>

#define DI_GETPWBYNAME		0
#define DI_GETPWBYUID		1
#define DI_SETPWENT		2
#define DI_GETPWENT		3
#define DI_ENDPWENT		4
#define DI_NPWFUNCS		5

typedef struct passwd *(*DI_GETPWBYNAME_SIG)(const char *);
typedef struct passwd *(*DI_GETPWBYUID_SIG)(uid_t uid);
typedef void 	       (*DI_SETPWENT_SIG)(void);
typedef struct passwd *(*DI_GETPWENT_SIG)(void);
typedef void 	       (*DI_ENDPWENT_SIG)(void);

extern int _getpwent_no_ssdi;

#ifdef __cplusplus
}
#endif
#endif /* !__DI_PASSWD_H__ */
