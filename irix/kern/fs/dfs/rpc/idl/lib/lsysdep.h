/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: lsysdep.h,v $
 * Revision 65.1  1997/10/20 19:15:36  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.10.2  1996/02/18  23:45:46  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:44:35  marty]
 *
 * Revision 1.1.10.1  1995/12/07  22:28:52  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  21:15:28  root]
 * 
 * Revision 1.1.6.1  1994/01/21  22:30:38  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  21:51:22  cbrooks]
 * 
 * Revision 1.1.4.2  1993/07/07  20:08:15  ganni
 * 	reduced stub idl sources
 * 	[1993/07/07  19:37:49  ganni]
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1989 by
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
**      lsysdep.h
**
**  FACILITY:
**
**      IDL Stub Runtime Support
**
**  ABSTRACT:
**
**      Header file for isolating platform dependencies.
**
**  VERSION: DCE 1.0
**
*/

#ifndef LSYSDEP_H
#define LSYSDEP_H

#ifndef _KERNEL
#if defined apollo
#   include <lapollo.h>
#else
#   include <stdlib.h>
#endif
#endif

#endif
