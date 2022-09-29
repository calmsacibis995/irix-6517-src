/*
 * acctcvt/main.c
 *	main program and utility functions for acctcvt
 *
 * Copyright 1997, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Revision: 1.3 $"

#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <libgen.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include "common.h"


/* Number of records per page - for text_acctcom version. */
#define RECS_PER_PAGE  75

/* Global variables */
char *MyName;
int  align[4] = { 0, 3, 2, 1 };
unsigned long Debug = DEBUG_DEFAULT;

ash_t    ashcut = 0;
char    *cname = NULL;   /* Command name pattern to match. */
int      combine = 0;    /* Combine some output values - for text_acctcom
                            version. */
double   cpucut = 0;
char     fieldsep[FIELDSEP_LENGTH];  /* For text_awk version. */
int      hdr_page = 0;   /* Column heading on each page - for text_acctcom
			    version. */
accum_t  iocut = 0;
double   memcut = 0;
int      no_header = 0;  /* For text_acctcom version. */
int      option = 0;     /* Options for the text_acctcom version. */
int      rppage = 0;     /* Number of records per page - for text_acctcom
			    version. */
double   syscut = 0;


/* These variables are non-local strictly so they can be manipulated */
/* by a signal handler. Otherwise they should be considered private. */
static output_t filterout;
static output_t output;


/* Internal function prototypes */
static void  BrokenPipe();
static char *Cmset(char *);
static void  TrapSignal();


int
main(int argc, char **argv)
{
	header_t hdr;
	image_t  image;
	input_t	 input;
	int	 argerror = 0;
	int	 i;
	int	 inputhdr   = -1;
	int	 filterhdr  = 0;
	int	 filtermode = 0;
	int	 outputhdr  = -1;
	record_t *rec;

	/* Determine our name */
	MyName = strrchr(argv[0], '/');
	if (MyName == NULL) {
		MyName = argv[0];
	}
	else {
		MyName++;
	}

	/* Initialize the field separator to NULL. */
	fieldsep[0] = '\0';
	
	/* Initialize I/O descriptors to default values */
	bzero(&input, sizeof(input));
	input.source = in_none;
	input.format = fmt_none;

	bzero(&output, sizeof(output));
	output.dest   = out_none;
	output.format = fmt_none;

	bzero(&filterout, sizeof(filterout));
	filterout.dest   = out_none;
	filterout.format = fmt_none;

	/* Process command line arguments */
	for (i = 1;  i < argc;  ++i) {
		if (strcmp(argv[i], "-") == 0) {

			/* Implicit "--input_source -" */
			if (input.source != in_none) {
				fprintf(stderr,
					"%s: only one input source may be "
					    "specified\n",
					MyName);
				argerror = 1;
				break;
			}

			input.source = in_stdin;
		}
		else if (strcmp(argv[i], "-a") == 0) {
			option |= ASH;
		}
		else if (strcmp(argv[i], "-A") == 0) {
			char *ptr;
			long long value;

			++i;
			if (i >= argc) {
				fprintf(stderr,
					"%s: must specify an ashcut value\n",
					MyName);
				argerror = 1;
				break;
			}

			value = strtoll(argv[i], &ptr, 0);
			if (ptr == NULL || *ptr != '\0') {
				fprintf(stderr,
					"%s: invalid ashcut value \"%s\"\n",
					MyName, argv[i]);
				argerror = 1;
				break;
			}
			
			ashcut = value;
			option |= ASH;
		}
		else if (strcmp(argv[i], "-c") == 0) {
			option |= AIO;
		}
		else if (strcmp(argv[i], "-C") == 0) {
			char *ptr;
			double value;

			++i;
			if (i >= argc) {
				fprintf(stderr,
					"%s: must specify a cpucut value\n",
					MyName);
				argerror = 1;
				break;
			}

			value = strtod(argv[i], &ptr);
			if (ptr == NULL || *ptr != '\0') {
				fprintf(stderr,
					"%s: invalid cpucut value \"%s\"\n",
					MyName, argv[i]);
				argerror = 1;
				break;
			}
			
			cpucut = value;
		}
		else if (strcmp(argv[i], "-d") == 0) {
			char *ptr;
			unsigned long value;

			++i;
			if (i >= argc) {
				fprintf(stderr,
					"%s: must specify a debug value\n",
					MyName);
				argerror = 1;
				break;
			}

			value = strtoul(argv[i], &ptr, 0);
			if (ptr == NULL || *ptr != '\0') {
				fprintf(stderr,
					"%s: invalid debug value \"%s\"\n",
					MyName, argv[i]);
				argerror = 1;
				break;
			}

			Debug = value;
		}
		else if (strcmp(argv[i], "-e") == 0) {
			option |= STATUS;
		}
		else if (strcmp(argv[i], "-f") == 0  ||
			 strcmp(argv[i], "--output_destination") == 0)
		{
			++i;
			if (i >= argc) {
				fprintf(stderr,
					"%s: must specify an output "
					    "destination\n",
					MyName);
				argerror = 1;
				break;
			}

			if (output.dest != out_none) {
				fprintf(stderr,
					"%s: only one output destination may "
					    "be specified\n",
					MyName);
				argerror = 1;
				break;
			}

			if (strcmp(argv[i], "-") == 0) {
				output.dest = out_stdout;
			}
			else if (argv[i][0] == '|') {
				if (argv[i][1] == '\0') {
					fprintf(stderr,
						"%s: null output command\n",
						MyName);
					argerror = 1;
				}
				else {
					output.dest = out_cmd;
					output.name = argv[i] + 1;
				}
			}
			else {
				output.dest = out_file;
				output.name = argv[i];
			}
		}
		else if (strcmp(argv[i], "-fs") == 0) {
			++i;
			if (i >= argc) {
				fprintf(stderr,
					"%s: must specify a field separator\n",
					MyName);
				argerror = 1;
				break;
			}

			strncpy(fieldsep, argv[i], FIELDSEP_LENGTH);
			/* Null-terminate fieldsep in case the argument
			   specified is longer than FIELDSEP_LENGTH. */
			fieldsep[FIELDSEP_LENGTH - 1] = '\0';
		}
		else if (strcmp(argv[i], "-g") == 0) {
			option |= GID;
		}
		else if (strcmp(argv[i], "-h") == 0) {
			no_header = 1;
		}
		else if (strcmp(argv[i], "-H") == 0) {
			hdr_page = 1;
		}
		else if (strcmp(argv[i], "-i") == 0) {
			input.format = fmt_ea65;
			input.source = in_stdin;
			inputhdr = 0;
		}
		else if (strcmp(argv[i], "-i62") == 0) {
			input.format = fmt_ea62;
			inputhdr = 1;
		}
		else if (strcmp(argv[i], "-i62nh") == 0) {
			input.format = fmt_ea62;
			inputhdr = 0;
		}
		else if (strcmp(argv[i], "-i64") == 0) {
			input.format = fmt_ea64;
			inputhdr = 1;
		}
		else if (strcmp(argv[i], "-i64nh") == 0) {
			input.format = fmt_ea64;
			inputhdr = 0;
		}
		else if (strcmp(argv[i], "-i65") == 0) {
			input.format = fmt_ea65;
			inputhdr = 1;
		}
		else if (strcmp(argv[i], "-i65nh") == 0) {
			input.format = fmt_ea65;
			inputhdr = 0;
		}
		else if (strcmp(argv[i], "-isvr4") == 0) {
			input.format = fmt_svr4;
			inputhdr = 0;
		}
		else if (strcmp(argv[i], "-io") == 0) {
			option |= IORW;
		}
		else if (strcmp(argv[i], "-I") == 0) {
			char *ptr;
			unsigned long long value;

			++i;
			if (i >= argc) {
				fprintf(stderr,
					"%s: must specify an iocut value\n",
					MyName);
				argerror = 1;
				break;
			}

			value = strtoull(argv[i], &ptr, 0);
			if (ptr == NULL || *ptr != '\0') {
				fprintf(stderr,
					"%s: invalid iocut value \"%s\"\n",
					MyName, argv[i]);
				argerror = 1;
				break;
			}
			
			iocut = value;
		}
		else if (strcmp(argv[i], "-k") == 0) {
			option |= KCOREMIN;
		}
		else if (strcmp(argv[i], "-l") == 0) {
			option |= TTY;
		}
		else if (strcmp(argv[i], "-m") == 0) {
			option |= MEANSIZE;
		}
		else if (strcmp(argv[i], "-M") == 0) {
			char *ptr;
			double value;

			++i;
			if (i >= argc) {
				fprintf(stderr,
					"%s: must specify a memcut value\n",
					MyName);
				argerror = 1;
				break;
			}

			value = strtod(argv[i], &ptr);
			if (ptr == NULL || *ptr != '\0') {
				fprintf(stderr,
					"%s: invalid memcut value \"%s\"\n",
					MyName, argv[i]);
				argerror = 1;
				break;
			}
			
			memcut = value;
		}
		else if (strcmp(argv[i], "-n") == 0) {
			++i;
			if (i >= argc) {
				fprintf(stderr,
					"%s: must specify a command name\n",
					MyName);
				argerror = 1;
				break;
			}

			if ((cname = Cmset(argv[i])) == NULL) {
				argerror = 1;
				break;
			}
		}
		else if (strcmp(argv[i], "-o") == 0  ||
			 strcmp(argv[i], "--filter") == 0)
		{
			filtermode = 1;
			filterhdr  = 0;
		}
		else if (strcmp(argv[i], "-oh") == 0  ||
			 strcmp(argv[i], "--filter_hdr") == 0)
		{
			filtermode = 1;
			filterhdr  = 1;
		}
		else if (strcmp(argv[i], "-o62") == 0) {
			output.format = fmt_ea62;
			outputhdr = 1;
		}
		else if (strcmp(argv[i], "-o62nh") == 0) {
			output.format = fmt_ea62;
			outputhdr = 0;
		}
		else if (strcmp(argv[i], "-o64") == 0) {
			output.format = fmt_ea64;
			outputhdr = 1;
		}
		else if (strcmp(argv[i], "-o64nh") == 0) {
			output.format = fmt_ea64;
			outputhdr = 0;
		}
		else if (strcmp(argv[i], "-o65") == 0) {
			output.format = fmt_ea65;
			outputhdr = 1;
		}
		else if (strcmp(argv[i], "-o65nh") == 0) {
			output.format = fmt_ea65;
			outputhdr = 0;
		}
		else if (strcmp(argv[i], "-osvr4") == 0) {
			output.format = fmt_svr4;
			outputhdr = 0;
		}
		else if (strcmp(argv[i], "-otext") == 0) {
			output.format = fmt_text;
		}
		else if (strcmp(argv[i], "-oacctcom") == 0) {
			output.format = fmt_acctcom;
		}
		else if (strcmp(argv[i], "-oawk") == 0) {
			output.format = fmt_awk;
			outputhdr = 0;
		}
		else if (strcmp(argv[i], "-O") == 0) {
			char *ptr;
			double value;

			++i;
			if (i >= argc) {
				fprintf(stderr,
					"%s: must specify a syscut value\n",
					MyName);
				argerror = 1;
				break;
			}

			value = strtod(argv[i], &ptr);
			if (ptr == NULL || *ptr != '\0') {
				fprintf(stderr,
					"%s: invalid syscut value \"%s\"\n",
					MyName, argv[i]);
				argerror = 1;
				break;
			}
			
			syscut = value;
		}
		else if (strcmp(argv[i], "-p") == 0) {
			option |= PID;
		}
		else if (strcmp(argv[i], "-P") == 0) {
			option |= PRID;
		}
		else if (strcmp(argv[i], "-q") == 0) {
			Debug = 0;
		}
		else if (strcmp(argv[i], "-r") == 0) {
			char *ptr;
			unsigned long value;

			++i;
			if (i >= argc) {
				fprintf(stderr,
					"%s: must specify a records-per-page "
					"value\n",
					MyName);
				argerror = 1;
				break;
			}

			value = strtoul(argv[i], &ptr, 0);
			if (ptr == NULL || *ptr != '\0') {
				fprintf(stderr,
					"%s: invalid records-per-page value "
					"\"%s\"\n",
					MyName, argv[i]);
				argerror = 1;
				break;
			}

			rppage = (int) value;
		}
		else if (strcmp(argv[i], "-s") == 0) {
			option |= SPI;
		}
		else if (strcmp(argv[i], "-t") == 0) {
			option |= SEPTIME;
		}
		else if (strcmp(argv[i], "-v") == 0) {
			Debug = DEBUG_VERBOSE;
		}
		else if (strcmp(argv[i], "-V") == 0) {
			combine = 1;
		}
		else if (strcmp(argv[i], "-w") == 0) {
			option |= WAITTIME;
		}
		else if (strcmp(argv[i], "--input_format") == 0) {
			++i;
			if (i >= argc) {
				fprintf(stderr,
					"%s: must specify an input format\n",
					MyName);
				argerror = 1;
				break;
			}

			if (strcasecmp(argv[i], "extacct6.2") == 0) {
				input.format = fmt_ea62;
			}
			else if (strcasecmp(argv[i], "extacct6.4") == 0) {
				input.format = fmt_ea64;
			}
			else if (strcasecmp(argv[i], "extacct6.5") == 0) {
				input.format = fmt_ea65;
			}
			else if (strcasecmp(argv[i], "svr4") == 0) {
				input.format = fmt_svr4;
				inputhdr = 0;
			}
			else {
				fprintf(stderr,
					"%s: invalid input format \"%s\"\n",
					MyName, argv[i]);
				argerror = 1;
			}
		}
		else if (strcmp(argv[i], "--input_header") == 0) {
			inputhdr = 1;
		}
		else if (strcmp(argv[i], "--input_noheader") == 0) {
			inputhdr = 0;
		}
		else if (strcmp(argv[i], "--input_source") == 0) {
			++i;
			if (i >= argc) {
				fprintf(stderr,
					"%s: must specify an input source\n",
					MyName);
				argerror = 1;
				break;
			}

			if (input.source != in_none) {
				fprintf(stderr,
					"%s: only one input source may be "
					    "specified\n",
					MyName);
				argerror = 1;
				break;
			}

			if (strcmp(argv[i], "-") == 0) {
				input.source = in_stdin;
			}
			else {
				input.source = in_file;
				input.name = argv[i];
			}
		}
		else if (strcmp(argv[i], "--output_format") == 0) {
			++i;
			if (i >= argc) {
				fprintf(stderr,
					"%s; must specify an output format\n",
					MyName);
				argerror = 1;
				break;
			}

			if (strcasecmp(argv[i], "text") == 0) {
				output.format = fmt_text;
			}
			else if (strcasecmp(argv[i], "text_acctcom") == 0) {
				output.format = fmt_acctcom;
			}
			else if (strcasecmp(argv[i], "text_awk") == 0) {
				output.format = fmt_awk;
				outputhdr = 0;
			}
			else if (strcasecmp(argv[i], "extacct6.2") == 0) {
				output.format = fmt_ea62;
			}
			else if (strcasecmp(argv[i], "extacct6.4") == 0) {
				output.format = fmt_ea64;
			}
			else if (strcasecmp(argv[i], "extacct6.5") == 0) {
				output.format = fmt_ea65;
			}
			else if (strcasecmp(argv[i], "svr4") == 0) {
				output.format = fmt_svr4;
				outputhdr = 0;
			}
			else {
				fprintf(stderr,
					"%s: invalid output format \"%s\"\n",
					MyName, argv[i]);
				argerror = 1;
			}
		}
		else if (strcmp(argv[i], "--output_header") == 0) {
			outputhdr = 1;
		}
		else if (strcmp(argv[i], "--output_noheader") == 0) {
			outputhdr = 0;
		}
		else if (argv[i][0] != '-') {

			/* Implicit "--input_source <file>" */
			if (input.source != in_none) {
				fprintf(stderr,
					"%s: only one input source may be "
					    "specified\n",
					MyName);
				argerror = 1;
				break;
			}

			input.source = in_file;
			input.name = argv[i];
		}
		else {

			/* Don't know this argument */
			fprintf(stderr,
				"%s: invalid argument \"%s\"\n",
				MyName, argv[i]);
			argerror = 1;
		}
	}

	/* Set up implied or default arguments */
	if (input.source == in_none) {
		input.source = in_stdin;
	}
	if (input.format == fmt_none) {
		input.format = fmt_ea65;
	}

	if (output.dest == out_none) {
		output.dest = out_stdout;
	}
	if (output.format == fmt_none) {
		output.format = fmt_text;
	}

	if (inputhdr < 0) {
		if (input.format == fmt_svr4) {
			inputhdr = 0;
		}
		else {
			inputhdr = 1;
		}
	}

	if (outputhdr < 0) {
		if (output.format == fmt_text ||
		    output.format == fmt_acctcom) {
			if (inputhdr  &&  (Debug & DEBUG_DETAILS)) {
				outputhdr = 1;
			}
			else {
				outputhdr = 0;
			}
		}
		else {
			outputhdr = 0;
		}
	}

	/* Make sure there are no conflicting or missing arguments */
	if (filtermode  &&  output.dest == out_stdout) {
		fprintf(stderr,
			"%s: cannot specify -o/--filter with an output "
			    "destination of stdout\n",
			MyName);
		argerror = 1;
	}
	if (inputhdr == 1  &&  input.format == fmt_svr4) {
		fprintf(stderr,
			"%s: cannot specify --input_header with SVR4 format\n",
			MyName);
		argerror = 1;
	}
	if (outputhdr == 1  &&  output.format == fmt_svr4) {
		fprintf(stderr,
			"%s: cannot specify --output_header with SVR4 "
			    "format\n",
			MyName);
		argerror = 1;
	}
	if (outputhdr == 1  &&  output.format == fmt_awk) {
		fprintf(stderr,
			"%s: cannot specify --output_header with text_awk "
			    "format\n",
			MyName);
		argerror = 1;
	}
	if (output.format != fmt_acctcom &&
	    (option & MEANSIZE || option & KCOREMIN || option & SEPTIME ||
	     option & IORW || option & AIO || option & ASH || option & GID ||
	     option & PID || option & PRID || option & SPI ||
	     option & STATUS || option & TTY || option & WAITTIME ||
	     combine || hdr_page || no_header || rppage)) {
		fprintf(stderr,
			"%s: can only specify -a, -c, -e, -g, -h, -H, -io, "
			"-k, -l,\n         -m, -p, -P, -r, -s, -t, -V, and "
			"-w with the\n         text_acctcom output format\n",
			MyName);
		argerror = 1;
	}
	if (input.format == fmt_svr4 &&
	    output.format == fmt_acctcom &&
	    (option & AIO || option & ASH || option & PID || option & PRID ||
	     option & SPI || option & WAITTIME || combine)) {
		fprintf(stderr,
			"%s: cannot specify -a, -c, -p, -P, -s, -V, or -w "
			"with\n         SVR4 input format for the "
			"text_acctcom output format\n",
			MyName);
		argerror = 1;
	}
	
	/* NOTE - There is currently a bug in acctcvt in that it does not
	   process the SAT exit status value (SAT_STATUS_TOKEN).  When that
	   bug is resolved, the user will be able to specify -e with extacct
	   input format.  The man page should be updated then.
	   Feb. 26, 1999.
	*/
	if (input.format != fmt_svr4 &&
	    output.format == fmt_acctcom &&
	    option & STATUS) {
		fprintf(stderr,
			"%s: can only specify -e with SVR4 input format\n"
			"         for the text_acctcom output format\n",
			MyName);
		argerror = 1;
	}

	if (output.format != fmt_awk && fieldsep[0] != '\0') {
		fprintf(stderr,
			"%s: can only specify -fs with the text_awk output "
			"format\n",
			MyName);
		argerror = 1;
	}
	if (input.format == fmt_svr4 && output.format == fmt_awk) {
		fprintf(stderr,
			"%s: cannot specify text_awk output format with SVR4 "
			"input format\n",
			MyName);
		argerror = 1;
	}
	if (no_header && hdr_page) {
		fprintf(stderr,
			"%s: options -h and -H are mutually exclusive\n",
			MyName);
		argerror = 1;
	}
	if (rppage && !hdr_page) {
		fprintf(stderr,
			"%s: must specify -r with the -H option\n",
			MyName);
		argerror = 1;
	}
	if (combine &&
	    !(option & IORW || option & AIO || option & WAITTIME)) {
		fprintf(stderr,
			"%s: must specify -V with the -c, -io, or -w "
			"option\n",
			MyName);
		argerror = 1;
	}

	/* If any argument errors were encountered, bail out */
	if (argerror) {
		exit(1);
	}

	/* Do those assignments that can only be done after all the legality
	   checks for arguments.
	*/
	
	/* We have to set combine to TRUE here because the SVR4 records only
	   have combined I/O information (read and write). */
	if (input.format == fmt_svr4 && output.format == fmt_acctcom)
		combine = 1;

	/* If the user wants to have the column heading on each page, set the
	   number of records per page. */
	if (hdr_page && rppage == 0)
		rppage = RECS_PER_PAGE;

	/* Initialize the field separator to a blank if it hasn't been set. */
	if (output.format == fmt_awk && fieldsep[0] == '\0')
		strcpy(fieldsep, " ");
	
	/* Trap various signals so we can exit gracefully */
	signal(SIGHUP,  TrapSignal);
	signal(SIGINT,  TrapSignal);
	signal(SIGQUIT, TrapSignal);
	signal(SIGTERM, TrapSignal);

	/* Handle SIGPIPE at the errno level */
	signal(SIGPIPE, SIG_IGN);

	/* Open the input and output files */
	if (OpenInput(&input) != 0) {
		exit(2);
	}
	if (OpenOutput(&output) != 0) {
		exit(2);
	}

	/* If filter mode is on, set up an output file for that too */
	if (filtermode) {
		filterout.format = input.format;
		filterout.dest   = out_stdout;

		if (OpenOutput(&filterout) != 0) {
			exit(2);
		}
	}

	/* If we are being verbose, spill our guts */
	if (Debug & DEBUG_OPTS) {
		fprintf(stderr,
			"\nInput: F=%d S=%d N=%s\n",
			input.format,
			input.source,
			(input.source == in_file) ? input.name : "n/a");

		fprintf(stderr,
			"Output: F=%d D=%d N=%s\n",
			output.format,
			output.dest,
			(output.dest != out_stdout) ? output.name : "n/a");

		fprintf(stderr, "filtermode=%d\n", filtermode);
		fprintf(stderr, "\n");
	}

	/* XXX This would be a nice place to try to guess the input format */

	/* Read the input file header if present */
	if (inputhdr) {

		/* Read the file header into storage */
		if (ReadHeader(&input, &image) != 0) {
			exit(2);
		}

		/* Echo it unmodified if desired */
		if (filterhdr) {
			if (Write(&filterout, image.data, image.len) != 0) {
				exit(2);
			}
		}

		/* Parse it into a canonical header_t */
		if (ParseHeader(&image, &hdr) != 0) {
			exit(2);
		}
	}

	/* Write out a file header if necessary */
	if (outputhdr) {

		/* If there was no input header, build a fake one */
		if (!inputhdr) {
			if (BuildFakeHeader(&hdr) != 0) {
				exit(2);
			}
		}

		/* Write out our canonical header */
		if (WriteHeader(&output, &hdr) != 0) {
			exit(2);
		}
	}

	/* All done with the header now */
	if (inputhdr || outputhdr) {
		FreeHeader(&hdr);
	}

	/* Process records until there are none left */
	while (ReadRecord(&input, &image) == 0) {

		/* If we are in filter mode, copy the record directly */
		/* to stdout.					      */
		if (filtermode) {
			if (Write(&filterout, image.data, image.len) != 0) {
				fprintf(stderr,
					"%s: WARNING - unable to copy record "
					    "to stdout - %s\n",
					MyName,
					strerror(errno));
			}
		}

		/* If this is an accounting record, */
		/* or this is an custom record, process it further */
		if ((image.type == SAT_PROC_ACCT)  ||
		    (image.type == SAT_SESSION_ACCT) ||
		    (image.type == SAT_AE_CUSTOM) ) {
			record_t rec;

			/* Be verbose if desired */
			if (Debug & DEBUG_RECORD) {
				fprintf(stderr,
					"RECORD   Type %d   Length %d\n",
					image.type,
					image.len);
			}

			/* Parse the image into a record_t and write */
			/* it out in the desired format		     */
			if (ParseRecord(&image, &rec) == 0) {
				if (WriteRecord(&output, &rec) != 0) {
					exit(3);
				}

				FreeRecord(&rec);
			}
		}

		/* Otherwise, say we are skipping it if desired */
		else if (Debug & DEBUG_SKIPPED) {
			fprintf(stderr,
				"SKIPPING Type %d   Length %d\n",
				image.type,
				image.len);
		}
	}

	/* Final cleanup */
	CloseInput(&input);
	CloseOutput(&output);

	exit(0);
}


/*
 * Cmatch
 *	Check to see if the command matches with a regular expression
 *	pattern.
 */

char *
Cmatch(char *comm, char *cstr)
{
	char	 xcomm[9];
	register i;

	for (i = 0; i < 8; i++) {
		if (comm[i] == ' ' || comm[i] == '\0')
			break;
		xcomm[i] = comm[i];
	}
	xcomm[i] = '\0';

	return(regex(cstr, xcomm));
}


/*
 * GroupName
 *	Convert a GID to a group name string, cleverly caching values
 *	we have already seen before. Adapted brazenly from SAT.
 */
char *
GroupName(gid_t gid, char *buffer, fmt_t format)
{
	struct group_list {
		struct group_list	*l_next;
		char			*l_name;
		gid_t			l_gid;
	};
	static struct group_list *list = NULL;
	struct group_list *lp;
	struct group *grp;

	/* See if we have already cached this GID */
	for (lp = list;  lp != NULL;  lp = lp->l_next) {

		/* We have already converted this GID to a name */
		if (lp->l_gid == gid) {
			if (format == fmt_text)
				sprintf(buffer, "%-8s (%d)", lp->l_name, gid);
			else
				sprintf(buffer, "%s", lp->l_name);
			return buffer;
		}
	}

	/* Ask libc to convert the GID to a name for us */
	if ((grp = getgrgid(gid)) == NULL) {

		/* The GID is unknown, return only the numeric value */
		if (format == fmt_text)
			sprintf(buffer, "GID %d", gid);
		else
			sprintf(buffer, "%d", gid);
		return buffer;
	}

	/* Remember the name for this GID for later use */
	lp = malloc(sizeof(struct group_list));
	lp->l_gid = gid;
	lp->l_name = strdup(grp->gr_name);
	lp->l_next = list;
	list = lp;

	/* Return a nicely formatted string with the name and ID */
	if (format == fmt_text)
		sprintf(buffer, "%-8s (%d)", grp->gr_name, gid);
	else
		sprintf(buffer, "%s", grp->gr_name);
	return buffer;
}


/*
 * StartChildProcess
 *	Start a child process with the specified command line
 *	and return the file descriptor of the write end of a
 *	pipe connected to the child's stdin (or -1 if unsuccessful).
 */
FILE *
StartChildProcess(char *Command)
{
	int childpid;
	int i;
	int pipefds[2];

	/* Create a pipe for I/O to the child */
	if (pipe(pipefds) < 0) {
		fprintf(stderr,
			"%s: unable to create pipe - %s\n",
			MyName,
			strerror(errno));
		return NULL;
	}

	/* Spawn off a child process */
	childpid = fork();
	if (childpid < 0) {
		fprintf(stderr,
			"%s; unable to fork output command - %s\n",
			MyName,
			strerror(errno));
		return NULL;
	}

	/* If this is the child, start up the second command */
	if (childpid == 0) {
		char *Args[4];

		/* Close unneeded files */
		close(0);
		for (i = 3;  i < getdtablehi();  ++i) {
			if (i != pipefds[0]) {
				close(i);
			}
		}

		/* Make the read end of the pipe our stdin */
		if (dup2(pipefds[0], 0) < 0) {
			fprintf(stderr,
				"%s: unable to dup stdin for "
				"output command - %s\n",
				MyName,
				strerror(errno));
			exit(2);
		}

		/* Build an argument list for exec */
		Args[0] = _PATH_BSHELL;
		Args[1] = "-c";
		Args[2] = Command;
		Args[3] = NULL;

		/* exec the new program */
		execvp(Args[0], Args);
		fprintf(stderr,
			"%s: unable to start output command %s - %s\n",
			MyName,
			Args[0],
			strerror(errno));
		exit(2);
	}

	/* If we make it to here, we are the parent */

	/* No longer need the read end of the pipe */
	close(pipefds[0]);

	/* Create a FILE* from the write end of the pipe and return it */
	return fdopen(pipefds[1], "w");
}


/*
 * UserName
 *	Convert a UID to a user name string, cleverly caching values
 *	we have already seen before. Adapted brazenly from SAT.
 */
char *
UserName(uid_t uid, char *buffer, fmt_t format)
{
	struct passwd_list {
		struct passwd_list	*l_next;
		char			*l_name;
		uid_t			l_uid;
	};
	static struct passwd_list *list = NULL;
	struct passwd_list *lp;
	struct passwd *pwp;

	/* See if we have already cached this UID */
	for (lp = list;  lp != NULL;  lp = lp->l_next) {

		/* We have already converted this UID to a name */
		if (lp->l_uid == uid) {
			if (format == fmt_text)
				sprintf(buffer, "%-8s (%d)", lp->l_name, uid);
			else
				sprintf(buffer, "%s", lp->l_name);
			return buffer;
		}
	}

	/* Ask libc to convert the UID to a name for us */
	if ((pwp = getpwuid(uid)) == NULL) {

		/* The UID is unknown, return only the numeric value */
		if (format == fmt_text)
			sprintf(buffer, "UID %d", uid);
		else
			sprintf(buffer, "%d", uid);
		return buffer;
	}

	/* Remember the name for this UID for later use */
	lp = malloc(sizeof(struct passwd_list));
	lp->l_uid = uid;
	lp->l_name = strdup(pwp->pw_name);
	lp->l_next = list;
	list = lp;

	/* Return a nicely formatted string with the name and ID */
	if (format == fmt_text)
		sprintf(buffer, "%-8s (%d)", pwp->pw_name, uid);
	else
		sprintf(buffer, "%s", pwp->pw_name);
	return buffer;
}


/****************************************************************/
/*			PRIVATE FUNCTIONS			*/
/****************************************************************/


/*
 * BrokenPipe
 *	Come here if we get a SIGPIPE. For now, I guess we just die.
 */
static void
BrokenPipe(int SigNum)
{
	fprintf(stderr, "Broken pipe - unable to continue\n");
	exit(4);
}


/*
 * Cmset
 *	Compile the regular expression pattern for command.
 */
static char *
Cmset(char *pattern)
{
	char *compiled;
	
	if ((compiled = (char *) regcmp(pattern, (char *) 0)) == NULL) {
		fprintf(stderr, "%s: incorrect pattern syntax \"%s\"\n",
			MyName, pattern);
	}

	return compiled;
}


/*
 * TrapSignal
 *	Come here if we receive a "stop yourself" type of signal
 *	(e.g. SIGINT) so we can gracefully shutdown. Fortunately,
 *	most of "gracefully shutting down" is handled by exit(3)
 *	(as opposed to _exit(2), which is what would be invoked
 *	if we didn't trap these signals).
 */
static void
TrapSignal(int SigNum)
{
	exit(0);
}
