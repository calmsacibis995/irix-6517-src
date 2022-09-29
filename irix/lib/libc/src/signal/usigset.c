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
        #pragma weak sigset = _sigset
#endif
#include "synonyms.h"

#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include "sigcompat.h"
#include "sig_extern.h"

void (*
sigset(int sig, void (*func)()))()
{
        sigaction_t osa, sa;
        sigset_t set;
        void (*ofunc)();
	const sigset_t cantmask = { sigmask(SIGKILL)|sigmask(SIGSTOP) };

        if (sig <= 0 || sig >= NSIG || sigismember(&cantmask, sig)) {
                setoserror(EINVAL);
                return(SIG_ERR);
        }

	/*
	 * NOTE: SVR4 permits POSIX semantics of SIGCONT unlike
	 * IRIX4 - so there are no checks for SIG_IGN or SIG_HOLD
	 * anymore
	 */
        if (func == SIG_HOLD) {
		sigset_t oset;

                sigemptyset(&set);
                sigaddset(&set, sig);
                if (sigprocmask(SIG_BLOCK, &set, &oset))
                        return(SIG_ERR);
		/* get the old sig handler */
		if (sigismember(&oset, sig))
			ofunc = SIG_HOLD;
		else {
			(void) sigaction(sig, 0, &sa);
			ofunc = sa.sa_handler;
		}
                return(ofunc);
        }

        /* get the old sig mask */
        if (sigprocmask(SIG_NOP, (sigset_t *)0, &set))
                return(SIG_ERR);
	ofunc = NULL;
        if (sigismember(&set, sig))
                ofunc = SIG_HOLD;

        /* set the new sig handler */
        sa.sa_flags = 0;
        sa.sa_handler = func;
        if (_ssig(sig, &sa, ofunc ? 0 : &osa))
                return(SIG_ERR);
	if (ofunc == NULL)
		ofunc = osa.sa_handler;
	else {
		/* was held - must release */
                sigemptyset(&set);
                sigaddset(&set, sig);
		(void) sigprocmask(SIG_UNBLOCK, &set, 0);
	}
        return(ofunc);
}
