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
/* Include files                                                             */
/*---------------------------------------------------------------------------*/
#include <stdio.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "sgmtask.h"
#include "sgmtaskerr.h"
#include "sgmstructs.h"
#include "sgmrpc.h"


/*---------------------------------------------------------------------------*/
/* Name    : xfrdata                                                         */
/* Purpose : Transfers the next packet of data to client                     */
/* Inputs  : client_xfr_t and server_xfr_t                                   */
/* Outputs : Number of bytes read from file                                  */
/*---------------------------------------------------------------------------*/

int xfrdata(sgmclntxfr_t *client, sgmsrvr_t *server)
{
    int            fildes;
    struct stat    statbuf;
    int            bytestoread;
    int            bytesremaining;
    char           *buf;
    int            tmp;
    int            attempts;


    if ( stat(client->filename, &statbuf) < 0 ) {
	server->errCode = ERR(SERVERERR, OSERR, errno);
	return(0);
    }

    if ( !(statbuf.st_mode & S_IFREG) ) {
	server->errCode = ERR(SERVERERR, PARAMERR, NOTAREGULARFILE);
	return(0);
    }

    if ( statbuf.st_size <= client->fileoffset ) {
	server->errCode = ERR(SERVERERR, PARAMERR, INVALIDOFFSET);
	return(0);
    }

    if ( (fildes = open(client->filename, O_RDONLY)) < 0 ) {
	server->errCode = ERR(SERVERERR, OSERR, errno);
	return(0);
    }

    bytesremaining = statbuf.st_size - client->fileoffset;
    lseek (fildes, client->fileoffset, SEEK_SET);

    if ( client->bytestotransfer > 0 ) {
	bytestoread = ( (bytesremaining-client->bytestotransfer) > 0 ?
			 client->bytestotransfer : bytesremaining );
    } else 
	bytestoread = ( (bytesremaining-MAXMEMORYALLOC) > 0 ? MAXMEMORYALLOC :
			 bytesremaining);

    if ( (buf = (char *) calloc(bytestoread, 1)) == NULL ) {
	server->errCode = ERR(SERVERERR, OSERR, MALLOCERR);
	return(0);
    }

    tmp = 0;
    attempts = 0;

    while ( attempts < 10 ) {
	tmp += read(fildes, buf, bytestoread);
	if ( tmp < bytestoread ) {
	    buf += tmp;
	    bytestoread -= tmp;
	} else break;

	attempts++;
    }

    /* Populate Server Structure */

    server->flag    = 0;
    server->errCode = 0;
    server->filename = client->filename;
    server->filenamelen = strlen(client->filename);
    server->bytes1 = tmp;
    server->bytes2 = bytesremaining - tmp;
    server->data = (char *) buf;
    close(fildes);

    return(1);

}
