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

#ident           "$Revision: 1.2 $"

#ifndef __SGMSTRUCTS_H_
#define __SGMSTRUCTS_H_

/*---------------------------------------------------------------------------*/
/*  Main structures used for Transfer of Data via RPC                        */
/*---------------------------------------------------------------------------*/

#include <sys/types.h>

typedef struct sgmclnt_s {
    __uint32_t    flag;
    __uint32_t    datalen;
    void          *dataptr;
} sgmclnt_t;


typedef struct sgmsrvr_s {
    __uint32_t    flag;
    __uint32_t    errCode;
    __uint32_t    bytes1;  /* Bytes Transferred/encrypted */
    __uint32_t    bytes2;  /* Bytes remaining/unencrypted */
    __uint32_t    filenamelen;
    char          *filename;
    char          *data;
} sgmsrvr_t;

typedef struct sgmclntxfr_s {
    __uint32_t    bytestotransfer;
    __uint32_t    fileoffset;
    __uint32_t    filenamelen;
    char          *filename;
} sgmclntxfr_t;

#endif /* __SGMSTRUCTS_H_ */

