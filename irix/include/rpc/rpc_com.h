/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _NET_RPC_RPC_COM_H	/* wrapper symbol for kernel use */
#define _NET_RPC_RPC_COM_H	/* subject to change without notice */

#ident	"@(#)uts-comm:net/rpc/rpc_com.h	1.2"

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*	PROPRIETARY NOTICE (Combined)
*
* This source code is unpublished proprietary information
* constituting, or derived under license from AT&T's UNIX(r) System V.
* In addition, portions of such source code were derived from Berkeley
* 4.3 BSD under license from the Regents of the University of
* California.
*
*
*
*	Copyright Notice 
*
* Notice of copyright on this source code product does not indicate 
*  publication.
*
*	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
*	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
*          All rights reserved.
*/ 
/*
 * rpc_com.h, Common definitions for both the server and client side.
 * All for the topmost layer of rpc
 *
 * Copyright (C) 1988, Sun Microsystems, Inc.
 */ 

/*
 * File descriptor to be used on xxx_create calls to get default descriptor
 */
#define	RPC_ANYSOCK	-1
#define RPC_ANYFD	RPC_ANYSOCK

/*
 * The max size of the transport, if the size cannot be determined
 * by other means.
 */
#define MAXTR_BSIZE 9000

extern u_int _rpc_get_t_size( int, long );
extern u_int _rpc_get_a_size( long );
extern int _rpc_dtbsize( void );
extern char *_rpc_gethostname( void );

#ifndef _KERNEL

bool_t rpc_control(int, void *);

/*
 * rpc_control commands
 */
#define	RPC_SVC_MTMODE_SET	1	/* set MT mode */
#define	RPC_SVC_MTMODE_GET	2	/* get MT mode */
#define	RPC_SVC_THRMAX_SET	3	/* set maximum number of threads */
#define RPC_SVC_THRMAX_GET	4	/* get maximum number of threads */
#define RPC_SVC_THRTOTAL_GET	5	/* get total number of threadsi */
#define RPC_SVC_THRCREATES_GET	6	/* get total threads created */
#define RPC_SVC_THRERRORS_GET	7	/* get total thread create errors */
#define	__RPC_CLNT_MINFD_SET	8	/* set min fd used for a clnt handle */
#define	__RPC_CLNT_MINFD_GET	9	/* get min fd used for a clnt handle */

#endif /* !_KERNEL */

#endif /* _NET_RPC_RPC_COM_H */
