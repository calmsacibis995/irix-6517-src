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

#ident           "$Revision: 1.6 $"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <crypt.h>
#include <time.h>
#include <ssdbapi.h>
#include "sgmauth.h"

/*---------------------------------------------------------------------------*/
/* sgmauth.c                                                                 */
/*   This file functions for getting security information for a given host.  */
/*   SGMAUTHISetAuth  - Sets/deletes the authorization details for a host    */
/*   SGMAUTHIGetAuth  - Retrieves/creates authorization details for a host   */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* External Declarations                                                     */
/*---------------------------------------------------------------------------*/

extern int getauthdetails(char *, __uint32_t *, __uint32_t *, char *, int *);
extern void doencr(void *, __uint32_t, __uint32_t, int);
extern int pack_data(char **, char *, int, void *, int, void *, int, int);
extern int unpack_data(char *, int, void **, int, void **, char *, int, int);
extern void SGMIfree(char *, void *);
extern int SGMSSDBSetTableData(char *,char *, int);
extern int SGMSSDBGetTableData(char *, char **, int *, int *, char *, char *);
extern int SGMNETIGetOffHostName(char *, char *);
extern int SGMSUBSICheckHostname(char *, char *, int);

/*---------------------------------------------------------------------------*/
/* Function Prototypes                                                       */
/*---------------------------------------------------------------------------*/

int SGMAUTHISetAuth(char *, char *, int);
int SGMAUTHIGetAuth(char *, char *, int *, int);
static int SGMAUTHINewAuth(char *, int);

/*---------------------------------------------------------------------------*/
/* SGMAUTHISetAuth                                                           */
/*   (Un)Sets the Authorization details in the Database                      */
/*---------------------------------------------------------------------------*/

int SGMAUTHISetAuth(char *hostname, char *authinfo, int flag)
{
    char   sqlstr[1024];
    char   offhname[512];
    __uint32_t key = 0;
    __uint32_t mask = 0;
    char   pwd[30];
    int    authtype = 0;

    if ( !SGMNETIGetOffHostName(hostname, offhname) ) return(0);

    if ( !getauthdetails(authinfo, &key, &mask, pwd, &authtype) ) {
	return(0);
    } 

    if ( flag == INSERTAUTH ) {
        snprintf(sqlstr, 1024,
             "insert into sss_auth values('%s','', '%x', '%x', '%s', %d)",
	     offhname, key, mask, pwd, authtype);
    } else {
	snprintf(sqlstr, 1024, 
                 "delete from sss_auth where auth_host like '%s%s'",
		 offhname, "%%");
    }

    if ( SGMSSDBSetTableData(sqlstr,"sss_auth", SSDB_REQTYPE_INSERT) )
	return(0);

    return(1);
}

/*---------------------------------------------------------------------------*/
/* SGMAUTHIGetAuth                                                           */
/*   Retrieves/Creates authorization string for a host                       */
/*---------------------------------------------------------------------------*/

int SGMAUTHIGetAuth(char *hname, char *authstr, int *authtype, int flag)
{
    char             sqlstr[1024];
    char             tmphname[512];
    char             *tmpAuthInfo = NULL;
    char             *junkvar = NULL;
    int              i = 0, j=0;
    __uint32_t       err = 0;

    if ( !hname ) return(0);
    if ( !authstr ) return(0);

    if ( !SGMNETIGetOffHostName(hname, tmphname) ) return(0);

    /* Generate a SQL String to send it to DB */

    snprintf(sqlstr, 1024, 
	     "select auth_host,auth_key,auth_mask,auth_encr,auth_pwd from sss_auth where auth_host like '%s%s'", 
            tmphname, "%%");

    if ((err = SGMSSDBGetTableData(sqlstr, &tmpAuthInfo, &i, &j, "@", ":"))) {
	return(0);
    }

    err = 1;
    snprintf(sqlstr, sizeof(sqlstr), "@%s", tmphname);
    if ( i ) {
	for ( j = 1;  *(tmpAuthInfo+j) && *(tmpAuthInfo+j) != ':'; j++ );
	if ( *(tmpAuthInfo+j+1) ) {
	    if ( (junkvar = strstr(tmpAuthInfo+j+1, sqlstr)) != NULL ) {
		*(junkvar) = '\0';
	    }
	    snprintf(authstr, AUTHSIZE, "%s:", tmpAuthInfo+j+1);
	    err = 0;
	}
    }

    SGMIfree("GetAuth:1", tmpAuthInfo);
    
    if ( err ) {
	if ( flag == CREATEAUTHIFNOTFOUND ) {
	    *authtype = STANDARD_AUTH|FULLENCR;
	    SGMAUTHINewAuth(authstr, AUTHSIZE);
	    if ( !SGMAUTHISetAuth(hname, authstr, INSERTAUTH) ) {
		return(0);
	    }
	} else {
	    return(0);
	}
    } else {
	*authtype = FULLENCR|CUSTOM_AUTH;
    }

    return(strlen(authstr));
}

static int SGMAUTHINewAuth(char *ptr, int len)
{
    __uint32_t  firstkey = 0;
    __uint32_t  firstmask = 0;
    __uint32_t  orgkey = 0;
    __uint32_t  orgmask = 0;
    struct      timespec tp;
    int         nobytes = 0;
    char        *pw;
    char        passwd[10];
    char        buf[17];
    char        salt[2];
    char        i;

    if ( !ptr ) return(0);

    /* Get the current time.  We will use nanoseconds */

    clock_gettime(CLOCK_REALTIME, &tp);
    srand(tp.tv_nsec);
    srandom(rand());
    firstkey = random();
    srand(firstkey-tp.tv_sec);
    srandom(rand());
    firstmask = random();

    /* Get a random auth_key */

    srandom(firstkey);
    orgkey = random();

    /* Get a random auth_mask */

    srandom(firstmask);
    orgmask = random();

    /* Generate a password */

    nobytes = snprintf(buf, 17, "%x%X", orgkey, orgmask);
    for ( i = 0; i < nobytes; i=i+2 ) {
	/*switch(buf[i]) {
	    case 'd':
	    case 'a':
		c = '+';
		break;
            case 'A':
		c = '$';
		break;
	    case '3':
	    case 'f':
		c = '_';
		break;
	    case '2':
	    case '9':
		c = ' ';
		break;
	    case '5':
		c = '[';
		break;
	    default:
		c = buf[i];
		break;
	}*/
	/*passwd[i/2] = c;*/
        passwd[i/2] = buf[i];
    }
    salt[0] = buf[11];
    salt[1] = buf[2];
    pw = crypt(passwd, salt);
    snprintf(ptr, len, "%x:%x:%d:%s:", orgkey, orgmask, FULLENCR|CUSTOM_AUTH,pw);
    SGMIfree("NewAuth:1", pw);
    return(1);
}
