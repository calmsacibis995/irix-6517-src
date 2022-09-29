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
 * sat_summarize -	generate statistics on a stream of audit records
 */

#ident  "$Id: sat_summarize31.c,v 1.1 1998/06/02 18:24:21 deh Exp $"

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <errno.h>
#include <malloc.h>
#include <signal.h>
#include <pwd.h>
#include <sys/mac_label.h>
#include <sat.h>

typedef	struct	user_list {
	struct	user_list *	next;
	struct	user_list *	prev;
	uid_t			uid;
	int			total;
	}	user_list_t;

typedef	struct	time_list {
	struct	time_list *	next;
	struct	time_list *	prev;
	time_t			tick;
	int			size;
	}	time_list_t;

#define	MAX_BODY_SZ	0x800

#define	VERBOSE		0x01		/* display option -v	*/
#define	BY_EVENT	0x02		/* display option -e	*/
#define	BY_USER		0x04		/* display option -u	*/
#define	BY_TIME		0x08		/* display option -t	*/
#define ECHO_IN		0x10		/* display option -o	*/

#define	ONE_MINUTE	60	/* number of seconds in one minute     */

/* function prototypes */
void	sat_sgnl_summ ();
void	sat_exit_summ (char *, char *, double);
void	sat_user_summ (uid_t, user_list_t *);
void	sat_time_summ (time_t, int, time_list_t *);
void	sat_disp_summ (int, time_t, time_t, time_list_t, user_list_t,
			long *, int, double);
int	sat_read_summ (char *, int, FILE *, int, char *, double);

int	sat_show_summ = 0;
int	file_major;
int	file_minor;

void
sat_sgnl_summ ()
	{
	sat_show_summ = 1;
	(void) signal (SIGUSR1, sat_sgnl_summ);
	}

void
main (argc, argv)
	int	argc;
	char *	argv[];
	{
	int	c;		/* option character	*/
	extern	char *	optarg;
	extern	int	optind;

	int	display_opts		= 0;

	int	size;

	FILE *	in;
	char *	infilename;

	struct sat_file_info	finfo;
	struct sat_hdr_info	head;
	char			body [MAX_BODY_SZ];

	double			total_trail_size = 0;
	static long		total_per_event	[SAT_NTYPES];
	int			total_records = 0;
	time_t			time_started;
	time_t			time_finished;

	user_list_t		user_list_head;
	time_list_t		time_list_head;

	int			more_data = 1;

	/* catch SIGUSR1 signals	*/
	(void) signal (SIGUSR1, sat_sgnl_summ);

	/* initialize user list head	*/
	user_list_head.next  = & user_list_head;
	user_list_head.prev  = & user_list_head;
	user_list_head.uid   = -1;
	user_list_head.total = -1;

	/* initialize time list head	*/
	time_list_head.next  = & time_list_head;
	time_list_head.prev  = & time_list_head;
	time_list_head.tick  = -1;

	/* parse command line options	*/
	while (0 < (c = getopt (argc, argv, "eotuvbdflnTz")))
		switch (c) {
		case 'e':
			display_opts |= BY_EVENT;
			break;
		case 'o':
			display_opts |= ECHO_IN;
			break;
		case 't':
			display_opts |= BY_TIME;
			break;
		case 'u':
			display_opts |= BY_USER;
			break;
		case 'v':
			display_opts |= VERBOSE;
			break;
		/* options for 6.5 version that have no meaning in this version */
		case 'b':
		case 'd':
		case 'f':
		case 'l':
		case 'n':
		case 'T':
			break;
		case 'z':
			break;
		default:
			(void) fprintf (stderr, "usage: sat_summarize ");
			(void) fprintf (stderr, "[-e] [-o] [-t] [-u] [-v] ");
			(void) fprintf (stderr, "[filename]\n");
			exit (1);
		}

	/* pick a reasonable default */
	if (display_opts == 0)
		display_opts = BY_EVENT;

	/* check for an optional input file in the command line	*/
	if (optind < argc) {
		infilename = argv[optind];
		if (VERBOSE & display_opts)
			(void) fprintf (stderr,
				"Input file = %s\n", infilename);
		in = fopen (infilename, "r" );
		if (in == NULL) {
			fprintf (stderr, "Cannot open %s for input: ",
				infilename);
			perror(NULL);
			exit (1);
			}
		}
	else
		in = stdin;

	/* read file header */

	if (ECHO_IN & display_opts) {
		if (sat_read_file_info(in, stdout, &finfo, SFI_NONE)
		    == SFI_ERROR)
			exit(1);
	} else {
		if (sat_read_file_info(in, NULL, &finfo, SFI_NONE) == SFI_ERROR)
			exit(1);
	}
	
	file_major = finfo.sat_major;
	file_minor = finfo.sat_minor;

	/* gather statistics	*/
	while (more_data) {
		/* on receipt of a sigusr1, display intermediate results     */
		if (sat_show_summ)
			(void) sat_disp_summ (display_opts, time_started,
			time_finished, time_list_head, user_list_head,
			total_per_event, total_records, total_trail_size);

		/* ingest, digest, and egest one record	*/

		more_data = sat_read_summ ((char *)&head, 0, in,
			display_opts, argv[0], total_trail_size);
		if (! more_data)
			break;

		if (head.sat_magic != 0x12345678)
			sat_exit_summ (argv[0], "Invalid record alignment",
				total_trail_size);

		size = head.sat_recsize;
		if (MAX_BODY_SZ < size)
			sat_exit_summ (argv[0], "SAT body too big",
				total_trail_size);

		more_data = sat_read_summ (body, size, in,
			display_opts, argv[0], total_trail_size);
		if (! more_data)
			break;

		/* initialize timer (audit time, not real time)	*/
		if (!total_records)
			time_started = head.sat_time;

		/* finalize timer (audit time, not real time)	*/
		time_finished = head.sat_time;

		/* tally this one record	*/
		total_records ++;
		total_trail_size += head.sat_hdrsize;
		total_trail_size += head.sat_recsize;
		total_per_event [head.sat_rectype] ++;
		if (BY_USER & display_opts) {
			(void) sat_user_summ (head.sat_id, & user_list_head);
			}
		if (BY_TIME & display_opts)
			(void) sat_time_summ (head.sat_time,
				head.sat_recsize, & time_list_head);
		}

	/* display statistics	*/
	(void) sat_disp_summ (display_opts, time_started, time_finished,
		time_list_head, user_list_head, total_per_event,
		total_records, total_trail_size);
	exit(0);
	}

/* give some information about the reason sat_summarize makes a failure exit */
void
sat_exit_summ (char * progname, char * errstring, double location)
	{
	perror (progname);
	fprintf (stderr, "%s (near byte %.0f)\n", errstring, location);
	exit (1);
	}

/* keep a tally of the number of times each user caused a record generation  */
void
sat_user_summ (uid_t uid, user_list_t * list_head_p)
	{
	user_list_t *	cursor;

	for (cursor = list_head_p->next ;; cursor = cursor->next) {
		if (uid == cursor -> uid) {	/* if uid found here  */
			cursor -> total ++;
			/* remove item from list.  outside of loop    */
			/* it'll go back into list, at the very front */
			cursor -> prev -> next = cursor -> next;
			cursor -> next -> prev = cursor -> prev;
			break;
			}
		if (list_head_p == cursor) {	/* if uid not in list */
			/* malloc new item for list.  outside of loop */
			/* it'll go into list, at the very front      */
			cursor = malloc (sizeof (user_list_t));
			cursor -> uid   = uid;
			cursor -> total = 1;
			break;
			}
		}
	cursor -> next = list_head_p -> next;
	list_head_p -> next -> prev = cursor;
	cursor -> prev = list_head_p;
	list_head_p -> next = cursor;
	}

/* keep a list of the times of events occurring during the last sixty secs   */
void
sat_time_summ (time_t tick, int size, time_list_t *list_head_p)
	{
	time_list_t *	oldest;
	time_list_t *	newest;

	/* remove obsolete entries	*/
	while ((list_head_p != (oldest = list_head_p->next)) &&
			(ONE_MINUTE < (tick - oldest->tick))) {
		oldest->next->prev = list_head_p;
		list_head_p->next = oldest->next;
		free (oldest);
		}

	/* enqueue the newest entry	*/
	newest = (time_list_t *) malloc (sizeof (time_list_t));
	newest->tick = tick;
	newest->size = size;
	newest->next = list_head_p;
	newest->prev = list_head_p->prev;
	list_head_p->prev->next = newest;
	list_head_p->prev = newest;
	}

void
sat_disp_summ (
	int display_opts, time_t time_started, time_t time_finished,
	time_list_t time_list_head, user_list_t user_list_head,
	long *total_per_event, int total_rcrd, double total_byte)
{
	time_list_t *	time_curs;
	time_t		secs_elapsed;
	int		rate_rcrd;
	double		rate_byte;
	int		last_rcrd = 0;
	int		last_byte = 0;
	user_list_t *	user_curs;

	int		days, hours, mins, secs;

	int		event_x;		/* event loop index	     */

	struct passwd *	pw_entry;

	if (BY_TIME  & display_opts) {
		for (time_curs = time_list_head.next;
				((time_curs !=  (&time_list_head))
					&& (-1 != time_curs->tick)
					&& (total_rcrd >= last_rcrd));
				time_curs =  time_curs -> next) {
			last_byte += time_curs -> size;
			last_rcrd ++;
			}
		secs_elapsed = time_finished - time_started;
		if (secs_elapsed == 0)	/* avoid div by zero */
			secs_elapsed = 1;
		if (350000 > total_rcrd) /* prevent overflow, keep signif.  */
			rate_rcrd = (ONE_MINUTE *  total_rcrd) / secs_elapsed;
		else
			rate_rcrd =  ONE_MINUTE * (total_rcrd  / secs_elapsed);

		rate_byte = ONE_MINUTE * total_byte / secs_elapsed;


		fprintf(stderr,
		    "Records: %7d total, %6d per minute, %6d in last minute\n",
		    total_rcrd, rate_rcrd, last_rcrd);

		fprintf(stderr,
		 "Bytes:   %7.0f total, %6.0f per minute, %6d in last minute\n",
		    total_byte, rate_byte, last_byte);

		fprintf(stderr, "Elapsed time = ");

		secs = secs_elapsed;

		days = secs / (60*60*24);
		secs %= (60*60*24);
		hours = secs / (60*60);
		secs %= (60*60);
		mins = secs / 60;
		secs %= 60;

		if (days > 0)
			fprintf(stderr, " %d day%s", days, days>1?"s":"");
		if (hours > 0)
			fprintf(stderr, " %2d:%02d", hours, mins);
		else if (mins > 0)
			fprintf(stderr, " %d minute%s", mins, mins>1?"s":"");
		else
			fprintf(stderr, " %d second%s", secs, secs>1?"s":"");

		fprintf(stderr, " (%.1f megabytes per hour)\n\n",
		    rate_byte * 60 / (1024*1024));
		}

	if (BY_EVENT & display_opts) {
		for (event_x = 0; event_x < SAT_NTYPES; event_x++) {
			if ((0 < total_per_event[event_x]) ||
					(VERBOSE & display_opts))
				fprintf (stderr, "%-30s%7d\n",
					sat_eventtostr (event_x),
					total_per_event[event_x]);
			}
		fprintf (stderr, "\n");
		}

	if (BY_USER  & display_opts) {
		for (user_curs = user_list_head.next;
				-1 != user_curs->total;
				user_curs =  user_curs -> next) {
			if ((0 == user_curs->total) &&
					!(VERBOSE & display_opts))
				continue;
			if (pw_entry = getpwuid ((uid_t) user_curs->uid))
				fprintf (stderr, "%s\t%d\n", pw_entry->pw_name,
					user_curs->total);
			else
				fprintf (stderr, "User #%d\t%d\n",
					user_curs->uid, user_curs->total);
			}
		}

	sat_show_summ = 0;

	}

/* read a unit of data, possibly copying it, and return completion */
/* return value:  0 = more data to read, 1 = end of file */
sat_read_summ (char * buf, int size, FILE * ifp, int display_opts,
		char * pname, double place)
	{
#define	SAT_MAX_INTR_PER_IO	8

	int	amt_read;
	int	i;		/* loop counter for repeated reads & writes  */
	int	io_done;	/* boolean, true if io succeeds		     */
	FILE *	out = stdout;
	int	read_header = (size == 0);

	for (i = SAT_MAX_INTR_PER_IO, io_done = 0; (0 < i) && !io_done; i--) {

		if (read_header) {
			struct sat_hdr_info *hinfo = (void *)buf;
			sat_read_header_info(ifp, hinfo, SHI_BUFFER, 
				file_major, file_minor);
			buf = hinfo->sat_buffer;
			amt_read = hinfo->sat_hdrsize;
		} else
			amt_read = fread(buf, size, 1, ifp);

		if (ferror(ifp)) {
			if (EINTR == errno) {
				clearerr (ifp);
				sat_show_summ = 1;
				if (read_header) free(buf);
				continue;
				}
			else
				sat_exit_summ (pname, "Read error", place);
			}
		else
			io_done = 1;

		if (feof(ifp)) {
			if (VERBOSE & display_opts)
				fprintf (stderr,"%s: EOF on input\n", pname);
			if (read_header) free(buf);
			return 0;
			}
		}


	for (i = SAT_MAX_INTR_PER_IO, io_done = 0; (0 < i) && !io_done; i--) {

		if (!(ECHO_IN & display_opts))
			break;

		fwrite (buf, amt_read, 1, out);

		if (ferror(out) || feof(out)) {
			if (EINTR == errno) {
				clearerr (out);
				sat_show_summ = 1;
				continue;
				}
			else
				sat_exit_summ (pname, "Write error", place);
			}
		else
			io_done = 1;
		}

	if (read_header)
		free(buf);

	return 1;
	}

