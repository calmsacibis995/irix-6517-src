/*
 * FILE: eoe/cmd/miser/cmd/miser_reset/miser_reset.c
 *
 * DESCRIPTION:
 *	miser_reset - reset miser with a new configuration file
 *
 *	The miser_reset command is used to force a running version
 * 	of miser to use a new configuration file (the format of the
 *	configuration file is detailed in miser(4) ).  The new
 *	configuration will succeed if and only if all currently
 *	scheduled jobs can be successfully scheduled against the new
 *	configuration. If the attempt at creating a new configuration 
 *	fails, then miser(1) retains the old configuration.
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


#include <sys/param.h>
#include <errno.h>
#include "libmiser.h"


int
usage(const char* pname)
/*
 * Print an usage message with a brief description of each possible
 * option and quit.
 */
{
	fprintf(stderr, "\nUsage: %s -f filename | -h\n\n"
		"Valid Arguments:\n"
		"\t-f filename\tReset miser with a new configuration file.\n"
		"\t-h\t\tPrint the command's usage message.\n\n",
		pname);
	return 1;	/* error */

} /* usage */


int
main(int argc, char ** argv)
{
	int  c;				/* command line option character */
	int  f_flag = 0;		/* f_flag initialized to 0 */
	char *f_name;			/* config file name */
	char f_path[MAXPATHLEN];	/* resolved path for config file */

	/* Check if miser running */
	if (miser_init()) {
		return 1;	/* error */
	}

	/* Parse command line arguments and set appropriate Flags. */
	while ((c = getopt(argc, argv, ":f:h")) != -1) {

		switch(c) {

		/*
		 * -f filename
		 *	Reset miser with a new configuration file.
		 */
		case 'f':
			f_flag++;
			f_name = optarg;
                        break;

		/* Getopt reports required argument missing from -f option. */
		case ':': 
			/* -f option requires an argument. */
			if (strcmp(argv[optind -1], "-f") == 0) {
				fprintf(stderr, "\n%s: ERROR - '-f' option "
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

	/* f_flag option not set */
	if (f_flag == 0) {
		return usage(argv[0]);
	}

	/* Verify miser config file location, return 1 if invalid */
	if (!realpath(f_name, f_path)) {
		fprintf(stderr, "\n%s: filename (%s) - %s.\n\n", 
			argv[0], f_name, strerror(errno));
		return 1;       /* error */
	}

	return miser_reset(f_path);	/* 0 - success */

} /* main */
