/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: ndrold.h,v $
 * Revision 65.1  1997/10/20 19:16:19  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.4.2  1996/02/18  22:57:23  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:16:17  marty]
 *
 * Revision 1.1.4.1  1995/12/08  00:23:26  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/08  00:01:10  root]
 * 
 * Revision 1.1.2.3  1993/01/04  00:10:09  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:15:30  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  21:20:31  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:47:11  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:13:27  devrcs
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
**      ndrold.h  
**
**  FACILITY:
**
**      Network Data Representation (NDR)
**
**  ABSTRACT:
**
**  This is a "hand-compiled" version of "ndrold.idl".  See the Abstract
**  in "ndrold.idl" for details.
**
**
*/

#ifndef ndrold_v0_included
#define ndrold_v0_included

/* 
 * Data representation descriptor type for NCS pre-v2.
 */
 
typedef struct {
    unsigned int int_rep: 4;
    unsigned int char_rep: 4;
    unsigned int float_rep: 8;
    unsigned int reserved: 16;
} ndr_old_format_t;

#endif

