/*
 * Copyright 1993, Silicon Graphics, Inc.
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

#ident "$Revision: 1.1 $"

#ifdef __STDC__
	#pragma weak ia_audit = _ia_audit
#endif

#include "synonyms.h"
#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <sat.h>


/*
 * The ia_audit module will send an I&A audit record
 * via satvwrite(), which takes care of ENOPKG concerns,
 * and allows a printf-like syntax.
 *
 * For consistency ia_audit will format the record body
 * with the following fields, each field separated by '|':
 *	1. program name.
 *	2. success or failure of the event. (+/-)
 *	3. username, UID, hostID, or hostname.
 *	4. rationale.
 *
 * For example the record body passed to satwrite looks like:
 *
 *  SU|+|root|Valid Password Obtained
 */
int
ia_audit(char *proginfo, char *username, int flag, char *rationale, ...)
{
	char *user;
	int status;

	/*
	 * Get the username used by the program.
	 * If not given, try to get the username from other sources.
	 */
	if (username != NULL && *username != '\0')
		user = username;
	else if ((user=cuserid(NULL)) == NULL) {
		user = "???";
	}

	if (flag) {
		status = satvwrite(SAT_AE_IDENTITY, SAT_SUCCESS,
		    "%s|+|%s|%s", proginfo, user, rationale);
	} else {
		status = satvwrite(SAT_AE_IDENTITY, SAT_FAILURE,
		    "%s|-|%s|%s", proginfo, user, rationale);
	}
  
	if (status < 0) {
		syslog(LOG_ERR|LOG_AUTH, "satwrite failure: %s", proginfo);
		return -1;
	}

	/* ia_audit successfully wrote the audit record */
	return(0);
}
