/*
 *****************************************************************************
 *
 *        Copyright 1989, Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use.
 * Unauthorized duplication, distribution or sale is strictly prohibited.
 *
 * Module Description::
 *
 * 	Compatibility include file for Excelan (EXOS-8011)
 *
 * Original Author: San Diego		Created on: May 26, 1986
 *
 * Module Reviewers:
 *
 *	harris, oneil, lint
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/port/RCS/EXOS.h,v 1.2 1996/10/04 12:05:23 cwilson Exp $
 *
 * This file created by RCS from:
 *
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/port/RCS/EXOS.h,v $:
 *
 * Revision History:
 *
 * $Log: EXOS.h,v $
 * Revision 1.2  1996/10/04 12:05:23  cwilson
 * latest rev
 *
 * Revision 1.10  1989/04/18  14:41:56  loverso
 * Use reverse logic on "need_experror"
 *
 * Revision 1.9  89/04/12  15:08:51  loverso
 * Check for experror
 * 
 * Revision 1.8  89/04/11  01:05:14  loverso
 * remove code handled by install-annex/libannex.h
 * 
 * Revision 1.7  89/04/05  14:42:37  root
 * Changed copyright notice
 * 
 * Revision 1.6  88/05/24  18:26:43  parker
 * Changes for new install-annex script
 * 
 * Revision 1.5  88/04/15  12:14:28  mattes
 * Improvements for Xenix 2.2
 * 
 * Revision 1.4  87/06/10  18:13:21  parker
 * Added support for Xenix/Excelan PC.
 * 
 * Revision 1.3  86/11/06  11:06:28  harris
 * Added RCS header.  Added defines which select compatibility routines.
 * These are various inet_*() calls, recv{from,msg}, send{msg,to}, & bcopy.
 * 
 *
 * This file is currently under revision by: * $Locker:  $
 *
 *****************************************************************************
 */

/*
 * use EXOS perror with network errnos, if it exists
 *
 * REVERSE logic:
 *    If "need_experror" set, we DON'T have experror, so do nothing.
 *    If unset, then experror was found, so substitute it for perror.
 */
#ifndef need_experror
#define perror experror
#endif
