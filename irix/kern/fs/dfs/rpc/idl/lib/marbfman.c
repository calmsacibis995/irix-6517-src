/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: marbfman.c,v 65.5 1998/03/24 17:42:43 lmc Exp $";
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
 * $Log: marbfman.c,v $
 * Revision 65.5  1998/03/24 17:42:43  lmc
 * Add or change typecasting to be correct on 64bit machines.  Added
 * end of comment where it was missing.
 *
 * Revision 65.4  1998/03/23  17:24:49  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.3  1998/01/07 17:19:51  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:55:59  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:15:36  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.10.2  1996/02/18  18:54:34  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  18:07:00  marty]
 *
 * Revision 1.1.10.1  1995/12/07  22:28:58  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  21:15:30  root]
 * 
 * Revision 1.1.6.2  1993/07/07  20:08:20  ganni
 * 	reduced stub idl sources
 * 	[1993/07/07  19:37:52  ganni]
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1990, 1991 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**  Digital Equipment Corporation, Maynard, Mass.
**  All Rights Reserved.  Unpublished rights reserved
**  under the copyright laws of the United States.
**
**  The software contained on this media is proprietary
**  to and embodies the confidential technology of
**  Digital Equipment Corporation.  Possession, use,
**  duplication or dissemination of the software and
**  media is authorized only pursuant to a valid written
**  license from Digital Equipment Corporation.
**
**  RESTRICTED RIGHTS LEGEND   Use, duplication, or
**  disclosure by the U.S. Government is subject to
**  restrictions as set forth in Subparagraph (c)(1)(ii)
**  of DFARS 252.227-7013, or in FAR 52.227-19, as
**  applicable.
**
**
**  NAME:
**
**      marbfman.c
**
**  FACILITY:
**
**      IDL Stub Runtime Support
**
**  ABSTRACT:
**
**      Buffer management when marshalling a node
**
**  VERSION: DCE 1.0
**
**  MODIFICATION HISTORY:
**
**  26-Dec-91 harrow      Correct alignment expression to use pointer arithmetic
**                        for portability.
*/

/* The ordering of the following 3 includes should NOT be changed! */
#include <dce/rpc.h>
#include <dce/stubbase.h>
#include <lsysdep.h>

#ifdef PERFMON
#include <dce/idl_log.h>
#endif

void rpc_ss_marsh_change_buff
#ifdef IDL_PROTOTYPES
(
    rpc_ss_marsh_state_t    *msp,  /* Pointer to marshalling state block */
    unsigned long size_next_structure
                            /* Size of next structure to be marshalled */
)
#else
( msp, size_next_structure )
    rpc_ss_marsh_state_t    *msp;  /* Pointer to marshalling state block */
    unsigned long size_next_structure;
                            /* Size of next structure to be marshalled */
#endif
{
    ndr_byte *wp_buff;
    unsigned long req_buff_size;
    int preserved_offset;    /* Must start marshalling in new buffer at
                                same offset (mod 8) we were at in old buffer */

#ifdef PERFMON
    RPC_SS_MARSH_CHANGE_BUFF_N;
#endif

    preserved_offset = ((long)msp->mp) % 8;
    /* If a current iovector and buffer exist */
    if (msp->p_iovec->elt[0].buff_addr != NULL)
    {
        /* Despatch buffer to the comm layer */
        msp->p_iovec->elt[0].data_len = msp->p_iovec->elt[0].buff_len
              - (msp->p_iovec->elt[0].data_addr - msp->p_iovec->elt[0].buff_addr)
                                    - msp->space_in_buff;
        rpc_call_transmit ( msp->call_h, msp->p_iovec, msp->p_st );

#ifdef NO_EXCEPTIONS
        if (*msp->p_st != error_status_ok) return;
#else
        /* If cancelled, raise the cancelled exception */
        if (*msp->p_st==rpc_s_call_cancelled) RAISE(RPC_SS_THREADS_X_CANCELLED);

        /*
         *  Otherwise, raise the pipe comm error which causes the stub to
         *  report the value of the status variable.
         */
        if (*msp->p_st != error_status_ok) RAISE(rpc_x_ss_pipe_comm_error);
#endif
    }
    /* Get a new buffer */
    req_buff_size = size_next_structure + 18;
    /* 18 = 7 bytes worst case alignment before data, then we expect another
                node to follow, so
            3 bytes worst case alignment
            4 bytes offset
            4 bytes node number */
    if (req_buff_size < NIDL_BUFF_SIZE) req_buff_size = NIDL_BUFF_SIZE;
    req_buff_size += 7; /* malloc may not deliver 8-byte aligned memory */
    wp_buff = (ndr_byte *)malloc( req_buff_size );
    if (wp_buff == NULL)
    {
        RAISE( rpc_x_no_memory );
        return;
    }
    /* Fill in fields of the iovector */
    msp->p_iovec->num_elt = 1;
    msp->p_iovec->elt[0].buff_dealloc = (rpc_ss_dealloc_t)free;
    msp->p_iovec->elt[0].flags = 0;
    msp->p_iovec->elt[0].buff_addr = wp_buff;
    msp->p_iovec->elt[0].buff_len = req_buff_size;
    /* malloc may not deliver 8-byte aligned memory */
    wp_buff = (byte_p_t)(((wp_buff - (byte_p_t)0) + 7) & ~7);
    msp->p_iovec->elt[0].data_addr = wp_buff + preserved_offset;
    /* Output parameters */
    msp->mp = (rpc_mp_t)msp->p_iovec->elt[0].data_addr;
    msp->space_in_buff = req_buff_size -
                 (msp->p_iovec->elt[0].data_addr - msp->p_iovec->elt[0].buff_addr);

#ifdef PERFMON
    RPC_SS_MARSH_CHANGE_BUFF_X;
#endif

}
