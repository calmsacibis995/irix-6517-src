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

#ident "$Revision: 1.4 $"


#ifndef DSHLIB
#ifdef __STDC__
	#pragma weak getpwnam = __sgi_getpwnam
	#pragma weak getpwuid = __sgi_getpwuid
	#pragma weak setpwent = __sgi_setpwent
	#pragma weak endpwent = __sgi_endpwent
	#pragma weak getpwent = __sgi_getpwent
	#pragma weak _getpwuid = __sgi_getpwuid
	#pragma weak _getpwnam = __sgi_getpwnam
#endif
#endif


#include "synonyms.h"

#include <pwd.h>
#include <ssdi.h>
#include <di_passwd.h>

extern _SSDI_VOIDFUNC __files_passwd_ssdi_funcs[];

/*
 * Initialized to compiled in sources, in default order
 */
static struct ssdisrcinfo SrcInfo[_SSDI_MAXSRCS] = {
	{ "files", __files_passwd_ssdi_funcs },
	{ "", NULL }

};

static	struct ssdisrcinfo 	*iterCurSrc = NULL;
static	struct ssdiconfiginfo 	ConfigInfo = _SSDI_InitConfigInfo;

int _getpwent_no_ssdi = 0;

#ifdef DI_PASSWDDEBUG
int di_passwd_debug = 0;
#define dprintf(x) if (di_passwd_debug) printf x
#else
#define dprintf(x)
#endif

#define ISVALIDSRC(sip) \
	((SrcInfo <= (sip) && (sip) < &SrcInfo[_SSDI_MAXSRCS]))
	
void
setpwent()
{
	iterCurSrc = SrcInfo;
	_SSDI_INVOKE_VOID(iterCurSrc, DI_SETPWENT,DI_SETPWENT_SIG);
	dprintf(("di_setpwent->Switch to src %s\n", iterCurSrc->ssdi_si_name));
}

void
endpwent()
{
	if (ISVALIDSRC(iterCurSrc) && !_SSDI_ISEMPTYSRC(iterCurSrc))
		_SSDI_INVOKE_VOID(iterCurSrc, DI_ENDPWENT,DI_ENDPWENT_SIG);
	else  {

		/* iterCurSrc may be pointing at a null src,
		 * try the previous one which we may have stepped over
		 */
		iterCurSrc--;

		if (ISVALIDSRC(iterCurSrc) && !_SSDI_ISEMPTYSRC(iterCurSrc))
		_SSDI_INVOKE_VOID(iterCurSrc, DI_ENDPWENT,DI_ENDPWENT_SIG);
	}
	iterCurSrc = NULL;
}

	
struct passwd *
getpwent()
{
	struct passwd *pwent = NULL;
	extern int 		_getpwent_no_yp;

	if (iterCurSrc == NULL)
		setpwent();

	while (!_SSDI_ISEMPTYSRC(iterCurSrc)) {
		if (pwent = (struct passwd *) _SSDI_INVOKE(iterCurSrc, DI_GETPWENT,DI_GETPWENT_SIG))
			return pwent;

		iterCurSrc++;
		if (_SSDI_ISEMPTYSRC(iterCurSrc))
			if (_getpwent_no_ssdi || ssdi_get_config_and_load("passwd", &ConfigInfo, iterCurSrc))
				return NULL;

		_SSDI_INVOKE_VOID((iterCurSrc-1), DI_ENDPWENT,DI_ENDPWENT_SIG);

		dprintf(("di_setpwent->Switch to src %s\n", iterCurSrc->ssdi_si_name));
		_SSDI_INVOKE_VOID(iterCurSrc, DI_SETPWENT,DI_SETPWENT_SIG);
	}
	return(NULL);
}


struct passwd *
getpwnam(const char *name)
{
	struct ssdisrcinfo *cursrc;
	struct passwd *pwent;

	cursrc = SrcInfo;
	while (!_SSDI_ISEMPTYSRC(cursrc)) {

		dprintf(("di_getpwnam->Trying src %s\n", cursrc->ssdi_si_name));

		if (pwent=(struct passwd *)_SSDI_INVOKE1(cursrc,DI_GETPWBYNAME,DI_GETPWBYNAME_SIG,name))
			return pwent;
		cursrc++;

		if (_SSDI_ISEMPTYSRC(cursrc))
			if (_getpwent_no_ssdi || ssdi_get_config_and_load("passwd", &ConfigInfo, cursrc))
				return NULL;
	}
	return (NULL); /* no src could resolve the name */
}

struct passwd *
getpwuid(uid_t uid)
{
	struct ssdisrcinfo *cursrc;
	struct passwd *pwent;

	cursrc = SrcInfo;
	while (!_SSDI_ISEMPTYSRC(cursrc)) {

		dprintf(("di_getpwuid->Trying src %s\n", cursrc->ssdi_si_name));

		if (pwent = (struct passwd *)_SSDI_INVOKE1(cursrc,DI_GETPWBYUID,DI_GETPWBYUID_SIG,uid))
			return pwent;
		cursrc++;

		if (_SSDI_ISEMPTYSRC(cursrc))
			if (_getpwent_no_ssdi || ssdi_get_config_and_load("passwd", &ConfigInfo, cursrc))
				return NULL;
	}
	return (NULL); /* no src could resolve the uid */
}
