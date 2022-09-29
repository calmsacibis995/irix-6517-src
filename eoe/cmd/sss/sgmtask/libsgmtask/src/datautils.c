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

#ident           "$Revision: 1.4 $"

/*---------------------------------------------------------------------------*/
/* datautils.c                                                               */
/*  This file contains 2 utilities, pack_data and unpack_data that are       */
/*  used by most of the RPC procedures                                       */
/*---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sgmauth.h"

/*---------------------------------------------------------------------------*/
/* External Declarations                                                     */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Function Prototypes                                                       */
/*---------------------------------------------------------------------------*/

int getauthdetails(char *, __uint32_t *, __uint32_t *, char *, int *);
void doencr(void *, __uint32_t, __uint32_t, int);
int pack_data(char **, char *, int, void *, int, void *, int, int);
int unpack_data(char *, int, void **, int, void **, char *, int, int);
void SGMIfree(char *, void *);

/*---------------------------------------------------------------------------*/
/* Name    : getauthdetails                                                  */
/* Purpose : Takes an auth string and breaks it down into details            */
/* Inputs  : Pointer to auth string, pointers to authkey, mask,pwd and type  */
/* Outputs : 1 for success and 0 otherwise                                   */
/*---------------------------------------------------------------------------*/

int getauthdetails(authstr, authkey, authmask, pwd, authtype)
    char *authstr;
    __uint32_t  *authkey;
    __uint32_t  *authmask;
    char        *pwd;
    int         *authtype;
{
    int   i = 0, j = 0, k = 0;
    char  tmpAuth[20];

    if ( !authstr ) return(0);

    for ( i = 0; *(authstr+i) != 0; i++ ) {
	if ( *(authstr+i) == ':') {
	    tmpAuth[k] = '\0';
	    if ( j == 0 ) {
		*authkey = strtoul(tmpAuth, NULL, 16);
		j++;
		k = 0;
	    } else if ( j == 1 ) {
		*authmask = strtoul(tmpAuth, NULL, 16);
                j++;
                k = 0;
	    } else if ( j == 2 ) {
                *authtype = atoi(tmpAuth);
                break;
            }
	} else {
	    tmpAuth[k++] = *(authstr+i);
	}
    }

    if ( pwd ) {
	k = i+1;
	for ( j = 0; *(authstr+k) != ':'; k++,j++)
	    *(pwd+j) = *(authstr+k);
	*(pwd+j) = '\0';
    }

    return(1);
}

/*---------------------------------------------------------------------------*/
/* Name    : doencr                                                          */
/* Purpose : Encrypts a data packet using special algorithm.                 */
/* Inputs  : Takes a key/mask pair using which a buffer is encrypted         */
/* Outputs : None                                                            */
/*---------------------------------------------------------------------------*/

void doencr( void *ptr, __uint32_t key, 
		    __uint32_t mask, int size)
{
    unsigned char *str = (unsigned char *) ptr;
    int   i;

    key ^= (mask + (size*size));

    if ( size & 1 ) {
	for ( i = 0; i < size; i++ )
	    str[i] ^= (unsigned char)((key >>((i&0xf)+(i&0x7)+(i&0x3)))+(size-i));
    } else {
	for ( i = 0; i < size; i++ )
	    str[i] ^= (unsigned char)((key >>((i&0xf)+(i&0x7)+(i&0x2)))+(size+i));
    }

    return;
}

/*---------------------------------------------------------------------------*/
/* Name    : pack_data                                                       */
/* Purpose : Forms a data packet that is suitable for RPC client/server      */
/* Inputs  : Pointer to return data, auth. string, auth.string length,       */
/*           header, header lenght, data, datalen  and type of autherization */
/* Outputs : 1 for success and 0 otherwise                                   */
/*---------------------------------------------------------------------------*/

int pack_data(retPtr, authstr, authstrlen, header, headerlen, data, datalen, authtype)
    char **retPtr;
    char *authstr;
    int  authstrlen;
    void *header;
    int  headerlen;
    void *data;
    int  datalen;
    int  authtype;
{
    __uint32_t    auth_key = 0;
    __uint32_t    auth_mask = 0;
    int           flag;

    if ( !retPtr || !header || !headerlen || !authstr || !authstrlen ) return 0; 
    if ( (datalen && !data) || (!datalen && data) ) return 0;

    if ( !getauthdetails(authstr, &auth_key, &auth_mask, NULL,&flag) ) {
	return(0);
    } else if ( !auth_key || !auth_mask ) {
	return(0);
    }

    if ( authtype ) flag = authtype;

    authstrlen += (8-(authstrlen%8));

    /*-----------------------------------------------------------------------*/
    /*  Allocate bytes.  Total bytes to allocate are headerlen + datalen + 2 */
    /*-----------------------------------------------------------------------*/

    if (((*retPtr) = (char *) calloc(1, authstrlen+headerlen+datalen+2)) == NULL ) 
	return 0;
    
    /*-----------------------------------------------------------------------*/
    /*  Copy bytes from pointers sent                                        */
    /*-----------------------------------------------------------------------*/

    memcpy((*retPtr), authstr, authstrlen);
    memcpy((*retPtr)+authstrlen, header, headerlen);
    if ( datalen ) memcpy((*retPtr)+headerlen+authstrlen, data, datalen);

    /* Do Encryption here */

    if ( flag & FULLENCR ) {
	if ( flag & STANDARD_AUTH ) {
	    doencr((*retPtr), STANDARD_KEY, STANDARD_MASK, 
                                      authstrlen+headerlen+datalen);
	} else {
	    doencr((*retPtr), auth_key, auth_mask, 
                                      authstrlen+headerlen+datalen);
	}
    } else if ( flag & KEYENCR ) {
	if ( flag & STANDARD_AUTH ) {
	    doencr((*retPtr), STANDARD_KEY, STANDARD_MASK, authstrlen);
	} else {
	    doencr((*retPtr), auth_key, auth_mask, authstrlen);
	}
    }

    return authstrlen+headerlen+datalen;
}

/*---------------------------------------------------------------------------*/
/* Name    : unpack_data                                                     */
/* Purpose : Unpacks data that is received from RPC client/server            */
/* Inputs  : pointer to header, header length, pointer to data and actual dat*/
/* Outputs : 1 for success and 0 otherwise                                   */
/*---------------------------------------------------------------------------*/

int unpack_data(authstr, authstrlen, header, headerlen, data, ptr, ptrlen, authtype)
    char *authstr;
    int  authstrlen;
    void **header;
    int  headerlen;
    void **data;
    char *ptr;
    int  ptrlen;
    int  authtype;
{
    __uint32_t    auth_key = 0;
    __uint32_t    auth_mask = 0;
    int           flag;

    if ( !header || !ptr || !ptrlen || !authstr || !authstrlen ) return 0;
    if ( data && !headerlen ) return(0);

    if ( !getauthdetails(authstr, &auth_key, &auth_mask, NULL, &flag) ) {
	return(0);
    } else if ( !auth_key || !auth_mask ) {
	return(0);
    }

    if ( authtype ) flag = authtype;

    if ( flag & FULLENCR ) {
	if ( flag & STANDARD_AUTH ) {
	    doencr(ptr, STANDARD_KEY, STANDARD_MASK, ptrlen);
	} else {
	    doencr(ptr, auth_key, auth_mask, ptrlen);
	}
    } else if ( flag & KEYENCR ) {
	if ( flag & STANDARD_AUTH ) {
	    doencr(ptr, STANDARD_KEY, STANDARD_MASK, authstrlen);
	} else {
	    doencr(ptr, auth_key, auth_mask, authstrlen);
	}
    }

    if ( strncmp(authstr, ptr, authstrlen) != 0 ) return(0);

    /*-----------------------------------------------------------------------*/
    /*  Nothing much to do here.  We just modify pointers passed to us to    */
    /*  point to right locations                                             */
    /*-----------------------------------------------------------------------*/

    authstrlen += (8-(authstrlen%8));
    (*header) = (void *) (ptr+authstrlen);
    if (data) (*data)   = (void *) (ptr+headerlen+authstrlen);
    return 1;
}

void SGMIfree(char *ptr, void *ptr1)
{
#ifdef NDEBUG
    fprintf(stderr, "Freeing %x from %s\n", ptr1, str);
#endif
    free(ptr1);
}

void *SGMIcalloc(size_t nelem, size_t elsize)
{
    void  *ptr = NULL;

    ptr = calloc(nelem, elsize);
    return(ptr);
}
