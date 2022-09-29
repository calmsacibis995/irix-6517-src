/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: ndrglob.c,v 65.4 1998/03/23 17:29:42 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif



/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: ndrglob.c,v $
 * Revision 65.4  1998/03/23 17:29:42  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.3  1998/01/07 17:21:27  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:57:30  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:17:04  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.508.2  1996/02/18  00:04:45  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:56:53  marty]
 *
 * Revision 1.1.508.1  1995/12/08  00:21:03  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:59:46  root]
 * 
 * Revision 1.1.506.1  1994/01/21  22:38:11  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:58:26  cbrooks]
 * 
 * Revision 1.1.4.3  1993/01/03  23:53:05  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:09:19  bbelch]
 * 
 * Revision 1.1.4.2  1992/12/23  21:12:22  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:41:04  zeliff]
 * 
 * Revision 1.1.2.2  1992/05/01  16:38:11  rsalz
 * 	30-Mar-1992 ebm  Add vms conditional compilation to provide psect
 * 	                 name for ndr_g_local_drep definition (in order to
 * 	                 place it at front of shareable image with the
 * 	                 transfer vector).
 * 	[1992/05/01  16:29:54  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:12:23  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      ndrglob.c
**
**  FACILITY:
**
**      Network Data Representation (NDR)
**
**  ABSTRACT:
**
**  Runtime global variable definitions.
**
**
*/

#include <commonp.h>
#include <ndrp.h>
#include <ndrglob.h>

GLOBAL u_char ndr_g_local_drep_packed[4] = {
    (NDR_LOCAL_INT_REP << 4) | NDR_LOCAL_CHAR_REP,
    NDR_LOCAL_FLOAT_REP,
    0,
    0,
};

GLOBAL 
#ifdef VMS
/* Provide psect name if VMS */
{"ndr_g_local_drep"} 
#endif
ndr_format_t ndr_g_local_drep = {
    NDR_LOCAL_INT_REP,
    NDR_LOCAL_CHAR_REP,
    NDR_LOCAL_FLOAT_REP,
    0
};

GLOBAL rpc_transfer_syntax_t ndr_g_transfer_syntax = {
    {
        {
            0x8a885d04U, 0x1ceb, 0x11c9, 0x9f, 0xe8, 
            {0x8, 0x0, 0x2b, 0x10, 0x48, 0x60}
        },
        2
    },
    0,
    NULL
};
