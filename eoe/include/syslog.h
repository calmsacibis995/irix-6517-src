#ifndef __SYSLOG_H__
#define __SYSLOG_H__
#ident "$Revision: 1.13 $"
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

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/syslog.h>

extern void openlog(const char *, int, int);
extern void syslog(int, const char *, ...);
extern void closelog(void);
extern int setlogmask(int);
#if _SGIAPI
#ifdef __STDC__
#include <stdarg.h>
extern void vsyslog(int, const char *, va_list);
#else
extern void vsyslog();
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* !__SYSLOG_H__ */
