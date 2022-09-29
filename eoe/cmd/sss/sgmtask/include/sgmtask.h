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

#ifndef __SGMTASK_H_
#define __SGMTASK_H_

#include <sys/types.h>

/*---------------------------------------------------------------------------*/
/* RPC Procedures                                                            */
/*---------------------------------------------------------------------------*/

#define	   SGMHNAMECHANGEPROC			1
#define    PROCESSPROC				2
#define    XMITPROC				3
#define    CLEANPROC				4
#define    CONFIGUPDTPROC			5
#define    SUBSCRIBEPROC                        6
#define    UNSUBSCRIBEPROC                      7
#define    DELIVERREMOTEPROC			8
#define    GETDATAPROC                          9
#define    SETAUTHPROC                          10
#define    SEMHNAMECHANGEPROC                   11

/*---------------------------------------------------------------------------*/
/* Common SSDB Definitions                                                   */
/*---------------------------------------------------------------------------*/

#define    SSDATABASE                           "ssdb"
#define    SSDBUSER                             NULL
#define    SSDBPASSWORD                         NULL

/*---------------------------------------------------------------------------*/
/* Some ConfigMon Definitions                                                */
/*---------------------------------------------------------------------------*/

#define    SYSINFOCHANGE			0x00200101
#define    CONFIGMONCLASS			4002

/*---------------------------------------------------------------------------*/
/* Generic Task Structure                                                    */
/*---------------------------------------------------------------------------*/

typedef struct sgmtaskrd_s {
    __uint32_t     flag;
    __uint32_t     datalen;
    void           *dataptr;
} sgmtaskrd_t;

typedef struct sgmtask_s {
    __uint32_t     datalen;
    void           *dataptr;
    sgmtaskrd_t    *rethandle;
} sgmtask_t;

typedef struct deliverEvent_s {
    int            eventClass;
    int            eventType;
    int            priority;
    time_t         eventTimeStamp;
    int            noOfEvents;
    int            evDataLength;
} deliverEvent_t;
#endif /* __SGMTASK_H_ */
