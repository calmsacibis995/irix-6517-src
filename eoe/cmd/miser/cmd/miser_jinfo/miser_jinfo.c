/*
 * FILE: eoe/cmd/miser/cmd/miser_jinfo/miser_jinfo.c
 *
 * DESCRIPTION:
 *	miser_jinfo - query miser about the schedule/description of a 
 *		      submitted job.
 *
 *	The miser_jinfo command is used to query the schedule/description 
 *	of a job that has already been scheduled by miser.  If the 
 *	specified job exists and miser is running, miser_jinfo will return 
 *	the schedule in the same format as miser_submit returns.
 */

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


#include <sys/sysmp.h>
#include <errno.h>
#include "libmiser.h"


int
usage(const char* pname)
/*
 * Print an usage message with a brief description of each possible
 * option and quit.
 */
{
	fprintf(stderr,
		"\nUsage: %s -j jid [-d] | -h\n\n"
                "Valid Arguments:\n"
                "\t-j jid\tPrint job status for the requested jobid.\n"
                "\t-d\tPrint job description (owner, command) for the "
			"requested jid.\n" 
                "\t-h\tPrint the command's usage message.\n\n",
		pname);
	return 1;	/* error */

} /* usage */


int
jinfo_desc(bid_t jid, int start)
/*
 * Print job description including the username of the job owner and 
 * the command name executing.
 */
{ 
	miser_seg_t*      seg_ptr;	/* Pointer to miser resource segment */
	miser_schedule_t* ms;		/* Hold incoming miser job schedule */

	/* Get a job schedule for specified jid */
	if ((ms = miser_get_jsched(jid, start)) == NULL) {
		fprintf(stderr, "\nmiser: failed to find job [%d].\n\n", jid);
		return 1;	/* error */
	}

	/* Point to the first seg in the list */
	seg_ptr = (miser_seg_t *)ms->ms_buffer;

	printf("\n  JOBID  CPU  MEM     START TIME     "
		"    END TIME        USER       COMMAND\n"
		"-------- --- ----- ----------------- "
		"----------------- -------- ---------------\n");

	printf("%8d ", jid);

	miser_print_job_status(seg_ptr);
	printf(" ");

	miser_print_job_desc(jid);
	printf("\n\n");

	return 0;	/* success */

} /* jinfo_desc */


int
jinfo_status(bid_t jid, int start)
/*
 * Print miser job status including resources, start/end times etc.
 */
{ 
	miser_seg_t*      seg_ptr;	/* Pointer to miser resource segment */
	miser_schedule_t* ms;		/* Hold incoming miser job schedule */

	/* Get a job schedule for specified jid */
	if ((ms = miser_get_jsched(jid, start)) == NULL) {
		fprintf(stderr, "\nmiser: failed to find job [%d].\n\n", jid);
		return 1;	/* error */
	}

	/* Point to the first seg in the list */
	seg_ptr = (miser_seg_t *)ms->ms_buffer;

        printf("\n  JOBID  CPU  MEM   DURATION     START TIME"
		"         END TIME      MLT PRI OPT\n"
		"-------- --- ----- ---------- ----------------- "
		"----------------- --- --- ---\n");

	printf("%8d ", jid);

	miser_print_job_sched(seg_ptr);

	printf("\n");

	return 0;	/* success */

} /* jinfo_status */


int
main(int argc, char ** argv)
{
	int	c;		/* Command line option character */ 
	int	start	= 0;	/* start segment */
	int	d_flag	= 0;	/* '-d' flag */
	int	s_flag	= 0;	/* '-s' flag */
	int	j_flag	= 0;	/* '-j' flag */
	bid_t	jid	= -1;	/* Job id to find information on */

	/* Initialize miser */
	if (miser_init()) {
		return 1;	/* error */
	}

	/* Parse command line arguments and set appropriate Flags */
	while ((c = getopt(argc, argv, ":dj:s:h")) != -1) {

		switch (c) {

		/* job description (submitter, command) */
		case 'd':
			d_flag++;
			break;

		/* job status display */
		case 'j':
			j_flag++;
			jid	= atoi(optarg);
			break;

		/* not documented - search start segment */
		case 's':
			s_flag++;
			start	= atoi(optarg);
			break;

                /* Getopt reports required argument missing from '-j' ption. */
                case ':':
                        /* '-j' option requires an argument. */
                        if (strcmp(argv[optind -1], "-j") == 0) {
                                fprintf(stderr, "\n%s: ERROR - '-j' option "
                                        "requires argument\n", argv[0]);
                                return usage(argv[0]);
                        }

			break;

		/* Getopt reports invalid option in command line. */
		case '?':
			fprintf(stderr, "\n%s: ERROR - invalid command line "
				"option '%s'\n", argv[0], argv[optind -1]);
			return usage(argv[0]);

		/* Print usage message */
		case 'h':
			return usage(argv[0]);

		} /* switch */

	} /* while */

	if (jid < 0) {
		return usage(argv[0]);
	}

	if (d_flag) {
		return jinfo_desc(jid, start);

	} else if (j_flag) {
		return jinfo_status(jid, start);
	}

} /* main */
