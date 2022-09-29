/*
 *****************************************************************************
 *
 *        Copyright 1993, Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use.
 * Unauthorized duplication, distribution or sale are strictly prohibited.
 *
 * Include file description:
 *	This file contains global host-related compatibility
 *	definitions.
 *
 * Original Author: James Carlson	Created on: 29DEC93
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/port/RCS/port.h,v 1.2 1996/10/04 12:06:19 cwilson Exp $
 *
 * This file created by RCS from $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/port/RCS/port.h,v $
 *
 * Revision History:
 *
 * $Log: port.h,v $
 * Revision 1.2  1996/10/04 12:06:19  cwilson
 * latest rev
 *
 * Revision 1.1  1993/12/29  16:47:03  carlson
 * Initial revision
 *
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *  DATE:	$Date: 1996/10/04 12:06:19 $
 *  REVISION:	$Revision: 1.2 $
 *
 ****************************************************************************
 */

/*
 * On 64 bit machines, "int"s are 32 bits and "long"s are 64 bits.
 */
#ifdef USE_64
#	define UINT32	unsigned int
#	define INT32	int
#else /* !USE_64 */
#	define UINT32	unsigned long
#	define INT32	long
#endif /* USE_64 */
