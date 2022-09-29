/*
 *****************************************************************************
 *
 *        Copyright 1989, Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use.
 * Unauthorized duplication, distribution or sale are strictly prohibited.
 *
 * Include file description:
 *	Defines structures used for UNIX scatter/gather I/O.
 *
 * Original Author: Glenn Weinberg	Created on: March 28, 1985
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/port/RCS/xuio.h,v 1.2 1996/10/04 12:06:36 cwilson Exp $
 *
 * This file created by RCS from $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/port/RCS/xuio.h,v $
 *
 * Revision History:
 *
 * $Log: xuio.h,v $
 * Revision 1.2  1996/10/04 12:06:36  cwilson
 * latest rev
 *
 * Revision 1.4  1989/09/25  17:52:06  loverso
 * Don't include struct iovec if have_iovec defined
 *
 * Revision 1.3  89/04/11  01:05:52  loverso
 * allow multiple inclusions safely
 * 
 * Revision 1.2  89/04/05  14:47:18  root
 * Changed copyright notice
 * 
 * Revision 1.1  88/05/24  18:26:59  parker
 * Changes for new install-annex script
 * 
 * Revision 0.8  87/08/12  07:57:47  dwm
 * RCS Historical Restoration (for TomW).
 * 
 * Revision 1.2  87/06/17  12:01:25  dwm
 * Initial 3.1 Bug-Fix Integration & check-in.
 * 
 * Revision 1.1  87/05/21  08:17:58  dwm
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *  DATE:	$Date: 1996/10/04 12:06:36 $
 *  REVISION:	$Revision: 1.2 $
 *
 ****************************************************************************
 */

#ifndef XUIO_H
#define XUIO_H

#ifndef have_iovec
/*
 * Definition of one I/O vector.
 */
struct iovec {
	char 	*iov_base;		/* pointer to a buffer */
	int	iov_len;		/* length of data in buffer */
};
#endif
/*
 * UNIX scatter/gather I/O structure.
 */
struct uio {
	struct	iovec	*uio_iov;	/* pointer to array of iovec's */
	int	uio_iovcnt;		/* number of iovec's in array */
	int	uio_offset;		/* offset in file */
	char	uio_segflg;		/* USER_SPACE or SYSTEM_SPACE */
	char	uio_append;		/* TRUE if append mode write */
	char	uio_spare1;
	char	uio_spare2;
	int	uio_resid;		/* sum of lengths in array of iovecs */
};
#endif /*XUIO_H*/
