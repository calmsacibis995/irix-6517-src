/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: ndrcharp.c,v 65.4 1998/03/23 17:25:04 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif



/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 * @DEC_COPYRIGHT@
 */
/*
 * HISTORY
 * $Log: ndrcharp.c,v $
 * Revision 65.4  1998/03/23 17:25:04  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.3  1998/01/07 17:19:55  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:56:04  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:15:37  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.8.2  1996/02/18  18:54:38  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  18:07:02  marty]
 *
 * Revision 1.1.8.1  1995/12/07  22:29:11  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/dat_xidl2/1  1995/11/17  17:07 UTC  dat
 * 	Merge second XIDL drop for DCE 1.2.1
 * 	[1995/12/07  21:15:34  root]
 * 
 * Revision 1.1.2.1  1995/11/01  14:22:19  bfc
 * 	idl cleanup
 * 	[1995/11/01  14:21:14  bfc]
 * 
 * 	DCE for DEC OSF/1: populate from OSF DCE R1.1
 * 	[1995/11/01  14:12:34  bfc]
 * 
 * Revision 1.1.4.2  1993/07/07  20:08:29  ganni
 * 	reduced stub idl sources
 * 	[1993/07/07  19:37:59  ganni]
 * 
 * $EndLog$
 */
/*
**
**  NAME:
**
**      ndrcharp.c
**
**  FACILITY:
**
**      IDL Stub Runtime Support
**
**  ABSTRACT:
**
**  This module contains the declarations of two character table pointers
**  used by the stub support library for translating to/from ascii/ebcdic.
**
**  VERSION: DCE 1.0
**
*/

#define NDRCHARP_C

/* The ordering of the following 3 includes should NOT be changed! */
#include <dce/rpc.h>
#include <dce/stubbase.h>
#include <lsysdep.h>

/*
 * Pointer cells for the two tables.  Note that for a VMS shareable image
 * version of the runtime, these two cells must be moved to a separate
 * module at the head of the image.
 */
#if (defined(VAX) || defined(__vax)) && (defined(VMS) || defined(__VMS))
#if defined(VAXC) && !defined(__DECC)
globaldef {"NDR_G_ASCII_TO_EBCDIC"} rpc_trans_tab_p_t ndr_g_ascii_to_ebcdic;
globaldef {"NDR_G_EBCDIC_TO_ASCII"} rpc_trans_tab_p_t ndr_g_ebcdic_to_ascii;
#else
#pragma extern_model save
#pragma extern_model strict_refdef "NDR_G_ASCII_TO_EBCDIC"
rpc_trans_tab_p_t ndr_g_ascii_to_ebcdic;
#pragma extern_model strict_refdef "NDR_G_EBCDIC_TO_ASCII"
rpc_trans_tab_p_t ndr_g_ebcdic_to_ascii;
#pragma extern_model restore
#endif
#else
globaldef rpc_trans_tab_p_t ndr_g_ascii_to_ebcdic;
globaldef rpc_trans_tab_p_t ndr_g_ebcdic_to_ascii;
#endif

#ifndef _KERNEL
/* 
** For alpha/VMS we need to export the globals to match the externs in
** stubbase.h because the default common model is ANSI-style
*/
#if defined(VMS) && defined(__alpha)
ndr_boolean rpc_ss_allocate_is_set_up = ndr_false;
ndr_boolean rpc_ss_context_is_set_up = ndr_false;
ndr_boolean rpc_ss_client_is_set_up = ndr_false;
ndr_boolean rpc_ss_server_is_set_up = ndr_false;
#endif

#endif
