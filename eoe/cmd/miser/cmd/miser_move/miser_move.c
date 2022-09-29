/*
 * FILE: eoe/cmd/miser/cmd/miser_move/miser_move.c
 * 
 * DESCRIPTION
 *	miser_move - move a block of resources from one queue to another.
 *
 *	The miser_move command removes a tuple of space from the
 *	source queue's vector and adds it to the destination queue's
 *	vector beginning at the start time and ending at the end
 *	time.  The resources added or removed do not change the
 *	vector definition and are, therefore, temporary.
 *
 *	The command returns a table that lists the start and end
 * 	times of each resource transfer and the amount of resources
 *	transfered.
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


#include "libmiser.h"


static char *qseg_opts[] = {

#define START	0
		"s",
#define END	1
		"e",
#define CPUS	2
		"c",
#define MEMORY	3
		"m",
		0
};


int
usage(const char *pname)
/*
 * Print an usage message with a brief description of each possible
 * option and quit.
 */
{
	fprintf(stderr, "Usage: %s -s srcq -d dstq "
			"[-o s=start,e=end,c=cpus,m=mem] "
			"[-f file] | -h\n", pname);
	return 1;	/* error */

} /* usage */


int
main(int argc, char **argv)
/*
 * Parse command line input and make miser library call to perform
 * requested miser_move operation.
 */
{
	int		opt;
	char*		subopt;
	char*		val;
	int		oflag	= 0;
	char*		file	= 0;
	char*		srcq	= 0;
	char*		dstq	= 0;
	miser_data_t*	req;
	miser_qseg_t*	qseg;

	if (miser_init()) {
		return 1;	/* error */
	}	

	while ((opt = getopt(argc, argv, "s:d:f:o:h")) != -1) {

		switch(opt) {

		/*
		 * -s srcq
		 *	Srcq is the name of any valid miser queue. 
		 *	This queue may be the system queue.
		 */
		case 's':
			srcq = optarg;
			break;

		/*
		 * -d destq
		 *	Destq is the name of any valid miser queue.
		 *	This queue may be the system queue.
		 */
		case 'd':
			dstq = optarg;
			break;

		/*
		 * -f file
		 *	File contains a resource block specification.
		 */
		case 'f':
			if (oflag || file) {
				return usage(argv[0]);
			}

			file = optarg;
			break;

		/*
		 * -o s=start,e=end,c=cpus,m=mem
		 *	This option specifies a block of resources 
		 *	to be moved.  The start and end times are 
		 *	relative to the current time. The CPUs are 
		 *	an integer up to the maximum free CPUs 
		 *	associated with a queue. The memory is an 
		 *	integer with an identifier (g, m, or k 
		 *	representing gigabyte, megabyte, and kilobyte, 
		 *	respectively) to indicate the amount of
		 *	memory to be transfered. If no unit is 
		 *	provided with the memory field then the 
		 *	memory value is assumed to be bytes.
		 */
		case 'o':
			if (file) {
				return usage(argv[0]);
			}

			if (!oflag) {
				oflag = 1;

				if (!parse_start(PARSE_QMOV, 0)) {
					return 1;	/* error */
				}
			}

			if (!(qseg = parse_qseg_start())) {
				return 1;	/* error */
			}

			subopt = optarg;

			while (*subopt != '\0') {

				switch(getsubopt(&subopt, qseg_opts, &val)) {

				case START:
					if (!fmt_str2time(val, 
							&qseg->mq_stime)) {
						return usage(argv[0]);
					}

					break;

				case END:
					if (!fmt_str2time(val, 
							&qseg->mq_etime)) {
						return usage(argv[0]);
					}

					break;

				case CPUS:
					qseg->mq_resources.mr_cpus = atol(val);
					break;

				case MEMORY:
					if (!fmt_str2bcnt(val, 
					    &qseg->mq_resources.mr_memory)) {
						return usage(argv[0]);
					}

					break;

				} /* switch */

			} /* while */

			if (!parse_qseg_stop()) {
				return 1;	/* error */
			}

			break;

		case 'h':
			return usage(argv[0]);

		} /* switch */

	} /* while */

	if (optind != argc || !srcq || !dstq || (!oflag && !file)) {
		return usage(argv[0]);
	}

	if (file) {
		req = parse(PARSE_QMOV, file);

	} else {
		req = parse_stop();
	}

	if (!req) {
		return 1;	/* error */
	}

	if (miser_move(srcq, dstq, req)) {
		return 1;	/* error */
	}

	print_move(stdout, req);

	return 0;	/* success */

} /* main */
