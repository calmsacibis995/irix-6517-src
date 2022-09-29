/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: osi_net_mach.h,v $
 * Revision 65.2  1998/01/07 15:48:56  lmc
 * Removed the #include for netdb.h for SGIMIPS.  Not needed.
 *
 * Revision 65.1  1997/10/24  14:29:51  jdoak
 * *** empty log message ***
 *
 * Revision 1.1  1997/10/21  15:59:40  jdoak
 * Initial revision
 *
 * Revision 64.1  1997/02/14  19:46:51  dce
 * *** empty log message ***
 *
 * Revision 1.2  1996/04/06  00:17:44  bhim
 * No Message Supplied
 *
 * Revision 1.1.707.2  1994/06/09  14:15:10  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:27:42  annie]
 *
 * Revision 1.1.707.1  1994/02/04  20:24:10  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:15:29  devsrc]
 * 
 * Revision 1.1.705.1  1993/12/07  17:29:35  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  15:58:58  jaffe]
 * 
 * Revision 1.1.2.4  1993/01/21  14:50:08  cjd
 * 	embedded copyright notice
 * 	[1993/01/20  14:51:59  cjd]
 * 
 * Revision 1.1.2.3  1992/09/25  18:32:10  jaffe
 * 	Remove duplicate HEADER and LOG entries
 * 	[1992/09/25  12:27:40  jaffe]
 * 
 * Revision 1.1.2.2  1992/08/31  20:23:40  jaffe
 * 	Transarc delta: jaffe-ot4719-cleanup-gcc-Wall-in-osi 1.3
 * 	  Selected comments:
 * 	    Fixed many compiler warnings in the osi directory.
 * 	    Reworked ALL of the header files.  All files in the osi directory now
 * 	    have NO machine specific ifdefs.  All machine specific code is in the
 * 	    machine specific subdirectories.  To make this work, additional flags
 * 	    were added to the afs/param.h file so that we can tell if a particular
 * 	    platform has any additional changes for a given osi header file.
 * 	    new file for machine specific additions for the osi_net.h file.
 * 	    Corrected errors that appeared while trying to build everything on AIX3.2
 * 	    cleanup for OSF1 compilation
 * 	[1992/08/30  03:12:34  jaffe]
 * 
 * Revision 1.1.1.2  1992/08/30  03:12:34  jaffe
 * 	Transarc delta: jaffe-ot4719-cleanup-gcc-Wall-in-osi 1.3
 * 	  Selected comments:
 * 	    Fixed many compiler warnings in the osi directory.
 * 	    Reworked ALL of the header files.  All files in the osi directory now
 * 	    have NO machine specific ifdefs.  All machine specific code is in the
 * 	    machine specific subdirectories.  To make this work, additional flags
 * 	    were added to the afs/param.h file so that we can tell if a particular
 * 	    platform has any additional changes for a given osi header file.
 * 	    new file for machine specific additions for the osi_net.h file.
 * 	    Corrected errors that appeared while trying to build everything on AIX3.2
 * 	    cleanup for OSF1 compilation
 * 
 * $EndLog$
 */
/*
 *      Copyright (C) 1992 Transarc Corporation
 *      All rights reserved.
 */

#ifndef TRANSARC_OSI_NET_MACH_H
#define	TRANSARC_OSI_NET_MACH_H

#include <sys/socket.h>
#include <netinet/in.h>
#ifndef SGIMIPS
#include <netdb.h>
#endif
#endif /* TRANSARC_OSI_NET_MACH_H */
