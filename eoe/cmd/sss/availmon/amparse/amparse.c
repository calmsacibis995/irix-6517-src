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
#ident "$Revision: 1.1 $"

/*---------------------------------------------------------------------------*/
/* amparse.c                                                                 */
/* Parses an availmon report and extracts requested information              */
/*---------------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <libgen.h>
#include "amdefs.h"
#include "oamdefs.h"

typedef enum {
    l_sysid, l_hostname, l_prevstart, l_stop, l_uptime, l_start,
    l_summary, l_uname, l_icrash, l_syslog, l_hinv,
    l_versions, l_gfxinfo, l_event, l_amrversion, l_statusinterval,
    l_serialnum, l_ntypes
} amlabel_t;

char	cmd[MAX_STR_LEN];
char	*types[] = {
	"SYSID|", "HOSTNAME|", "PREVSTART|", "STOP|", "UPTIME|", "START|",
	"SUMMARY|", "UNAME|", "ICRASH|", "SYSLOG|", "HINV|",
	"VERSIONS|", "GFXINFO|", "EVENT|", "AMRVERSION|", "STATUSINTERVAL|",
	"SERIALNUM|"
};
int	typelen[] = {
	6, 9, 10, 5, 7, 6, 8, 6, 7, 7, 5, 9, 8, 6, 11, 15, 10
};

/*---------------------------------------------------------------------------*/
/* Name    : errorexit                                                       */
/* Purpose : Error handling routine                                          */
/* Inputs  : Character *                                                     */
/* Outputs : Never returns                                                   */
/*---------------------------------------------------------------------------*/

void
errorexit(char *msg)
{
    fprintf(stderr, "Error: %s: %s\n", cmd, msg);
    fprintf(stderr, "Usage: %s -{SYSID|HOSTNAME|PREVSTART|PREVSTART1|PREVSTART2"
	    "|STOP|STOP1|STOP2|STOPC|EVENT|EVENT1|EVENT2|EVENTC|UPTIME"
	    "|START|START1|START2|AMRVERSION|SUMMARY|UNAME|STATUSINTERVAL"
	    "|ICRASH|SYSLOG|HINV|VERSIONS|GFXINFO}"
	    " [ <input file> ]\n", cmd);
    exit(-1);
}

/*---------------------------------------------------------------------------*/
/* Name    : main                                                            */
/* Inputs  : Command line arguments                                          */
/* Outputs : As requested                                                    */
/*---------------------------------------------------------------------------*/

main(int argc, char *argv[])
{
    char 	fieldname[MAX_STR_LEN], fieldname1[MAX_STR_LEN];
    char	line[MAX_LINE_LEN], *p, *p1;
    int		fieldtype, fieldtype1, backcompat = 0;
    FILE	*fp;

    strcpy(cmd, basename(argv[0]));
    if (argc == 3) {
	if (argv[1][0] == '-' && argv[2][0] != '-') {
	    strcpy(fieldname, argv[1] + 1);
	    if ((fp = fopen(argv[2], "r")) == NULL) {
		sprintf(line, "cannot open file %s", argv[2]);
		errorexit(line);
	    }
	} else if (argv[1][0] != '-' && argv[2][0] == '-') {
	    strcpy(fieldname, argv[2] + 1);
	    if ((fp = fopen(argv[1], "r")) == NULL) {
		sprintf(line, "cannot open file %s", argv[1]);
		errorexit(line);
	    }
	} else
	    errorexit("incorrect arguments");
    } else if (argc ==  2) {
	if (argv[1][0] == '-') {
	    strcpy(fieldname, argv[1] + 1);
	    fp = stdin;
	} else
	    errorexit("incorrect argument");
    } else
	errorexit("incorrect argument number");

    for (fieldtype = 0; fieldtype < l_ntypes; fieldtype++)
	if (strncmp(fieldname, types[fieldtype], typelen[fieldtype] - 1) == 0)
	    break;

    /* handling the ugly back compatibility part */

    switch(fieldtype) {
    case l_sysid:		/* SYSID and SERIALNUM are the same */
	backcompat = 1;
	fieldtype1 = l_serialnum;
	break;
    case l_serialnum:		/* SYSID and SERIALNUM are the same */
	backcompat = 1;
	fieldtype1 = l_sysid;
	break;
    case l_stop:		/* STOP and EVENT are the same */
	backcompat = 1;
	fieldtype1 = l_event;
	strcpy(fieldname1, "EVENTC");
	fieldname1[5] = fieldname[4];
	break;
    case l_event:		/* STOP and EVENT are the same */
	backcompat = 1;
	fieldtype1 = l_stop;
	strcpy(fieldname1, "STOPC");
	fieldname1[4] = fieldname[5];
	break;
    case l_ntypes:
	sprintf(line, "incorrect argument -%s", fieldname);
	errorexit(line);
    }

    while (p = fgets(line, MAX_LINE_LEN, fp))
	if (strncmp(line, types[fieldtype], typelen[fieldtype]) == 0) {
	    break;
	} else if (backcompat) {
	    if (strncmp(line, types[fieldtype1], typelen[fieldtype1]) == 0) {
		/* substitute the option */
		fieldtype = fieldtype1;
		strcpy(fieldname, fieldname1);
		break;
	    }
	}

    if (p == NULL)
	errorexit("not an Availmon report or no such field");

    switch(fieldtype) {
    case l_sysid:
    case l_hostname:
    case l_uptime:
    case l_uname:
    case l_amrversion:
    case l_statusinterval:
    case l_serialnum:
	fputs(line + typelen[fieldtype], stdout);
	break;
    case l_prevstart:
    case l_start:
	p = line + typelen[fieldtype];
	switch (fieldname[typelen[fieldtype] - 1]) {
	case '\0':
	    fputs(p, stdout);
	    break;
	case '1':
	    if ((p1 = strchr(p, '|')) == NULL)
		errorexit("incorrect Availmon report");
	    *p1 = '\0';
	    puts(p);
	    break;
	case '2':
	    if ((p1 = strchr(p, '|')) == NULL)
		errorexit("incorrect Availmon report");
	    p = p1 + 1;
	    if (p1 = strchr(p, '|'))
		*p1 = '\0';		/* get rid of NOTMULTIUSER field */
	    puts(p);
	    break;
	default:
	    sprintf(line, "incorrect argument -%s", fieldname);
	    errorexit(line);
	}
	break;
    case l_stop:
	p = line + 5;
	switch (fieldname[4]) {
	case '\0':
	    fputs(p, stdout);
	    break;
	case '1':
	    if ((p1 = strchr(p, '|')) == NULL)
		errorexit("incorrect Availmon report");
	    *p1 = '\0';
	    puts(p);
	    break;
	case '2':
	    if ((p1 = strrchr(p, '|')) == NULL)
		errorexit("incorrect Availmon report");
	    fputs(p1 + 1, stdout);
	    break;
	case 'C':
	    if ((p1 = strchr(p, '|')) == NULL)
		errorexit("incorrect Availmon report");
	    p = p1 + 1;
	    if ((p1 = strchr(p, '|')) == NULL)
		errorexit("incorrect Availmon report");
	    *p1 = '\0';
	    puts(p);
	    break;
	default:
	    sprintf(line, "incorrect argument -%s", fieldname);
	    errorexit(line);
	}
	break;
    case l_event:
	p = line + 6;
	switch (fieldname[5]) {
	case '\0':
	    fputs(p, stdout);
	    break;
	case '1':
	    if ((p1 = strchr(p, '|')) == NULL)
		errorexit("incorrect Availmon report");
	    p = p1 + 1;
	    if ((p1 = strchr(p, '|')) == NULL)
		errorexit("incorrect Availmon report");
	    *p1 = '\0';
	    puts(p);
	    break;
	case '2':
	    if ((p1 = strrchr(p, '|')) == NULL)
		errorexit("incorrect Availmon report");
	    fputs(p1 + 1, stdout);
	    break;
	case 'C':
	    if ((p1 = strchr(p, '|')) == NULL)
		errorexit("incorrect Availmon report");
	    *p1 = '\0';
	    puts(p);
	    break;
	default:
	    sprintf(line, "incorrect argument -%s", fieldname);
	    errorexit(line);
	}
	break;
    case l_summary:
    case l_icrash:
    case l_syslog:
    case l_hinv:
    case l_versions:
    case l_gfxinfo:
	while (fgets(line, MAX_LINE_LEN, fp)) {
	    if (strncmp(line, types[fieldtype], typelen[fieldtype]) == 0)
		break;
	    fputs(line, stdout);
	}
    }

    fclose(stdin);
}
