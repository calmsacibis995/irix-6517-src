/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: alt_common_krpc.h,v $
 * Revision 65.1  1997/10/20 19:16:24  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.36.2  1996/02/18  23:46:36  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:45:17  marty]
 *
 * Revision 1.1.36.1  1995/12/08  00:15:00  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/10/19  15:30 UTC  jrr
 * 	Remove HP internal code.
 * 
 * 	HP revision /main/HPDCE02/1  1995/01/16  19:19 UTC  markar
 * 	Merging in Local RPC mods
 * 
 * 	HP revision /main/markar_local_rpc/1  1995/01/16  14:32 UTC  markar
 * 	Implementing Local RPC bypass
 * 	[1995/12/07  23:56:15  root]
 * 
 * Revision 1.1.34.1  1994/01/21  22:31:57  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  20:57:18  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  22:36:09  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  19:52:16  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  19:38:42  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  14:52:58  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:16:09  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
#ifndef _ALT_COMMON_KRPC_H
#define _ALT_COMMON_KRPC_H 1

/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**
**
**  NAME:
**
**      alt_common_krpc.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Alternate common include files for kernel rpc.
**
**
*/

#include <dce/dce.h>
#include <rpcfork.h>
#include <rpcdbg.h>
#include <rpcclock.h>
#include <rpcmem_krpc.h>
#include <rpcmutex.h>
#include <rpctimer.h>
#include <rpclist.h>
#include <rpcrand_krpc.h>

/*
 * get the runtime's exception handling ids.
 */
#include <dce/ker/exc_handling_ids_krpc.h>

/*
 * If sysconf.h doesn't define uiomove, assume it's the "traditional
 * UNIX" definition.
 */
    
#ifndef rpc__uiomove
#define rpc__uiomove(buf,len,dir,uio) uiomove(buf,len,dir,uio)
#endif

#endif /* _ALT_COMMON_KRPC_H */
