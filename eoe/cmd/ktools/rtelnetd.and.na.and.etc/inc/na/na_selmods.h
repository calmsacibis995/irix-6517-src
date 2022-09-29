/*
 *****************************************************************************
 *
 *        Copyright 1991, Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use.
 * Unauthorized duplication, distribution or sale are strictly prohibited.
 *
 * Include file description:
 *	Interpretation of selectable modules parameter
 *
 * Original Author: Jim Barnes		Created on: 3 February 1992
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/na/RCS/na_selmods.h,v 1.2 1996/10/04 12:04:42 cwilson Exp $
 *
 * This file created by RCS from:
 *
 * Revision History:
 *
 * $Log: na_selmods.h,v $
 * Revision 1.2  1996/10/04 12:04:42  cwilson
 * latest rev
 *
 * Revision 1.16  1994/09/12  11:15:31  geiser
 * Make OPT_DEFAULT = OPT_DEC (not OPT_SELEC_NONE)
 * SPR 3379
 *
 * Revision 1.15  1994/08/03  13:49:23  geiser
 * Add OPT_DEC for DECserver disable-able module
 *
 * Revision 1.14  1994/05/23  14:14:44  russ
 * Fixed conflicting defines added in a previous merge.
 *
 * Revision 1.13  1994/05/13  09:48:36  thuan
 * IPX merge attempt
 *
 * Revision 1.12  1994/03/25  10:26:31  carlson
 * Added ftpd as a disabled module.
 *
 * Revision 1.11  1994/01/25  13:35:47  russ
 * added fingerd as a disabled_modules option. removed dptg.
 *
 * Revision 1.10.3.1  1994/01/25  14:01:08  jayesh
 * Added mask for IPX option
 *
 * Revision 1.10  1993/04/15  11:50:24  wang
 * Added in TN3270 and Dialout options define.
 *
 * Revision 1.9  93/01/19  12:56:19  grant
 * Added OPT_ATALK.
 * 
 * Revision 1.8  92/07/26  21:28:02  raison
 * allow verbose-help in disabled_modules list for non-AOCK,non-ELS images
 * 
 * Revision 1.7  92/05/12  16:52:20  raison
 * default disabled_modules excludes slip, ppp, and admin on ELS
 * 
 * Revision 1.6  92/04/23  18:21:50  raison
 * added lat to default disabled list
 * 
 * Revision 1.5  92/04/22  21:18:08  raison
 * default option for selectable modules
 * 
 * Revision 1.4  92/04/16  13:43:27  barnes
 * don't disable verbose-help
 * 
 * Revision 1.3  92/04/02  14:30:08  barnes
 * change OPT_ALL flag to exclude new OPT_SELEC_NONE bit
 * 
 * Revision 1.2  92/04/01  19:20:38  barnes
 * add bit mask definitions for cli-edit and none
 * 
 * Revision 1.1  92/02/07  09:29:29  barnes
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *  REVISION:	$Revision: 1.2 $
 *  DATE:	$Date: 1996/10/04 12:04:42 $
 *
 ****************************************************************************
 */

#ifndef NA_SELMODS_H
#define NA_SELMODS_H

#define OPT_ADMIN       0x0001          /* admin cli command */
#define OPT_VERBOSEHELP 0x0002          /* display verbose help */
#define OPT_LAT         0x0004          /* lat protocol and connect,
                                           services cli commands */
#define OPT_PPP         0x0008          /* ppp procotol and ppp
                                           cli command */
#define OPT_SLIP        0x0010          /* slip protocol and slip
                                           cli command */
#define OPT_SNMP        0x0020          /* snmp protocol and agent */
#define OPT_NAMESERVER  0x0040          /* nameserver */
#define OPT_FINGERD	0x0080		/* fingerd */
#define OPT_CLIEDIT	0x0100		/* cli_edit */
#define OPT_ATALK	0x0200		/* AppleTalk and ARAP	*/
#define OPT_TN3270	0x0400		/* Tn3270 */
#define OPT_DIALOUT	0x0800		/* Dial out and Active routing */
#define OPT_FTPD	0x1000		/* FTP Daemon */
#define OPT_IPX		0x2000		/* IPX Stuff */
#define OPT_DEC		0x4000		/* DECserver interface */
#define OPT_SELEC_NONE	0x8000		/* nothing disabled */
#define OPT_ALL		0x7fff		/* everything disabled */

/* a change for the default, do not forget dfe/dfe_defaults.c */

#define OPT_DEFAULT	OPT_DEC

#endif
