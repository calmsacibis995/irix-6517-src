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

#ifndef __SGMRPC_H_
#define __SGMRPC_H_

#include <rpc/rpc.h>

/*---------------------------------------------------------------------------*/
/* RPC Definitions                                                           */
/*---------------------------------------------------------------------------*/

#define    RPCPROGRAM				( (u_long) 391029)
#define	   RPCVERSIONS				1
#define    MAXRPCPROC				9

/*---------------------------------------------------------------------------*/
/* RPC Parameters                                                            */
/*---------------------------------------------------------------------------*/

#define    DEFAULTTIMEOUT			300
#define    MAXBUFSIZE                           1024
#define    MAXMEMORYALLOC              		20000
#define    NOKEY                        	0
#define    TIMEKEY                      	1
#define    ISFILE                       	16
#define    ENCRON                       	32

#define	   EXIT					1
#define    NOEXIT				0


#endif /* __SGMRPC_H_ */
