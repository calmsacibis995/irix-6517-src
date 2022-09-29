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

#ident  "$Id: sat_reduce31.c,v 1.1 1998/06/01 18:29:16 deh Exp $"

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/mac_label.h>
#include <sys/mac.h>
#include <string.h>
#include <malloc.h>
#include <pwd.h>
#include <time.h>
#include <mls.h>
#include <sat.h>
#include <sys/sat_compat.h>
#include <libgen.h>
#include <bstring.h>

#define	MAX_BODY_SZ	0x800

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

union token {
	char		*str;
	int		num;
	mac_label	*lbl;
};

typedef	struct list_t {
	struct list_t	*next;
	union token	datum;
} list_t;

int file_major;
int file_minor;
int select_opts;

char *program;

long before;
long after;

list_t *users_list;	/* list of user IDs			*/
list_t *event_list;	/* list of events sat_eventtostr(3)	*/
list_t *label_list;	/* list of labels			*/
list_t *named_list;	/* list of named objects		*/
list_t *program_list;	/* programs to select			*/

/* prototypes */

void	sat_usag_redu	  (void);
void	sat_exit_redu	  (char *, char *, long);
char *	sat_find_pathname (u_char event, char * body);
int	sat_pick_time	  (char *);
void	sat_pick_token	  (list_t **, char *, char);
int	sat_parse_users	  (char *);
int	sat_parse_event	  (char *);
mac_label *	sat_parse_label	  (char *);
char *	sat_parse_named	  (char *);
int	sat_filter_time   (struct sat_hdr_info *, int, time_t);
int	sat_filter_outcm  (struct sat_hdr_info *, int);
int	sat_filter_item   (struct sat_hdr_info *, char *, char, int, list_t *);
int	sat_names_match	  (char * test_name, char * base_name);
void	process_file  	  (FILE *, int);

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
 * sat_find_pathname - return pointer to pathname object (or NULL if absent)
 * event	event type, from head.sat_rectype
 * body		pointer to record body
 */
char *
sat_find_pathname(u_char event, char *body)
{
	struct sat_pathname pn;
	char *actname;				/* filename in pathname rcd */
	int skip;				/* bytes to skip */
	static char object [MAXPATHLEN];	/* path name buffer  */
	static int align[4] = { 0, 3, 2, 1 };

	/* locate pathname object in record, if there is one */
	switch (event) {
	case SAT_OPEN:
	case SAT_OPEN_RO:
		skip = sizeof(struct sat_open);
		break;
	case SAT_MOUNT:
		if (file_major == 1)
			skip = sizeof(struct sat_mount_1_0);
		else
			skip = 0;
		break;
	case SAT_FILE_ATTR_WRITE:
		if (file_major == 1)
			skip = sizeof(struct sat_file_attr_write_1_0);
		else
			skip = sizeof(struct sat_file_attr_write);
		break;
	case SAT_EXEC:
		if (file_major == 1)
			skip = sizeof(struct sat_exec_1_0);
		else
			skip = sizeof(struct sat_exec);
		break;
	case SAT_ACCESS_DENIED:
	case SAT_ACCESS_FAILED:
	case SAT_CHDIR:
	case SAT_CHROOT:
	case SAT_READ_SYMLINK:
	case SAT_FILE_CRT_DEL:
	case SAT_FILE_WRITE:
	case SAT_FILE_ATTR_READ:
		skip = 0;
		break;
	default:
		return (NULL);
	}

	/* locate filename within pathname object			     */
	body += skip + align[ skip % 4 ];

	if (sat_intrp_pathname(&body, &pn, NULL, &actname, NULL, file_major, 
			      file_minor) < 0)
		exit(1);

	strcpy(object, actname);
	free(actname);

	return (object);
}

/*
 * sat_pick_time - convert mmddhhmm[[cc]yy] time string to integer
 *		seconds since midnight 1/1/1970, returns -1 on error
 */
int
sat_pick_time(char * timestr)
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
		yy = 1900 + atoi(& timestr[8]);
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

/*
 * sat_parse_label - convert name pointer to a malloc'ed label string
 */
mac_label *
sat_parse_label(char *name_p)
{
	mac_label *lp;

	if (!(lp = mac_from_text(name_p)))
		return (NULL);

	return (lp);
}

/*
 * sat_parse_named - convert object name pointer to a malloc'ed name string
 */
char *
sat_parse_named(char * name_p)
{
	/* regcmp does a malloc().  sat_reduce never frees the buffer	     */
	/* that holds the compiled regular expression.  Do we care?  No!     */
	return (regcmp(name_p, (char *) 0));
}

/*
 * sat_parse_users - convert name pointer to a uid
 */
int
sat_parse_users(char *name_p)
{
	struct passwd *pwd;

	if (!(pwd = getpwnam(name_p)))
		return (-1);
	else
		return ((int) pwd->pw_uid);
}

/*
 * sat_parse_event - convert name pointer to a event
 */
int
sat_parse_event(char * name_p)
{
	int evt;

	if (!(evt = sat_strtoevent(name_p)))
		return (-1);
	else
		return (evt);
}

/*
 * sat_pick_token - parse tokens from an ascii list of users-names,
 *		    event-names, label-names or object names
 */
void
sat_pick_token(list_t **head_p, char *argu_p, char flavor)
{
	union token token;
	char *name_p;
	list_t *new_item_p;
	int bad_token = 0;
	int token_error;

	name_p = strtok(argu_p, ", \t");
	
	while (name_p) {
		/* flavor is either 'c', 'u', 'e', or 'l'  */
		token_error = SAT_FALSE;
		switch (flavor) {
		case 'c':
		case 'C':
			if ((token.str = strdup(name_p)) == NULL)
				token_error = SAT_TRUE;
			break;
		case 'u':
		case 'U':
			if ((token.num = sat_parse_users(name_p)) < 0)
				token_error = SAT_TRUE;
			break;
		case 'e':
		case 'E':
			if ((token.num = sat_parse_event(name_p)) < 0)
				token_error = SAT_TRUE;
			break;
		case 'l':
		case 'L':
			if ((token.lbl = sat_parse_label(name_p)) == NULL)
				token_error = SAT_TRUE;
			break;
		case 'n':
		case 'N':
			if ((token.str = sat_parse_named(name_p)) == NULL)
				token_error = SAT_TRUE;
			break;
		default:
			fprintf(stderr,
				"sat_reduce internal error - sat_pick_token\n");
			exit(1);
		}

		if (token_error) {
			fprintf(stderr,
				"sat_reduce: unknown token: %s\n", name_p);
			bad_token++;
			continue;
		}

		new_item_p = malloc(sizeof(list_t));
		new_item_p->next = *head_p;
		new_item_p->datum = token;
		*head_p = new_item_p;

		name_p = strtok(0, ", \t");
	}

	if (bad_token) {
		fprintf(stderr, "%d bad token errors encountered -- exiting\n",
			bad_token);
		exit(1);
	}
}

/*
 * sat_filter_time  - return 1 if record is in selected time range (else 0)
 */
/* struct sat_hdr_info *hp;		** record head pointer	     */
/* int			sequence;	** ANTECEDENT or AFTERWARDS  */
/* long			reference;	** compare ev time to refer. */

int
sat_filter_time(struct sat_hdr_info *hp, int sequence, time_t reference)
{
	switch (sequence) {
	case ANTECEDENT:
		return ((reference >= hp->sat_time) ? 1 : 0);
	case AFTERWARDS:
		return ((reference <= hp->sat_time) ? 1 : 0);
	}
	fprintf(stderr, "sat_reduce internal error in sat_filter_time\n");
	exit(1);
	return(1);
}

/*
 * sat_filter_outcm - return 1 if record is in selected outcome   (else 0)
 */
/* struct sat_hdr_info *hp;		** record head pointer	     */
/* int			restriction;	** PERMITTED  or PROHIBITED */

int
sat_filter_outcm(struct sat_hdr_info *hp, int restriction)
{
	switch (restriction) {
	case PERMITTED:
		return ((SAT_SUCCESS & hp->sat_outcome) ? 1 : 0);
	case PROHIBITED:
		return ((SAT_SUCCESS & hp->sat_outcome) ? 0 : 1);
	}
	fprintf(stderr, "sat_reduce internal error in sat_filter_outcm\n");
	exit(1);
	return(1);
}

/*
 * sat_names_match - compare two pathnames, return SAT_TRUE if equal
 *			allow wildcard characters in base name
 */
/* char *	test_name	** actual name of named object	     */
/* char *	base_name	** compiled regular expression from  */
/*				** command line parameter	     */
int
sat_names_match(char *test_name, char *base_name)
{
	int		match;		/* SAT_TRUE if test_name matches base_   */
	char *		dupe_name;	/* modifiable duplicate of test name */
	char *		part_name;	/* interesting part of test_name     */
	char *		save_name;	/* spare restorable copy - part_name */

	/* make working copy of test_name and make an extra pointer to it    */
	dupe_name = strdup(test_name);
	part_name = dupe_name;

	/* In test_name, eliminate symbolic links back to '/'		     */
	while ((char *) NULL != part_name) { 	/* find last link to root    */
		save_name = part_name;
		part_name = strstr(part_name+1, "/@/");
	}
	while ((char *) NULL != part_name) {
		save_name = part_name;
		part_name = strstr(part_name+1, "/!/");
	}

	part_name = save_name;
	if (part_name != dupe_name)  	/* skip leading '/@' from '/@/'	     */
		part_name +=2;
	save_name = part_name;

	/* In test_name, strip moldy directories			     */
	while ((char *) NULL != part_name) { 	/* find moldy dir marker     */
		if (!(part_name = strstr(part_name, "/>")))
			break;
		part_name++;
		*part_name = '\0';
		part_name++;
		if (part_name = strchr(part_name, '/'))
			save_name = strcat(save_name, part_name);
		part_name = save_name;
	}
	part_name = save_name;

	/* In test_name, strip "/!" markers for mount points		     */
	/* (algorithm works even if test_name ends in '/!')		     */
	while ((char *) NULL != part_name) { 	/* find mount point	     */
		if (!(part_name = strstr(part_name, "/!")))
			break;
		part_name++;
		*part_name = '\0';
		part_name++;
		save_name = strcat(save_name, part_name);
		part_name = save_name;
	}
	part_name = save_name;

	/* In test_name, strip "//" markers for normal directories	     */
	/* (algorithm works even if test_name ends in '//')		     */
	while ((char *) NULL != part_name) { 	/* find mount point	     */
		if (!(part_name = strstr(part_name, "//")))
			break;
		*part_name = '\0';
		part_name++;
		save_name = strcat(save_name, part_name);
		part_name = save_name;
	}
	part_name = save_name;

	/* In test_name, retrace over references to parent directory, but    */
	/*		 beware of references to the parent of '/'.	     */
	while ((char *) NULL != part_name) { 	/* find parent references    */
		if (!(part_name = strstr(part_name, "/../")))
			break;
		if (part_name == save_name) /* change '^/../xy' into '^/xy'  */
			save_name += 3;
		else {		/* change 'st/uvw/../xy' into 'st/xy'	     */
			char *	slash_p;
			*part_name = '\0';
			part_name +=3;
			if (slash_p = strrchr(save_name, '/'))
				*slash_p = '\0';
			save_name  = strcat(save_name, part_name);
		}
		part_name = save_name;
	}
	part_name = save_name;

	/* Compare base_name to stripped test_name; return SAT_TRUE on match     */
	match = regex(base_name, part_name) ? SAT_TRUE : SAT_FALSE;
	free(dupe_name);
	return (match);
}

/*
 * sat_filter_item - return 1 if record is in selected item set  (else 0)
 */
/* struct sat_hdr_info *hp;		** record head pointer	     */
/* char *		object;		** optional pathname ptr     */
/* char			flavor;		** 'u', 'e', 'l' or 'n'      */
/* int			included;	** boolean: incl or excl     */
/* list_t *		list_p;		** list of selected tokens   */
int
sat_filter_item(struct sat_hdr_info *hp, char *object, char flavor,
		int included, list_t *list_p)
{
	list_t *cursor;		/* pointer into token list   */

	if (!list_p)				/* should never happen       */
		return (1);

	included = included ? 1 : 0;

	for (cursor = list_p; cursor; cursor = cursor->next) {
		switch (flavor) {
		case 'l':
		case 'L':
			if (mac_size(hp->sat_plabel) ==
			    mac_size(cursor->datum.lbl) &&
			    (bcmp(hp->sat_plabel, cursor->datum.lbl,
			        mac_size(hp->sat_plabel)) == 0) )
				return (included);
			break;
		case 'c':
		case 'C':
			if (object && !strcmp(object, cursor->datum.str))
				return (included);
			break;
		case 'n':
		case 'N':
			if (object &&
			    sat_names_match(object, cursor->datum.str))
				return (included);
			break;
		case 'u':
		case 'U':
			if (hp->sat_id == cursor->datum.num)
				return (included);
			break;
		case 'e':
		case 'E':
			if (hp->sat_rectype == cursor->datum.num)
				return (included);
			break;
		}

	}

	/*
	 * Not in the list.
	 */
	return (included ? 0 : 1);
}


void
process_file(FILE *in, int head_done)
{
	struct sat_file_info finfo;
	struct sat_hdr_info hinfo;
	char body[MAX_BODY_SZ];
	char *object;		/* pts to pathname in some records  */
	int size;		/* record body size		    */
	int hdr_mask;		/* what fields we want from the hdr */

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
		if ((finfo.sat_major > SAT_VERSION_MAJOR_3)
		     || (finfo.sat_minor > SAT_VERSION_MINOR_1)) {
			fprintf(stderr,
			    "%s: Don't know how to decode version %d.%d!\n",
			    program, finfo.sat_major, finfo.sat_minor);
			exit(1);
		}
	} else {

		if (sat_read_file_info(in, NULL, &finfo,
			SFI_TIMEZONE|SFI_BUFFER) == SFI_ERROR)
			exit(1);
		if (head_done == 0) {
			if (finfo.sat_major == 1 && finfo.sat_minor == 0) {
				struct sat_filehdr_1_0 *fh_1_0 =
				    (struct sat_filehdr_1_0 *)finfo.sat_buffer;

				size = fh_1_0->sat_total_bytes;
				fh_1_0->sat_stop_time = 0;
			} else if ((finfo.sat_major == 3) ||
				(finfo.sat_major == 2) ||       
				(finfo.sat_major == 1 && finfo.sat_minor > 0)) {
				struct sat_filehdr *fh =
				    (struct sat_filehdr *)finfo.sat_buffer;

				size = fh->sat_total_bytes;
				fh->sat_stop_time = 0;
			} else {
				fprintf(stderr,
				    "%s: Don't know how to decode version %d.%d!\n",
				    program, finfo.sat_major, finfo.sat_minor);
				exit(1);
			}
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
		 * Don't bother if the file was closed before the initial
		 * (-a) time.
		 */
		if ((select_opts & ANTECEDENT) &&
		    finfo.sat_stop_time != 0 &&
		    finfo.sat_stop_time < after)
			return;
		/*
		 * Don't bother if the file was created after the final
		 * (-A) time. Do the check iff there is a valid stop time.
		 */
		if ((select_opts & AFTERWARDS) &&
		    finfo.sat_start_time > before)
			return;
	}

	file_major = finfo.sat_major;
	file_minor = finfo.sat_minor;
	hdr_mask = SHI_BUFFER;
	if ((LABEL_INCL & select_opts) || (LABEL_EXCL & select_opts)) {
		if (!finfo.sat_mac_enabled) {
			fprintf(stderr, "No labels in this audit file.\n");
			exit(0);
		}
		hdr_mask |= SHI_PLABEL;
	}
	if ((PROGRAM_INCL & select_opts) || (PROGRAM_EXCL & select_opts))
		hdr_mask |= SHI_PNAME;

	/*
	 * Gather statistics
	 */
	for (bzero(&hinfo, sizeof(hinfo));;) {

		if (hinfo.sat_plabel)
			free(hinfo.sat_plabel);
		if (hinfo.sat_pname)
			free(hinfo.sat_pname);
		if (hinfo.sat_buffer)
			free(hinfo.sat_buffer);

		/* ingest, digest, and egest one record	*/

		sat_read_header_info(in,&hinfo,hdr_mask,file_major,file_minor);
		if (hinfo.sat_hdrsize > MAX_BODY_SZ)
			sat_exit_redu(program, "SAT header too big", ftell(in));

		if (ferror(in))
			sat_exit_redu(program,
				"Error reading record head", ftell(in));

		if (feof(in)) {
			if ((VERBOSE & select_opts) || (stdin == in))
				fprintf(stderr,"%s: EOF on input\n", program);
			break;
		}

		if (hinfo.sat_magic != 0x12345678)
			sat_exit_redu(program,
				"Invalid record alignment", ftell(in));

		size = hinfo.sat_recsize;
		if (size > MAX_BODY_SZ)
			sat_exit_redu(program, "SAT body too big", ftell(in));

		(void) fread(body, size, 1, in);
		if (ferror(in) || feof(in))
			sat_exit_redu(program,
			"Error reading record body", ftell(in));

		if ((ANTECEDENT & select_opts) &&
		    !sat_filter_time(&hinfo, ANTECEDENT, before))
			continue;
		if ((AFTERWARDS & select_opts) &&
		    !sat_filter_time(&hinfo, AFTERWARDS, after))
			continue;

		if ((PROGRAM_INCL & select_opts) &&
		    !sat_filter_item(&hinfo,hinfo.sat_pname,'c',1,program_list))
			continue;
		if ((PROGRAM_EXCL & select_opts) &&
		    !sat_filter_item(&hinfo,hinfo.sat_pname,'C',0,program_list))
			continue;

		if ((USERS_INCL & select_opts) &&
		    !sat_filter_item(&hinfo,NULL,'u',1,users_list))
			continue;
		if ((USERS_EXCL & select_opts) &&
		    !sat_filter_item(&hinfo,NULL,'U',0,users_list))
			continue;
		if ((EVENT_INCL & select_opts) &&
		    !sat_filter_item(&hinfo,NULL,'e',1,event_list))
			continue;
		if ((EVENT_EXCL & select_opts) &&
		    !sat_filter_item(&hinfo,NULL,'E',0,event_list))
			continue;
		if ((LABEL_INCL & select_opts) &&
		    !sat_filter_item(&hinfo,NULL,'l',1,label_list))
			continue;
		if ((LABEL_EXCL & select_opts) &&
		    !sat_filter_item(&hinfo,NULL,'L',0,label_list))
			continue;

		if ((NAMED_INCL | NAMED_EXCL) & select_opts)
			object = sat_find_pathname(hinfo.sat_rectype, body);

		if ((NAMED_INCL & select_opts) &&
		    !sat_filter_item(&hinfo, object, 'n', 1, named_list))
			continue;
		if ((NAMED_EXCL & select_opts) &&
		    !sat_filter_item(&hinfo, object, 'N', 0, named_list))
			continue;

		if ((PERMITTED & select_opts) &&
		    !sat_filter_outcm(&hinfo, PERMITTED))
			continue;
		if ((PROHIBITED & select_opts) &&
		    !sat_filter_outcm(&hinfo, PROHIBITED))
			continue;

		fwrite(hinfo.sat_buffer, hinfo.sat_hdrsize, 1, stdout);
		if (ferror(stdout))
			sat_exit_redu(program,
				"Error writing record head", ftell(in));

		if (feof(stdout)) {
			fprintf(stderr, "%s: output EOF\n", program);
			break;
		}

		fwrite(body, hinfo.sat_recsize, 1, stdout);
		if (ferror(stdout))
			sat_exit_redu(program,
				"Error writing record body", ftell(in));

		if (feof(stdout)) {
			fprintf(stderr, "%s: output EOF\n", program);
			break;
		}
	}
	if (hinfo.sat_plabel)
		free(hinfo.sat_plabel);
	if (hinfo.sat_pname)
		free(hinfo.sat_pname);
	if (hinfo.sat_buffer)
		free(hinfo.sat_buffer);
}

main(int argc, char *argv[])
{
	int c;			/* option character		    */
	extern char *optarg;	/* required by getopt(3)	    */
	extern int optind;	/* required by getopt(3)	    */
	FILE *in;

	program = argv[0];
	/* parse command line options	*/
	while (0 < (c = getopt(argc, argv, "a:A:c:C:u:U:e:E:fl:L:n:N:pPv"))) {
		switch (c) {
		case 'a':
			if (-1 == (before = sat_pick_time(optarg)))
				sat_usag_redu();
			else
				select_opts |= ANTECEDENT;
			break;
		case 'A':
			if (-1 == (after  = sat_pick_time(optarg)))
				sat_usag_redu();
			else
				select_opts |= AFTERWARDS;
			break;
		case 'c':
			if (PROGRAM_EXCL & select_opts) {
				fprintf(stderr, "%s: Don't use -c with -C.\n",
					program);
				sat_usag_redu();
			}
			else
				select_opts |= PROGRAM_INCL;
			sat_pick_token(&program_list, optarg, c);
			break;
		case 'C':
			if (PROGRAM_INCL & select_opts) {
				fprintf(stderr, "%s: Don't use -C with -c.\n",
					program);
				sat_usag_redu();
			}
			else
				select_opts |= PROGRAM_EXCL;
			sat_pick_token(&program_list, optarg, c);
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
			else
				select_opts |= USERS_INCL;
			sat_pick_token(&users_list, optarg, c);
			break;
		case 'U':
			if (USERS_INCL & select_opts) {
				fprintf(stderr, "%s: Don't use -U with -u.\n",
					program);
				sat_usag_redu();
			}
			else
				select_opts |= USERS_EXCL;
			sat_pick_token(&users_list, optarg, c);
			break;
		case 'e':
			if (EVENT_EXCL & select_opts) {
				fprintf(stderr, "%s: Don't use -e with -E.\n",
					program);
				sat_usag_redu();
			}
			else
				select_opts |= EVENT_INCL;
			sat_pick_token(&event_list, optarg, c);
			break;
		case 'E':
			if (EVENT_INCL & select_opts) {
				fprintf(stderr, "%s: Don't use -E with -e.\n",
					program);
				sat_usag_redu();
			}
			else
				select_opts |= EVENT_EXCL;
			sat_pick_token(&event_list, optarg, c);
			break;
		case 'l':
			if (LABEL_EXCL & select_opts) {
				fprintf(stderr, "%s: Don't use -l with -L.\n",
					program);
				sat_usag_redu();
			}
			else
				select_opts |= LABEL_INCL;
			sat_pick_token(&label_list, optarg, c);
			break;
		case 'L':
			if (LABEL_INCL & select_opts) {
				fprintf(stderr, "%s: Don't use -L with -l.\n",
					program);
				sat_usag_redu();
			}
			else
				select_opts |= LABEL_EXCL;
			sat_pick_token(&label_list, optarg, c);
			break;
		case 'n':
			if (NAMED_EXCL & select_opts) {
				fprintf(stderr, "%s: Don't use -n with -N.\n",
					program);
				sat_usag_redu();
			}
			else
				select_opts |= NAMED_INCL;
			sat_pick_token(& named_list, optarg, c);
			break;
		case 'N':
			if (NAMED_INCL & select_opts) {
				fprintf(stderr, "%s: Don't use -N with -n.\n",
					program);
				sat_usag_redu();
			}
			else
				select_opts |= NAMED_EXCL;
			sat_pick_token(& named_list, optarg, c);
			break;
		case 'p':
			if (PROHIBITED & select_opts)
				sat_usag_redu();
			else
				select_opts |= PERMITTED;
			break;
		case 'P':
			if (PERMITTED  & select_opts)
				sat_usag_redu();
			else
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
		process_file(stdin, -1);

	for (c = (optind + 1 == argc) ? -1 : 0; optind < argc; c++, optind++) {
		if (VERBOSE & select_opts)
			(void) fprintf(stderr,"Input file: %s\n",argv[optind]);
		if ((in = fopen(argv[optind], "r" )) == NULL) {
			perror(argv[optind]);
			exit(1);
		}
		process_file(in, c);
		fclose(in);
	}
	exit(0);
	return (0);	/* needed to prevent compiler warning */
}
