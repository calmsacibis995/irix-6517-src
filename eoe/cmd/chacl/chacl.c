/* 
 * COPYRIGHT NOTICE
 * Copyright 1995, Silicon Graphics, Inc. All Rights Reserved.
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

#ident "$Revision: 1.1 $"
/* 
 *
 * chacl - change file Access Control List
 */

#include <sys/types.h>
#include <sys/acl.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>

static void
usage (const char *program)
{
	fprintf (stderr, "%s: usage: \n", program);
	fprintf (stderr, "\t%s acl pathname ...\n", program);
	fprintf (stderr, "\t%s -b acl dacl pathname ...\n", program);
	fprintf (stderr, "\t%s -d dacl pathname ...\n", program);
	exit (1);
}

int
main (int argc, char *argv[])
{
	const char *inv_acl = "%s: \"%s\" is an invalid ACL specification.\n";
	char *program, *p;
	int c;				/* For use by getopt(3) */
	int dflag = 0;			/* a Default ACL is desired */
	acl_t acl = NULL;		/* File ACL */
	acl_t dacl = NULL;		/* Directory Default ACL */

	/* create program name */
	p = strrchr (argv[0], '/');
	program = p != NULL ? p + 1 : argv[0];

	/* continue only if ACLs enabled */
	if (sysconf (_SC_ACL) <= 0)
	{
		fprintf (stderr, "%s ACLs not enabled.\n", program);
		return (1);
	}

	/* parse arguments */
	while ((c = getopt (argc, argv, "b:d")) != -1)
	{
		switch (c)
		{
			case 'b':
				acl = acl_from_text (optarg);
				if (acl == NULL)
				{
					fprintf (stderr, inv_acl, program,
						 optarg);
					return (1);
				}
				/* NO BREAK */
			case 'd':
				dflag = 1;
				break;
			default:
				usage (program);
				break;
		}
	}

	/* if no file specified, quit */
	if (optind >= argc)
		usage (program);

	/* use default acl or regular acl */
	if (dflag)
	{
		dacl = acl_from_text (argv[optind]);
		if (dacl == NULL)
		{
			fprintf (stderr, inv_acl, program, argv[optind]);
			return (1);
		}
	}
	else
	{
		acl = acl_from_text (argv[optind]);
		if (acl == NULL)
		{
			fprintf (stderr, inv_acl, program, argv[optind]);
			return (1);
		}
	}

	/* operate on file args */
	for (optind++; optind < argc; optind++)
	{
		/* set regular acl */
		if (acl &&
		    acl_set_file (argv[optind], ACL_TYPE_ACCESS, acl) == -1)
			perror (argv[optind]);

		/* set default acl */
		if (dacl &&
		    acl_set_file (argv[optind], ACL_TYPE_DEFAULT, dacl) == -1)
			perror (argv[optind]);
	}

	if (acl)
		acl_free (acl);
	if (dacl)
		acl_free (dacl);

	return (0);
}
