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

#ident           "$Revision: 1.7 $"

#ifndef  __SGMTASKERR_H_
#define  __SGMTASKERR_H_

#include  <sys/types.h>

/*---------------------------------------------------------------------------*/
/*  Error codes are 32 bit integers.  These 32 bit integers are diveded as   */
/*  follows :                                                                */
/*                                                                           */
/*  Bits 31-24                  Module bits                                  */
/*  Bits 23-20                  Represents Client Side Error or Server       */
/*                              Side error                                   */
/*  Bits 19-16                  Represent whether the error is an OS         */
/*                              Error or RPC error or SSS error.             */
/*  Bits 15-0                   are left for type of error.  For SSS         */
/*                              modules, bits 15-8 will represent the        */
/*                              major module and 7-0 will represent          */
/*                              the type of error that happened.             */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Module Definition (Bits 31 - 24)                                          */
/*---------------------------------------------------------------------------*/

#define  SGMTASK                       4    /* SGM Task Module               */
#define  SSSERROR                      3    /* Problem in SSS Modules        */
                                            /* SSS Errors are again broken   */
					    /* into 2 parts, with bits 15-8  */
					    /* indicating the SSS Module and */
					    /* bits 7-0 indicating the error */

/*---------------------------------------------------------------------------*/
/* Sub Module Definition (Bits 23 - 20)                                      */
/*---------------------------------------------------------------------------*/

#define  CLIENTERR                     1    /* Indicates problem on client   */
#define  SERVERERR                     2    /* Indicates problem on server   */

/*---------------------------------------------------------------------------*/
/* Error Type Definition (Bits 19 - 16)                                      */
/*---------------------------------------------------------------------------*/

#define  OSERR                         1    /* Indicates OS Problem          */
#define  RPCERR                        2    /* Error happened in RPC Proto   */
#define  PARAMERR                      4    /* Call Parameters problems      */
#define  NETERR                        5    /* Problem in network routines   */
#define  AUTHERR                       6    /* Authentication Error          */

/*---------------------------------------------------------------------------*/
/* Sub Type definitions for OSERR are always errno that is returned          */
/*---------------------------------------------------------------------------*/

#define  MALLOCERR                     159
#define  GETTEMPFILENAME               160

/*---------------------------------------------------------------------------*/
/* Sub Type definitions for AUTHERR                                          */
/*---------------------------------------------------------------------------*/

#define CANTCREATEAUTHINFO             1
#define CANTUNPACKDATA                 2
#define CANTPACKDATA                   3
#define CANTGETAUTHINFO                4

/*---------------------------------------------------------------------------*/
/* Sub Type definitions for NETERR (Bits 15-0)                               */
/*---------------------------------------------------------------------------*/

#define  INVALIDHOST                   1
#define  CANTGETHOSTINFO               2    /* Can't get Host information    */

/*---------------------------------------------------------------------------*/
/* Sub Type definitions for RPCERR (Bits 15-0)                               */
/*---------------------------------------------------------------------------*/

#define  RPCTCPPROTOINITERR            40   /* Error in TCP init             */
#define  CLOBBEREDDATA                 41   /* Data clobbered                */

/*---------------------------------------------------------------------------*/
/* Sub type definitions for PARAMERR (Bits 15-0)                             */
/*---------------------------------------------------------------------------*/

#define  INVALIDARGS                   10   /* Error in svc_getargs          */
#define  INVALIDHOSTNAME               1    /* Invalid Hostname specified    */
#define  INVALIDTASKPTR                2    /* Task Pointer is not valid     */
#define  INVALIDDATAPTR                3    /* Data Pointer in Task not valid*/
#define  INVALIDPROC                   4    /* Invalid RPC Procedure         */
#define  INVALIDOFFSET                 5    /* Invalid offset specified      */
#define  INVALIDFILENAME               6    /* Invalid filename specified    */
#define  NOTAREGULARFILE               7    /* File is not a regular file    */
#define  INCOMPLETEDATA                8    /* Data received is not complete */
#define  INVALIDMULTIPLEHOST           9    /* Multiple hosts found in DB    */ 

/*---------------------------------------------------------------------------*/
/* Sub Type definitions for SSSERR (Bits 15-8)                               */
/*---------------------------------------------------------------------------*/

#define  SSDBERR                       1    /* Error happened in SSDB        */
#define  EVENTMON                      2    /* Error happened in EVENTMON    */
#define  SEM                           3    /* Error happened in SEM         */

#define  EVMONNOTRUNNING               1    /* EventMon not running          */
#define  EVMONAPIERR                   2    /* Error in EventMon API         */

#define  GETTABLEHEADER                1    /* SGM Conf Task Error           */
#define  XTRACTDATA                    2    /* SGM Conf Task Error           */

#define NORECORDSFOUND                 29
#define UNKNOWNSSDBERR                 30
#define UNABLETOGETACTIONID	       31
#define TOOMANYSUBSCRIPTIONS	       32

/*---------------------------------------------------------------------------*/
/* Some Macros to handle error codes                                         */
/*---------------------------------------------------------------------------*/

#define  ERR(b16,b8,b0)                (SGMTASK<<24|b16<<16|b8<<8|b0)
#define  SRVRERR(b)                    (SGMTASK<<24|b)
#define  SSSERR(b16,b8,b0)             (SSSERROR<<24|b16<<16|b8<<8|b0)



/*---------------------------------------------------------------------------*/
/* Function Prototypes                                                       */
/*---------------------------------------------------------------------------*/

extern  void pSSSErr(__uint32_t);

#endif /*__SGMTASKERR_H_ */
