/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: ctype.h,v $
 * Revision 65.2  1998/04/09 20:37:33  lmc
 * Fixed the prototypes for isdigit and toupper.  They aren't recognized
 * as valid prototypes if they are incomplete.
 *
 * Revision 65.1  1997/10/20  19:16:25  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.58.2  1996/02/18  23:46:37  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:45:18  marty]
 *
 * Revision 1.1.58.1  1995/12/08  00:15:02  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:56:16  root]
 * 
 * Revision 1.1.56.1  1994/01/21  22:31:59  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  20:57:21  cbrooks]
 * 
 * Revision 1.1.4.3  1993/01/03  22:36:14  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  19:52:24  bbelch]
 * 
 * Revision 1.1.4.2  1992/12/23  19:38:53  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  14:53:07  zeliff]
 * 
 * Revision 1.1.2.2  1992/01/22  23:04:18  melman
 * 	Adding changes from defect_fixes archive.
 * 	[1992/01/22  22:20:51  melman]
 * 
 * $EndLog$
 */
/*
 * ctype.h
 *
 * Just something to resolve the "include" reference(s).
 */

#ifndef _CTYPE_H 
#define _CTYPE_H 1 

extern int isdigit(int);
extern int toupper(int);

#endif /* _CTYPE_H */
