/*
 * errorhandler.h --
 *
 * 	Definition of the RPC library error handler.
 *
 * Copyright 1990, Silicon Graphics, Inc.
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

#ifndef __RPC_ERRORHANDLER_H__
#define __RPC_ERRORHANDLER_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.2 $"

#include <syslog.h>

extern void _rpc_errorhandler(int, const char *, ...);

/*
 * This stdarg(5) routine takes at least 2 arguments:
 *  - a priority, as defined in <syslog.h>,
 *  - a printf(3) format string without a trailing \n.
 * Additional arguments are printed under control of the format string.
 * The default routine in libsun prints error messages on stderr
 * or using syslog(3) if openlog(3) has been called:
 *
 *	{
 *	    extern int _using_syslog;
 *	    va_list ap;
 *	
 *	    va_start(ap, fmt);
 *	    if (_using_syslog) {
 *		vsyslog(pri, fmt, ap);
 *	    } else {
 *		vfprintf(stderr, fmt, ap);
 *		putc('\n', stderr);
 *	    }
 *	    va_end(ap);
 *	}
 *
 * For more sophisticated error handling (e.g., setjmp/longjmp), create 
 * your own version of this routine.
 */

#ifdef __cplusplus
}
#endif

#endif /* !__RPC_ERRORHANDLER_H__ */
