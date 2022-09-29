/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.1 $"
/*
 * Update Plan G data.
 */

#include <sys/types.h>
#include <sys/statfs.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/syssgi.h>
#include <sys/mac_label.h>
#include <sys/inf_label.h>
#include <sys/acl.h>
#include <sys/capability.h>
#include <sys/eag.h>
#include <mls.h>

char *program;

void
arg_error(char *arg, char *value)
{
	fprintf(stderr, "%s: argument %s cannot use value \"%s\".\n",
	    program, arg, value);
	exit(1);
}

main(int argc, char *argv[])
{
	int i;
	char *value;
	char *attrs = NULL;
	cap_eag_t *caps;
	int cap_got_allowed = 0;
	int cap_got_forced = 0;
	int cap_got_flags = 0;

	program = argv[0];

	for (i = 1; i < argc; i++) {
		/*
		 * No '-' means it's a file system name, and the option
		 * list is complete.
		 */
		if (argv[i][0] != '-')
			break;
		if ((value = strchr(argv[i], '=')) == NULL) {
			fprintf(stderr, "%s: no value given for \"%s\".\n",
			    program, argv[i]);
			exit(1);
		}
		*value++ = '\0';

		/*
		 * Database types.
		 */
		if (!strcmp(argv[i], "-acl")) {
			acl_eag_t *acl = NULL;

			if ((acl = sgi_acl_strtoacl(value)) == NULL) 
				arg_error(argv[i], value);
			attrs = sgi_eag_form_attr(EAG_ACL,
			    sizeof(acl_eag_t), acl, attrs);
			continue;
		}
		if (!strcmp(argv[i], "-acl-default")) {
			acl_eag_t *acl_dir = NULL;

			if ((acl_dir = sgi_acl_strtoacl(value)) == NULL) 
				arg_error(argv[i], value);
			attrs = sgi_eag_form_attr(EAG_DEFAULT_ACL,
			    sizeof(acl_eag_t), acl_dir, attrs);
			continue;
		}
		if (!strcmp(argv[i], "-cap-forced")) {
			capability_t *cap = NULL;

			cap_got_forced = 1;
			if (!caps)
				caps = calloc(1, sizeof(cap_eag_t));

			if ((cap = sgi_cap_strtocap(value)) == NULL) 
				arg_error(argv[i], value);
			caps->cap_forced = *cap;
			continue;
		}
		if (!strcmp(argv[i], "-cap-allowed")) {
			capability_t *cap = NULL;
			char *cp;
			int j;

			cap_got_allowed = 1;
			if (!caps)
				caps = calloc(1, sizeof(cap_eag_t));

			if ((cap = sgi_cap_strtocap(value)) == NULL) 
				arg_error(argv[i], value);

			caps->cap_allowed = *cap;
			continue;
		}
		if (!strcmp(argv[i], "-cap-set-effective")) {
			cap_got_flags = 1;
			if (!caps)
				caps = calloc(1, sizeof(cap_eag_t));

			if (strcmp(value, "on") == 0)
				caps->cap_flags |= CAP_SET_EFFECTIVE;
			continue;
		}
		if (!strcmp(argv[i], "-inf")) {
			inf_label *inf = NULL;

			if ((inf = inf_strtolabel(value)) == NULL) 
				arg_error(argv[i], value);

			attrs = sgi_eag_form_attr(EAG_INF,
			    inf_size(inf), inf, attrs);
			continue;
		}
		if (!strcmp(argv[i], "-mac")) {
			mac_label *mac = NULL;

			if ((mac = mac_strtolabel(value)) == NULL) 
				arg_error(argv[i], value);

			attrs = sgi_eag_form_attr(EAG_MAC,
			    mac_size(mac), mac, attrs);
			continue;
		}
		/*
		 * Not something we know about.
		 */
		fprintf(stderr, "%s: unknown argument \"%s\".\n",
		    program, argv[i]);
		exit(1);
	}

	if (attrs == NULL && caps == NULL) {
		fprintf(stderr, "%s: No attributes specified.\n", program);
		exit(1);
	}

	/*
	 * Now loop through the desired paths.
	 */
	for (; i < argc; i++) {
		char *tattrs = attrs;
		cap_eag_t grumble;
		short s;

		if (caps) {
			if (attrs) {
				bcopy(attrs, &s, sizeof(short));
				tattrs = malloc(s);
				bcopy(attrs, tattrs, s);
			}
			if (sgi_eag_getattr(argv[i], EAG_CAP_FILE, &grumble)){
				perror(argv[i]);
				continue;
			}
			if (cap_got_allowed)
				grumble.cap_allowed = caps->cap_allowed;
			if (cap_got_forced)
				grumble.cap_forced = caps->cap_forced;
			if (cap_got_flags)
				grumble.cap_flags = caps->cap_flags;
			tattrs = sgi_eag_form_attr(EAG_CAP_FILE,
			    sizeof(cap_eag_t), caps, tattrs);
		}
		if (sgi_eag_setattr(argv[i], tattrs))
			perror(argv[i]);
	}

	exit(0);
}
