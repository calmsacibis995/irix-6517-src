/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/* 
 * (c) Copyright 1991, 1992 
 *     Siemens-Nixdorf Information Systems, Burlington, MA, USA
 *     All Rights Reserved
 */
/*
 * HISTORY
 * $Log: propagat.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.3.5  1993/02/01  20:27:28  hinman
 * 	[hinman@sni] - Final merge before bsubmit
 * 	[1993/01/31  15:59:26  hinman]
 *
 * Revision 1.1.3.4  1993/01/03  21:41:30  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:38:16  bbelch]
 * 	Revision 1.1.4.2  1993/01/11  16:10:50  hinman
 * 	[hinman] - Check in merged SNI version
 * 
 * Revision 1.1.3.3  1992/12/23  18:51:17  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:05:38  zeliff]
 * 
 * Revision 1.1.3.2  1992/09/29  20:41:19  devsrc
 * 	SNI/SVR4 merge.  OT 5373
 * 	[1992/09/11  15:35:16  weisman]
 * 
 * Revision 9.5.3.3  93/01/05  10:16:51  greg
 * 	Fold in new OSF copyright.
 * 
 * Revision 9.5.3.2  92/12/10  16:19:46  greg
 * 	Put #ifndef mips_no_decl in a form acceptable to OSF.
 * 
 * Revision 9.5.1.3  92/10/01  11:12:13  greg
 * 	Add SNI copyright info.  This file is currently not auto-updateable
 * 	due to the unresolved "mips" preprocessor symbol issue, but should
 * 	become so once that issue is resolved.
 * 
 * Revision 9.5.2.2  92/09/30  16:14:57  greg
 * 	add SNI copyright info
 * 
 * Revision 9.5.1.2  92/08/14  09:23:17  hinman
 * 	[hinman for greg] - Fold backward from DCE1TOP
 * 
 * Revision 8.1.1.3  92/08/06  20:30:45  greg
 * 	fold in OSF_101_UPDATE mods
 * 
 * Revision 8.1.3.2  92/08/06  09:09:09  greg
 * 	fold in OSF_101_UPDATE mods
 * 
 * Revision 9.5  92/07/24  15:12:36  hinman
 * 	Update checkin
 * 
 * Revision 1.1  1992/01/19  03:03:11  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      propagat.h
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**  Declaration of functions exported by propagat.c.
**
**  VERSION: DCE 1.0
**
*/

#ifndef PROPAGATH_INCL
#define PROPAGATH_INCL

#include <ast.h>                /* Abstract Syntax Tree defs */

/*
**  P R O P _ m a i n
**
**  Main routine for propagation component of IDL compiler.
**  Propagates attributes and other information throughout the AST.
**  Much of propagation is done by earlier compiler phases - this
**  routine handles any propagation that is easier to do if saved
**  until after the parsing/AST-building phases are complete.
*/

extern boolean PROP_main(       /* Returns true on success */
#ifdef PROTO
    boolean     *cmd_opt_arr,   /* [in] Array of command option flags */
    void        **cmd_val_arr,  /* [in] Array of command option values */
    AST_interface_n_t *int_p    /* [in] Ptr to AST interface node */
#endif
);

#if !defined(mips) || (defined(SNI_SVR4) && (!defined(mips_no_decl)))
void PROP_set_type_attr
(
#ifdef PROTO
    AST_type_n_t *type_node_ptr,
    AST_flags_t  type_attr
#endif
);
#endif


#endif
