/*
 * FILE: eoe/cmd/miser/cmd/miser_submit/miser_submit.c
 *
 * DESCRIPTION:
 *	miser_submit - submit a job to a miser queue.
 *
 *	The miser_submit command is used to submit a job (the command) 
 *	to a miser queue.  A job is an application that will be scheduled 
 *	by miser.  Any application that does not change its session ID or 
 *	change its process group ID can be submitted to a miser queue.  
 *	For an application to be properly submitted to a miser queue, it 
 *	needs to specify its resource schedule.  A resource schedule is 
 *	a list of resource specifications, called segments, that define 
 *	the resource requirements of a particular job.  A resource
 *	specification is a tuple of CPUs, memory and wall clock time.  
 *	Currently miser only supports resource schedules consisting 
 *	of one segment.  A segment also has additional optional fields 
 *	that specify how the job is to be scheduled.  These are defined 
 *	in miser_submit man page.
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


#include <errno.h>
#include "libmiser.h"


static char *seg_opts[] = {

#define TIME	0
		"t",
#define CPUS	1
		"c",
#define MEMORY	2
		"m",
#define STATIC	3
		"static",
		0

}; /* seg_opts */


int
usage(const char* pname)
/*
 * Print an usage message with a brief description of each possible
 * option and quit.
 */
{
	fprintf(stderr, 
		"\nUsage: %s -q qname -o c=cpus,m=mem,t=time[,static] command\n"
		"       -h | %s -q qname -f file command\n\n"
		"Valid Arguments:\n"
		"\t-q\tQueue name in which to schedule the job to.\n"
		"\t-o\tSpecify job's resource requirements in command line.\n"
		"\t-f\tFilename containing job's resource requirements.\n"
		"\t-h\tPrints the command's usage message.\n\n",
		pname, pname);
	return 1;	/* error */

} /* usage */


int
parse_subopt(char* subopt, const char* pname)
/*
 * Parse job's resource requirements specified after '-o' option.
 */
{
	char*        val;	/* Resource value from the command line */
	miser_seg_t* seg;	/* Pointer to resource segment structure */

	if (!(seg = parse_jseg_start())) {
		return 1;	/* error */
	}

	while (*subopt != '\0') {

		switch(getsubopt(&subopt, seg_opts, &val)) {

		case CPUS:
			seg->ms_resources.mr_cpus = atol(val);
			break;

		case TIME:
			if (!fmt_str2time(val, &seg->ms_rtime)) {
				return usage(pname);
			}

			break;

		case MEMORY:
			if (!fmt_str2bcnt(val, &seg->ms_resources.mr_memory)) {
				return usage(pname);
			}

			break;

		case STATIC:
			seg->ms_flags &= ~MISER_SEG_DESCR_FLAG_NORMAL;
			seg->ms_flags |= MISER_SEG_DESCR_FLAG_STATIC;
			break;

		default:
			return usage(pname);
		}

	} /* while */

	if (!parse_jseg_stop()) {
		return 1;	/* error */
	}

	return 0;	/* success */

} /* parse_subopt */


int
main(int argc, char **argv)
{
	int   c;
	int   o_flag = 0;
	int   q_flag = 0;
	char* queue = 0;
	char* file  = 0;

	miser_data_t* req;	/* Pointer to miser_data structure */
	miser_job_t*  job;	/* Pointer to miser_job structure */ 

	/* Initialize miser */
	if (miser_init()) {
		return 1;	/* error */
	}

	/* Parse arguments - commandline */
	while ((c = getopt(argc, argv, ":q:f:o:h")) != -1) {

		switch(c) {

		/*
		 * -q qname
		 *	Specifies the queue against which to schedule 
		 *	the application.  The user must have execute 
		 *	permissions on the queue definition file to 
		 *	schedule an application against the resources 
		 *	of a particular queue.  The queue name must be 
		 *	a valid queue name.
		 */
		case 'q':
			q_flag++;
			queue = optarg;
			break;

		/*
		 * -f file
		 *	This file specifies a list of resource segments.  
		 *	Using the file allows greater control over the 
		 *	scheduling parameters of a particular job.
		 */
		case 'f':
			if (o_flag || file) {
				return usage(argv[0]);
			}

			file = optarg;
			break;

		/*
		 * -o c=CPUs,m=mem,t=time[,static]
		 *	Specifies a block of resources from the command 
		 *	line.
		 */	
		case 'o':
			if (file) {
				return usage(argv[0]);
			}

			if (!o_flag) {
				o_flag++;

				if (!parse_start(PARSE_JSUB, 0)) {
					return 1;	/* error */
				}
			}

			if (parse_subopt(optarg, argv[0])) {
				return 1;	/* error */
			}

			break;

		/* Get job ids for the specified queue. */
		case 'h':
			return usage(argv[0]);

		/* Getopt reports required argument missing from -j option. */
		case ':':
			/* -q option requires an argument. */
			if (strcmp(argv[optind -1], "-q") == 0) {
				fprintf(stderr, "\n%s: ERROR - '-q' option "
					"requires an argument\n", argv[0]);
				return usage(argv[0]);
			}

			/* -f option requires an argument. */
			if (strcmp(argv[optind -1], "-f") == 0) {
				fprintf(stderr, "\n%s: ERROR - '-f' option "
					"requires an argument\n", argv[0]);
				return usage(argv[0]);
			}

			/* -o option requires an argument. */
			if (strcmp(argv[optind -1], "-o") == 0) {
				fprintf(stderr, "\n%s: ERROR - '-o' option "
					"requires an argument\n", argv[0]);
				return usage(argv[0]);
			}

			break;

		/* getopt reports invalid option in command line. */
		case '?':
			fprintf(stderr, "\n%s: ERROR - invalid command "
				"line option '%s'\n",
				argv[0], argv[optind -1]);
			return usage(argv[0]);

                } /* switch */

	} /* while */

	if (optind >= argc || !queue || (!o_flag && !file)) {
		return usage(argv[0]);
	}

	/* Parse contents of the job resource configuration file */
	if (file) {
		req = parse(PARSE_JSUB, file);
	} else {
		req = parse_stop();
	}

	/* Job submit argument file parsing failed - exit */
	if (!req) {
		return 1;	/* error */
	}

	/* Send submit, job reservation request to the kernel */
	if (!miser_submit(queue, req)) {
		return 1;	/* error */
	}

	job = (miser_job_t *) req->md_data; 

        printf("\n\nMiser Job Successfully Scheduled:\n\n");

        printf("  JOBID  CPU  MEM   DURATION     START TIME         "
		"END TIME      MLT PRI OPT\n"
		"-------- --- ----- ---------- ----------------- "
		"----------------- --- --- ---\n");

	printf("%8d ", job->mj_bid);

	job->mj_segments[0].ms_etime = job->mj_etime;

	/* Print job schedule */
        miser_print_job_sched(&job->mj_segments[0]);

        printf("\n");
	fflush(NULL);

	/* Execute command - blocking */
	execvp(argv[optind], &argv[optind] );

	/* Returned from execvp - must have failed, print error message */
	merror("miser_submit: Failed to exec process: %s", strerror(errno));

	return 1;	/* error */

} /* main */
