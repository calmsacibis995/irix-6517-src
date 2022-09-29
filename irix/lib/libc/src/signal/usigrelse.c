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
        #pragma weak sigrelse = _sigrelse
#endif
#include "synonyms.h"

#include <signal.h>
#include <errno.h>

#define MAXBITNO (32)
#define sigword(n) ((n-1)/MAXBITNO)
/* bit mask position within a word, to be used with sigword() or word 0 */
#undef sigmask	/* make sure local sigmask is used */
#define sigmask(n) (1L<<((n-1)%MAXBITNO))

int
sigrelse(int sig)
{
        sigset_t nset;
	const sigset_t cantmask = { sigmask(SIGKILL)|sigmask(SIGSTOP) };

        if (sig <= 0 || sig >= NSIG || sigismember(&cantmask, sig)) {
                setoserror(EINVAL);
                return(-1);
        }
        sigemptyset(&nset);
        sigaddset(&nset, sig);
        return(sigprocmask(SIG_UNBLOCK, &nset, 0));
}
