/*
 * Copyright 1990, Silicon Graphics, Inc. 
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
 * sat_reduce -		filter interesting records from the system audit trail
 */

#ident  "$Revision"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/mac.h>
#include <string.h>
#include <pwd.h>
#include <time.h>
#include <sat.h>
#include <libgen.h>
#include <bstring.h>
#include "sat_token.h"

extern	int	errno;

#define VERBOSE         0x00001		/* select option -v     */
#define	ANTECEDENT	0x00010		/* select option -a	*/
#define	AFTERWARDS	0x00020		/* select option -A	*/
#define	USERS_INCL	0x00040		/* select option -u	*/
#define	USERS_EXCL	0x00080		/* select option -U	*/
#define	EVENT_INCL	0x00100		/* select option -e	*/
#define	EVENT_EXCL	0x00200		/* select option -E	*/
#define	LABEL_INCL	0x00400		/* select option -l	*/
#define	LABEL_EXCL	0x00800		/* select option -L	*/
#define	NAMED_INCL	0x01000		/* select option -n	*/
#define	NAMED_EXCL	0x02000		/* select option -N	*/
#define	PERMITTED	0x04000		/* select option -p	*/
#define	PROHIBITED	0x08000		/* select option -P	*/
#define FILE_TIME       0x10000		/* select option -f     */
#define PROGRAM_INCL    0x20000		/* select option -c     */
#define PROGRAM_EXCL    0x40000		/* select option -C     */
#define PRIVILEGED      0x80000		/* select option -q     */


/*
 * List of interesting uid's
 */
struct uid_list_t {
	struct uid_list_t	*next;
	uid_t			uid;
};
typedef	struct uid_list_t uid_list_t;
uid_list_t *uid_list;

int
find_user_in_list(uid_t uid)
{
	uid_list_t *up;

	for (up = uid_list; up; up = up->next)
		if (up->uid == uid)
			return 1;
	return 0;
}

int
add_user_to_list(char *username)
{
	uid_list_t *up;
	struct passwd *pwd;

	if ((pwd = getpwnam(username)) == NULL)
		return 0;
	
	if (find_user_in_list(pwd->pw_uid))
		return 1;

	up = (uid_list_t *)malloc(sizeof(uid_list_t));
	up->uid = pwd->pw_uid;
	up->next = uid_list;
	uid_list = up;

	return 1;
}

/*
 * List of interesting events
 */
struct event_list_t {
	struct event_list_t	*next;
	int			event;
};
typedef	struct event_list_t event_list_t;

event_list_t *event_list;

int
find_event_in_list(int event)
{
	event_list_t *up;
	
	for (up = event_list; up; up = up->next)
		if (up->event == event)
			return 1;
	return 0;
}

int
add_event_to_list(char *eventname)
{
	event_list_t *up;
	int event = sat_strtoevent(eventname);
	
	if (find_event_in_list(event))
		return 1;

	up = (event_list_t *)malloc(sizeof(event_list_t));
	up->event = event;
	up->next = event_list;
	event_list = up;

	return 1;
}

/*
 * List of interesting MAC labels
 */
struct mac_list_t {
	struct mac_list_t	*next;
	mac_t			mac;
};
typedef	struct mac_list_t mac_list_t;

mac_list_t *mac_list;

int
find_mac_in_list(mac_t mac)
{
	mac_list_t *up;
	
	for (up = mac_list; up; up = up->next)
		if (mac_equal(mac, up->mac) &&
		    mac->ml_msen_type == up->mac->ml_msen_type &&
		    mac->ml_mint_type == up->mac->ml_mint_type)
			return 1;
	return 0;
}

int
add_mac_to_list(char *macname)
{
	mac_list_t *up;
	mac_t mac = mac_from_text(macname);
	
	if (mac == NULL)
		return 0;
	if (find_mac_in_list(mac)) {
		mac_free(mac);
		return 1;
	}

	up = (mac_list_t *)malloc(sizeof(mac_list_t));
	up->mac = mac;
	up->next = mac_list;
	mac_list = up;

	return 1;
}

/*
 * Lists of interesting strings
 */
struct string_list_t {
	struct string_list_t	*next;
	char 			*string;
};
typedef	struct string_list_t string_list_t;

int
find_string_in_list(char *string, string_list_t *list)
{
	string_list_t *up;
	
	for (up = list; up; up = up->next)
		if (strcmp(up->string, string) == 0)
			return 1;
	return 0;
}

string_list_t *
add_string_to_list(char *string, string_list_t *list)
{
	string_list_t *up;
	
	if (find_string_in_list(string, list))
		return list;

	up = (string_list_t *)malloc(sizeof(string_list_t));
	up->string = strdup(string);
	up->next = list;

	return up;
}

/*
 * Lists of interesting commands
 */
string_list_t *command_list;

int
find_command_in_list(char *command)
{
	return find_string_in_list(command, command_list);
}

void
add_command_to_list(char *command)
{
	command_list = add_string_to_list(command, command_list);
}

/*
 * Lists of interesting file names
 */
string_list_t *name_list;

int
find_list_entry_in_name(char *name)
{
	string_list_t *up;
	
	for (up = name_list; up; up = up->next)
		if (strstr(name, up->string) != NULL)
			return 1;
	return 0;
}

void
add_name_to_list(char *name)
{
	name_list = add_string_to_list(name, name_list);
}

int file_major;
int file_minor;
int select_opts;

char *program;

long before;
long after;

/*
 * display usage string, invoked when options are used improperly
 */
void
sat_usag_redu(void)
{
	(void) fprintf(stderr, "usage: sat_reduce ");
	(void) fprintf(stderr, "[-a mmddhhmm[[cc]yy]] [-A mmddhhmm[[cc]yy]] ");
	(void) fprintf(stderr, "[-(u|U) user ...]\n\t");
	(void) fprintf(stderr, "[-(e|E) event ... ] ");
	(void) fprintf(stderr, "[-(l|L) label ... ] ");
	(void) fprintf(stderr, "[-(n|N) named-object ... ]\n\t");
	(void) fprintf(stderr, "[-p] [-P] [-v] [infile]\n");
	exit(1);
}

/*
 * give some information about the reason sat_reduce makes a failure exit
 */
void
sat_exit_redu(char * progname, char * errstring, long location)
{
	perror(progname);
	fprintf(stderr, "%s (near byte %ld)\n", errstring, location);
	exit(1);
}

/*
 * pick_time - convert mmddhhmm[[cc]yy] time string to integer
 *		seconds since midnight 1/1/1970, returns -1 on error
 */
int
pick_time(char * timestr)
{
	static	short	mm_sz[12] = {31,28,31,30,31,30,31,31,30,31,30,31};

	int	i;	/* loop index	*/
	int	yy;	/* years	*/
	int	mm;	/* month	*/
	int	dd;	/* days 	*/
	int	hh;	/* hours	*/
	int	mi;	/* mins 	*/
	time_t	now;	/* current time	*/
	int	yr;	/* current year */
	int	ss = 0;	/* seconds	*/
	char	*dotp;	/* Where the '.' is */
	long	secs;	/* seconds since 1970 */

	/* get current moment */
	time(& now);
	yr = localtime(& now) -> tm_year;

	if (dotp = strchr(timestr, '.')) {
		*dotp++ = '\0';
		ss = atoi(dotp);
	}

	/*  Parse year */
	switch (strlen(timestr)) {
	case 12:
		yy = atoi(& timestr[8]);
		break;
	case 10:
		yy =  atoi(& timestr[8]);
                if (yy >= 0 && yy < 38)
                        yy = 2000 + yy;
                else if (yy > 69 && yy <= 99)
                        yy = 1900 + yy;
		break;
	case 8:
		yy = 1900 + yr;
		break;
	default:
		return (-1);
	}
	timestr[8] = '\0';

	/* Parse remainder of time string */
	mi = atoi(timestr + 6);
	timestr[6] = '\0';
	hh = atoi(timestr + 4);
	timestr[4] = '\0';
	dd = atoi(timestr + 2);
	timestr[2] = '\0';
	mm = atoi(timestr);
	if(hh == 24) {
		hh = 0;
		dd++;
	}

	/*  Validate date elements  */

	if (mm < 1 || mm > 12 || dd < 1 || dd > 31 || hh < 0 || hh > 23 ||
	    mi < 0 || mi > 59 || ss < 0 || ss > 59 || yy < 1970 || yy > 2099)
		return (-1);
	

	/*  Build date into seconds since 1970	*/
	for(secs = 0, i = 1970; i < yy; i++)
		secs += ((i%4) ? 365 : 366);
	/*  Adjust for leap year  */
	if ((!(yy%4)) && (mm >= 3))
		secs += 1;
	/*  Adjust for different month lengths  */
	while(--mm)
		secs += mm_sz[mm - 1];
	/*  Include days, hours, minutes, and seconds	*/
	secs += (dd - 1);
	secs *= 24;
	secs += hh;
	secs *= 60;
	secs += mi;
	secs *= 60;
	secs += ss;

	/* convert to GMT assuming standard time */
	/* correction is made in localtime(3C) */
	secs += timezone;

	/* correct if daylight savings time in effect */
	if (localtime(&secs)->tm_isdst)
		secs = secs - (timezone - altzone); 

	return(secs);
}

void
process_file_header(FILE *in, int head_done, char* args[])
{
	struct sat_file_info finfo;
	int size;		/* record body size		    */

	/*
	 * Read file header.
	 * Write it out as is iff head_done is < 0. 
	 * Write it out with the stop time zeroed iff head_done is 0. 
	 * Don't write it out at all iff head_done is > 0. 
	 */
	if (head_done < 0) {
		if (sat_read_file_info(in, stdout, &finfo, SFI_TIMEZONE) ==
		    SFI_ERROR)
			exit(1);
		if (finfo.sat_major < SAT_VERSION_MAJOR) {
			free(finfo.sat_timezone);
			if (execv("/usr/bin/sat_reduce31",&args[0]) != 0) {
				perror("sat_reduce failed to exec sat_reduce31");
				exit(1);
			}
		}
	} else {

		if (sat_read_file_info(in, NULL, &finfo,
			SFI_TIMEZONE|SFI_BUFFER) == SFI_ERROR)
			exit(1);

		if (head_done == 0) {
			struct sat_filehdr *fh =
			    (struct sat_filehdr *)finfo.sat_buffer;
			if (finfo.sat_major < SAT_VERSION_MAJOR) {
				if (execv("/usr/bin/sat_reduce31",&args[0]) != 0) {
					perror("sat_reduce failed to exec sat_reduce31");
					exit(1);
			}
			}

			if (finfo.sat_major != 4 || finfo.sat_minor != 0) {
				fprintf(stderr,
				"%s: Don't know how to decode version %d.%d!\n",
				    program, finfo.sat_major, finfo.sat_minor);
				exit(1);
			}

			size = fh->sat_total_bytes;
			fh->sat_stop_time = 0;
			fwrite(finfo.sat_buffer, size, 1, stdout);
		}
	}

	putenv(finfo.sat_timezone);
	free(finfo.sat_timezone);

        /*
         * Check the open and close times if requested by the '-f' flag.
         */
	if (select_opts & FILE_TIME) {
		/*
		 * Don't bother if the file was opened after the initial
		 * (-a) time.
		 */
		if ((select_opts & ANTECEDENT) &&
		    (finfo.sat_start_time > before))
			return;
		/*
		 * Don't bother if the file was closed before the final
		 * (-A) time. Do the check iff there is a valid start time.
		 */
		if ((select_opts & AFTERWARDS) &&
		    (finfo.sat_start_time == 0 ||
		    finfo.sat_stop_time < after))
			return;
	}

	file_major = finfo.sat_major;
	file_minor = finfo.sat_minor;
}

struct token_list {
	struct token_list	*tl_next;
	sat_token_t		tl_token;
};

struct token_list *start_of_record;
struct token_list *end_of_record;

void
remember_token(sat_token_t token)
{
	struct token_list *tlp;

	tlp = (struct token_list *)malloc(sizeof(struct token_list));
	tlp->tl_token = token;
	tlp->tl_next = NULL;

	if (start_of_record == NULL)
		start_of_record = tlp;
	if (end_of_record)
		end_of_record->tl_next = tlp;
	end_of_record = tlp;
}

void
dump_record(int emit_record)
{
	struct token_list *tlp = start_of_record;
	struct token_list *ftlp;

	while (tlp != NULL) {
		if (emit_record == SAT_TRUE)
			fwrite(tlp->tl_token,
			    tlp->tl_token->token_header.sat_token_size,
			    1, stdout);
		ftlp = tlp;
		tlp = tlp->tl_next;
		free(ftlp->tl_token);
		free(ftlp);
	}
	start_of_record = NULL;
	end_of_record = NULL;
}

void
process_file(FILE *in, int head_done, char* args[])
{
	sat_token_t token;
	sat_token_t header;
        int32_t record_magic;
        sat_token_size_t record_size;
        int8_t record_rectype;
        int8_t record_outcome;
        int8_t record_sequence;
        int8_t record_error;
	int emit_record = SAT_FALSE;
	char *cp;
	time_t when;
	uint8_t ticks;
	uid_t uid;
	mac_t mac;

	process_file_header(in, head_done, &args[0]);

	for (;;) {
		if ((token = sat_fread_token(in)) == NULL)
			break;

		switch (token->token_header.sat_token_id) {
		case SAT_RECORD_HEADER_TOKEN:
			/*
			 * This is a new token. Emit the current record,
			 * should that be appropriate
			 */
			dump_record(emit_record);

			emit_record = SAT_TRUE;
			header = token;

			(void)sat_fetch_record_header_token(header,
			    &record_magic, &record_size, &record_rectype,
			    &record_outcome, &record_sequence, &record_error);

			if ((EVENT_INCL & select_opts) &&
			    !find_event_in_list(record_rectype))
				emit_record = SAT_FALSE;

			if ((EVENT_EXCL & select_opts) &&
			    find_event_in_list(record_rectype))
				emit_record = SAT_FALSE;

			if ((PERMITTED & select_opts) &&
			    (SAT_SUCCESS & record_outcome) == 0)
				emit_record = SAT_FALSE;

			if ((PROHIBITED & select_opts) &&
			    (SAT_SUCCESS & record_outcome) != 0)
				emit_record = SAT_FALSE;

			break;
		case SAT_COMMAND_TOKEN:
			if (((PROGRAM_INCL | PROGRAM_EXCL) & select_opts) == 0)
				break;

			sat_fetch_command_token(token, &cp);

			if ((PROGRAM_INCL & select_opts) &&
			    !find_command_in_list(cp))
				emit_record = SAT_FALSE;

			if ((PROGRAM_EXCL & select_opts) &&
			    find_command_in_list(cp))
				emit_record = SAT_FALSE;

			free(cp);
			break;
		case SAT_TIME_TOKEN:
			if (((ANTECEDENT | AFTERWARDS) & select_opts) == 0)
				break;

			sat_fetch_time_token(token, &when, &ticks);

			if ((ANTECEDENT & select_opts) && when > before)
				emit_record = SAT_FALSE;

			if ((AFTERWARDS & select_opts) && when < after)
				emit_record = SAT_FALSE;

			break;
		case SAT_SATID_TOKEN:
			if (((USERS_INCL | USERS_EXCL) & select_opts) == 0)
				break;

			sat_fetch_satid_token(token, &uid);

			if ((USERS_INCL & select_opts) &&
			    !find_user_in_list(uid))
				emit_record = SAT_FALSE;

			if ((USERS_EXCL & select_opts) &&
			    find_user_in_list(uid))
				emit_record = SAT_FALSE;

			break;
		case SAT_MAC_LABEL_TOKEN:
			if (((LABEL_INCL | LABEL_EXCL) & select_opts) == 0)
				break;

			sat_fetch_mac_label_token(token, &mac);

			if ((LABEL_INCL & select_opts) &&
			    !find_mac_in_list(mac))
				emit_record = SAT_FALSE;

			if ((LABEL_EXCL & select_opts) &&
			    find_mac_in_list(mac))
				emit_record = SAT_FALSE;

			mac_free(mac);
			break;
		case SAT_PATHNAME_TOKEN:
		case SAT_LOOKUP_TOKEN:
			if (((NAMED_INCL | NAMED_EXCL) & select_opts) == 0)
				break;

			switch (token->token_header.sat_token_id) {
			case SAT_PATHNAME_TOKEN:
				sat_fetch_pathname_token(token, &cp);
				break;
			case SAT_LOOKUP_TOKEN:
				sat_fetch_lookup_token(token, &cp);
				break;
			}

			if ((NAMED_INCL & select_opts) &&
			    !find_list_entry_in_name(cp))
				emit_record = SAT_FALSE;

			if ((NAMED_EXCL & select_opts) &&
			    find_list_entry_in_name(cp))
				emit_record = SAT_FALSE;

			free(cp);
			break;
		default:
			break;
		}
		remember_token(token);
	}

	dump_record(emit_record);
}

int
main(int argc, char *argv[])
{
	int c;			/* option character		    */
	extern char *optarg;	/* required by getopt(3)	    */
	extern int optind;	/* required by getopt(3)	    */
	FILE *in;

	program = argv[0];
	/* parse command line options	*/
	while ((c = getopt(argc, argv, "a:A:c:C:u:U:e:E:fl:L:n:N:pPv")) > 0) {
		switch (c) {
		case 'a':
			if ((before = pick_time(optarg)) == -1)
				sat_usag_redu();
			select_opts |= ANTECEDENT;
			break;
		case 'A':
			if ((after = pick_time(optarg)) == -1)
				sat_usag_redu();
			select_opts |= AFTERWARDS;
			break;
		case 'c':
			if (PROGRAM_EXCL & select_opts) {
				fprintf(stderr, "%s: Don't use -c with -C.\n",
					program);
				sat_usag_redu();
			}
			select_opts |= PROGRAM_INCL;
			add_command_to_list(optarg);
			break;
		case 'C':
			if (PROGRAM_INCL & select_opts) {
				fprintf(stderr, "%s: Don't use -C with -c.\n",
					program);
				sat_usag_redu();
			}
			select_opts |= PROGRAM_EXCL;
			add_command_to_list(optarg);
			break;
		case 'f':
			select_opts |= FILE_TIME;
			break;
		case 'u':
			if (USERS_EXCL & select_opts) {
				fprintf(stderr, "%s: Don't use -u with -U.\n",
					program);
				sat_usag_redu();
			}
			select_opts |= USERS_INCL;
			add_user_to_list(optarg);
			break;
		case 'U':
			if (USERS_INCL & select_opts) {
				fprintf(stderr, "%s: Don't use -U with -u.\n",
					program);
				sat_usag_redu();
			}
			select_opts |= USERS_EXCL;
			add_user_to_list(optarg);
			break;
		case 'e':
			if (EVENT_EXCL & select_opts) {
				fprintf(stderr, "%s: Don't use -e with -E.\n",
					program);
				sat_usag_redu();
			}
			select_opts |= EVENT_INCL;
			add_event_to_list(optarg);
			break;
		case 'E':
			if (EVENT_INCL & select_opts) {
				fprintf(stderr, "%s: Don't use -E with -e.\n",
					program);
				sat_usag_redu();
			}
			select_opts |= EVENT_EXCL;
			add_event_to_list(optarg);
			break;
		case 'l':
			if (LABEL_EXCL & select_opts) {
				fprintf(stderr, "%s: Don't use -l with -L.\n",
					program);
				sat_usag_redu();
			}
			select_opts |= LABEL_INCL;
			add_mac_to_list(optarg);
			break;
		case 'L':
			if (LABEL_INCL & select_opts) {
				fprintf(stderr, "%s: Don't use -L with -l.\n",
					program);
				sat_usag_redu();
			}
			select_opts |= LABEL_EXCL;
			add_mac_to_list(optarg);
			break;
		case 'n':
			if (NAMED_EXCL & select_opts) {
				fprintf(stderr, "%s: Don't use -n with -N.\n",
					program);
				sat_usag_redu();
			}
			select_opts |= NAMED_INCL;
			add_name_to_list(optarg);
			break;
		case 'N':
			if (NAMED_INCL & select_opts) {
				fprintf(stderr, "%s: Don't use -N with -n.\n",
					program);
				sat_usag_redu();
			}
			select_opts |= NAMED_EXCL;
			add_name_to_list(optarg);
			break;
		case 'p':
			if (PROHIBITED & select_opts)
				sat_usag_redu();
			select_opts |= PERMITTED;
			break;
		case 'P':
			if (PERMITTED  & select_opts)
				sat_usag_redu();
			select_opts |= PROHIBITED;
			break;
		case 'v':
			select_opts |= VERBOSE;
			break;
		default:
			sat_usag_redu();
		}
	}

	if (optind >= argc)
		process_file(stdin, -1, &argv[0]);

	for (c = (optind + 1 == argc) ? -1 : 0; optind < argc; c++, optind++) {
		if (VERBOSE & select_opts)
			(void) fprintf(stderr,"Input file: %s\n",argv[optind]);
		if ((in = fopen(argv[optind], "r" )) == NULL) {
			perror(argv[optind]);
			exit(1);
		}
		process_file(in, c, &argv[0]);
		fclose(in);
	}
	exit(0);
	/*NOTREACHED*/
}
