/*---------------------------------------------------------------------------*/
/*                                                                           */
/* Copyright 1992-1998 Silicon Graphics, Inc.                                */
/* All Rights Reserved.                                                      */
/*                                                                           */
/* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics Inc.;     */
/* contents of this file may not be disclosed to third parties, copied or    */
/* duplicated in any form, in whole or in part, without the prior written    */
/* permission of Silicon Graphics, Inc.                                      */
/*                                                                           */
/* RESTRICTED RIGHTS LEGEND:                                                 */
/* Use,duplication or disclosure by the Government is subject to restrictions*/
/* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data    */
/* and Computer Software clause at DFARS 252.227-7013, and/or in similar or  */
/* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -   */
/* rights reserved under the Copyright Laws of the United States.            */
/*                                                                           */
/*---------------------------------------------------------------------------*/
#ident "$Revision: 1.2 $"

/*---------------------------------------------------------------------------*/
/* amsyslog.c                                                                */
/* amsyslog extracts all the syslog messags of requested priority from SYSLOG*/
/* and OSYSLOG files and prints them to stdout.                              */
/*---------------------------------------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
/* #include <syslog.h> */
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include "amdefs.h"
#include "oamdefs.h"

#define MAXLINE		1024
#define BUFSIZE		128
#define DATELEN		15
#define PRIOFFSET	16
#define FACOFFSET	17
#define HOSTOFFSET	19

/*---------------------------------------------------------------------------*/
/* SYSLOG Message structure                                                  */
/*---------------------------------------------------------------------------*/

typedef struct slstruct {
    int		priority;
    int		facility;
    char	*date;
    char	*hostname;
    char	*source;
    char	*message;
    int		repeated;
    int		len;
    int		hostlen;
    int		srclen;
    int		msglen;
    char	lmsgdate[DATELEN + 1];
} syslogmsg;

/*---------------------------------------------------------------------------*/
/* Global Variables                                                          */
/*---------------------------------------------------------------------------*/

syslogmsg	slmsgbuf[BUFSIZE];
syslogmsg	*slmsg = slmsgbuf;
char		linebuf[BUFSIZE][MAXLINE];
char		*line = linebuf[0];
int		slidx = 0, size = 0, overflow = 0;
int		removedups = 1;
char		prilevel = '4';
char		lastdate[MAXLINE] = "Jan  1 00:00:00";
char		slfn[64] = "/var/adm/SYSLOG";
char		oslfn[64] = "/var/adm/oSYSLOG";
char		lastslfn[64] = "/var/adm/avail/.save/lastsyslog";

void	parse();
void	updateduplicate(syslogmsg *slmsg, int index, int freq);
int 	isdate(char *str);

main(int argc, char **argv)
{
    FILE	*fp, *fp1;
    int		c, k, errflg = 0;
    char	cmd[MAX_STR_LEN];

    strcpy(cmd, basename(argv[0]));

    /*-----------------------------------------------------------------------*/
    /* process online options                                                */
    /*-----------------------------------------------------------------------*/

    while ((c = getopt(argc, argv, "dp:S:O:l:")) != EOF)
	switch (c) {
	case 'd':
	    removedups = 0;
	    break;
	case 'p':
	    if ((optind - 1 == argc) || (optarg[0] == '-')) {
		/* no priority was specified */
		errflg++;
	    } else
		prilevel = optarg[0];
	    break;
	case 'S':
	    if ((optind - 1 == argc) || (optarg[0] == '-')) {
		/* no file was specified */
		errflg++;
	    } else
		strcpy(slfn, optarg);
	    break;
	case 'O':
	    if ((optind - 1 == argc) || (optarg[0] == '-')) {
		/* no file was specified */
		errflg++;
	    } else
		strcpy(oslfn, optarg);
	    break;
	case 'l':
	    if ((optind - 1 == argc) || (optarg[0] == '-')) {
		/* no file was specified */
		errflg++;
	    } else
		strcpy(lastslfn, optarg);
	    break;
	case '?':
	    errflg++;
	}

    if (errflg) {
	fprintf(stderr, "Usage: %s [-d] [-p<priority>] [-S <syslog file>]"
			" [-O <old systlog file>]\n", cmd);
	exit(1);
    }

    /*-----------------------------------------------------------------------*/
    /* initialization                                                        */
    /*-----------------------------------------------------------------------*/

    for (k = 0; k < BUFSIZE; k++)
	slmsgbuf[k].lmsgdate[DATELEN] = '\0';

    /*-----------------------------------------------------------------------*/
    /* get the date of last record from /var/adm/avail/.save/lastsyslog      */
    /*-----------------------------------------------------------------------*/

    if (fp = fopen(lastslfn, "r")) {
	fgets(lastdate, MAXLINE, fp);
	fclose(fp);
    }

    /*-----------------------------------------------------------------------*/
    /* open log file, defaults to /var/adm/SYSLOG                            */
    /*-----------------------------------------------------------------------*/

    if (fp = fopen(slfn, "r")) {
	if (fgets(line, MAXLINE, fp)) {
	    /* precheck the date */
	    if (datecmp(line, lastdate) > 0) {
		/* open /var/adm/oSYSLOG to get previous data */
		if (fp1 = fopen(oslfn, "r")) {
		    parse(fp1);
		    fclose(fp1);
		}
		parse(fp);
		fclose(fp);
	    } else {
		rewind(fp);
		parse(fp);
		fclose(fp);
	    }
	} else {
	    fclose(fp);
     /*----------------------------------------------------------------------*/
     /* open /var/adm/oSYSLOG to get previously saved data                   */
     /*----------------------------------------------------------------------*/

	    if (fp1 = fopen(oslfn, "r")) {
		parse(fp1);
		fclose(fp1);
	    }
	}
    } else if (fp1 = fopen(oslfn, "r")) {
	parse(fp1);
	fclose(fp1);
    } else
	return(0);

    /*-----------------------------------------------------------------------*/
    /* output the rest of the slmsgs in the buffer                           */
    /*-----------------------------------------------------------------------*/

    if (overflow) {
	for (k = (slidx + 1) & (BUFSIZE - 1); k != slidx;
					      k = (k+1) & (BUFSIZE - 1)) {
	    if (slmsgbuf[k].repeated) {
		linebuf[k][slmsgbuf[k].len - 1] = '\0';
		printf("%s <rpt %d tms, last msg %s>\n", linebuf[k],
		       slmsgbuf[k].repeated + 1, slmsgbuf[k].lmsgdate);
	    } else
		fputs(linebuf[k], stdout);
	}
    } else
	for (k = 0; k < slidx; k++) {
	    if (slmsgbuf[k].repeated) {
		linebuf[k][slmsgbuf[k].len - 1] = '\0';
		printf("%s <rpt %d tms, last msg %s>\n", linebuf[k],
		       slmsgbuf[k].repeated + 1, slmsgbuf[k].lmsgdate);
	    } else
		fputs(linebuf[k], stdout);
	}
}

/*---------------------------------------------------------------------------*/
/* Name    : parse                                                           */
/* Purpose : Parses the syslog file and prints out the required lines        */
/* Inputs  : File pointer                                                    */
/* Outputs : None                                                            */
/*---------------------------------------------------------------------------*/
void
parse(FILE *fp)
{
    int		len, hostlen, sourcelen, sourcelen1, msgoffset;
    char	*p, pri[2], c, *lp;
    int		index, repeated, lastsl = -1;

    while (p = fgets(line, MAXLINE, fp)) {
	if (!isdate(line))
	    continue;
	if (datecmp(line, lastdate) >= 0)
	    break;
    }

    if (p == NULL)
	return;

    pri[1] = NULL;

    do {
       /*--------------------------------------------------------------------*/
       /* parse the line, check the date and extract the later portion       */
       /*--------------------------------------------------------------------*/

	if (!isdate(line))
	    continue;

	slmsg->len = len = strlen(line);
	slmsg->date = line;
	/*
	pri[0] = *(line + PRIOFFSET);
	slmsg->priority = atoi(pri);     
	slmsg->facility = *(line + FACOFFSET);
	*/

	if (len < HOSTOFFSET + 1)
	    continue;
	hostlen = 0;
	lp = line + HOSTOFFSET;
	while (*lp != ' ' && *lp) {
	    hostlen++;
	    lp++;
	}
	if (*lp == '\0')
	    continue;
	slmsg->hostname = line + HOSTOFFSET;
	slmsg->hostlen = hostlen;

	sourcelen = sourcelen1 = 0;
	lp = line + HOSTOFFSET + hostlen + 1;
	while (*lp != ':') {
	    sourcelen++;
	    lp++;
	    if (*lp == ' ' || *lp == '\n') {
		sourcelen = 0;
		break;
	    } else if (*lp == '[')
		sourcelen1 = sourcelen - 1;
	}

	if (sourcelen) {
	    slmsg->source = line + HOSTOFFSET + hostlen + 1;
	    slmsg->srclen = sourcelen1 ? sourcelen1 : sourcelen;
	    msgoffset = HOSTOFFSET + hostlen + sourcelen + 3;
	} else {
	    slmsg->srclen = 0;
	    msgoffset = HOSTOFFSET + hostlen + 1;
	}

       /*--------------------------------------------------------------------*/
       /* a quick check for priority of the syslog message                   */
       /*--------------------------------------------------------------------*/

	if (line[PRIOFFSET] > prilevel)
	    if (!slmsg->srclen ||
                (slmsg->srclen && strncmp(slmsg->source, "sysctlrd", 8) &&
		strncmp(slmsg->source, "savecore", 8)) )
	    	continue;

	if (len <= msgoffset + 1)
	    continue;

	slmsg->message = line + msgoffset;
	/* slmsg->msglen = len - msgoffset; */

        /*-------------------------------------------------------------------*/
        /* check for the message 'last message repeated %d times'            */
        /*-------------------------------------------------------------------*/

	repeated = 0;
	sscanf(slmsg->message, "last message repeated %d times", &repeated);

	if (repeated) {
	    updateduplicate(slmsg, lastsl, repeated);
	    continue;  
	}

	if (removedups)
	    if ((index = duplicate(slmsg)) >= 0) {
		updateduplicate(slmsg, index, 1);
		lastsl = index;
		continue;  
	    }

        /*-------------------------------------------------------------------*/
        /* empty the slmsg buffer for the next line to be read in            */
        /*-------------------------------------------------------------------*/

	lastsl = slidx;
	size++;
	slidx = (slidx + 1) & (BUFSIZE - 1);
	line = linebuf[slidx];
	slmsg = &slmsgbuf[slidx];
	if (overflow) {

        /*-------------------------------------------------------------------*/
        /* If we have repeated messaeges, then, lets output the latest one   */
	/* and mention that the message repeated that many times             */
        /*-------------------------------------------------------------------*/

	    if (slmsg->repeated) {
		line[slmsg->len - 1] = '\0';
		printf("%s <rpt %d tms, last msg %s>\n", line,
		       slmsg->repeated + 1, slmsg->lmsgdate);
	    } else
		fputs(line, stdout);
	} else if (slidx == BUFSIZE - 1)
	    overflow = 1;
	slmsg->repeated = 0;

        /*-------------------------------------------------------------------*/
        /* read the next SYSLOG line                                         */
        /*-------------------------------------------------------------------*/
    } while (fgets(line, MAXLINE, fp));
}

/*---------------------------------------------------------------------------*/
/* Name    : duplicate                                                       */
/* Purpose : This routine checks for existance of duplicate syslog messages  */
/* Inputs  : syslog message                                                  */
/* Outputs : index value of slmsgbuf where syslog message is found or -1     */
/*---------------------------------------------------------------------------*/

int
duplicate(syslogmsg *slmsg)
{
    int k;

    if (size == 0) return(-1);

    if (overflow) {
	for (k = (slidx - 1) & (BUFSIZE - 1); k != slidx;
					      k = (k - 1) & (BUFSIZE - 1))
	    if (!strcmp(slmsgbuf[k].message, slmsg->message))
		if (!strncmp(slmsgbuf[k].source, slmsg->source, slmsg->srclen))
		    return(k);
    } else {
	for (k = slidx - 1; k >= 0; k--)
	    if (!strcmp(slmsgbuf[k].message, slmsg->message))
		if (!strncmp(slmsgbuf[k].source, slmsg->source, slmsg->srclen))
		    return(k);
    }

    return(-1);
}

void
updateduplicate(syslogmsg *slmsg, int index, int freq)
{
    strncpy(slmsgbuf[index].lmsgdate, slmsg->date, DATELEN);
    slmsgbuf[index].repeated += freq;
}

static char *mtable[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

/*---------------------------------------------------------------------------*/
/* Name    : month2int                                                       */
/* Purpose : converts character month to an integer                          */
/* Inputs  : character month                                                 */
/* Outputs : integer month                                                   */
/*---------------------------------------------------------------------------*/
int
month2int(const char * mstr)
{
    int i = 11;

    do {
        if (!strncmp(mtable[i], mstr, 3))
            return(i);
        i--;
    } while (i >= 0);
    return(-1); /* BUG are we checking for this return value ?? */

}

/*---------------------------------------------------------------------------*/
/* Name    : datecmp                                                         */
/* Purpose : Compares 2 character dates                                      */
/* Inputs  : character dates                                                 */
/* Outputs : integer                                                         */
/*---------------------------------------------------------------------------*/
int
datecmp(char *a, char *b)
{
    int d;

    if (strncmp(a, b, 3)) {
	d = month2int(a) - month2int(b);
	if (d < -7)
	    return(d + 12);
	else if (d > 7)
	    return(d - 12);
	else
	    return(d);
    } else
	return(strncmp(a + 3, b + 3, 12));
}

#define	L	1		/* A lower case char */
#define	S	2		/* A space */
#define	D	3		/* A digit */
#define	O	4		/* An optional digit or space */
#define	C	5		/* A colon */
#define	N	6		/* A new line */
#define U	7		/* An upper case char */
#define A	8		/* A alphabetic (upper or lower) */

/*		    A u g     5   2 3 : 4 8 : 5 1 */
char datetypes[] = {U,L,L,S,O,D,S,D,D,C,D,D,C,D,D,0};

/*---------------------------------------------------------------------------*/
/* Name    : isdate                                                          */
/* Purpose : checks whether a character string passed represents a date or no*/
/* Inputs  : character string                                                */
/* Outputs : integer, 1 for a date or 0 otherwise                            */
/*---------------------------------------------------------------------------*/
int
isdate(char *str)
{
	register char *cp, *tp;
	register int c;

	cp = str;
	tp = datetypes;
	while (*cp != '\0' && *tp != 0) {
		c = *cp++;
		switch (*tp++) {
		case L:
			if (c < 'a' || c > 'z')
				return(0);
			break;
		case U:
			if (c < 'A' || c > 'Z')
				return(0);
			break;
		case S:
			if (c != ' ')
				return(0);
			break;
		case D:
			if (!isdigit(c))
				return(0);
			break;
		case O:
			if (c != ' ' && !isdigit(c))
				return(0);
			break;
		case C:
			if (c != ':')
				return(0);
			break;
		}
	}
	if (*tp != 0)
		return(0);
	return(1);
}
