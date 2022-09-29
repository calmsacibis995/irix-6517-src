/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: pthread_exc.h,v $
 * Revision 65.1  1997/10/20 19:16:28  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.56.2  1996/02/18  23:46:47  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:45:26  marty]
 *
 * Revision 1.1.56.1  1995/12/08  00:15:24  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:56:29  root]
 * 
 * Revision 1.1.54.1  1994/01/21  22:32:25  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  20:57:36  cbrooks]
 * 
 * Revision 1.1.4.3  1993/01/03  22:36:48  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  19:53:22  bbelch]
 * 
 * Revision 1.1.4.2  1992/12/23  19:40:07  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  14:54:07  zeliff]
 * 
 * Revision 1.1.2.2  1992/05/01  17:56:26  rsalz
 * 	"Changed as part of rpc6 drop."
 * 	[1992/05/01  17:51:03  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:15:18  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
*/
#ifndef _PTHREAD_EXC_H
#define _PTHREAD_EXC_H 1
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**
**  NAME:
**
**      exc.h
**
**  FACILITY:
**
**      DCE Exception Package
**
**  ABSTRACT:
**
**  An exception based pthread API.  Such an API is essentially a NO-OP
**  for the kernel.  The runtime only wants an exception raising API
**  so that it doesn't have to explicitly check for errors (which, if they
**  occur cause nasty unwinds to occur).
**
*/

#include <dce/ker/pthread.h>
#include <dce/ker/exc_handling.h>

#endif /* _PTHREAD_EXC_H */
