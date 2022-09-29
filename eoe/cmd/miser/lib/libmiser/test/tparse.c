/*
 * FILE: eoe/cmd/miser/lib/libmiser/test/tparse.c
 *
 * DESCRIPTION:
 *	Test miser library parse functions.
 */

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


#include "../src/libmiser.h"


void
usage(const char* pname)
{
	fprintf(stderr, "Usage: %s [-j | -m | -q] -f fname\n", pname);
	exit(1);

} /* usage */


int
main(int argc, char **argv)
{
	miser_data_t *	req;
        miser_job_t*    job;
	int		opt;
	char *		file	= 0;
	int16_t		type	= PARSE_JSUB;

	while ((opt = getopt(argc, argv, "jmqf:")) != -1) {

		switch (opt) {

		case 'j':
			type = PARSE_JSUB;
			break;

		case 'm':
			type = PARSE_QMOV;
			break;

		case 'q':
			type = PARSE_QDEF;
			break;

		case 'f':
			file = optarg;
			break;
		}
	}

	if (optind != argc || !file)
		usage(argv[0]);

	req = parse(type, file);

	if (!req) exit(1);

	switch (type) {

	case PARSE_JSUB:
		job = (miser_job_t *) req->md_data;
	        miser_print_job_sched(&job->mj_segments[0]);
		break;

	case PARSE_QMOV:
		print_move(stdout, req);
		break;

	case PARSE_QDEF:

	default:
		usage(argv[0]);
	}

	return 0;

} /* main */
