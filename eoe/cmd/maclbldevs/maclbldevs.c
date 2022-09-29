/*
 * maclbldev.c
 * 	Set the labels of user devices
 *
 * Copyright 1991, Silicon Graphics, Inc.
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

#include <sys/types.h>
#include <sys/mac.h>
#include <pwd.h>

/*
 * Usage:
 *	maclbldevs uid mac
 */
int
main(int argc, char *argv[])
{
	struct passwd *pwent;
	mac_t pmac;
	uid_t uid;

	if (argc != 3) {
		fprintf(stderr, "%s usage: %s uid mac\n",
		    argv[0], argv[0]);
		return(1);
	}
	if ((pwent = getpwnam(argv[1])) != NULL)
		uid = pwent->pw_uid;
	else if (isdigit(*argv[1]))
		uid = atoi(argv[1]);
	else {
		fprintf(stderr, "%s: Invalid uid specification \"%s\"\n",
		    argv[0], argv[1]);
		return(1);
	}
	if ((pmac = mac_from_text(argv[2])) == NULL) {
		fprintf(stderr, "%s: Invalid mac specification \"%s\"\n",
		    argv[0], argv[2]);
		return(1);
	}
	(void) mac_label_devs("/etc/CMWdevice.conf", pmac, uid);
	(void) mac_free(pmac);

	return(0);
}
