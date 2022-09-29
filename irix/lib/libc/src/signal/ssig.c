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
#include "synonyms.h"
#include <signal.h>
#include <errno.h>

/*
 * SysV blocks all signals while handling a signal -> sa_mask = emptyset
 * By default, SysV doesn't want SIGCLD when a child stop/continue. 
 * By default, SysV doesn't zombify chldren if parent ignore SIGCLD.
 */
int
_ssig(int sig, sigaction_t *sa, sigaction_t *osa)
{
	sigemptyset(&sa->sa_mask);
	if (sig == SIGCLD) {
		sa->sa_flags |= SA_NOCLDSTOP;
		if (sa->sa_handler == SIG_IGN)
			sa->sa_flags |= SA_NOCLDWAIT;
	}
	return(sigaction(sig, sa, osa));
}
