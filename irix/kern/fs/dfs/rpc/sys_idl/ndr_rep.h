/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/* 
 * HISTORY 
 * $Log: ndr_rep.h,v $
 * Revision 65.1  1997/10/24 14:30:01  jdoak
 * *** empty log message ***
 *
 * Revision 1.1  1997/10/21  15:59:31  jdoak
 * Initial revision
 *
 * Revision 64.1  1997/02/14  19:45:21  dce
 * *** empty log message ***
 *
 * Revision 1.2  1995/04/24  19:46:23  dcebuild
 * File added by SGI
 *
 * Revision 1.1.128.2  1994/06/10  20:54:53  devsrc
 * 	cr10871 - fix copyright
 * 	[1994/06/10  15:00:19  devsrc]
 *
 * Revision 1.1.128.1  1994/01/21  22:39:46  cbrooks
 * 	platform dependent NDR local representation
 * 	[1994/01/21  17:13:40  cbrooks]
 * 
 * $EndLog$
 */
/*
 * ndr_rep.h 
 * platform dependent (OS + Architecture) file split out from stubbase.h 
 * for DCE 1.1 code cleanup.
 * This file contains the architecture specific definitions of the 
 * local scaler data representation used 
 *
 * This file is always included as part of stubbase.h 
 */

#ifndef _NDR_REP_H 
#define _NDR_REP_H

#define NDR_LOCAL_INT_REP     ndr_c_int_big_endian
#define NDR_LOCAL_FLOAT_REP   ndr_c_float_ieee
#define NDR_LOCAL_CHAR_REP    ndr_c_char_ascii

#endif /* _NDR_REP_H */
