/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: krpc_helper_data.h,v $
 * Revision 65.2  1997/10/20 19:16:27  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.39.2  1996/02/18  23:46:43  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:45:22  marty]
 *
 * Revision 1.1.39.1  1995/12/08  00:15:16  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:56:23  root]
 * 
 * Revision 1.1.37.1  1994/01/21  22:32:11  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  20:57:29  cbrooks]
 * 
 * Revision 1.1.2.4  1993/01/03  22:36:34  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  19:52:57  bbelch]
 * 
 * Revision 1.1.2.3  1992/12/23  19:39:31  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  14:53:40  zeliff]
 * 
 * Revision 1.1.2.2  1992/12/03  22:25:37  delgado
 * 	New krpc helper interface
 * 	[1992/12/03  22:24:58  delgado]
 * 
 * $EndLog$
 */

#ifndef _KRPC_HELPER_DATA_H 
#define _KRPC_HELPER_DATA_H 1

#ifdef SGIMIPS
struct krpc_helper
{
    struct krpc_helper *next;           /* next helper in queue */
    struct krpc_helper *prev;
    int states;                        /* states info */
    unsigned char *buffer;                       /* pointer to the basic buffer */
    unsigned char *bufptr;                       /* marshalling/unmarshalling position */
    int bytes;                         /* bytes of real data in buffer */
    int started;                       /* time started executing (WORKING state) */
    int returnCode;                    /* return code from send response call */
    int requestID;                     /* Id for matching request & response */
    int opcode;                       /* for prioritization */
    rpc_cond_t ccond;                    /* client process condition */
};
#else
struct krpc_helper
{
    struct krpc_helper *next;           /* next helper in queue */
    struct krpc_helper *prev;
    long states;                        /* states info */
    unsigned char *buffer;                       /* pointer to the basic buffer */
    unsigned char *bufptr;                       /* marshalling/unmarshalling position */
    long bytes;                         /* bytes of real data in buffer */
    long started;                       /* time started executing (WORKING state) */
    long returnCode;                    /* return code from send response call */
    long requestID;                     /* Id for matching request & response */
    long opcode;                       /* for prioritization */
    rpc_cond_t ccond;                    /* client process condition */
};
#endif /* SGIMIPS */

#endif /* _KRPC_HELPER_DATA_H  */
