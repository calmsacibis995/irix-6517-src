/*
 * FILE: eoe/cmd/miser/cmd/miser_qinfo/miser_qinfo.c
 *
 * DESCRIPTION:
 *      miser_qinfo - query information on miser queues, queue resource
 *                    status, and list of jobs scheduled against a queue
 *
 *      The miser_qinfo command is used to retrieve information about
 *      the free resources of a queue, the names of all miser queues,
 *      and to query all jobs currently scheduled against a particular
 *      queue.
 */

/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/


#include "libmiser.h"


#define MAX_CHAR 80


int
usage(const char* pname)
/*
 * Print an usage message with a brief description of each possible
 * option and quit.
 */
{
	fprintf(stderr,
		"\nUsage: %s | -Q | -q qname [-j] | [-a] | -h\n\n"
/*		"\nUsage: %s | -Q | -q qname [-j] | [-a] | [-A] | -h\n\n" */
                "Valid Arguments:\n"
		"\t\"blank\"\t\tList all queues and their jobs and schedules.\n"
		"\t-Q\t\tList all defined miser queues.\n"
		"\t-q qname\tList current status of the queue specified.\n"
		"\t-q qname -j\tList all submitted jobids in the queue.\n"
		"\t-a\t\tList all queues and their jobs and schedules.\n"
/*		"\t-A\t\tList current status of all defined miser queues.\n" */
		"\t-h\t\tPrint the command's usage message.\n\n",
			pname);
	return 1;	/* error */

} /* usage */


int
main(int argc, char **argv)
{
	char q_name[MAX_CHAR];		/* Pointer to qname */
	char error_str[MAX_CHAR];	/* Pointer to erronous string */
	char *error_char = "?";		/* Erronous char in command line */

	int c;			/* Command line option character */
	int start	= 0;	/* Argument variable */
	int a_flag	= 0;	/* '-a' flag */
	int j_flag	= 0;	/* '-j' flag */
	int Q_flag	= 0;	/* '-Q' flag */
	int q_flag	= 0;	/* '-q' with queue name flag */
	int qAll_flag	= 0;	/* '-q' without queue name flag */ 
	int s_flag	= 0;	/* '-s' flag */

	extern char	*optarg;
	extern int	optind;

	/* Initialize miser */
	if (miser_init()) { 
		return 1;	/* error */
	}


	/* Parse command line arguments and set appropriate Flags. */
	while ((c = getopt(argc, argv, ":aAQq:js:h")) != -1) {

		switch(c) {

		/* Get all queues, submitted jobs, and their schedules. */
		case 'a':
			a_flag++;
			break;

		/* Get queue statistics for all queues. */
		case 'A':
			qAll_flag++;
			break;
 
		/* Get list of configured queue names. */
		case 'Q':
			Q_flag = 1;
			break;

		/* Get queue statistics for the specified queue. */
		case 'q':
			/* If not a parameter, get queue name. */ 
			if ( (optarg[0] == '-') ||
				(sscanf(optarg, "%s", q_name) != 1) ) {
				fprintf(stderr, "\n%s: ERROR - invalid '-q' "
					"option argument \"%s\"\n", 
					argv[0], optarg);
				return usage(argv[0]);
			}

			q_flag = 1;
			break;
          
		/* Get job ids for the specified queue. */
		case 'j':
			j_flag = 1;
			break;
          
		/* Specified start time for query - not published. */
		case 's':
			/* If not a parameter, get start value. */ 
			if ( (optarg[0] == '-') ||
				(sscanf(optarg, "%i%c", &start, error_char) 
				!= 1) ) {
				fprintf(stderr, "\n%s: ERROR - invalid start "
					"number \"%s\"\n", argv[0], optarg);
				return usage(argv[0]);
			}

			s_flag = 1;
			break;

		/* 
		 * Getopt reports required argument missing from -q, 
		 * or -s option. 
		 */
		case ':':  
			/* -q option requires an argument. */
			if (strcmp(argv[optind -1], "-q") == 0) {
				fprintf(stderr, "\n%s: ERROR - '-q' option "
					"requires argument\n", argv[0]);
				return usage(argv[0]);
			}

			/* -s option requires an argument. */
			if (strcmp(argv[optind -1], "-s") == 0) {
				fprintf(stderr, "\n%s: ERROR - '-s' option "
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

	/* Validate user selection, return information. */

	/* No options used, same as -a option. */
	if ( (a_flag == 0) && (Q_flag == 0) && (q_flag == 0) && (j_flag == 0)
			&& (qAll_flag == 0) ) {

		/* No other command line parameters valid, except -s option. */
		if ( ((argc > 1) && (s_flag == 0)) || (argc > 3) ) {
			fprintf(stderr, "\n%s: ERROR - incorrect command line "
				"option\n", argv[0]);
			return usage(argv[0]);

		/* Get all queues, submitted jobs, and schedules. */
		} else {
			return miser_qjsched_all(start);
		}

	/* -a option, without any other parameters. */
	} else if ( a_flag == 1 ) {

		/* -a (and -s) options used, no other command line 
			parameters valid. */
		if ( ((argc > 2) && (s_flag == 0)) || (argc > 4) ) {
			fprintf(stderr, "\n%s: ERROR - incorrect use of '-a' "
				"option\n", argv[0]);
			return usage(argv[0]);

		/* Get all queues, submitted jobs, and schedules. */
		} else {
			return miser_qjsched_all(start);
		}
      
	/* -Q option alone */
	} else if (Q_flag == 1) {
		if (argc > 2) {
			fprintf(stderr, "\n%s: ERROR - incorrect use of '-Q' "
				"option\n", argv[0]);
			return usage(argv[0]);
      
		/* Get list of configured queue names. */
		} else {
			return miser_qname();
		}
    
	/* -j option, must have -q and a queue name. */
	} else if (j_flag) {

		/* Error if not four arguments. */
		if ( (argc > 4) || (q_flag != 1) || (s_flag != 0) ) {
			fprintf(stderr, "\n%s: ERROR - incorrect command line "
				"arguments with '-j' option\n", argv[0]);
			return usage(argv[0]);

		/* '-q qname -j' - get job ids for the specified queue. */
		} else {
			return miser_qjid(q_name, start);
		}
     
	/* -q option and a queue name */
	} else if ( (q_flag == 1) && (j_flag == 0) && (qAll_flag == 0) ) {
		if ( ((argc > 3) && (s_flag == 0)) || 
			((argc > 5) && (s_flag == 1)) ) {
			fprintf(stderr, "\n%s: ERROR - incorrect use of '-q' "
				"option\n", argv[0]);
			return usage(argv[0]);

		/* Get queue statistics for specified queue. */ 
		} else {
			return miser_qstat(q_name, start);
		}

	/* -A option, no queue name specififd. */
	} else if ( qAll_flag == 1 ) {
      
		/* Error if not three arguments. */
		if (((argc > 2) && (s_flag == 0)) ||
			((argc > 4) && (s_flag == 1))) {
        
			fprintf(stderr, "\n%s: ERROR - incorrect command line "
				"arguments with '-A' option\n", argv[0]);
			return usage(argv[0]);
          
		/* Get queue statistics for all defined queues */
		} else {
/*			return miser_qstat_all(start); */

			fprintf(stderr, "\n%s: ERROR - invalid command line "
				"option '%s'\n", argv[0], argv[optind -1]);
			return usage(argv[0]);
		}
    
	/* Somthings wrong, should never get here. */
	} else {
		return usage(argv[0]);
	}

} /* main */
