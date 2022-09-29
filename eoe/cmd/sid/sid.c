/*
 * COPYRIGHT NOTICE
 * Copyright 1996, Silicon Graphics, Inc. All Rights Reserved.
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
 *
 */
#ident	"$Revision: 1.2 $"

#include <sys/types.h>
#include <errno.h>
#include <proj.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


main(int argc, char **argv)
{
	int curropt;
	int error = 0;
	int showash = 0, showcurrproj = 0, showallproj = 0;
	int showname = 0;
	char *username = NULL;

	/* Parse command line args */
	while ((curropt = getopt(argc, argv, "Panp")) != EOF) {
		switch(curropt) {
		    case 'P':
			showallproj = 1;
			break;

		    case 'a':
			showash = 1;
			break;

		    case 'n': 
			showname = 1;
			break;

		    case 'p':
			showcurrproj = 1;
			break;

		    case '?':
			error = 1;
			break;
		}
	}
	if (optind < argc) {
		if (optind + 1  != argc) {
			fprintf(stderr, "%s: too many arguments\n", argv[0]);
			error = 1;
		}
		else {
			username = argv[optind];
		}
	}

	/* Make sure no incompatible args were specified */
	if (showname && !(showallproj || showcurrproj)) {
		fprintf(stderr, "%s: -n only valid with -p or -P\n", argv[0]);
		error = 1;
	}
	if (username != NULL  && !(showallproj || showcurrproj)) {
		fprintf(stderr, "%s: user name only valid with -p or -P\n",
			argv[0]);
		error = 1;
	}
	if (showash + showallproj + showcurrproj > 1) {
		fprintf(stderr, "%s: may only specify one of -a, -p or -P\n",
			argv[0]);
		error = 1;
	}

	/* Barf if an error occurred */
	if (error) {
		fprintf(stderr, "Usage: %s [-a]\n", argv[0]);
		fprintf(stderr, "       %s -p [-n] [user]\n", argv[0]);
		fprintf(stderr, "       %s -P [-n] [user]\n", argv[0]);
		exit(1);
	}

	/* Proceed according to the selected display format */
	if (showash) {

		/* Just an ASH */
		printf("0x%016llx\n", getash());
	}
	else if (showcurrproj) {

		/* Just the current/default project ID */
		prid_t prid;

		if (username == NULL) {
			prid = getprid();
		}
		else {
			prid = getdfltprojuser(username);
			if (prid < 0) {
				fprintf(stderr,
					"Cannot find default project for %s\n",
					username);
				exit(1);
			}
		}

		if (showname) {
			char name[MAXPROJNAMELEN];

			if (!projname(prid, name, MAXPROJNAMELEN)) {
				printf("%lld\n", prid);
			}
			else {
				printf("%s\n", name);
			}
		}
		else {
			printf("%lld\n", prid);
		}
	}
	else if (showallproj) {

		/* All projects for which user is authorized */
		projid_t projids[32];
		int numids, i;
		struct passwd *entry;

		if (username == NULL) {
			entry = getpwuid(geteuid());
			if (entry == NULL) {
				fprintf(stderr,
					"Cannot find your passwd entry!\n");
				exit(1);
			}
			username = entry->pw_name;
		}

		numids = getprojuser(username, projids, 32);
		if (numids < 0) {
			fprintf(stderr, "Unable to find project info for %s\n",
				username);
			exit(1);
		}

		for (i = 0;  i < numids;  ++i) {
			if (i > 0) {
				printf(" ");
			}
			if (showname) {
				printf("%s", projids[i].proj_name);
			}
			else {
				printf("%lld", projids[i].proj_id);
			}
		}
		printf("\n");
	}
	else {
		/* Show both ASH and current project */
		char name[MAXPROJNAMELEN];
		prid_t prid;

		prid = getprid();
		printf("ash=0x%016llx prid=%lld", getash(), prid);

		if (!projname(prid, name, MAXPROJNAMELEN)) {
			printf("\n");
		}
		else {
			printf("(%s)\n", name);
		}
	}
	exit(0);
}
