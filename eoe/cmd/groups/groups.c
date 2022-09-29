/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1980 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)groups.c	5.3 (Berkeley) 6/29/88";
#endif /* not lint */

/*
 * groups
 */

#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <unistd.h>

static long ngroups_max;

main(argc, argv)
	int argc;
	char *argv[];
{
	int ngroups, i;
	char *sep = "";
	struct group *gr;
	static gid_t *groups;

        ngroups_max = sysconf(_SC_NGROUPS_MAX);

        if (ngroups_max < 0) {
                fprintf(stderr,"groups: could not get configuration info\n");
                exit(1);
        }

        if (ngroups_max == 0)
                exit(0);        

	if (argc > 1)
		showgroups(argv[1]);

	groups = (gid_t *) malloc(sizeof(gid_t) * ngroups_max);

	if (groups == NULL) {
                fprintf(stderr,"groups: Malloc failed\n");
                exit(1);
	}

	ngroups = getgroups(ngroups_max, groups);
	if (!ngroups) {
		gid_t egid;

		egid = getegid();
		if (gr = getgrgid(egid))
			printf("%s\n",gr->gr_name);
		exit(0);
	}

	for (i = 0; i < ngroups; i++) {
		gr = getgrgid(groups[i]);
		if (gr == NULL)
			printf("%s%d", sep, groups[i]);
		else
			printf("%s%s", sep, gr->gr_name);
		sep = " ";
	}
	printf("\n");
	exit(0);
}

showgroups(user)
	register char *user;
{
	register struct group *gr;
	register struct passwd *pw;
	gid_t *groups;
	int ngroups, i;

	groups = (gid_t *)malloc(sizeof(gid_t) * ngroups_max);
	if (! groups) {
                fprintf(stderr,"groups: Malloc failed\n");
                exit(1);
	}

	if ((pw = getpwnam(user)) == NULL) {
		fprintf(stderr, "groups: no such user \'%s\'.\n",user);
		exit(1);
	}
	groups[0] = pw->pw_gid;

	ngroups = getgrmember(user, groups, ngroups_max, 1);
	for (i = 0; i < ngroups; i++) {
		gr = getgrgid(groups[i]);
		if (gr) {
			printf("%s ", gr->gr_name);
		} else {
			printf("%d ", groups[i]);
		}
	}
	printf("\n");

	exit(0);
}
