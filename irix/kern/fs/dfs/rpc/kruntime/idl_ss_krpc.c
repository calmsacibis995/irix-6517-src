/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: idl_ss_krpc.c,v 65.4 1998/03/23 17:27:07 gwehrman Exp $";
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
 * $Log: idl_ss_krpc.c,v $
 * Revision 65.4  1998/03/23 17:27:07  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.3  1998/01/07 17:20:55  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:56:54  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:16:26  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.42.2  1996/02/18  00:00:41  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:53:47  marty]
 *
 * Revision 1.1.42.1  1995/12/08  00:15:09  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:56:20  root]
 * 
 * Revision 1.1.40.1  1994/01/21  22:32:04  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  20:57:24  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  22:36:24  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  19:52:41  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  19:39:18  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  14:53:25  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:16:28  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**  Copyright (c) 1990 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME
**
**      idl_ss_krpc.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Kernel memory allocation routines for use by the stubs / libnidl.
**
**
*/
#include <dce/dce.h>
#include <commonp.h>    /* Common declarations for all RPC runtime */
#include <com.h>        /* Common communications services */

#include <dce/idlbase.h>    /* because the prototypes use NIDL_PROTOTYPES */
#include <dce/ker/idl_ss_krpc.h>

/*
**++
**
**  ROUTINE NAME:       idl_malloc_krpc
**
**  SCOPE:              PRIVATE
**
**  DESCRIPTION:
**      
**  These memory allocation routines are intended to be used only by
**  v2 stubs.  Internal memory allocation requirements should use
**  the internal RPC_MEM_ALLOC macro.
**
**  INPUTS:
**
**      count           The number of bytes of contiguous memory to be
**                      allocated.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            none
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:
**
**      A pointer to the base of the memory that was allocated. 
**      A NULL pointer will be returned if memory cannot be allocated.
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE pointer_t idl_malloc_krpc
#ifdef _DCE_PROTO_
(size_t count)
#else
(count)
size_t           count;
#endif
{
    pointer_t                    *ptr;

    RPC_MEM_ALLOC (
        ptr,
        pointer_t,
        ((uint)count),
        RPC_C_MEM_UTIL,
        RPC_C_MEM_WAITOK);
    if(ptr == NULL) 
    {
        DIE("(idl_malloc_krpc)"); 
    }

    RPC_DBG_PRINTF(19, 5, ("(idl_malloc_krpc) 0x%x bytes at 0x%x\n",
        count, ptr));

    return (ptr);
}

/*
**++
**
**  ROUTINE NAME:       idl_free_krpc
**
**  SCOPE:              PRIVATE
**
**  DESCRIPTION:
**      
**  Free the memory allocated by a call to rpc_util_malloc. Internal
**  runtime routines should use the RPC_MEM_FREE macro directly.
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:
**
**      ptr             A pointer to the base of the memory to be freed.
**
**  OUTPUTS:            none
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE void idl_free_krpc
#ifdef _DCE_PROTO_
(pointer_t ptr)
#else
(ptr)
pointer_t ptr;
#endif
{
    RPC_DBG_PRINTF(19, 5, ("(idl_free_krpc) of 0x%x\n", ptr));

    RPC_MEM_FREE (ptr, RPC_C_MEM_UTIL);
}

