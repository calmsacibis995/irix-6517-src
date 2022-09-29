/*
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
#ifdef __STDC__
        #pragma weak sigtimedwait = _sigtimedwait
#endif
#define _POSIX_4SOURCE
#include "synonyms.h"
#include <signal.h>
#include "sig_extern.h"
#include "mplib.h"

int
sigtimedwait(const sigset_t *set, struct siginfo *info, const struct timespec *ts)
{
	MTLIB_RETURN( (MTCTL_SIGTIMEDWAIT, set, info, ts) );
	return sigpoll(set, info, ts);
}
	

