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

/*
 * sat_echo -	echo the contents of standard input to the system audit trail
 *		(only the first 0x1000 bytes of standard input are used)
 */

#ident  "$Revision"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syssgi.h>
#include <sys/capability.h>
#include <sat.h>

#define	BUFSZ	0x1000
#define	UNDEF	(SAT_NTYPES + 1)

typedef int (*satwrite_fn_t)(int, int, char *, unsigned int);

static int  satwrite_stub (int, int, char *, unsigned int);
static void sat_echo_usage(void);

int
main(int argc, char *argv[])
{
	int		event;
	int		outcome = UNDEF;
	char		buffer [BUFSZ + 1];
	unsigned int	buflen = 0;
	int		c;
	satwrite_fn_t	sw = satwrite;
	cap_t		ocap;
	cap_value_t	cap_audit_write = CAP_AUDIT_WRITE;
	FILE *		fp = stdin;

	while ((c = getopt(argc, argv, "FSdf:")) != EOF)
	{
		switch (c)
		{
			case 'F':
				outcome = SAT_FAILURE;
				break;
			case 'S':
				outcome = SAT_SUCCESS;
				break;
			case 'd':
				sw = satwrite_stub;
				break;
			case 'f':
				fp = fopen (optarg, "r");
				if (fp == NULL) {
					perror(optarg);
					exit(1);
				}
				break;
			default:
				sat_echo_usage();
				break;
		}
	}

	/* complain about missing arguments */
	if (UNDEF == outcome || NULL == argv[optind])
		sat_echo_usage ();

	/* convert first non-option argument to event code */
	if (-1 == (event = sat_strtoevent (argv[optind]))) {
		fprintf (stderr, "unrecognized audit event: %s\n",
			 argv[optind]);
		exit(1);
	}

	/* fill buffer with data from specified source */
	while (BUFSZ > buflen) {
		if ((c = getc(fp)) == EOF)
			break;
		buffer[buflen++] = c;
	}
	buffer[buflen] = '\0';

	/* send data to audit trail, with optional debug output */
	ocap = cap_acquire(1, &cap_audit_write);
	if (-1 == (*sw) (event, outcome, buffer, buflen)) {
		cap_surrender(ocap);
		perror ("can't write sat record");
		exit (1);
	}
	cap_surrender(ocap);

	fprintf (stderr, "%d bytes written to system audit trail\n", buflen);
	return (0);
}

/*
 * sat_echo_usage -	remind user of program calling sequence
 */
static void
sat_echo_usage(void)
{
	fprintf (stderr,
		 "usage: sat_echo (-F|-S) [-d] [-f input-file] event-name\n");
	exit(1);
}

static int
satwrite_stub (int rectype, int outcome, char *buffer, unsigned int len)
{
	fprintf (stderr, "\tEvent = %s\n", sat_eventtostr(rectype));
	fprintf (stderr, "\tOutcome = %s\n",
		 outcome == SAT_SUCCESS ? "SAT_SUCCESS" : "SAT_FAILURE");
	fprintf (stderr, "\tLength = %d\n", len);
	fprintf (stderr, "\n%s\n", buffer);
	return (satwrite (rectype, outcome, buffer, len));
}
