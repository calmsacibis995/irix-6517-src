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
        #pragma weak sigpause = _sigpause
#endif
#include "synonyms.h"
#include "mplib.h"

#include <signal.h>
#include <errno.h>
#include "sigcompat.h"

int
sigpause(int sig)
{
        sigset_t set;
	const sigset_t cantmask = { sigmask(SIGKILL)|sigmask(SIGSTOP) };
	int error, wasblked = 0;

	MTLIB_CNCL_TEST();

        if (sig <= 0 || sig >= NSIG || sigismember(&cantmask, sig)) {
                setoserror(EINVAL);
                return(-1);
        }
        (void) sigprocmask(SIG_NOP, (sigset_t *)0, &set);
	if (sigismember(&set, sig))
		wasblked++;
        sigdelset(&set, sig);
        error = sigsuspend(&set);

	/* sigsuspend restores mask, but sigpause doesn't */
	if (wasblked) {
		sigemptyset(&set);
		sigaddset(&set, sig);
		(void) sigprocmask(SIG_UNBLOCK, &set, (sigset_t *)0);
	}

	return error;
}

