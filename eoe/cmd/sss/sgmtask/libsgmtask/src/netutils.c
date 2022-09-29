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

#ident           "$Revision: 1.5 $"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

/*---------------------------------------------------------------------------*/
/* Name    : SGMNETIGetOffHostNmFrAddr                                       */
/* Purpose : Takes a sockadd_in structure and returns a hostname             */
/* Inputs  : sockaddr_in structure                                           */
/* Outputs : A newly allocated hostname pointer                              */
/*---------------------------------------------------------------------------*/

int SGMNETIGetOffHostNmFrAddr(struct sockaddr_in *host, char *ptr)
{
    struct hostent        *hp = NULL;    
    int                   i = 0;
    int                   junk = 1;

    if ( !host || !ptr ) return(1);

    while (junk) {
	hp = gethostbyaddr(&host->sin_addr.s_addr, 
			   sizeof(struct in_addr),
			   AF_INET);
	if ( hp != NULL ) break;
	if ( h_errno == TRY_AGAIN ) {
	    i++;
	    if ( i > 3 ) return(1);
	} else return(1);
    }

    strcpy(ptr, hp->h_name);
    return(0);
}

/*---------------------------------------------------------------------------*/
/* Name    : SGMNETIGetOffHostEnt                                            */
/* Purpose : Takes ahostname and returns the hostentry structure             */
/* Inputs  : Hostname                                                        */
/* Outputs : Host entry sturcture                                            */
/*---------------------------------------------------------------------------*/

struct hostent *SGMNETIGetOffHostEnt(char *host)
{
    struct  hostent    *hp = NULL;
    int                i = 0;
    int                junk = 1;

    if ( host == NULL ) return(NULL);
    
    while ( junk ) {
	hp = gethostbyname(host);
	if ( hp != NULL ) break;

	if ( h_errno == TRY_AGAIN ) {
	    i++;
	    if ( i > 3 ) {
		return(NULL);
	    }
	} else {
	    return(NULL);
	}
    }

    return(hp);
}

/*---------------------------------------------------------------------------*/
/* Name    : SGMNETIGetOffHostEnt                                            */
/* Purpose : Takes ahostname and returns the official hostname               */
/* Inputs  : Hostname                                                        */
/* Outputs : Official Hostname                                               */
/*---------------------------------------------------------------------------*/

int SGMNETIGetOffHostName(char *host, char *ptr)
{
    struct hostent     *hp = NULL;

    if ( !host || !ptr ) return(1);

    if ( (hp = SGMNETIGetOffHostEnt(host)) == NULL ) {
	return(0);
    } else {
        strcpy(ptr, hp->h_name);
        return(1);
    }
}

/*---------------------------------------------------------------------------*/
/* Name    : SGMNETICheckHostID                                              */
/* Purpose : Takes an ip address and checks if it is valid                   */
/* Inputs  : IP Address                                                      */
/* Outputs : 1 or 0                                                          */
/*---------------------------------------------------------------------------*/

char *SGMNETIGetHostIP(char *hostname)
{
    int       orghostid = 0;
    int       hostid = 0;
    struct in_addr addr;
    struct hostent *hp = NULL;

    if (hostname) {
	if ( (hp = SGMNETIGetOffHostEnt(hostname)) ) {
	    bcopy(hp->h_addr, (caddr_t) &addr, hp->h_length);
            return(inet_ntoa(addr));
	}
    }
    return(NULL);
}

/*---------------------------------------------------------------------------*/
/* Name    : SGMSUBSICheckHostname                                           */
/* Purpose : Takes 2 hostnames and compares their official hostnames         */
/* Inputs  : 2 hostnames and a flag that indicates whether comparision should*/
/*           be for same or for different                                    */
/* Outputs : 1 for error, 0 for success                                      */
/*---------------------------------------------------------------------------*/

int SGMSUBSICheckHostname(char *myhost, char *remotehost, int flag)
{
    struct hostent       *hp = NULL;
    char                 mhost[512];
    char                 remhost[512];
    int                  cmp = 0;

    /*-----------------------------------------------------------------------*/
    /* If flag is 3, then, dont' check for remotehost                        */
    /*-----------------------------------------------------------------------*/

    if ( flag == 3  && myhost == NULL ) return(1);
    else if ( flag != 3 && (remotehost == NULL || myhost == NULL) ) return(1);

    if ( (hp = SGMNETIGetOffHostEnt(myhost)) == NULL ) {
        return(1);
    } else {
	if ( flag == 3 ) return(0);
        strcpy(mhost,hp->h_name);
    }

    if ( (hp = SGMNETIGetOffHostEnt(remotehost)) == NULL ) {
        return(1);
    } else {
        strcpy(remhost,hp->h_name);
    }

    cmp = strcasecmp(mhost, remhost);

    /*-----------------------------------------------------------------------*/
    /*  If flag = 1, then compare for same hostnames in which                */
    /*  case cmp should be 0.  If flag == 0 then compare for                 */
    /*  different hosts in which case cmp should not be 0                    */
    /*-----------------------------------------------------------------------*/

    if ( flag == 1 && cmp != 0 ) return(1);
    if ( flag == 0 && cmp == 0 ) return(1);

    return(0);
}

