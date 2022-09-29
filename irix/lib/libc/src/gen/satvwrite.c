/*
 * Copyright 1993, Silicon Graphics, Inc.
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

#ident "$Revision: 1.4 $"

#ifdef __STDC__
	#pragma weak satvwrite = _satvwrite
#endif

#include "synonyms.h"
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syssgi.h>
#include <syslog.h>
#include <errno.h>
#include <sat.h>

/*
 * varargs interface to satwrite.  Useful since data is a text string,
 * often built on the fly.
 */
int
satvwrite(int rectype, int outcome, char *format, ...)
{
	static int state;
	int nchars;
	char buf[SAT_MAX_USER_REC];
	va_list args;

	if (state == 0)
		state = (sysconf(_SC_AUDIT) > 0) ? 1 : -1;

	if (state < 0)
		return 0;

	va_start(args, format);
	nchars = vsprintf(buf, format, args);
	va_end(args);

	return satwrite(rectype, outcome, buf, (unsigned int) nchars+1);
}
