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
 *	%$(description)$%
 *
 * Original Author: %$(author)$%	Created on: %$(created-on)$%
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/port/RCS/config.h,v 1.2 1996/10/04 12:05:56 cwilson Exp $
 *
 * This file created by RCS from $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/port/RCS/config.h,v $
 *
 * Revision History:
 *
 * $Log: config.h,v $
 * Revision 1.2  1996/10/04 12:05:56  cwilson
 * latest rev
 *
 * Revision 1.1  1989/04/05  14:42:40  root
 * Changed copyright notice
 *
 * Revision 1.9  88/04/12  19:50:51  parker
 * Added EXOS_DIR
 * 
 * Revision 1.8  88/02/24  13:38:14  harris
 * Added software environment - MACH.
 * 
 * Revision 1.7  88/01/04  17:12:56  parker
 * Removed ^L
 * 
 * Revision 1.6  87/12/23  09:47:51  emond
 * Removed product name to make file more generic for OEM customers
 * 
 * Revision 1.5  87/12/04  12:41:36  harris
 * Added MASSCOMP to list of hardware environments.
 * 
 * Revision 1.4  87/11/24  09:55:46  harris
 * Corrected define of BSD4.2 - should have been BSD4_2.
 * 
 * Revision 1.3  87/11/19  15:22:11  harris
 * Changed INSTALL_DIR to /etc by default.
 * 
 * Revision 1.2  87/11/06  13:41:41  parker
 * Added define for INSTALL_DIR
 * 
 * Revision 1.1  87/07/29  13:48:54  parker
 * Initial revision
 * 
 * Revision 1.3  87/06/10  18:13:23  parker
 * Added support for Xenix/Excelan PC.
 * 
 * Revision 1.1  86/06/16  09:58:35  parker
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *  DATE:	$Date: 1996/10/04 12:05:56 $
 *  REVISION:	$Revision: 1.2 $
 *
 ****************************************************************************
 */
/*
 *	Define Hardware Environment.
 *		{ GENERIC, SUN, EXCELAN, MASSCOMP }
 *
 */
#define GENERIC

/*
 * 	Define Software Environment.
 *		{ BSD4_2, UMAX_V, XENIX, PYRAMID, MACH }
 *
 *      Defining Xenix creates a system which compiles large model
 *      and works around some bugs in the Xenix release of the 
 *      Excelan libraries.
 *
 */
#define BSD4_2

/*
 *	Define where the utilities (na, erpcd, etc.) will be installed
 */
#define INSTALL_DIR /etc

/*
 *	Define where the Excelan includes are located (if used)
 */
#define EXOS_DIR /usr/include/EXOS

/*
 *      It is assumed that all Xenix systems are system V.
 */
#ifdef XENIX
#define SYS_V
#define i8086
#endif
