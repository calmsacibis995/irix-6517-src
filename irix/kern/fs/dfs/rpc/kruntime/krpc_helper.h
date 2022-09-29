/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: krpc_helper.h,v $
 * Revision 65.2  1997/10/20 19:16:27  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.78.2  1996/02/18  23:46:41  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:45:21  marty]
 *
 * Revision 1.1.78.1  1995/12/08  00:15:14  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:56:23  root]
 * 
 * Revision 1.1.76.2  1994/07/13  22:33:19  devsrc
 * 	merged with bl-10
 * 	[1994/06/29  12:18:07  devsrc]
 * 
 * 	Make dfsbind/krpc_helper buffers configureable
 * 	[1994/04/25  15:02:04  delgado]
 * 
 * 	dfsbind configurable request buffers
 * 	[1994/03/23  20:43:07  delgado]
 * 
 * Revision 1.1.76.1  1994/01/21  22:32:09  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  20:57:28  cbrooks]
 * 
 * Revision 1.1.74.1  1993/10/05  13:41:46  root
 * 	Add ports restriction support for krpc.
 * 	[1993/10/05  13:37:59  root]
 * 
 * Revision 1.1.5.5  1993/01/27  19:02:19  delgado
 * 	Add support for krpc test driver
 * 	[1993/01/27  19:01:54  delgado]
 * 
 * Revision 1.1.5.4  1993/01/03  22:36:32  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  19:52:53  bbelch]
 * 
 * Revision 1.1.5.3  1992/12/23  19:39:29  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  14:53:37  zeliff]
 * 
 * Revision 1.1.5.2  1992/12/03  22:16:46  delgado
 * 	New kprc helper interface
 * 	[1992/12/03  22:16:01  delgado]
 * 
 * Revision 1.1.2.2  1992/05/01  17:56:09  rsalz
 * 	"Changed as part of rpc6 drop."
 * 	[1992/05/01  17:50:54  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:16:35  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
*/
/*
**  Copyright (c) 1991 by
**      Hewlett-Packard Company, Palo Alto, Ca. 
**
**  NAME
**
**      krpc_helper.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**      Define interface to kernel upcall helper.
**
*/

/* Copyright (C) 1990 Transarc Corporation - All rights reserved */


#ifndef _KRPC_HELPER_H
#define _KRPC_HELPER_H 1

#include <dce/dce.h>

#define KRPC_PORT_STRING_SIZE 256

#ifdef _KERNEL
#include <dce/nbase.h>

/*
 * Helper structure: an opaque data type.
 */

struct krpc_helper;

/*
 * place the specified item in the specified queue.
 */

#define ENQUEUE_ITEM(queue, item) \
                      (queue)->prev->next = item; \
                      (item)->next = queue; \
                      (item)->prev = (queue)->prev; \
                      (queue)->prev = item;


/*
 * remove the specified entry from its queue
 */

#define DEQUEUE_ITEM(item) \
                      (item)->next->prev = (item)->prev; \
                      (item)->prev->next = (item)->next; \
                      (item)->prev = (item)->next = (struct krpc_helper *)NULL;

/*
 * Remove next item off of specified queue
 */

#define DEQUEUE_NEXT(queue, ptr) \
                      ptr = (queue)->next; \
                      (queue)->next = (queue)->next->next; \
                      (queue)->next->prev = (queue); \
                      ptr->next = ptr->prev = (struct krpc_helper *)NULL; 



#define QUEUE_EMPTY(queue) \
                   (((queue)->next == queue) && ((queue)->prev == queue))

#endif /* _KERNEL */

#define HIGH_PRIORITY(op) (((op) == 10) || ((op) == 11))

#define KRPCH_CONFIGURE _IOW('k' , 1 ,struct krpch_config)
#define KRPCH_ENABLE    _IO('k', 2)
#define KRPCH_RESTRICT_PORTS _IOW('k', 3, char[KRPC_PORT_STRING_SIZE] )

#define KRPC_MIN_BUFSIZE    2048

#ifdef	SGIMIPS
struct krpch_config{
       int qsize;
       int bufsize;
     };
#else
struct krpch_config{
       long qsize;
       long bufsize;
     };
#endif

#ifdef _KERNEL

/*
 * krpc_GetHelper:
 * 
 * Called by kernel client to make a request; waits for a helper, if all are 
 * busy, but fails (returning NULL) if none are even registered.
 */

struct krpc_helper *krpc_GetHelper _DCE_PROTOTYPE_ (( long));

/*
 * krpc_PutHelper:
 * 
 * Called by kernel client to release helper when requiest is done.
 */

void krpc_PutHelper _DCE_PROTOTYPE_ ((struct krpc_helper *)); /* helper to release */


/*
 * krpc_WriteHelper:
 * 
 * Called by client to marshall some data into helper's buffer.
 * May only be called if krpc_ReadHelper has not been called on this
 * helper since it was fetched.
 *
 * Returns status code indicating success or failure.
 */

unsigned32 krpc_WriteHelper _DCE_PROTOTYPE_ (
( 
  struct krpc_helper *, /* helper to write to */
  unsigned char *,         /* what to write */
  long
)
);

/* 
 * krpc_ReadHelper:
 * 
 * Called by client to get some response bytes.
 * If no reads have yet been done, blocks until helper responds to request.
 *
 * Returns status code indicating success or failure.
 * If rlen is non-NULL, *rlen will contain the number of bytes actually
 * transferred by this call; if rlen is NULL and not enough bytes are
 * available, returns rpc_s_helper_short_read.
 */

unsigned32 krpc_ReadHelper _DCE_PROTOTYPE_ (( 
  struct krpc_helper *, /* helper to read from */
  unsigned char *,         /* where to put read data */
  long ,              /* how much to read */
  long  *             /* how much actually read. */
 ));


/*
 * Called to initialize helper process mechanism.
 */

void krpc_helper_init _DCE_PROTOTYPE_ (( void));

/*
 * Called to cancel any pending requests that this process is supposed
 * to be serving.
 */

void krpc_CancelHelper _DCE_PROTOTYPE_ (( void ));

 /* prototypes for generic helper interfaces */

int krpch_open _DCE_PROTOTYPE_ ((void));


void krpch_close _DCE_PROTOTYPE_ (( void ));

int krpch_read _DCE_PROTOTYPE_ (( struct uio * ));

int krpch_write _DCE_PROTOTYPE_ (( struct uio * ));

int krpch_ioctl _DCE_PROTOTYPE_ (( int, caddr_t) );

int krpch_test_driver_open _DCE_PROTOTYPE_ (( void ));

#endif /* _KERNEL */

#include <dce/ker/krpc_helper_mach.h> /* platform specific items */

#endif /* _KRPC_HELPER_H__ */











