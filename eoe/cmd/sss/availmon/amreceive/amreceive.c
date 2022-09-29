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
/* amreceive.c                                                               */
/* This is a generic receiver utility which is of use for any site           */
/* that needs to receive availmon reports of all forms.  This utility        */
/* converts compressed and/or, encrypted reports to text form.               */
/*---------------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <libgen.h>
#include <syslog.h>
#include "amdefs.h" 
#include "oamdefs.h" 

char	cmd[MAX_STR_LEN];
char	*tfn = NULL;

/*---------------------------------------------------------------------------*/
/* Name    : errorexit                                                       */
/* Purpose : Error handler                                                   */
/* Inputs  : character message                                               */
/* Outputs : never returns                                                   */
/*---------------------------------------------------------------------------*/

void
errorexit(char *msg)
{
    if (tfn)
	unlink(tfn);
    if (isatty(fileno(stderr))) {
	fprintf(stderr, "%s: Error! %s\n", cmd, msg);
	exit(1);
    } else {
	openlog(cmd, LOG_PID, 0);
	syslog(LOG_WARNING, "%s\n", msg);
	exit(0);
    }
}

/*---------------------------------------------------------------------------*/
/* Name    : main                                                            */
/* Purpose : main amreceive utility                                          */
/* Inputs  : command line arguments                                          */
/* Outputs : as requested                                                    */
/*---------------------------------------------------------------------------*/

main(int argc, char *argv[])
{
    char 	filename[MAX_LINE_LEN], syscmd[MAX_LINE_LEN];
    char	line[MAX_LINE_LEN], *p, errmsg[MAX_LINE_LEN];
    char	flag = '\0';
    int		amrtype, usetmp, r;
    FILE	*fp;
    extern int	errno;

    /*-----------------------------------------------------------------------*/
    /* Parse command line arguments                                          */
    /*-----------------------------------------------------------------------*/

    strcpy(cmd, basename(argv[0]));
    if (argc == 1) {
	usetmp = 1;
    } else if (argc == 2) {
	usetmp = 1;
	if (!strcmp(argv[1], "-a"))
	    flag = 'A';
	else if (!strcmp(argv[1], "-d"))
	    flag = 'D';
	else if (!strcmp(argv[1], "-i"))
	    flag = 'I';
	else {
	    strcpy(filename, argv[1]);
	    usetmp = 0;
	}
    } else if (argc == 3) {
	if (!strcmp(argv[1], "-a"))
	    flag = 'A';
	else if (!strcmp(argv[1], "-d"))
	    flag = 'D';
	else if (!strcmp(argv[1], "-i"))
	    flag = 'I';
	else
	    goto error;
	strcpy(filename, argv[2]);
	usetmp = 0;
    } else {
      error:
	sprintf(errmsg, "incorrect arguments\n"
		"Usage: %s [-a|-d|-i] [ <input file> ]", cmd);
	errorexit(errmsg);
    }

    /*-----------------------------------------------------------------------*/
    /* If there is no filename specified, then we need to read from stdin.   */
    /* open a temp file to store the data read in from stdin                 */
    /*-----------------------------------------------------------------------*/

    if (usetmp) {
	if ((tfn = tempnam(NULL, "avail")) == NULL) {
	    errorexit("cannot create temp file name");
	}
	if ((fp = fopen(tfn, "w")) == NULL) {
	    errorexit("cannot create temp file");
	}
	while (fgets(line, MAX_LINE_LEN, stdin))
	    fputs(line, fp);
	fclose(fp);
	strcpy(filename, tfn);
	fp = fopen(tfn, "r");
    } else {
	if ((fp = fopen(filename, "r")) == NULL) {
	    sprintf(errmsg, "cannot open email file %s", filename);
	    errorexit(errmsg);
	}
    }

    /*-----------------------------------------------------------------------*/
    /* start reading the data from fp looking for Subject: line              */
    /*-----------------------------------------------------------------------*/

    while (p = fgets(line, MAX_LINE_LEN, fp))
	if (strncmp("Subject:", line, 8) == 0)
	    break;

    if (p == NULL) {
	errorexit("not an Availmon report email");
    }

    if ((p = strstr(line, "AMR-")) == NULL) {
	errorexit("not an Availmon report email");
    }

    /*-----------------------------------------------------------------------*/
    /* If subject doesn't contain AMR-, then, it is not an availmon report.  */
    /* otherwise, parse the rest of the subject line to get in what form the */
    /* report is.  Perform required action (uncompress and/or decrypt)       */
    /*-----------------------------------------------------------------------*/

    p += 4;
    if (flag) {
	/* filter out unwanted reports */
	if ((*p != flag) && ((*p != 'I') || (flag != 'D')))
	    goto out;
    }

    p += 2;
    if (*p == 'R')
	/* skip registration flag */
	p += 2;

    line[strlen(line) - 1] = '\0';
    if (!strcmp(p, "T")) {
	while (p = fgets(line, MAX_LINE_LEN, fp))
	    if (!strncmp("SYSID|", line, 6) || !strncmp("SERIALNUM|", line, 10))
		break;
	if (p) {
	    do {
		if (line[0] != '\n')
		    fputs(line, stdout);
	    } while (fgets(line, MAX_LINE_LEN, fp));
	}
	fclose(fp);
    } else if (!strcmp(p, "Z-X-U")) {
	fclose(fp);
	sprintf(syscmd, "/var/adm/avail/amuudecode %s | /usr/bin/crypt %s"
		"| /usr/bsd/uncompress", filename, AMR_CODE);
	system(syscmd);
    } else if (!strcmp(p, "Z-U")) {
	fclose(fp);
	sprintf(syscmd, "/var/adm/avail/amuudecode %s | /usr/bsd/uncompress",
		filename);
	system(syscmd);
    } else if (!strcmp(p, "X-U")) {
	fclose(fp);
	sprintf(syscmd, "/var/adm/avail/amuudecode %s | /usr/bin/crypt %s",
		filename, AMR_CODE);
	system(syscmd);
    } else {
	errorexit("not an Availmon report email");
    }

out:
    if (tfn)
	unlink(tfn);
}
