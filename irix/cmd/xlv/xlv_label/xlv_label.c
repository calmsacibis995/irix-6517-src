/**************************************************************************
 *                                                                        *
 *            Copyright (C) 1993-1994, Silicon Graphics, Inc.             *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.12 $"
/*
 * Driver routine to locate all XLV volumes on system.
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <sys/syssgi.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/dvh.h>
#include <sys/xlv_vh.h>
#include <xlv_oref.h>
#include <xlv_lab.h>
#include <xlv_utils.h>

#define	EXIT_SUCCESS	0
#define	EXIT_ERROR	1
#define	EXIT_NOLABEL	2

int delete = 0;
int verbose = 0;
int quiet = 0;
int secondary = 0;

static void
usage(void)
{
	fprintf(stderr, "usage: xlv_label [-d]\n");
	fprintf(stderr, "-d    Delete the volume label.\n");
	fprintf(stderr, "-q    Be quiet and don't display label.\n");
	fprintf(stderr, "-s    Display the secondary label.\n");
	fprintf(stderr, "-v    Display verbose label information.\n");
	exit(EXIT_ERROR);
}

void
main (int argc, char **argv)
{
	xlv_vh_entry_t	*vh, *vh_list;
	int		exit_code = 0;
	int		status;
	int		ch;

	if (getuid()) {
		fprintf(stderr, "%s: must be started by super-user\n", argv[0]);
		exit(EXIT_ERROR);
	}

	vh = vh_list = NULL;
	status = -1;

	while ((ch = getopt(argc, argv, "dqsv")) != EOF)
		switch((char)ch) {
		case 'd':
			delete++;
			break;
		case 'q':
			quiet++;
			break;
		case 's':
			secondary++;
			break;
		case 'v':
			verbose = XLV_PRINT_ALL;
			break;
		default:
			usage();
		}

	if (argc -= optind)
		usage();

	xlv_lab2_create_vh_info_from_hinv (&vh_list, NULL, &status);

	if (vh_list) {
		for (vh = vh_list; vh != NULL; vh = vh->next) {
			if (!quiet) {
				xlv_print_vh (vh, verbose, secondary);
			}
			if (delete) {
				status = xlv_delete_xlv_label(vh, vh_list);
				printf("Deleted XLV label on %s ... %s\n",
					vh->vh_pathname,
					(status == 0) ? "done" : "failed");
			}
		}
		exit_code = EXIT_SUCCESS;
	}
	else {
		printf("XLV labels not found on any disks.\n");
		exit_code = EXIT_NOLABEL;
	}

	exit(exit_code);
}
