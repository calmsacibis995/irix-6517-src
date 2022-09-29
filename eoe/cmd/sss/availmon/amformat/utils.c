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

#ident           "$Revision: 1.1 $"

/*---------------------------------------------------------------------------*/
/* utils.c                                                                   */
/*    various utilities used by amformat and parser                          */
/*---------------------------------------------------------------------------*/

#include <amformat.h>
#include <amdefs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <netdb.h>

/*---------------------------------------------------------------------------*/
/* Function Prototypes                                                       */
/*---------------------------------------------------------------------------*/

__uint64_t checkCmd(char *);
__uint64_t appendOutput(FILE *, char *, int, char *);
char *getCanonicalHostName(char *);

/*---------------------------------------------------------------------------*/
/* Name    : checkCmd                                                        */
/* Purpose : Takes a command string and checks for its existance             */
/* Inputs  : A String pointer, cmd                                           */
/* Outputs : 1 for failure or a 0 for sucessful completion                   */
/*---------------------------------------------------------------------------*/

__uint64_t checkCmd(char *cmd)
{
    struct stat statbuf;

    if ( (stat(cmd, &statbuf) < 0) || !S_ISREG(statbuf.st_mode) ) return(0);
    return(1);
}

/*---------------------------------------------------------------------------*/
/* Name    : appendOutput                                                    */
/* Purpose : Takes a file pointer, a string (which can either be a command   */
/*           name or a filename) type of which is specified by fpType.       */
/*           Appends the contents/output of pFile to pfp and returns         */
/* Inputs  : A File pointer, pfp                                             */
/*           A file name or a command string, pFile                          */
/*           Type of pFile, whether it is a command or a file                */
/*           OutType, to print out Begin & End routines                      */
/* Outputs : An error code, __uint64_t or a 0 for sucessful completion       */
/*---------------------------------------------------------------------------*/

__uint64_t appendOutput(FILE *pfp, char *pFile, int fpType, char *OutType) 
{

    char         pTmpBuffer[MAX_LINE_LEN];
    FILE         *pfp1 = NULL;

    if ( pfp == NULL ) return (-1);
    if ( pFile == NULL ) return (-1);

    /*-----------------------------------------------------------------------*/
    /* If fpType is a command, then we need to open a pipe to read the output*/
    /* of the command.  Otherwise, pFile points to a filename and let's try  */
    /* to open the file.                                                     */
    /*-----------------------------------------------------------------------*/

    if ( fpType == ISCOMMAND ) {
	if ( (pfp1 = popen(pFile, "r")) == NULL ) {
	    return(-1);
	}
    } else {
	if ( (pfp1 = fopen(pFile, "r")) == NULL ) {
	    return(-1);
	}
    }

    if ( OutType != NULL ) {
	fprintf(pfp, "%s|Begin\n", OutType);
    }

    /*-----------------------------------------------------------------------*/
    /* Read the output of the command or contents of the file and add to pfp */
    /*-----------------------------------------------------------------------*/

    while ( fgets(pTmpBuffer, MAX_LINE_LEN, pfp1) != NULL ) {
        fputs(pTmpBuffer, pfp);
    }

    if ( OutType != NULL ) {
	fprintf(pfp, "%s|End\n", OutType);
    }

    if ( fpType == ISCOMMAND ) {
	pclose(pfp1);
    } else {
	fclose(pfp1);
    }

    return(0);

}

/*---------------------------------------------------------------------------*/
/* Name    : getCanonicalHostName                                            */
/* Purpose : Takes a hostname and returns the canonical hostname for the same*/
/* Inputs  : A hostname                                                      */
/* Outputs : Canonical hostname, char *                                      */
/*---------------------------------------------------------------------------*/

char *getCanonicalHostName(char *hname)
{
    char               host[MAXHOSTNAMELEN+1];
    struct hostent     *hent;
    int                i = 0;

    if ( gethostname(host, MAXHOSTNAMELEN) != 0 ) {
	fprintf(errorfp, "Can't invoke gethostname");
	return(strdup(hname));
    }

    /*-----------------------------------------------------------------------*/
    /* if gethostname fails, then there is a problem.  Return whatever is    */
    /* to us.  Otherwise, compare host and hname.  If they both are different*/
    /* and if hname is 'localhost', then we don't need to do anything. Return*/
    /* whatever is passed to us.                                             */
    /*-----------------------------------------------------------------------*/

    if ( strcmp(hname, host) != 0 ) {  /* Not matching.  Could be */
				       /* localhost.  Check for it*/
        if ( strcmp(hname, "localhost") != 0 ) {
	    return(strdup(hname));
	}
    } 


    /*-----------------------------------------------------------------------*/
    /* Now, lets cycle thro' what is returned by gethostbyname call.  This   */
    /* will return us the canonical hostname for the host.                   */
    /*-----------------------------------------------------------------------*/

    i = 0;

    while ( 1 ) {
	hent = gethostbyname(host);
	if ( hent != NULL ) 
	    break;
	if ( h_errno == TRY_AGAIN ) {
	    i++;
	    if ( i > 3 ) {
		fprintf(errorfp, "Can't get hostname after 3 tries");
		return(strdup(hname));
	    }
	} else {
	    fprintf(errorfp, "Can't get hostname (unrecoverable error)");
	    return(strdup(hname));
	}
    }

    return(strdup(hent->h_name));
}
