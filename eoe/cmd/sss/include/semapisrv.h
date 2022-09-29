/* --------------------------------------------------------------------------- */
/* -                             SEMAPISRV.C                                 - */
/* --------------------------------------------------------------------------- */
/*                                                                             */
/* Copyright 1992-1999 Silicon Graphics, Inc.                                  */
/* All Rights Reserved.                                                        */
/*                                                                             */
/* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;      */
/* the contents of this file may not be disclosed to third parties, copied or  */
/* duplicated in any form, in whole or in part, without the prior written      */
/* permission of Silicon Graphics, Inc.                                        */
/*                                                                             */
/* RESTRICTED RIGHTS LEGEND:                                                   */
/* Use, duplication or disclosure by the Government is subject to restrictions */
/* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data      */
/* and Computer Software clause at DFARS 252.227-7013, and/or in similar or    */
/* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -     */
/* rights reserved under the Copyright Laws of the United States.              */
/*                                                                             */
/* --------------------------------------------------------------------------- */
#ifndef H_SEMAPISRV_H
#define H_SEMAPISRV_H

#define EVENTBLK_MAXHOSTNAME 256  /* Max host name in EVENTBLK structure */

typedef struct s_semevent {
 unsigned long struct_size;            /* IN: sizeof(EVENTBLK) */
 char hostname[EVENTBLK_MAXHOSTNAME];  /* OUT: hostname ASCIZ string */
 int hostnamelen;                      /* OUT: real host name len */
 int msgcnt;                           /* OUT: messages counter */
 int msgsize;                          /* IN: buffer size, OUT: accepted buffer size */
 char *msgbuffer;                      /* IN: buffer pointer */
 int eclass;                           /* OUT: event class */
 int etype;                            /* OUT: event type */
 int epri;                             /* OUT: event priority */
 unsigned long timestamp;              /* OUT: timestamp */
} EVENTBLK;

/* ------------------------- semsrvInit -------------------------------------- */
/* This function initialize SEM API Server socket and bind it to fixed socket  */
/* name.                                                                       */
/* parameter(s):                                                               */
/*  none                                                                       */
/* return code:                                                                */
/*  != 0 - success, == 0 - error                                               */
/* --------------------------------------------------------------------------- */
int semsrvInit();

/* ------------------------- semsrvDone -------------------------------------- */
/* This function close and unlink permanent UNIX socket.                       */
/* parameter(s):                                                               */
/*  none                                                                       */
/* return code:                                                                */
/*  != 0 - success, == 0 - error                                               */
/* --------------------------------------------------------------------------- */
void semsrvDone();

/* ------------------------ semsrvGetEvent ----------------------------------- */
/* This function accepts incoming events, parse, check and validate it.        */
/* The EVENTBLK have to be initialized before usage.                           */
/* parameter(s):                                                               */
/*  EVENTBLK *eb - pointer to event block structure                            */
/* return code:                                                                */
/*  != 0 - data is "ready" and EVENTBLK have correct fields                    */
/*  == 0 - data is not "ready"                                                 */
/* --------------------------------------------------------------------------- */
int semsrvGetEvent(EVENTBLK *eb);

#endif
