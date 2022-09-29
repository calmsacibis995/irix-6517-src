/*
 * xactl/xactl.c
 *	Extended Accounting Control
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

#ident "$Revision: 1.1 $"

#include <sys/types.h>

#include <sys/arsess.h>
#include <sys/capability.h>
#include <sys/extacct.h>
#include <sys/syssgi.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>


/*
 * Constants
 */
#define MAX_CMD_LEN	128
#define MY_NAME		"xactl"


/* Global options */
typedef struct options {
	char	*progname;		/* Name we were invoked with */
	char	*subcmd;		/* Name of requested subcommand */

	int	got;			/* Map of specified options */
	int	flags;			/* Binary option flags */
	ash_t	ash;			/* Array session handle */
	int	format;			/* SPI format */
	int	length;			/* SPI length */
	int	fill;			/* SPI fill byte */

	/* specific SPI fields */
	char	company[8];		/* spi_company */
	char	initiator[8];		/* spi_initiator */
	char	jobname[32];		/* spi_jobname */
	char	origin[16];		/* spi_origin */
	char	spi[16];		/* spi_spi */
	int64_t exectime;		/* spi_exectime */
	int64_t subtime;		/* spi_subtime */	
	int64_t waittime;		/* spi_waittime */
} options_t;

#define OPT_BRIEF	0x00000001	/* Brief output */
#define OPT_VERBOSE	0x00000002	/* Debugging output */

#define GOT_ASH		0x00000001
#define GOT_FORMAT	0x00000002
#define GOT_LENGTH	0x00000004
#define GOT_CLEAR	0x00000008
#define GOT_DEFAULT	0x00000010
#define GOT_FILL	0x00000020
#define GOT_COMPANY	0x00000040
#define GOT_INITIATOR	0x00000080
#define GOT_ORIGIN	0x00000100
#define GOT_SPI		0x00000200
#define GOT_JOBNAME	0x00000400
#define GOT_EXECTIME	0x00000800
#define GOT_SUBTIME	0x00001000
#define GOT_WAITTIME	0x00002000

#define GOT_FORMAT_0	(GOT_CLEAR | GOT_DEFAULT | GOT_FILL)
#define GOT_FORMAT_1	(GOT_COMPANY | GOT_INITIATOR | GOT_ORIGIN | GOT_SPI)
#define GOT_FORMAT_2	(GOT_JOBNAME | GOT_EXECTIME | GOT_SUBTIME | \
			 GOT_WAITTIME)
#define GOT_FORMAT_1_OR_2 (GOT_FORMAT_1 | GOT_FORMAT_2)


/* Command handlers */
void doALLOWNEW(options_t *);
void doFLUSHALLSESSIONS(options_t *);
void doFLUSHSESSION(options_t *);
void doGETDFLTSPI(options_t *);
void doGETDFLTSPILEN(options_t *);
void doGETSAF(options_t *);
void doGETSPI(options_t *);
void doGETSPILEN(options_t *);
void doRESTRICTNEW(options_t *);
void doSESSIONINFO(options_t *);
void doSETDFLTSPI(options_t *);
void doSETDFLTSPILEN(options_t *);
void doSETSAF(options_t *);
void doSETSPI(options_t *);
void doSETSPILEN(options_t *);

typedef struct cmdinfo {
	char *name;
	void (*handler)(options_t *);
	char *info;
} cmdinfo_t;

cmdinfo_t commands[] = {
	{ "ALLOWNEW",		doALLOWNEW,
	  "Allow array session to start new array sessions" },
	{ "FLUSHALLSESSIONS",	doFLUSHALLSESSIONS,
	  "Flush session accounting data for all array sessions" },
	{ "FLUSHSESSION",	doFLUSHSESSION,
	  "Flush session accounting data for an array session" },
	{ "GETDFLTSPI",		doGETDFLTSPI,
	  "Print default Service Provider Info" },	
	{ "GETDFLTSPILEN",	doGETDFLTSPILEN,
	  "Print default length of Service Provider Info" },
	{ "GETSAF",		doGETSAF,
	  "Print session accounting format" },
	{ "GETSPI",		doGETSPI,
	  "Get Service Provider Info for an array session" },
	{ "GETSPILEN",		doGETSPILEN,
	  "Get length of Service Provider Info for an array session" },
	{ "RESTRICTNEW",	doRESTRICTNEW,
	  "Restrict array session from starting new array sessions" },
	{ "SESSIONINFO",	doSESSIONINFO,
	  "Print session accounting information" },
	{ "SETDFLTSPI",		doSETDFLTSPI,
	  "Set the default Service Provider Info" },
	{ "SETDFLTSPILEN",	doSETDFLTSPILEN,
	  "Set the default length of Service Provider Info" },
	{ "SETSAF",		doSETSAF,
	  "Set the session accounting format" },
	{ "SETSPI",		doSETSPI,
	  "Set Service Provider Info for an array session" },
	{ "SETSPILEN",		doSETSPILEN,
	  "Set length of Service Provider Info for an array session" },
};

#define NUM_CMDS (sizeof(commands) / sizeof(commands[0]))


/* Utility functions */
void	assert_privilege(options_t *, cap_value_t);
int	build_spi(options_t *, char *, int);
int	parse_ash(char **, int, ash_t *);
int	parse_int(char **, int, int *, int, int);
int	parse_string(char **, int, char *, int);
int	parse_ticks(char **, int, int64_t *);
void	print_spi(options_t *, void *);
void	print_usage(void);

int
main(int argc, char **argv)
{
	cmdinfo_t *command = NULL;
	int	error = 0;
	int	i;
	options_t options;

	/* Initialize the options buffer */
	bzero(&options, sizeof(options));
	options.progname = argv[0];

	/* Be intolerant of too few arguments */
	if (argc < 2) {
		print_usage();
		exit(0);
	}

	/* Parse the generic command line args */
	for (i = 1;  i < argc;  ++i) {
		if (strcmp(argv[i], "-b") == 0  ||
		    strcmp(argv[i], "-brief") == 0)
		{
			options.flags |= OPT_BRIEF;
		}
		else if (strcmp(argv[i], "-clear") == 0) {
			options.got |= GOT_CLEAR;
			options.fill = '\0';
		}
		else if (strcmp(argv[i], "-company") == 0) {
			if (parse_string(argv, i, options.company, 8) == 0) {
				options.got |= GOT_COMPANY;
			}
			else {
				error = 1;
			}
			++i;
		}
		else if (strcmp(argv[i], "-default") == 0) {
			options.got |= GOT_DEFAULT;
			options.fill = '\0';
		}
		else if (strcmp(argv[i], "-exectime") == 0) {
			if (parse_ticks(argv, i, &options.exectime) == 0) {
				options.got |= GOT_EXECTIME;
			}
			else {
				error = 1;
			}
			++i;
		}
		else if (strcmp(argv[i], "-fill") == 0) {
			if (parse_int(argv, i, &options.fill, 0, 255) == 0) {
				options.got |= GOT_FILL;
			}
			else {
				error = 1;
			}
			++i;
		}
		else if (strcmp(argv[i], "-format") == 0) {
			if (parse_int(argv, i, &options.format, 0, 2) == 0) {
				options.got |= GOT_FORMAT;
			}
			else {
				error = 1;
			}
			++i;
		}
		else if (strcmp(argv[i], "-h") == 0  ||
			 strcmp(argv[i], "-ash") == 0)
		{
			if (parse_ash(argv, i, &options.ash) == 0) {
				options.got |= GOT_ASH;
			}
			else {
				error = 1;
			}
			++ i;
		}
		else if (strcmp(argv[i], "-initiator") == 0) {
			if (parse_string(argv, i, options.initiator, 8) == 0) {
				options.got |= GOT_INITIATOR;
			}
			else {
				error = 1;
			}
			++i;
		}
		else if (strcmp(argv[i], "-jobname") == 0) {
			if (parse_string(argv, i, options.jobname, 32) == 0) {
				options.got |= GOT_JOBNAME;
			}
			else {
				error = 1;
			}
			++i;
		}
		else if (strcmp(argv[i], "-length") == 0) {
			if (parse_int(argv, i, &options.length, 0, MAX_SPI_LEN)
			    == 0)
			{
				options.got |= GOT_LENGTH;
			}
			else {
				error = 1;
			}
			++i;
		}
		else if (strcmp(argv[i], "-origin") == 0) {
			if (parse_string(argv, i, options.origin, 16) == 0) {
				options.got |= GOT_ORIGIN;
			}
			else {
				error = 1;
			}
			++i;
		}
		else if (strcmp(argv[i], "-spi") == 0) {
			if (parse_string(argv, i, options.spi, 8) == 0) {
				options.got |= GOT_SPI;
			}
			else {
				error = 1;
			}
			++i;
		}
		else if (strcmp(argv[i], "-subtime") == 0) {
			if (parse_ticks(argv, i, &options.subtime) == 0) {
				options.got |= GOT_SUBTIME;
			}
			else {
				error = 1;
			}
			++i;
		}
		else if (strcmp(argv[i], "-v") == 0) {
			options.flags |= OPT_VERBOSE;
		}
		else if (strcmp(argv[i], "-waittime") == 0) {
			if (parse_ticks(argv, i, &options.waittime) == 0) {
				options.got |= GOT_WAITTIME;
			}
			else {
				error = 1;
			}
			++i;
		}
		else if (argv[i][0] == '-') {
			fprintf(stderr,
				"%s: \"%s\" is not a valid option\n",
				argv[0], argv[i]);
			error = 1;
		}
		else if (options.subcmd == NULL) {
			options.subcmd = argv[i];
		}
		else {
			fprintf(stderr,
				"%s: \"%s\" is extraneous, a subcommand has "
				    "already been specified\n",
				argv[0], argv[i]);
			error = 1;
		}
	}

	/* If no ASH was specified, use our own */
	if (!(options.got & GOT_ASH)) {
		options.ash = getash();
	}

	/* If we are being verbose, spill our guts */
	if (options.flags & OPT_VERBOSE) {
		printf("\n");
		printf("program: %s\n", options.progname);
		printf("subcmd:  %s\n",
		       (options.subcmd == NULL) ? "<none>" : options.subcmd);
		printf("brief:   %s\n",
		       (options.flags & OPT_BRIEF) ? "yes" : "no");
		printf("ASH:     0x%016llx\n", options.ash);

		printf("\n");
		printf("\"got\" flags: 0x%08x (0=%x 1=%x 2=%x)\n",
		       options.got,
		       options.got & GOT_FORMAT_0,
		       options.got & GOT_FORMAT_1,
		       options.got & GOT_FORMAT_2);

		if (options.got & GOT_FORMAT) {
			printf("-format    %d\n", options.format);
		}
		if (options.got & GOT_LENGTH) {
			printf("-length    %d\n", options.length);
		}

		if (options.got & GOT_CLEAR) {
			printf("-clear\n");
		}
		if (options.got & GOT_FILL) {
			printf("-fill      0x%02x\n", options.fill);
		}

		if (options.got & GOT_COMPANY) {
			printf("-company   \"%.8s\"\n", options.company);
		}
		if (options.got & GOT_INITIATOR) {
			printf("-initiator \"%.8s\"\n", options.initiator);
		}
		if (options.got & GOT_ORIGIN) {
			printf("-origin    \"%.16s\"\n", options.origin);
		}
		if (options.got & GOT_SPI) {
			printf("-spi       \"%.16s\"\n", options.spi);
		}

		if (options.got & GOT_JOBNAME) {
			printf("-jobname   \"%.32s\"\n", options.jobname);
		}
		if (options.got & GOT_EXECTIME) {
			printf("-exectime  0x%llx\n", options.exectime);
		}
		if (options.got & GOT_SUBTIME) {
			printf("-subtime   0x%llx\n", options.subtime);
		}
		if (options.got & GOT_WAITTIME) {
			printf("-waittime  0x%llx\n", options.waittime);
		}

		printf("\n");
	}

	/* Make sure a subcommand was specified */
	if (options.subcmd == NULL) {
		fprintf(stderr,
			"%s: no subcommand was specified\n",
			argv[0]);
		error = 1;
	}

	/* Make sure we recognize the subcommand */
	else {
		for (i = 0;  command == NULL  &&  i < NUM_CMDS;  ++i) {
			if (strcasecmp(options.subcmd, commands[i].name) == 0)
			{
				command = &commands[i];
			}
		}

		if (command == NULL) {
			fprintf(stderr,
				"%s: \"%s\" is not a valid subcommand\n",
				argv[0], options.subcmd);
			error = 1;
		}
	}

	/* Bail out now if a command-line error occurred */
	if (error) {
		print_usage();
		exit(1);
	}

	/* Call the appropriate handler */
	(command->handler)(&options);
	
	exit(0);
}

void
doALLOWNEW(options_t *options)
{
	/* Allow the array session to do newarraysess() */
	assert_privilege(options, CAP_ACCT_MGT);
	if (!(options->flags & OPT_BRIEF)) {
		printf("Allowing new array sessions in ASH 0x%016llx\n",
		       options->ash);
	}

	if (arsop(ARSOP_ALLOW_NEW, options->ash, NULL, 0) != 0) {
		perror("ARSOP_ALLOW_NEW");
		exit(2);
	}
}

void
doFLUSHALLSESSIONS(options_t *options)
{
	ash_t *ashlist;
	int   i;
	int   numashs;
	int   size;

	/* Make sure the user is allowed to do this */
	assert_privilege(options, CAP_ACCT_MGT);

	/* Allocate an initial block of storage for the array of ASHs */
	size = 200;
	ashlist = malloc(size * sizeof(ash_t));
	if (ashlist == NULL) {
		fprintf(stderr,
			"%s: unable to allocate storage for ASH list\n",
			options->progname);
		exit(2);
	}

	/* Keep syssgi'ing until we don't get an ENOMEM error */
	numashs = syssgi(SGI_ENUMASHS, ashlist, size);
	while (numashs < 0  &&  errno == ENOMEM) {
		size += 100;
		ashlist = realloc(ashlist, size * sizeof(ash_t));
		if (ashlist == NULL) {
			fprintf(stderr,
				"%s: unable to reallocate storage for %d "
				    "ASHs\n",
				options->progname, size);
			exit(2);
		}

		numashs = syssgi(SGI_ENUMASHS, ashlist, size);
	}

	/* If syssgi failed for another reason, bail out */
	if (numashs < 0) {
		perror("SGI_ENUMASHS");
		exit(2);
	}

	/* Say what we are about to do if desired */
	if (!(options->flags & OPT_BRIEF)) {
		printf("\nFlushing session accounting data for the following "
		           "array sessions:\n");
	}

	/* Invoke ARSOP_FLUSHACCT on each ASH in the list */
	for (i = 0;  i < numashs;  ++i) {
		if (!(options->flags & OPT_BRIEF)) {
			printf("    0x%016llx\n", ashlist[i]);
		}
		if (arsop(ARSOP_FLUSHACCT, ashlist[i], NULL, 0) != 0) {
			perror("ARSOP_FLUSHACCT");
			exit(2);
		}
	}
}

void
doFLUSHSESSION(options_t *options)
{
	/* Flush session accounting data for an array session */
	assert_privilege(options, CAP_ACCT_MGT);
	if (!(options->flags & OPT_BRIEF)) {
		printf("Flushing session accounting data for ASH 0x%016llx\n",
		       options->ash);
	}

	if (arsop(ARSOP_FLUSHACCT, options->ash, NULL, 0) != 0) {
		perror("ARSOP_FLUSHACCT");
		exit(2);
	}
}

void
doGETDFLTSPI(options_t *options)
{
	/* Print the default Service Provider Information */
	char spibuf[MAX_SPI_LEN];
	int  maxspilen;

	/* Determine the length of the service provider info if necessary */
	if (!(options->got & GOT_LENGTH)) {
		if (arsctl(ARSCTL_GETDFLTSPILEN,
			   &options->length,
			   sizeof(options->length)) != 0)
		{
			perror("ARSCTL_GETDFLTSPILEN");
			exit(2);
		}
	}

	/* Extract the default service provider info */
	if (arsctl(ARSCTL_GETDFLTSPI, spibuf, options->length) != 0) {
		perror("ARSCTL_GETDFLTSPI");
		exit(2);
	}

	/* Print it out */
	if (!(options->flags & OPT_BRIEF)) {
		printf("\nDefault service provider information\n");
		printf("------------------------------------\n");
	}
	print_spi(options, spibuf);
}

void
doGETDFLTSPILEN(options_t *options)
{
	/* Print the default Service Provider Information length */
	int length;

	/* Extract it from the kernel */
	if (arsctl(ARSCTL_GETDFLTSPILEN, &length, sizeof(length)) != 0) {
		perror("ARSCTL_GETDFLTSPILEN");
		exit(2);
	}

	/* Print it out in the desired style */
	if (options->flags & OPT_BRIEF) {
		printf("%d\n", length);
	}
	else {
		printf("Default length of service provider information "
		           "in new array sessions is %d\n",
		       length);
	}
}

void
doGETSAF(options_t *options)
{
	/* Print the session accounting format */
	int saf;

	/* Extract it from the kernel */
	if (arsctl(ARSCTL_GETSAF, &saf, sizeof(saf)) != 0) {
		perror("ARSCTL_GETSAF");
		exit(2);
	}

	/* Print it out in the desired style */
	if (options->flags & OPT_BRIEF) {
		printf("%d\n", saf);
	}
	else {
		printf("Session accounting records are being written in "
		           "format %d\n",
		       saf);
	}
}

void
doGETSPI(options_t *options)
{
	/* Print the Service Provider Information for an array session */
	char spibuf[MAX_SPI_LEN];
	int  maxspilen;

	/* Determine the length of the service provider info if necessary */
	if (!(options->got & GOT_LENGTH)) {
		if (arsop(ARSOP_GETSPILEN,
			  options->ash,
			  &options->length,
			  sizeof(options->length)) != 0)
		{
			perror("ARSOP_GETSPILEN");
			exit(2);
		}
	}

	/* Extract the default service provider info */
	if (arsop(ARSOP_GETSPI, options->ash, spibuf, options->length) != 0) {
		perror("ARSOP_GETSPI");
		exit(2);
	}

	/* Print it out */
	if (!(options->flags & OPT_BRIEF)) {
		printf("\nService provider information for ASH 0x%016llx:\n",
		       options->ash);
		printf("-----------------------------------------------"
		           "---------\n");
	}
	print_spi(options, spibuf);
}

void
doGETSPILEN(options_t *options)
{
	/* Print the Service Provider Information length of an array session */
	int length;

	/* Extract it from the kernel */
	if (arsop(ARSOP_GETSPILEN,
		  options->ash,
		  &length,
		  sizeof(length)) != 0)
	{
		perror("ARSOP_GETSPILEN");
		exit(2);
	}

	/* Print it out in the desired style */
	if (options->flags & OPT_BRIEF) {
		printf("%d\n", length);
	}
	else {
		printf("Maximum size of service provider information in ASH "
		           "0x%016llx is %d\n",
		       options->ash, length);
	}
}

void
doRESTRICTNEW(options_t *options)
{
	/* Restrict the array session from doing newarraysess() */
	if (!(options->flags & OPT_BRIEF)) {
		printf("Restricting new array sessions in ASH 0x%016llx\n",
		       options->ash);
	}

	if (arsop(ARSOP_RESTRICT_NEW, options->ash, NULL, 0) != 0) {
		perror("ARSOP_RESTRICT_NEW");
		exit(2);
	}
}

void
doSESSIONINFO(options_t *options)
{
	/* Print accounting information for an array session */
	arsess_t arsess;
	shacct_t shacct;

	/* Gather the information from the kernel */
	if (arsop(ARSOP_GETINFO, options->ash, &arsess, sizeof(arsess)) != 0) {
		perror("ARSOP_GETINFO");
		exit(2);
	}
	if ((arsess.as_flag & AS_GOT_CHGD_INFO)  &&
	    arsop(ARSOP_GETCHGD, options->ash, &shacct, sizeof(shacct)) != 0)
	{
		perror("ARSOP_GETCHGD");
		exit(2);
	}

	/* Print it out */
	if (options->flags & OPT_BRIEF) {
		printf("%lld\n", arsess.as_prid);
		printf("%s", ctime(&arsess.as_start));
		printf("%d\n", arsess.as_ticks);
		printf("%d\n", arsess.as_pid);
		printf("0x%08x\n", arsess.as_flag);
		printf("%d\n", arsess.as_nice);
		if (arsess.as_flag & AS_GOT_CHGD_INFO) {
			printf("%lld %lld\n",
			       arsess.as_timers.ac_utime,
			       shacct.sha_timers.ac_utime);
			printf("%lld %lld\n",
			       arsess.as_timers.ac_stime,
			       shacct.sha_timers.ac_stime);
			printf("%lld %lld\n",
			       arsess.as_timers.ac_bwtime,
			       shacct.sha_timers.ac_bwtime);
			printf("%lld %lld\n",
			       arsess.as_timers.ac_rwtime,
			       shacct.sha_timers.ac_rwtime);
			printf("%lld %lld\n",
			       arsess.as_timers.ac_qwtime,
			       shacct.sha_timers.ac_qwtime);
			printf("%lld %lld\n",
			       arsess.as_counts.ac_mem,
			       shacct.sha_counts.ac_mem);
			printf("%lld %lld\n",
			       arsess.as_counts.ac_swaps,
			       shacct.sha_counts.ac_swaps);
			printf("%lld %lld\n",
			       arsess.as_counts.ac_chr,
			       shacct.sha_counts.ac_chr);
			printf("%lld %lld\n",
			       arsess.as_counts.ac_chw,
			       shacct.sha_counts.ac_chw);
			printf("%lld %lld\n",
			       arsess.as_counts.ac_br,
			       shacct.sha_counts.ac_br);
			printf("%lld %lld\n",
			       arsess.as_counts.ac_bw,
			       shacct.sha_counts.ac_bw);
			printf("%lld %lld\n",
			       arsess.as_counts.ac_syscr,
			       shacct.sha_counts.ac_syscr);
			printf("%lld %lld\n",
			       arsess.as_counts.ac_syscw,
			       shacct.sha_counts.ac_syscw);
		}
		else {
			printf("%lld\n",
			       arsess.as_timers.ac_utime);
 			printf("%lld\n",
 			       arsess.as_timers.ac_stime);
			printf("%lld\n",
			       arsess.as_timers.ac_bwtime);
			printf("%lld\n",
			       arsess.as_timers.ac_rwtime);
			printf("%lld\n",
			       arsess.as_timers.ac_qwtime);
			printf("%lld\n",
			       arsess.as_counts.ac_mem);
			printf("%lld\n",
			       arsess.as_counts.ac_swaps);
			printf("%lld\n",
			       arsess.as_counts.ac_chr);
			printf("%lld\n",
			       arsess.as_counts.ac_chw);
			printf("%lld\n",
			       arsess.as_counts.ac_br);
			printf("%lld\n",
			       arsess.as_counts.ac_bw);
			printf("%lld\n",
			       arsess.as_counts.ac_syscr);
			printf("%lld\n",
			       arsess.as_counts.ac_syscw);
		}
	}
	else {
		printf("\nInformation about ASH 0x%016llx:\n", options->ash);
		printf("-----------------------------------------\n");
		printf("Project ID:     %lld\n", arsess.as_prid);
		printf("Start time:     %s", ctime(&arsess.as_start));
		printf("Total ticks:    %d\n", arsess.as_ticks);
		printf("Initial PID:    %d\n", arsess.as_pid);

		printf("Flags:          0x%08x   ", arsess.as_flag);
		if (arsess.as_flag & AS_NEW_RESTRICTED) {
			printf(" NEW_RESTRICTED");
		}
		if (arsess.as_flag & AS_GOT_CHGD_INFO) {
			printf(" GOT_CHGD_INFO");
		}
		if (arsess.as_flag & AS_FLUSHED) {
			printf(" FLUSHED");
		}
		printf("\n");

		printf("Initial nice:   %d\n", arsess.as_nice);
		printf("\n");
		if (arsess.as_flag & AS_GOT_CHGD_INFO) {
			printf("User time:      %14lld ns "
			           "(%lld ns already charged)\n",
			       arsess.as_timers.ac_utime,
			       shacct.sha_timers.ac_utime);
			printf("System time:    %14lld ns "
			           "(%lld ns already charged)\n",
			       arsess.as_timers.ac_stime,
			       shacct.sha_timers.ac_stime);
			printf("Block I/O wait: %14lld ns "
			           "(%lld ns already charged)\n",
			       arsess.as_timers.ac_bwtime,
			       shacct.sha_timers.ac_bwtime);
			printf("Raw I/O wait:   %14lld ns "
			           "(%lld ns already charged)\n",
			       arsess.as_timers.ac_rwtime,
			       shacct.sha_timers.ac_rwtime);
			printf("Run queue wait: %14lld ns "
			           "(%lld ns already charged)\n",
			       arsess.as_timers.ac_qwtime,
			       shacct.sha_timers.ac_qwtime);
			printf("\n");
			printf("Mem integral:   %8lld "
			           "(%lld already charged)\n",
			       arsess.as_counts.ac_mem,
			       shacct.sha_counts.ac_mem);
			printf("# swaps:        %8lld "
			           "(%lld already charged)\n",
			       arsess.as_counts.ac_swaps,
			       shacct.sha_counts.ac_swaps);
			printf("Bytes read:     %8lld "
			           "(%lld already charged)\n",
			       arsess.as_counts.ac_chr,
			       shacct.sha_counts.ac_chr);
			printf("Bytes written:  %8lld "
			           "(%lld already charged)\n",
			       arsess.as_counts.ac_chw,
			       shacct.sha_counts.ac_chw);
			printf("Blocks read:    %8lld "
			           "(%lld already charged)\n",
			       arsess.as_counts.ac_br,
			       shacct.sha_counts.ac_br);
			printf("Blocks written: %8lld "
			           "(%lld already charged)\n",
			       arsess.as_counts.ac_bw,
			       shacct.sha_counts.ac_bw);
			printf("Read syscalls:  %8lld "
			           "(%lld already charged)\n",
			       arsess.as_counts.ac_syscr,
			       shacct.sha_counts.ac_syscr);
			printf("Write syscalls: %8lld "
			           "(%lld already charged)\n",
			       arsess.as_counts.ac_syscw,
			       shacct.sha_counts.ac_syscw);
			printf("\n");
		}
		else {
			printf("User time:      %lld ns\n",
			       arsess.as_timers.ac_utime);
			printf("System time:    %lld ns\n",
			       arsess.as_timers.ac_stime);
			printf("Block I/O wait: %lld ns\n",
			       arsess.as_timers.ac_bwtime);
			printf("Raw I/O wait:   %lld ns\n",
			       arsess.as_timers.ac_rwtime);
			printf("Run queue wait: %lld ns\n",
			       arsess.as_timers.ac_qwtime);
			printf("\n");
			printf("Mem integral:   %lld\n",
			       arsess.as_counts.ac_mem);
			printf("# swaps:        %lld\n",
			       arsess.as_counts.ac_swaps);
			printf("Bytes read:     %lld\n",
			       arsess.as_counts.ac_chr);
			printf("Bytes written:  %lld\n",
			       arsess.as_counts.ac_chw);
			printf("Blocks read:    %lld\n",
			       arsess.as_counts.ac_br);
			printf("Blocks written: %lld\n",
			       arsess.as_counts.ac_bw);
			printf("Read syscalls:  %lld\n",
			       arsess.as_counts.ac_syscr);
			printf("Write syscalls: %lld\n",
			       arsess.as_counts.ac_syscw);
			printf("\n");
		}
	}
}

void
doSETDFLTSPI(options_t *options)
{
	/* Set the default service provider information */
	char spibuf[MAX_SPI_LEN];
	int  maxspilen;

	/* Make sure the user is allowed to do this */
	assert_privilege(options, CAP_SYSINFO_MGT);

	/* Determine the maximum length of the service provider info */
	if (arsctl(ARSCTL_GETDFLTSPILEN, &maxspilen, sizeof(maxspilen)) != 0) {
		perror("ARSCTL_GETDFLTSPILEN");
		exit(2);
	}

	/* Make sure the user's length isn't too big */
	if (options->got & GOT_LENGTH) {
		if (options->length > maxspilen) {
			fprintf(stderr,
				"%s: specified length %d exceeds the current "
				    "maximum of %d\n",
				options->progname, options->length, maxspilen);
			exit(1);
		}
	}
	else {
		options->length = maxspilen;
		options->got |= GOT_LENGTH;
	}

	/* Start with a clean buffer */
	bzero(spibuf, options->length);

	/* Build the service provider information */
	if (build_spi(options, spibuf, options->length) < 0) {
		exit(1);
	}

	/* Be verbose if necessary */
	if (!(options->flags & OPT_BRIEF)) {
		printf("\nAbout to set the following default service provider "
		           "information:\n");
		printf("----------------------------------------------------"
		           "------------\n");
		print_spi(options, spibuf);
	}

	/* Do it */
	if (arsctl(ARSCTL_SETDFLTSPI, spibuf, options->length) != 0) {
		perror("ARSCTL_SETDFLTSPI");
		exit(2);
	}
}

void
doSETDFLTSPILEN(options_t *options)
{
	/* Set the default length of service provider information */

	/* Make sure the user is allowed to do this */
	assert_privilege(options, CAP_SYSINFO_MGT);

	/* Make sure a length was specified in the first place */
	if (!(options->got & GOT_LENGTH)) {
		fprintf(stderr,
			"%s: the -length option is required for "
			    "setdfltspilen\n",
			options->progname);
		exit(1);
	}

	/* Be verbose if necessary */
	if (!(options->flags & OPT_BRIEF)) {
		printf("Setting default length of service provider "
		           "information to %d\n",
		       options->length);
	}

	/* Do it */
	if (arsctl(ARSCTL_SETDFLTSPILEN,
		   &options->length,
		   sizeof(options->length)) != 0)
	{
		perror("ARSCTL_SETDFLTSPILEN");
		exit(2);
	}
}

void
doSETSAF(options_t *options)
{
	/* Set the session accounting format */

	/* Make sure the user is allowed to do this */
	assert_privilege(options, CAP_SYSINFO_MGT);

	/* Make sure a format was specified in the first place */
	if (!(options->got & GOT_FORMAT)) {
		fprintf(stderr,
			"%s: the -format option is required for setsaf",
			options->progname);
		exit(1);
	}

	/* The initial command-line options parsing allows format "0" */
	/* but that isn't a valid session accounting format.	      */
	if (options->format < 1  ||  options->format > 2) {
		fprintf(stderr,
			"%s: %d is not a valid session accounting format\n",
			options->progname, options->format);
		exit(1);
	}

	/* Be verbose if necessary */
	if (!(options->flags & OPT_BRIEF)) {
		printf("Setting the session accounting format to %d\n",
		       options->format);
	}

	/* Do it */
	if (arsctl(ARSCTL_SETSAF, &options->format, sizeof(options->format))
	    != 0)
	{
		perror("ARSCTL_SETSAF");
		exit(2);
	}
}

void
doSETSPI(options_t *options)
{
	/* Set the service provider information for an array session */
	char spibuf[MAX_SPI_LEN];
	int  maxspilen;

	/* Make sure the user is allowed to do this */
	assert_privilege(options, CAP_ACCT_MGT);

	/* Determine the maximum length of the service provider info */
	if (arsop(ARSOP_GETSPILEN,
		  options->ash,
		  &maxspilen,
		  sizeof(maxspilen)) != 0)
	{
		perror("ARSOP_GETSPILEN");
		exit(2);
	}

	/* Make sure the user's length isn't too big */
	if (options->got & GOT_LENGTH) {
		if (options->length > maxspilen) {
			fprintf(stderr,
				"%s: specified length %d exceeds the current "
				    "maximum of %d\n",
				options->progname, options->length, maxspilen);
			exit(1);
		}
	}
	else {
		options->length = maxspilen;
		options->got |= GOT_LENGTH;
	}

	/* Start with a clean buffer */
	bzero(spibuf, options->length);

	/* Build the service provider information */
	if (build_spi(options, spibuf, options->length) < 0) {
		exit(1);
	}

	/* Proceed differently if -default was specified */
	if (options->got & GOT_DEFAULT) {
		if (!(options->flags & OPT_BRIEF)) {
			printf("Restoring default service provider "
			           "information for ASH 0x%016llx\n",
			       options->ash);
		}
		if (arsop(ARSOP_SETSPI, options->ash, NULL, 0) != 0) {
			perror("ARSOP_SETSPI");
			exit(2);
		}
	}
	else {
		/* Be verbose if necessary */
		if (!(options->flags & OPT_BRIEF)) {
			printf("\nAbout to set service provider information "
			           "for ASH 0x%016llx to:\n",
			       options->ash);
			printf("----------------------------------------------"
			           "--------------------------\n");
			print_spi(options, spibuf);
		}

		/* Do it */
		if (arsop(ARSOP_SETSPI, options->ash, spibuf, options->length)
		    != 0)
		{
			perror("ARSOP_SETSPI");
			exit(2);
		}
	}
}

void
doSETSPILEN(options_t *options)
{
	/* Set the length of service provider info for an array session */

	/* Make sure the user is allowed to do this */
	assert_privilege(options, CAP_ACCT_MGT);

	/* Make sure a length was specified in the first place */
	if (!(options->got & GOT_LENGTH)) {
		fprintf(stderr,
			"%s: the -length option is required for "
			    "setspilen\n",
			options->progname);
		exit(1);
	}

	/* Be verbose if necessary */
	if (!(options->flags & OPT_BRIEF)) {
		printf("Setting size of service provider "
		           "information for ASH 0x%016llx to %d\n",
		       options->ash,
		       options->length);
	}

	/* Do it */
	if (arsop(ARSOP_SETSPILEN,
		  options->ash,
		  &options->length,
		  sizeof(options->length)) != 0)
	{
		perror("ARSOP_SETSPILEN");
		exit(2);
	}
}


/*
 * assert_privilege
 *	Checks if the user has the specified capability. If not,
 *	an error message is printed and the program exits. Code
 *	stolen brazenly from mkpart (which got it from su?).
 */
void
assert_privilege(options_t *options, cap_value_t cap)
{
	cap_t cap_state;
	int cap_enabled;

	/* Find out what style of capabilities are done here */
	cap_enabled = sysconf(_SC_CAP);
	if (cap_enabled < 0) {
                cap_enabled = CAP_SYS_DISABLED;
	}

	/* In a strict or augmented superuser environment, an effective */
	/* UID of 0 (root) is proof enough				*/
	if (cap_enabled == CAP_SYS_DISABLED  ||
	    cap_enabled == CAP_SYS_SUPERUSER)
	{
		if (geteuid() == 0) {
			return;		/* Success */
		}

		fprintf(stderr,
			"%s: you must be root to use that function\n",
			options->progname);
		exit(1);
	}

	/* In a no-superuser environment, we must check the specific */
	/* capabilities						     */
	cap_state = cap_get_proc();
	if (cap_state == NULL) {
		perror("cap_get_proc");
		exit(2);
	}
	if (!CAP_ID_ISSET(cap, cap_state->cap_effective)) {
		fprintf(stderr,
			"%s: you must have %s capability to use that "
			    "function\n",
			options->progname,
			cap_value_to_text(cap));
		exit(1);
	}

	return;		/* Success */
}


/*
 * build_spi
 *	Given an options_t with command-line options, build a buffer
 *	containing the corresponding service provider information.
 *	An error will occur if the buffer is not large enough to hold
 *	the specified fields, if incompatible options are specified,
 *	or if an invalid value is specified. Returns 0 if successful,
 *	-1 if not.
 */
int
build_spi(options_t *options, char *spibuf, int buflen)
{
	/* Infer the format from the specified options if necessary */
	if (!(options->got & GOT_FORMAT)) {
		if (options->got & GOT_FORMAT_0) {
			options->format = 0;
		}
		else if (options->got & GOT_FORMAT_2) {
			options->format = 2;
		}
		else {
			options->format = 1;
		}

		if (options->flags & OPT_VERBOSE) {
			printf("Inferred SPI format %d\n\n", options->format);
		}
	}


	/* Infer the length from the format if necessary */
	if (!(options->got & GOT_LENGTH)) {
		switch (options->format) {

		    case 0:
			options->length = buflen;
			break;

		    case 1:
			options->length = sizeof(acct_spi_t);
			break;

		    case 2:
			options->length = sizeof(acct_spi_2_t);
			break;

		    default:
			fprintf(stderr,
				"%s: internal error at line %d\n",
				options->progname, __LINE__);
			break;
		}

		if (options->flags & OPT_VERBOSE) {
			printf("Inferred SPI length %d\n\n", options->length);
		}
	}

	/* Make sure no conflicting options were specified */
	if (((options->got & GOT_FORMAT_0)  &&
	     (options->got & GOT_FORMAT_1_OR_2))
	    ||
	    ((options->got & GOT_FILL)  &&
	     (options->got & GOT_CLEAR)  &&
	     (options->fill != 0))
	    ||
	    ((options->got & GOT_DEFAULT)  &&
	     (options->got & GOT_FILL))
	    ||
	    ((options->got & GOT_DEFAULT)  &&
	     (options->got & GOT_CLEAR)))
	{
		fprintf(stderr,
			"%s: conflicting service provider info options\n",
			options->progname);
		return -1;
	}

	/* If an explicit format was specified, don't allow options from */
	/* other formats						 */
	if (options->got & GOT_FORMAT) {
		if ((options->format == 0  &&
		     (options->got & GOT_FORMAT_1_OR_2))
		    ||
		    (options->format == 1  &&
		     (options->got & GOT_FORMAT_0))
		    ||
		    (options->format == 1  &&
		     (options->got & GOT_FORMAT_2))
		    ||
		    (options->format == 2  &&
		     (options->got & GOT_FORMAT_0)))
		{
			fprintf(stderr,
				"%s: service provider options inconsistent "
				    "with format %d were specified\n",
				options->progname, options->format);
			return -1;
		}
	}

	/* Make sure the length is consistent with the format */
	if ((options->format == 1  &&
	     options->length < sizeof(acct_spi_t))
	    ||
	    (options->format == 2  &&
	     options->length < sizeof(acct_spi_2_t)))
	{
		fprintf(stderr,
			"%s: SPI length too small (%d bytes) for format %d\n",
			options->progname, options->length, options->format);
		return -1;
	}

	/* It is now safe to start filling the SPI buffer with data */
	switch (options->format) {

	    case 0:
		memset(spibuf, options->fill, options->length);
		break;

	    case 1:
	    {
		    acct_spi_t *s = (acct_spi_t *) spibuf;

		    strncpy(s->spi_company,
			    options->company,
			    sizeof(options->company));
		    strncpy(s->spi_initiator,
			    options->initiator,
			    sizeof(options->initiator));
		    strncpy(s->spi_origin,
			    options->origin,
			    sizeof(options->origin));
		    strncpy(s->spi_spi,
			    options->spi,
			    sizeof(options->spi));

		    break;
	    }

	    case 2:
	    {
		    acct_spi_2_t *s = (acct_spi_2_t *) spibuf;

		    strncpy(s->spi_company,
			    options->company,
			    sizeof(options->company));
		    strncpy(s->spi_initiator,
			    options->initiator,
			    sizeof(options->initiator));
		    strncpy(s->spi_origin,
			    options->origin,
			    sizeof(options->origin));
		    strncpy(s->spi_spi,
			    options->spi,
			    sizeof(options->spi));
		    strncpy(s->spi_jobname,
			    options->jobname,
			    sizeof(options->jobname));
		    s->spi_exectime = options->exectime;
		    s->spi_subtime = options->subtime;
		    s->spi_waittime = options->waittime;

		    break;
	    }

	    default:
		fprintf(stderr,
			"%s: internal error at line %d",
			options->progname, __LINE__);
		exit(2);
	}

	return 0;
}


/*
 * parse_ash
 *	Parse an ASH argument to a command line option. The specified
 *	arg number should be the index of the option itself, the ASH
 *	is assumed to be the next argument. An error occurs if the
 *	argument does not exist or is invalid. Returns 0 if successful,
 *	or -1 if not.
 */
int
parse_ash(char **argv, int optnum, ash_t *resultp)
{
	char  *ptr = NULL;
	int   argnum = optnum + 1;
	ash_t value;

	if (argv[argnum] == NULL) {
		fprintf(stderr,
			"%s: %s requires an argument\n",
			argv[0], argv[optnum]);
		return -1;
	}

	errno = 0;
	value = strtoll(argv[argnum], &ptr, 0);
	if (ptr == NULL  ||  *ptr != '\0'  ||  errno != 0  ||  value < 0LL) {
		fprintf(stderr,
			"%s: \"%s\" is not a valid ASH argument for %s\n",
			argv[0],
			argv[argnum],
			argv[optnum]);
		return -1;
	}

	*resultp = value;
	return 0;
}


/*
 * parse_int
 *	Parse an integer argument to a command line option. The specified
 *	arg number should be the index of the option itself, the integer
 *	is assumed to be the next argument. An error occurs if the
 *	argument does not exist, is invalid, or is outside of the
 *	specified range. Returns 0 if successful or -1 if not.
 */
int
parse_int(char **argv, int optnum, int *resultp, int minval, int maxval)
{
	char *ptr = NULL;
	int  argnum = optnum + 1;
	long value;

	if (argv[argnum] == NULL) {
		fprintf(stderr,
			"%s: %s requires an argument\n",
			argv[0], argv[optnum]);
		return -1;
	}

	errno = 0;
	value = strtol(argv[argnum], &ptr, 0);
	if (ptr == NULL  ||  *ptr != '\0'  ||  errno != 0) {
		fprintf(stderr,
			"%s: \"%s\" is not a valid integer argument for %s\n",
			argv[0], argv[argnum], argv[optnum]);
		return -1;
	}

	if (value < minval  ||  value > maxval) {
		fprintf(stderr,
			"%s: the argument for %s must be between %d and %d\n",
			argv[0], argv[optnum], minval, maxval);
		return -1;
	}

	*resultp = (int) value;
	return 0;
}


/*
 * parse_string
 *	Parse a string argument to a command line option. The specified
 *	arg number should be the index of the option itself, the string
 *	is assumed to be the next argument. An error occurs if the
 *	string does not exist or is bigger than the specified buffer.
 *	Returns 0 if successful, -1 if not.
 */
int
parse_string(char **argv, int optnum, char *buffer, int maxlen)
{
	int strnum = optnum + 1;

	if (argv[strnum] == NULL) {
		fprintf(stderr,
			"%s: %s requires an argument\n",
			argv[0], argv[optnum]);
		return -1;
	}

	if (strlen(argv[strnum]) > maxlen) {
		fprintf(stderr,
			"%s: argument for %s (\"%s\") is too long\n",
			argv[0],
			argv[optnum],
			argv[strnum]);
		return -1;
	}

	strncpy(buffer, argv[strnum], maxlen);

	return 0;
}


/*
 * parse_ticks
 *	Parse a "ticks" argument to a command line option. The specified
 *	arg number should be the index of the option itself, the "ticks"
 *	value is assumed to be the next argument. An error occurs if the
 *	argument does not exist, or is invalid. Returns 0 if successful,
 *	or -1 if not.
 */
int
parse_ticks(char **argv, int optnum, int64_t *resultp)
{
	char *ptr = NULL;
	int  argnum = optnum + 1;
	int64_t value;

	if (argv[argnum] == NULL) {
		fprintf(stderr,
			"%s: %s requires an argument\n",
			argv[0], argv[optnum]);
		return -1;
	}

	errno = 0;
	value = strtoll(argv[argnum], &ptr, 0);
	if (ptr == NULL  ||  *ptr != '\0'  ||  errno != 0) {
		fprintf(stderr,
			"%s: \"%s\" is not a valid argument for %s\n",
			argv[0], argv[argnum], argv[optnum]);
		return -1;
	}

	*resultp = value;
	return 0;
}


/*
 * print_spi
 *	Print the specified service provider information in human-readable
 *	format.
 */
void
print_spi(options_t *options, void *bufptr)
{
	int format;

	/* Infer the format of the service provider info if necessary */
	if (!(options->got & GOT_FORMAT)) {
		if (options->length == sizeof(acct_spi_t)) {
			format = 1;
		}
		else if (options->length == sizeof(acct_spi_2_t)) {
			format = 2;
		}
		else {
			format = 0;
		}
	}
	else {
		format = options->format;
	}

	/* Proceed according to the format */
	switch (format) {

	    case 0:
	    {
		    char *spibuf = (char *) bufptr;
		    int i ;

		    for (i = 0;  i < options->length;  ++i) {
			    if (i % 16  == 0) {
				    printf("\n");
			    }
			    else if (i % 4  == 0) {
				    printf(" ");
			    }

			    printf("%02x", spibuf[i]);
		    }
		    printf("\n");

		    break;
	    }

	    case 1:
	    {
		    acct_spi_t *spi = (acct_spi_t *) bufptr;

		    if (options->flags & OPT_BRIEF) {
			    printf("%s\n", spi->spi_company);
			    printf("%s\n", spi->spi_initiator);
			    printf("%s\n", spi->spi_origin);
			    printf("%s\n", spi->spi_spi);
		    }
		    else {
			    printf("Company  : %.8s\n", spi->spi_company);
			    printf("Initiator: %.8s\n", spi->spi_initiator);
			    printf("Origin   : %.16s\n", spi->spi_origin);
			    printf("SPI      : %.16s\n", spi->spi_spi);
		    }

		    break;
	    }

	    case 2:
	    {
		    acct_spi_2_t *spi = (acct_spi_2_t *) bufptr;

		    if (options->flags & OPT_BRIEF) {
			    printf("%s\n", spi->spi_company);
			    printf("%s\n", spi->spi_initiator);
			    printf("%s\n", spi->spi_origin);
			    printf("%s\n", spi->spi_spi);
			    printf("%s\n", spi->spi_jobname);
			    printf("%lld\n", spi->spi_subtime);
			    printf("%lld\n", spi->spi_exectime);
			    printf("%lld\n", spi->spi_waittime);
		    }
		    else {
			    printf("Company  : %.8s\n", spi->spi_company);
			    printf("Initiator: %.8s\n", spi->spi_initiator);
			    printf("Origin   : %.16s\n", spi->spi_origin);
			    printf("SPI      : %.16s\n", spi->spi_spi);
			    printf("Job Name : %.32s\n", spi->spi_jobname);
			    printf("Sub Time : %lld\n", spi->spi_subtime);
			    printf("Exec Time: %lld\n", spi->spi_exectime);
			    printf("Wait Time: %lld\n", spi->spi_waittime);
		    }

		    break;
	    }

	    default:
	    {
		    fprintf(stderr,
			    "%s: internal error at line %d\n",
			    options->progname, __LINE__);
		    exit(2);
	    }
	}
}


/*
 * print_usage
 *	Print a usage message to stderr
 */
void
print_usage(void)
{
	int i;

	fprintf(stderr,
		"\nUsage: " MY_NAME " <options> <subcmd> [<args>...]\n");
	fprintf(stderr, "options:\n");
	fprintf(stderr, "    -ash <array-session-handle>\n");
	fprintf(stderr, "    -brief\n");
	fprintf(stderr, "    -format <fmt>\n");
	fprintf(stderr, "    -length <len>\n");
	fprintf(stderr, "subcommands:\n");

	for (i = 0;  i < NUM_CMDS;  ++i) {
		fprintf(stderr, "    %-16s %s\n",
			commands[i].name,
			commands[i].info);
	}

	fprintf(stderr, "Specifying Service Provider Info:\n");
	fprintf(stderr, "  format 0 (unformatted):\n");
	fprintf(stderr, "    -clear\n");
	fprintf(stderr, "    -fill <byte>\n");
	fprintf(stderr, "  format 1:\n");
	fprintf(stderr, "    -company <up-to-8-chars>\n");
	fprintf(stderr, "    -initiator <up-to-8-chars>\n");
	fprintf(stderr, "    -origin <up-to-16-chars>\n");
	fprintf(stderr, "    -spi <up-to-16-chars>\n");
	fprintf(stderr, "  format 2 adds these:\n");
	fprintf(stderr, "    -jobname <up-to-32-chars>\n");
	fprintf(stderr, "    -subtime <ticks>\n");
	fprintf(stderr, "    -exectime <ticks>\n");
	fprintf(stderr, "    -waittime <ticks>\n");
	fprintf(stderr, "\n");
}
