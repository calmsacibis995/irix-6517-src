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
        #pragma weak sigwait = _sigwait
#endif
#include "synonyms.h"
#include <signal.h>
#include <errno.h>
#include "sig_extern.h"
#include "mplib.h"

int
sigwait(const sigset_t *set, int *sig)
{
	int retval;

	MTLIB_RETURN( (MTCTL_SIGWAIT, set, sig) );

	if ((retval = sigpoll(set, 0, 0)) == -1)
		return (oserror());
	if (sig)
		*sig = retval;
	return (0);
}
	

