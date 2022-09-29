/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: macros.h,v $
 * Revision 65.1  1997/10/20 19:46:11  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.36.2  1996/02/18  22:58:07  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:16:58  marty]
 *
 * Revision 1.1.36.1  1995/12/13  16:24:36  root
 * 	Submit
 * 	[1995/12/11  15:14:20  root]
 * 
 * Revision 1.1.33.4  1994/06/02  21:42:43  mdf
 * 	Merged with changes from 1.1.33.3
 * 	[1994/06/02  21:42:20  mdf]
 * 
 * 	hp_sec_to_osf_3 drop, merge up with latest.
 * 	[1994/05/24  15:14:33  mdf]
 * 
 * 	hp_sec_to_osf_3 drop
 * 
 * Revision 1.1.33.3  1994/06/02  20:21:27  mdf
 * 	hp_sec_to_osf_3 drop, merge up with latest.
 * 	[1994/05/24  15:14:33  mdf]
 * 
 * 	hp_sec_to_osf_3 drop
 * 
 * Revision 1.1.33.1  1994/05/11  19:04:47  ahop
 * 	hp_sec_to_osf_2 drop
 * 	include dce.h
 * 	[1994/04/29  20:47:59  ahop]
 * 
 * 	hp_sec_to_osf_2 drop
 * 	include dce.h
 * 
 * 	hp_sec_to_osf_2 drop
 * 	include dce.h
 * 
 * Revision 1.1.31.1  1993/10/05  22:30:26  mccann
 * 	CR8651 64 bit porting changes
 * 	[1993/10/04  19:10:55  mccann]
 * 
 * Revision 1.1.2.2  1992/12/29  13:06:50  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/28  20:14:27  zeliff]
 * 
 * Revision 1.1  1992/01/19  14:42:40  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
*/

/*  macros.h V=dce/4 09/04/91 //nbs/dds_tools/src
**
** Copyright (c) Hewlett-Packard Company 1991
** Unpublished work. All Rights Reserved.
**
*/
/* 
 * Useful macros
 */

#if !defined(_MACROS_H)
#define _MACROS_H

#include <dce/dce.h>

#ifndef NULL
#   define NULL 0L
#endif

typedef unsigned long lboolean;

#define FAILURE 0
#define SUCCESS !FAILURE

#define PRIVATE static
#define INTERNAL static
#define PUBLIC

#define STATUS_OK(stp)            ((stp)==NULL || (*stp) == error_status_ok)
#define GOOD_STATUS(stp)          ((stp)==NULL || (*stp) == error_status_ok)
#define BAD_STATUS(stp)           ((*stp) != error_status_ok)
#define SET_STATUS(stp, val)      ((*stp) = val)
#define CLEAR_STATUS(stp)         ((*stp) = error_status_ok)
#define STATUS_EQUAL(stp, st_val) ((*stp) == (st_val))
#define STATUS(stp)               (*stp) 
#define STATUS_V(st)              (st)
#define COPY_STATUS(stp1, stp2)   ((*stp2) = (*stp1))

#define FLAG_SET(v,f)       (((v) & (f)) == (f))
#define SET(v,f)            ((v) |= (f))
#define UNSET(v,f)          ((v) &= ~(f))

#define EQ_NIL(a)           { error_status_t lst; (uuid_is_nil(&(a), &lst)) }
#define BCOPY(a,b)          (bcopy(&(a), &(b), sizeof(a)))
#define EQ_STR(a, b, len)   (strncmp((a), (b), (len)) == 0)
#define EQ_OBJ(a, b, len)   (bcmp(&(a), &(b), (len)) == 0)

#define NEW(type)           (type *)malloc(sizeof(type))
#define DISPOSE(p)          free((char *) p)

#define DIFF(a, b)          ((a) > (b) ? (a-b) : (b-a))

#define NULL_STRING(s)      ((s) == NULL || *(s) == '\0')

#endif /* _MACROS_H */
