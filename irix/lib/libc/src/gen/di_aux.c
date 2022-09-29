/*
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

#ident "$Revision: 1.3 $"

#ifndef DSHLIB
#ifdef __STDC__
	#pragma weak initauxgroup = _initauxgroup
#endif
#endif


#include "synonyms.h"

#include <grp.h>
#include <ssdi.h>
#include <di_aux.h> 

/*
 * We just define a dummy files backend, which does not support any functions
 */
static _SSDI_VOIDFUNC files_aux_ssdi_funcs[] = {
	(_SSDI_VOIDFUNC) NULL 		/* No INITAUXGROUP function in files */
};

/*
 * Initialized to compiled in sources, in default order
 */
static struct ssdisrcinfo SrcInfo[_SSDI_MAXSRCS] = {
	{ "files", files_aux_ssdi_funcs },
	{ "", NULL }

};

static	struct ssdiconfiginfo 	ConfigInfo = _SSDI_InitConfigInfo;

#ifdef DI_AUXDEBUG
int di_aux_debug = 0;
#define dprintf(x) if (di_aux_debug) printf x
#else
#define dprintf(x)
#endif

int
initauxgroup(const char *user, gid_t gid, FILE *display)
{
	struct ssdisrcinfo *cursrc;
	int	result;

	cursrc = SrcInfo;
	while (!_SSDI_ISEMPTYSRC(cursrc)) {

		dprintf(("di_initauxgroup->Trying source %s\n", cursrc->ssdi_si_name));
		if (result=(int)_SSDI_INVOKE3(cursrc,DI_INITAUXGROUP,DI_INITAUXGROUP_SIG,user,gid,display))
			return result;
		cursrc++;

		if (_SSDI_ISEMPTYSRC(cursrc))
			if (ssdi_get_config_and_load("aux",&ConfigInfo,cursrc))
				return NULL;
	}
	/* No src could resolve the name  or no valid srcs, assume success*/
	return (NULL);
}
