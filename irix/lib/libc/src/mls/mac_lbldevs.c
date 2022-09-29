/*
 * Copyright 1990, Silicon Graphics, Inc. 
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
	#pragma weak mac_label_devs = _mac_label_devs
#endif

#include "synonyms.h"
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mac.h>
#include <sys/capability.h>
#include <mls.h>


static const cap_value_t caps[] = {
	CAP_MAC_UPGRADE,
	CAP_MAC_DOWNGRADE,
	CAP_MAC_RELABEL_OPEN,
	CAP_FOWNER,
	CAP_DEVICE_MGT,
};

static const char __highflbl[]= { MSEN_HIGH_LABEL, MINT_LOW_LABEL, 0, 0, 0,
				  0, 0, 0 };

/*
 * mac_label_devs - assign the given MAC label to all the devices whose 
 *			 names are listed in the given file
 */
int
mac_label_devs (char *devlist_fname, mac_t labelp, uid_t owner)
{
	FILE *devlist_fp;
	char line[256];
	char *path;
	char *skip;
	char *begin_comment;
	int n_tries = 0;
	int n_okay = 0;
	struct stat statbuf;
	cap_t ocap;

	if ((devlist_fp = fopen( devlist_fname, "r" )) == NULL)
		return (MAC_LD_DEVL_FOPEN);

	ocap = cap_acquire(sizeof(caps)/sizeof(cap_value_t), caps);

	while (fgets( line, sizeof(line), devlist_fp )) {

		if (begin_comment = strchr( line, '#' ))
			*begin_comment = '\0';	/* strip comments */

		/* skip white space */
		skip = line;
		while (isspace(*skip))
			skip++;
		path = skip;
		while (!isspace(*skip))
			skip++;
		*skip = '\0';

		if (*path == '\0')
			continue;	/* ignore null or comment only lines */

		n_tries++;

		/*
		 * Give up if the device isn't there
		 */
		if (stat(path, &statbuf) < 0)
			continue;
		/*
		 * Give up if it isn't a device
		 */
		if (!(S_ISCHR (statbuf.st_mode) || S_ISBLK (statbuf.st_mode)))
			continue;
		/*
		 * These should always succeed.
		 */
		if (chmod(path, 0000) < 0) {
			fprintf(stderr, "Device assignment chmod 0 ");
			perror(path);
		}
		if (mac_set_file (path, (mac_t) __highflbl) == -1) {
			fprintf(stderr, "Device assignment setlabel high ");
			perror(path);
		}
		/*
		 * Revoke access to anyone with this open.
		 */
		if (sgi_revoke(path) < 0) {
			fprintf(stderr, "Device assignment revokation ");
			perror(path);
		}
		/*
		 * This chmod should always succeed
		 */
		if (chmod(path, 0600) < 0) {
			fprintf(stderr, "Device assignment chmod for user ");
			perror(path);
		}
		/*
		 * XXX - should probably use "-1" for the group,
		 * but that gets an EINVAL, so a gid of 0 will have to do.
		 */
		if (chown(path, owner, 0) < 0) {
			fprintf(stderr, "Device assignment chown for user ");
			perror(path);
		}
		/*
		 * No reason for this to fail, either.
		 */
		if (mac_set_file (path, labelp) == -1)
			continue;

		n_okay++;
	}
	cap_surrender (ocap);

	if (n_tries != n_okay) {
		if (!n_okay)
			return (MAC_LD_ALL_FAIL);
		else
			return (MAC_LD_SOME_FAIL);
	}
	return (0);
}
