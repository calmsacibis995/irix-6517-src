/*--------------------------------------------------------------------*/
/*                                                                    */
/* Copyright 1992-1998 Silicon Graphics, Inc.                         */
/* All Rights Reserved.                                               */
/*                                                                    */
/* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics    */
/* Inc.; the contents of this file may not be disclosed to third      */
/* parties, copied or duplicated in any form, in whole or in part,    */
/* without the prior written permission of Silicon Graphics, Inc.     */
/*                                                                    */
/* RESTRICTED RIGHTS LEGEND:                                          */
/* Use, duplication or disclosure by the Government is subject to     */
/* restrictions as set forth in subdivision (c)(1)(ii) of the Rights  */
/* in Technical Data and Computer Software clause at                  */
/* DFARS 252.227-7013, and/or in similar or successor clauses in      */
/* the FAR, DOD or NASA FAR Supplement.  Unpublished - rights         */
/* reserved under the Copyright Laws of the United States.            */
/*                                                                    */
/*--------------------------------------------------------------------*/

#ifndef __AVAILMON_STRUCTS_H
#define __AVAILMON_STRUCTS_H

#include   <stdio.h>
#include   <stdlib.h>
#include   <string.h>
#include   <sys/types.h>
#include   <sys/param.h>


#ifndef MAXTYPES
#define MAXTYPES              7
#endif

/*--------------------------------------------------------------------*/
/* Event Structure definition                                         */
/*--------------------------------------------------------------------*/

typedef struct Event_s {
    __uint32_t                sys_id;
    char                      *serialnum;
    char                      *hostname;
    __uint32_t                eventcode;
    time_t                    eventtime;
    time_t                    lasttick;
    time_t                    prevstart;
    time_t                    starttime;
    __uint32_t                statusinterval;
    __int32_t                 bounds;
    char                      *diagtype;
    char                      *diagfile;
    char                      *syslog;
    __uint32_t                hinvchange;
    __uint32_t                verchange;
    __uint32_t                metrics;
    __uint32_t                flag;
    char                      *shutdowncomment;
    char                      *summary;
    __uint32_t                notifyflags;
} Event_t;

/*--------------------------------------------------------------------*/
/* Event & their descriptions                                         */
/*--------------------------------------------------------------------*/

typedef struct amrEvDesc_s {
    int                      eventcode;
    char                     *description;
} amrEvDesc_t;

/*--------------------------------------------------------------------*/
/* E-mail notification structures                                     */
/*--------------------------------------------------------------------*/

typedef struct emailaddrlist_s {
    char                     *address;
    struct emailaddrlist_s   *next;
} emailaddrlist_t;

typedef struct repparams_s {
    int                      reportType;
    char                     subject[4];
    char                     *filename;
    emailaddrlist_t          *addrlist;
} repparams_t;

/*--------------------------------------------------------------------*/
/* Avaimon's configuration structure                                  */
/*--------------------------------------------------------------------*/

typedef struct amConfig_s {
    __uint32_t               configFlag;      /* Bit fields signify 
                                                  various configuration
						  flags of availmon */
    __uint32_t               statusinterval;
    repparams_t              *notifyParams[MAXTYPES];
} amConfig_t;

#endif /* __AVAILMON_STRUCTS_H */
