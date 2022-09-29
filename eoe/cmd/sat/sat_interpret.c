/*
 * Copyright 1997, Silicon Graphics, Inc.
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
 * sat_interpret -	convert audit records from binary to human
 *			readable form
 */

#ident  "$Revision: 1.46 $"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys.s>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <netinet/in.h>			/* required for ip.h */
#include <sys/mac.h>
#include <sys/capability.h>
#include <sys/acl.h>
#include <sat.h>
#include <getopt.h>
#include <string.h>
#include <time.h>
#include <netdb.h>
#include <sys/stat.h>
#include <sys/syssgi.h>
#include <assert.h>
#include <net/if.h>
#include <net/if_sec.h>
#include <net/route.h>
#include <sys/ioctl.h>			/* also gets <net/soioctl.h> */
#include <sys/stropts.h>
#include <sys/termios.h>
#include <netinet/in_systm.h>		/* required for ip.h */
#include <netinet/ip.h>
#include <netinet/ip_var.h>		/* required for tcpip.h */
#include <netinet/tcp.h>		/* required for tcpip.h */
#include <netinet/tcpip.h>
#include <netinet/in.h>			/* for sockaddr_in */
#include <arpa/inet.h>			/* for inet_ntoa() */
#include <sys/sat_compat.h>
#include <sys/extacct.h>
#include "sat_token.h"

extern	int	errno;
/*
 * Output modes: verbose, brief, linear
 */
#define OM_BRIEF	0
#define OM_VERBOSE	1
#define OM_LINEAR	2

int format = OM_VERBOSE;	/* output format */
int debug = SAT_FALSE;		/* include debugging output? */
int fdmap = 0;			/* map file descriptors to file names? */
int titles = SAT_TRUE;		/* put titles on the data */
int fileheaders = SAT_TRUE;	/* print file headers */
char *dotline =	"\n.....................................................\n\n";

int mac_enabled = SAT_TRUE;	/* was mac enabled when the file was produced? */
int cap_enabled = SAT_TRUE;	/* was cap enabled when the file was produced? */
int cipso_enabled = SAT_TRUE; /* was cipso enabled when the file was produced? */
int normalize = SAT_FALSE;	/* normalized output? */

struct sat_list_ent **users = NULL;	/* username <-> userid table */
struct sat_list_ent **groups = NULL;	/* groupname <-> groupid table */
struct sat_list_ent **hosts = NULL;	/* hostname <-> hostid table */
int n_users, n_groups, n_hosts;

int file_major, file_minor;		/* version number of current file */

static void
print_file_header(FILE *in, char *timezone, char *filename, char *args[])
{
	struct sat_file_info finfo;	/* audit file header */
	char buffer[256];
	int i;

	if (users) {
		for (i = 0; users[i]; i++)
			free(users[i]);
		free(users);
	}
	if (groups) {
		for (i = 0; groups[i]; i++)
			free(groups[i]);
		free(groups);
	}

	/*
	 * Read file header
	 */
	if (sat_read_file_info(in, NULL, &finfo, SFI_ALL) == SFI_ERROR) {
		fprintf (stderr, "Bad header in %s: ", filename);
		perror(NULL);
	}

	n_users = finfo.sat_user_entries;
	users = finfo.sat_users;

	n_groups = finfo.sat_group_entries;
	groups = finfo.sat_groups;

	n_hosts = finfo.sat_host_entries;
	hosts = finfo.sat_hosts;

	file_major = finfo.sat_major;
	file_minor = finfo.sat_minor;

	if (debug) {
		printf("debug: file header:\n");
		printf("  sat_major = %d\n", finfo.sat_major);
		printf("  sat_minor = %d\n", finfo.sat_minor);
		printf("  sat_start_time = %d\n", finfo.sat_start_time);
		printf("  sat_stop_time = %d\n", finfo.sat_stop_time);
		printf("  sat_host_id = %#x\n", finfo.sat_host_id);
		printf("  sat_user_entries = %d\n", finfo.sat_user_entries);
		printf("  sat_group_entries = %d\n", finfo.sat_group_entries);
		printf("  sat_host_entries = %d\n", finfo.sat_host_entries);
	}

	if (file_major < SAT_VERSION_MAJOR) {
		if (execv("/usr/bin/sat_interpret31",&args[0]) != 0) {
			perror("sat_interpret failed to exec sat_interpret31");
			exit(1);
		}
	}

	if (file_major > SAT_VERSION_MAJOR ||
	    (file_major == SAT_VERSION_MAJOR &&
	     file_minor > SAT_VERSION_MINOR)) {
		fprintf(stderr,
"Error: this file is version %d.%d; this version of sat_interpret can only\n\
interpret file versions up to and including %d.%d\n",
			file_major, file_minor,
			SAT_VERSION_MAJOR, SAT_VERSION_MINOR);
		exit (1);
	}

	/* set our timezone to that of the file */
	/* don't free finfo.sat_timezone, putenv keeps it! */
	if (timezone) {
		char *tz;
		tz = malloc(strlen(timezone) + 4);
		strcpy(tz, "TZ=");
		strcat(tz, timezone);
		putenv(tz);
	} else {
		putenv(finfo.sat_timezone);
	}
	tzset();

	/*
	 * If not printing fiel headers we're done
	 */
	if (fileheaders == SAT_FALSE)
		return;

	printf("File version %d.%d\n", finfo.sat_major, finfo.sat_minor);
	cftime(buffer, "%a %b %e %T %Z %Y", &finfo.sat_start_time);
	printf("Created %s\n", buffer);
	if (finfo.sat_stop_time != 0) {
		cftime(buffer, "%a %b %e %T %Z %Y", &finfo.sat_stop_time);
		printf("Closed  %s\n", buffer);
	} else
		printf("Closed  <unknown>\n");

	if (n_hosts > 1) {
		struct in_addr addr;
		printf("Merged from the following hosts:\n");
		for (i = 0; i < n_hosts; i++) {
			addr.s_addr = hosts[i]->sat_id;
			printf("    %s, hostid %s\n", hosts[i]->sat_name.data,
			       inet_ntoa(addr));
		}
		putchar('\n');
	} else {
		struct in_addr addr;
		addr.s_addr = finfo.sat_host_id;
		printf("Written on host %s", finfo.sat_hostname);
		if (*finfo.sat_domainname)
			printf(".%s", finfo.sat_domainname);
		free(finfo.sat_hostname);
		free(finfo.sat_domainname);
		printf(", hostid %s\n\n", inet_ntoa(addr));
	}
}

main(int argc, char *argv[])
{
	sat_token_t token;
	char *token_text;
	char *token_title = NULL;
	FILE *in;
	char *filename;
	char *timezone = NULL;	/* override audit file timezone */
	int c;		/* getopt */

	while ((c = getopt(argc, argv, "bdfhlntuvz:")) != -1) {
		switch (c) {
		case 'b':
			format = OM_BRIEF;
			break;
		case 'd':
			debug = SAT_TRUE;
			setlinebuf(stderr);
			setlinebuf(stdout);
			break;
		case 'f':
			fdmap = SAT_TRUE;
			break;
		case 'h':
			fileheaders = SAT_FALSE;
			break;
		case 'l':
			format = OM_LINEAR;
			break;
		case 'n':
			normalize = SAT_TRUE;
			break;
		case 't':
			titles = SAT_FALSE;
			break;
		case 'u':
			setbuf(stdout, (char *)NULL);
			break;
		case 'v':
			format = OM_VERBOSE;
			break;
		case 'z':
			timezone = optarg;
			break;
		default:
			fprintf(stderr,
			    "usage: %s [-bdl] [-z timezone] [filename]\n",
			    argv[0]);
			exit(1);
		}
	}

	/* check for an optional input file in the command line */
	if (optind < argc) {
		filename = argv[optind];
		in = fopen( filename, "r" );
		if (in == NULL) {
			fprintf (stderr, "Cannot open %s for input: ",
			    filename);
			perror(NULL);
			exit(1);
		}
	} else {
		in = stdin;
		filename = "<stdin>";
	}

	/*
	 * Process the file header, printing it if required.
	 */
	print_file_header(in, timezone, filename, &argv[0]);

	/*
	 * Read all of the tokens.
	 */
	while (token = sat_fread_token(in)) {

		token_text = sat_token_to_text(token);

		switch (token->token_header.sat_token_id) {
		case SAT_RECORD_HEADER_TOKEN:
			switch (format) {
			case OM_VERBOSE:
			case OM_BRIEF:
				printf("%s%s", dotline, token_text);
				break;
			case OM_LINEAR:
				printf("\n%s", token_text);
				break;
			}
			break;
		default:
			if (titles)
				token_title = sat_token_name(
				    token->token_header.sat_token_id);
			switch (format) {
			case OM_VERBOSE:
			case OM_BRIEF:
				if (titles)
					printf("\n\t%-20s\t= %s",
					    token_title, token_text);
				else
					printf("\n\t%-20s", token_text);
				break;
			case OM_LINEAR:
				if (titles)
					printf(" %s=%s",token_title,token_text);
				else
					printf(" %s", token_text);
				break;
			}
			if (titles)
				free(token_title);
			break;
		}
		free(token_text);
	}
	printf("\n");
	exit(0);
	/*NOTREACHED*/
}
