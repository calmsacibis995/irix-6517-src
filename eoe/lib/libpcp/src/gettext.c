/*
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Id: gettext.c,v 2.4 1997/09/12 03:17:42 markgw Exp $"

/*
 * Get help text from ndbm(3B) files (presumed to be built using newhelp)
 * -- in its own source file 'cause most folks don't want this, but PMDAs
 * would really like it
 */

#include <stdio.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"

extern int	errno;

/*
 * retrieve help text, ...
 *
 * result is NULL for fatal errors, else something the caller should
 * free()
 */
char *
__pmGetText(DBM *dbf, int ident, int type)
{
    datum	res;
    datum	key;
    __pmHelpKey	helpkey;
    char	*tp;

    if (dbf == NULL)
	return NULL;
    helpkey.ident = ident;
    helpkey.type = type;
    key.dptr = (char *)&helpkey;
    key.dsize = sizeof(__pmHelpKey);
    res = dbm_fetch(dbf, key);
    if (res.dptr == NULL)
	res.dsize = 0;		/* just in case */
    if ((tp = (char *)malloc(res.dsize+1)) == NULL) {
	__pmNotifyErr(LOG_WARNING, "__pmGetText: %s", strerror(errno));
	return NULL;
    }
    if (res.dptr != NULL)
	memcpy(tp, res.dptr, res.dsize);
    tp[res.dsize] = '\0';
    return tp;
}

