/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: pkl_krpc.c,v 65.4 1998/03/23 17:26:27 gwehrman Exp $";
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
 * $Log: pkl_krpc.c,v $
 * Revision 65.4  1998/03/23 17:26:27  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.3  1998/01/07 17:20:48  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:56:43  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:16:28  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.54.2  1996/02/18  00:00:49  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:53:51  marty]
 *
 * Revision 1.1.54.1  1995/12/08  00:15:22  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:56:27  root]
 * 
 * Revision 1.1.52.1  1994/01/21  22:32:21  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  20:57:34  cbrooks]
 * 
 * Revision 1.1.4.3  1993/01/03  22:36:43  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  19:53:14  bbelch]
 * 
 * Revision 1.1.4.2  1992/12/23  19:39:57  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  14:53:59  zeliff]
 * 
 * Revision 1.1.2.2  1992/07/17  17:40:45  sommerfeld
 * 	Pick up type-uuid fixes from user-space version.
 * 	[1992/07/16  19:52:47  sommerfeld]
 * 
 * Revision 1.1  1992/01/19  03:16:38  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**  Copyright (c) 1991 by
**      Hewlett-Packard Company, Palo Alto, Ca.
**
**
**  NAME:
**
**      pkl.c
**
**  FACILITY:
**
**      Pickling support
**
**  ABSTRACT:
**
**      routines for manipulating pickles.
**
**
*/

#include <dce/dce.h>
#include <dce/idlbase.h>
#include <dce/rpc.h>
#include <dce/stubbase.h>
#include <dce/pkl.h>


#define swap_unsigned16(src, dst)\
    dst[0]=src[1];\
    dst[1]=src[0]

#define swap_unsigned32(src, dst)\
    dst[0]=src[3];\
    dst[1]=src[2];\
    dst[2]=src[1];\
    dst[3]=src[0]

#define swap_uuid(src, dst)\
    swap_unsigned32(src, dst);\
    swap_unsigned16((&src[4]),(&dst[4]));\
    swap_unsigned16((&src[6]),(&dst[6]));\
    {int i; for (i=8; i<16; i++) dst[i]=src[i];}


/*
 * i d l _ p k l _ g e t _ t y p e _ u u i d
 *
 * Extract a type uuid from (the header part of) an
 * idl_pkl_t.
 */
void idl_pkl_get_type_uuid
#ifdef _DCE_PROTO_
  (
    idl_pkl_t pickle,
    uuid_t *type
  )
#else 
    (pickle, type) 
    idl_pkl_t pickle;
    uuid_t *type;
#endif
{
#if NDR_LOCAL_INT_REP == ndr_c_int_big_endian
    *type = *(uuid_t *)&pickle[24];
#else
    swap_uuid((pickle+24), ((idl_byte *)type));
#endif
}

/*
 * i d l _ p k l _ g e t _ h e a d e r
 *
 * Extract a header structure (in local byte order) from (the header
 * part of) an idl_pkl_t.
 */
void idl_pkl_get_header
#ifdef _DCE_PROTO_
  (
    idl_pkl_t pickle,
    unsigned8 *version,
    unsigned32 *length,
    rpc_syntax_id_t *syntax,
    uuid_t *type
  )
#else 
    (pickle, version, length, syntax, type) 
    idl_pkl_t pickle;
    unsigned8 *version;
    unsigned32 *length;
    rpc_syntax_id_t *syntax;
    uuid_t *type;
#endif
{
#if NDR_LOCAL_INT_REP == ndr_c_int_big_endian
    *version = *(unsigned8 *)&pickle[0];
    *length = (*(unsigned32 *)&pickle[0]) & 0x00ffffff;
    *syntax = *(rpc_syntax_id_t *)&pickle[4];
    *type = *(uuid_t *)&pickle[24];
#else
    *version = *(unsigned8 *)&pickle[0];
    swap_unsigned32((pickle+0), ((idl_byte *)length));
    *length &= 0x00ffffff;
    swap_uuid((pickle+4), ((idl_byte *)&(syntax->id)));
    swap_unsigned32((pickle+20), ((idl_byte *)&(syntax->version)));
    swap_uuid((pickle+4), ((idl_byte *)&(syntax->id)));
    swap_uuid((pickle+24), ((idl_byte *)type));
#endif
}

/*
 * i d l _ p k l _ s e t _ h e a d e r
 *
 * Inject a header structure (in local byte order) into (the header
 * part of) an idl_pkl_t.
 */
void idl_pkl_set_header
#ifdef _DCE_PROTO_
  (
    idl_pkl_t pickle,
    unsigned32 version,
    unsigned32 length,
    rpc_syntax_id_t *syntax,
    uuid_t *type
  )
#else 
    (pickle, version, length, syntax, type) 
    idl_pkl_t pickle;
    unsigned8 version;
    unsigned32 length;
    rpc_syntax_id_t *syntax;
    uuid_t *type;
#endif 

{
    version = version & 255;  /* mask to fit in one byte */

#if NDR_LOCAL_INT_REP == ndr_c_int_big_endian
    *(unsigned32 *)&pickle[0] = length;
    *(unsigned8 *)&pickle[0] = version;
    *(rpc_syntax_id_t *)&pickle[4] = *syntax;
    *(uuid_t *)&pickle[24] = *type;
#else
    swap_unsigned32(((idl_byte *)&length), (pickle+0));
    pickle[0] = version;
    swap_uuid(((idl_byte *)&syntax->id), (pickle+4));
    swap_unsigned32(((idl_byte *)&syntax->version), (pickle+20));
    swap_uuid(((idl_byte *)type), (pickle+24));
#endif
}

