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
	#pragma weak getgrgid = _getgrgid
	#pragma weak getgrnam = _getgrnam
	#pragma weak setgrent = _setgrent
	#pragma weak endgrent = _endgrent
	#pragma weak getgrent = _getgrent
	#pragma weak getgrmember = _getgrmember
#endif
#endif


#include "synonyms.h"

#include <grp.h>
#include <ssdi.h>
#include <di_group.h>

extern _SSDI_VOIDFUNC __files_group_ssdi_funcs[];

/*
 * Initialized to compiled in sources, in default order
 */
static struct ssdisrcinfo SrcInfo[_SSDI_MAXSRCS] = {
	{ "files", __files_group_ssdi_funcs },
	{ "", NULL }

};

static	struct ssdisrcinfo 	*iterCurSrc = NULL;
static	struct ssdiconfiginfo 	ConfigInfo = _SSDI_InitConfigInfo;

#ifdef DI_GROUPDEBUG
int di_group_debug = 0;
#define dprintf(x) if (di_group_debug) printf x
#else
#define dprintf(x)
#endif

#define ISVALIDSRC(sip) \
	((SrcInfo <= (sip) && (sip) < &SrcInfo[_SSDI_MAXSRCS]))
void
setgrent()
{
	iterCurSrc = SrcInfo;
	_SSDI_INVOKE_VOID(iterCurSrc, DI_SETGRENT,DI_SETGRENT_SIG);
	dprintf(("di_setgrent->Switching to source %s\n", iterCurSrc->ssdi_si_name));
}

void
endgrent()
{
	if (ISVALIDSRC(iterCurSrc) && !_SSDI_ISEMPTYSRC(iterCurSrc))
		_SSDI_INVOKE_VOID(iterCurSrc, DI_ENDGRENT,DI_ENDGRENT_SIG);
	else  {

		/* iterCurSrc may be pointing at a null src,
		 * try the previous one which we may have stepped over
		 */
		iterCurSrc--;

		if (ISVALIDSRC(iterCurSrc) && !_SSDI_ISEMPTYSRC(iterCurSrc))
		_SSDI_INVOKE_VOID(iterCurSrc, DI_ENDGRENT,DI_ENDGRENT_SIG);
	}
	iterCurSrc = NULL;
}


struct group *
getgrent()
{
	struct group *grent = NULL;

	if (iterCurSrc == NULL)
		setgrent();

	while (!_SSDI_ISEMPTYSRC(iterCurSrc)) {
		if (grent = (struct group *) _SSDI_INVOKE(iterCurSrc, DI_GETGRENT,DI_GETGRENT_SIG))
			return grent;

		iterCurSrc++;
		if (_SSDI_ISEMPTYSRC(iterCurSrc))
			if (ssdi_get_config_and_load("group", &ConfigInfo, iterCurSrc))
				return NULL;

		_SSDI_INVOKE_VOID((iterCurSrc-1), DI_ENDGRENT,DI_ENDGRENT_SIG);

		dprintf(("di_setgrent->Switching to src %s\n", iterCurSrc->ssdi_si_name));
		_SSDI_INVOKE_VOID(iterCurSrc, DI_SETGRENT,DI_SETGRENT_SIG);
	}
	return(NULL);
}


struct group *
getgrnam(const char *name)
{
	struct ssdisrcinfo *cursrc;
	struct group *grent;

	cursrc = SrcInfo;
	while (!_SSDI_ISEMPTYSRC(cursrc)) {

		dprintf(("di_getgrnam->Trying src %s\n", cursrc->ssdi_si_name));

		if (grent=(struct group *)_SSDI_INVOKE1(cursrc,DI_GETGRBYNAME,DI_GETGRBYNAME_SIG,name))
			return grent;
		cursrc++;

		if (_SSDI_ISEMPTYSRC(cursrc))
			if (ssdi_get_config_and_load("group", &ConfigInfo, cursrc))
				return NULL;
	}
	return (NULL); /* no src could resolve the name */
}

struct group *
getgrgid(gid_t gid)
{
	struct ssdisrcinfo *cursrc;
	struct group *grent;

	cursrc = SrcInfo;
	while (!_SSDI_ISEMPTYSRC(cursrc)) {

		dprintf(("di_getgrgid->Trying src %s\n", cursrc->ssdi_si_name));

		if (grent = (struct group *)_SSDI_INVOKE1(cursrc,DI_GETGRBYGID,DI_GETGRBYGID_SIG,gid))
			return grent;
		cursrc++;

		if (_SSDI_ISEMPTYSRC(cursrc))
			if (ssdi_get_config_and_load("group", &ConfigInfo, cursrc))
				return NULL;
	}
	return (NULL); /* no src could resolve the gid */
}



/*
 * _getgrmem(uname, gid_array, maxgids, numgids):
 *      Private interface for initgroups().  It returns the group ids of
 *      groups of which the specified user is a member.
 *
 * Arguments:
 *   username   Username of the putative member
 *   gid_array  Space in which to return the gids.  The first [numgids]
 *              elements are assumed to already contain valid gids.
 *   maxgids    Maximum number of elements in gid_array.
 *   vgids    Number of elements (normally 0 or 1) that already contain
 *              valid gids.
 * Return value:
 *   number of valid gids in gid_array (may be zero)
 *      or
 *   -1 (and errno set appropriately) on errors (none currently defined)
 */

/* ARGSUSED */
int
getgrmember(const char *username, gid_t gid_array[], int maxgids, int vgids)
{
	struct ssdisrcinfo *cursrc;
	int ngids, sgids, validsrc;

	cursrc = SrcInfo;
	ngids = vgids;
	validsrc = 0; /* will be non-zero if some source supported the BYMEMBER function */

	while (!_SSDI_ISEMPTYSRC(cursrc)) {

		dprintf(("di_getgrmem->Trying src %s\n", cursrc->ssdi_si_name));

		if (cursrc->ssdi_si_funcs[DI_GETGRBYMEMBER])
			validsrc = 1;

		sgids = (int) _SSDI_INVOKE2(cursrc,DI_GETGRBYMEMBER,DI_GETGRBYMEMBER_SIG,username,&gid_array[ngids]);
		if (sgids > 0)
			ngids += sgids;

		cursrc++;

		if (_SSDI_ISEMPTYSRC(cursrc))
			if (ssdi_get_config_and_load("group", &ConfigInfo, cursrc))
				break;
	}
	return (validsrc ? ngids : -1);
}
