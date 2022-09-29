#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/ggd/daemon/RCS/griotab.c,v 1.9 1998/07/31 17:24:28 kanoj Exp $"

/*
 * griotab.c
 *	This file contains the routines which read and parse the
 *	/etc/grio.config file.
 *
 */

#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <bstring.h>
#include <grio.h>
#include <sys/types.h>
#include <sys/bsd_types.h>
#include <sys/stat.h>
#include <sys/uuid.h>
#include <fcntl.h>
#include <ctype.h>
#include "ggd.h"
#include "griotab.h"
#include "externs.h"
#include <syslog.h>
#include <stdarg.h>

/*
 * griotab.c
 *	The routines in this file process the commands specified in the
 *	/etc/grio_disks file. This file can be edited by the system
 *	administrator to customize the GRIO subsystem.
 *
 *	Currently there are TWO supported commands:
 *		ADD and REPLACE
 *
 *	ADD is of the form:
 *		ADD	"28 character disk id string"	IOSIZE	NUMIOS
 *
 *	This line indicates that a disk drive with the given id string
 *	can guarantee NUMIOS operations of size IOSIZE each second.
 *	This information is stored in the the "devdisks" array in the
 *	tabcomm.c file and consulted when the cfg utility is generating
 *	the /etc/grio_config file
 *
 *	REPLACE is of the form:
 *		REPLACE nodename	IOSIZE 	NUMIOS
 *
 *	This line indicates that the bandwidth characteristics of the
 *	given node should be set to optimal I/O size of IOSIZE and
 *	number of iops to NUMIOS. The REPLACE lines in the /etc/grio_disks
 *	are processed by a final pass in the cfg utility when generating
 *	the /etc/grio_config file.
 *
 *	Blank lines, and line beginning with the character "#" are ignored.
 *
 */

/*
 * Global definitions
 */
#define LINE_SIZE	250
#define TOKEN_SIZE	80

#define KEYW_ADD	1
#define KEYW_REPLACE	2
#define KEYW_COMMENT	3
#define KEYW_MAX	4

typedef struct keyword_map_elmnt {
	char		name[TOKEN_SIZE];
	int		id;
} keyword_map_elmnt_t;

keyword_map_elmnt_t	keywordmap[] = {
	{ "ADD",	KEYW_ADD },
	{ "REPLACE",	KEYW_REPLACE },
	{ "#",		KEYW_COMMENT }
};

struct replace_info {
	dev_info_t	devparms;
	char		name[DEV_NMLEN];
} replace_info;

char griotab_error_buffer[LINE_SIZE];

/*
 * STATIC and external function declarations. 
 */
STATIC int read_griotab(FILE *);
STATIC int griotab_get_number( char *);
STATIC int decode_keyword(char *);
STATIC void griotab_error(int);
extern int add_disk_io_limits(char *, int, int);
extern int add_default_disk_size(dev_t, int);

/*
 * process_griotab() 
 * 	This routine opens the given file (presumably /etc/grio_disks)
 * 	and calls read_griotab() to process its contents.
 * 
 * RETURNS: 
 * 	0 on success
 * 	-1 on error
 */
int
process_griotab(char *filename)
{
	int 	ret = -1;
	FILE 	*fd;
	
	if ((fd = fopen(filename, "r")) == NULL ) {
		printf("Could not open %s.\n",filename);
		return( ret );
	}

	ret = read_griotab(fd);

	fclose( fd );

	return( ret );
}

/*
 * decode_keywork() 
 * 	This routine takes the given string, looks it up in the
 * 	keyword table, and returns the number of the command.
 * 
 * RETURNS: 
 * 	0 if keyword was not found
 * 	positive integer representing the keyword on success
 */
STATIC int
decode_keyword(char *keyword)
{
	int i, id = 0;

	/*
	 * make a blankline seem like a comment
	 */
	if ( strlen(keyword) == 0 ) {
		return( KEYW_COMMENT );
	}

	for (i = 0; i < KEYW_MAX; i++) {
		if (strncmp(keywordmap[i].name, keyword,
				strlen(keywordmap[i].name)) == 0) {
			id = keywordmap[i].id;
			break;
		}
	}
	return(id);
}


/*
 * griotab_add() 
 * 
 * LINE FORMAT:
 *	ADD	" disk id string "	IOSIZE	IOCOUNT
 *
 *	Parse the arguements from a ADD command and pass them
 *	to the add_disk_io_limits() routine.
 *
 * RETURNS: 
 * 	0 on success.
 * 	-1 on failure; 
 */
int
griotab_add(char *line, int lnum)
{
	int	maxioops, optiosize, i;
	char	idstr[DISK_ID_SIZE + 1]; 
	char	maxioopsstr[TOKEN_SIZE], optiosizestr[TOKEN_SIZE];

	bzero(idstr, DISK_ID_SIZE + 1 );
	line = strchr(line, '"');
	if (line == NULL) {
		/* ERROR */
		sprintf(griotab_error_buffer,
			"Cannot parse ADD command line.\n");
		griotab_error(lnum);
		return( -1 );
	}
	line++;
	for (i = 0 ; i < DISK_ID_SIZE; i++ ) {
		idstr[i] = *line++;
	}
	if (*line != '"') {
		/* ERROR */
		sprintf(griotab_error_buffer,
			"Error in id string.\n");
		griotab_error(lnum);
		return( -1 );
	}
	line++;
	bzero(maxioopsstr, 10);
	bzero(optiosizestr, 10);
	sscanf(line,"%s %s",optiosizestr,maxioopsstr);
	optiosize = griotab_get_number(optiosizestr);
	if ( optiosize == -1 ) {
		/* ERROR */
		sprintf(griotab_error_buffer,
			"Error reading optimal I/O size .\n");
		griotab_error(lnum);
		return( -1 );
	}
	maxioops = griotab_get_number(maxioopsstr);
	if ( maxioops == -1 ) {
		/* ERROR */
		sprintf(griotab_error_buffer,
			"Error reading maximum number of operations.\n");
		griotab_error(lnum);
		return( -1 );
	}

	add_disk_io_limits( idstr, maxioops, optiosize);

	return( 0 );
}


/*
 *  griotab_get_number()
 *	This routine takes a character string as a parameter and converts
 *	it into an interger. If the string is of the form "##K", the K will
 *	cause the number to be multiplied by 1024.
 *
 * RETURNS:
 *	non-negative integer on success 
 * 	-1 on failure
 */
int
griotab_get_number( char *location)
{
	int num  = -1;
	char *str;

	if (location != NULL) {
		location = strpbrk(location, "0123456789");
		if (location != NULL) {
			num = atoi(location);
			str = location;
			while (isdigit(*str)) str++;
			/*
			 * Allow only K after the number.
			 */
			if (*str == 'K') {
				num = num * 1024;
			} else if (isalpha(*str)) {
				num = -1;
			}
		}
	}
	return(num);
}

/*
 * griotab_replace() 
 * 
 * LINE FORMAT:
 *	REPLACE	NODENAME IOSIZE	IOCOUNT
 *
 *	Parse the arguements from a REPLCE command and update the
 *	contents of the config file
 *
 * RETURNS: 
 * 	0 on success.
 * 	-1 on failure; 
 */
int
griotab_replace(char *line, int lnum)
{
	int	maxioops, optiosize;
	char	nodename[DEV_NMLEN + 1]; 
	char	maxioopsstr[TOKEN_SIZE], optiosizestr[TOKEN_SIZE];

	bzero(nodename, sizeof( nodename ) );
	bzero(maxioopsstr, sizeof(maxioopsstr));
	bzero(optiosizestr, sizeof(optiosizestr));
	sscanf(line,"REPLACE %s %s %s",nodename, optiosizestr, maxioopsstr);
	optiosize = griotab_get_number(optiosizestr);
	if ( optiosize == -1 ) {
		/* ERROR */
		sprintf(griotab_error_buffer,
			"Error reading optimal I/O size .\n");
		griotab_error(lnum);
		return( -1 );
	}
	maxioops = griotab_get_number(maxioopsstr);
	if ( maxioops == -1 ) {
		/* ERROR */
		sprintf(griotab_error_buffer,
			"Error reading maximum number of operations.\n");
		griotab_error(lnum);
		return( -1 );
	}

	bcopy(nodename, replace_info.name, sizeof(nodename));
	replace_info.devparms.maxioops = maxioops;
	replace_info.devparms.optiosize = optiosize;

	return( 0 );
}


/*
 * read_griotab()
 *	This routine reads the lines from the griotab file, decodes the
 *	keyword at the beginning of each line, and sends the line to the
 *	appropriate routine to be processed. Processing of the griotab
 *	file stops if an error is encountered.
 *
 * RETURNS:
 *	0 on succes
 *	-1 if an error was encountered in the griotab file.
 *
 */
int
read_griotab(FILE *fd)
{
	int error = 0, linenum = 1;
	char line[LINE_SIZE], keyword[TOKEN_SIZE];
	
	bzero(line, LINE_SIZE);
	bzero(keyword, TOKEN_SIZE);

	while ((fgets( line, sizeof(line), fd ))) {
		sscanf(line,"%s",keyword);
		switch (decode_keyword(keyword)) {
			case KEYW_ADD:
				error = griotab_add(line, linenum);
				break;
			case KEYW_COMMENT:
				break;
			case KEYW_REPLACE:
				error = griotab_replace(line, linenum);
				if (!error) 
				   	error = add_admin_info(replace_info.name, &replace_info.devparms);
				if (error)
					griotab_error(linenum);
				break;
			default:
				sprintf(griotab_error_buffer,
					"Unknown keyword %s.\n", keyword);
				griotab_error(linenum);
				error = -1;
				break;
		}
		linenum++;
		bzero(line, LINE_SIZE);
		bzero(keyword, TOKEN_SIZE);
	}
	return( error );
}

/*
 * griotab_error
 *	Print an error in the griotab file in a common format.
 *
 *
 *
 */
STATIC void
griotab_error(int linenum)
{
	errorlog("Error in %s file at line %d.\n",GRIODISKS, linenum);
	errorlog("%s",griotab_error_buffer);
	bzero( griotab_error_buffer, LINE_SIZE);
}

void
errorlog(char *string, ...)
{
	auto int done_init = 0;
	va_list	 ap;

	va_start(ap, string);
	if (done_init == 0) {
		done_init = 1;
		openlog("ggd", LOG_PERROR, LOG_DAEMON);
	}
	vsyslog(LOG_NOTICE, string, ap);
	va_end(ap);
}
