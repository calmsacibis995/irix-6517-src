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

#ident           "$Revision: 1.3 $"

#ifndef __SUBSCRIBE_H_
#define __SUBSCRIBE_H_

/*---------------------------------------------------------------------------*/
/* Typedefs for Phases of (Un)Subscription                                   */
/*---------------------------------------------------------------------------*/

#define    PHASE1			10
#define    PHASE1_OK                    11
#define    PHASE1_NOK                   12
#define	   PHASE2			20
#define	   PHASE2_OK			21
#define	   PHASE2_NOK			22
#define    SUBSCRIBE_OK                 30
#define    UNSUBSCRIBE_OK               31
#define    CLEANUP_OK                   40
#define    NEWHOST                      1
#define    ENBLHOST                     2
#define    DSBLHOST                     4
#define    PROCDATA                     8
#define    SUBSCRIBE                    12
#define    UNSUBSCRIBE                  13
#define    CLEANUP                      14

/*---------------------------------------------------------------------------*/
/*  Structure for System Information.  This structure is used                */
/*  by both SEM and SGM to propogate their system information                */
/*---------------------------------------------------------------------------*/
 
typedef struct sysinfo_s {
    char         sys_id[18];
    __uint32_t   sys_type;
    char         sys_serial[18];
    char         hostname[64];
    char         ip_addr[24];
} sysinfo_t;

/*---------------------------------------------------------------------------*/
/*  Main Subscribe Structure used by both Subscribe and                      */
/*  Unsubscribe                                                              */
/*---------------------------------------------------------------------------*/
typedef struct subscribe_s {
    sysinfo_t    sysinfo;             /* System Information */
    __uint32_t   phase;               /* Current Phase      */
    __uint32_t   errCode;             /* Error Code         */
    __uint32_t   datalen;             /* Data Length        */
    time_t       timekey;             /* A Unique key       */
} subscribe_t;

/*---------------------------------------------------------------------------*/
/*  Internal Subscribe/UnSubscribe Structures                                */
/*---------------------------------------------------------------------------*/

typedef struct type_data_s  {
    __uint32_t          type_id;
    char                type_desc[128];
    struct type_data_s  *next;
} type_data_t;

typedef struct class_data_s {
    struct class_data_s *next;
    __uint32_t          class_id;
    char                class_desc[128];
    __uint32_t          nooftypes;
    type_data_t         *type;
} class_data_t;

#endif /* __SUBSCRIBE_H_ */
