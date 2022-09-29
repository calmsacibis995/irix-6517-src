/*
 *****************************************************************************
 *
 *        Copyright 1992, Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use.
 * Unauthorized duplication, distribution or sale are strictly prohibited.
 *
 * Include file description:
 *
 *	Common display header text shared by na/parse.c and dfe/cli_adm.c
 *
 * Original Author: Jim Barnes
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/na/RCS/displaytext.h,v 1.2 1996/10/04 12:03:55 cwilson Exp $
 *
 * This file created by RCS from:
 *
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/na/RCS/displaytext.h,v $
 *
 * Revision History:
 *
 * $Log: displaytext.h,v $
 * Revision 1.2  1996/10/04 12:03:55  cwilson
 * latest rev
 *
 * Revision 1.9.102.1  1995/01/11  15:54:19  carlson
 * SPR 3342 -- Removed TMUX (not supported until R9.3).
 *
 * Revision 1.9  1994/08/04  11:09:04  sasson
 * SPR 3211: #ifdef was meant to be #if
 *
 * Revision 1.8  1994/05/23  13:56:50  russ
 * Bigbird na/adm merge.
 *
 * Revision 1.7  1993/06/30  18:02:41  wang
 * Removed DPTG support.
 *
 * Revision 1.6  93/04/15  16:19:59  wang
 * Fixed spr1068, show printer header problem.
 * 
 * Revision 1.5  93/03/30  11:04:58  raison
 * reserve parm ids for decserver and add them, turned off
 * 
 * Revision 1.4  93/02/26  20:31:12  wang
 * Changed ARAP header to AppleTalk.
 * 
 * Revision 1.3  93/01/22  17:50:01  wang
 * New parameters support for Rel8.0
 * 
 * Revision 1.2  92/10/15  14:23:49  wang
 * Added new NA parameters for ARAP support.
 * 
 * Revision 1.1  92/06/12  14:08:21  barnes
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 ****************************************************************************
 */

#ifndef _NA_DISP_TEXT

#define _NA_DISP_TEXT

static char *box_generic = "\n\t\t\tAnnex Generic Parameters\n\n";
static char *box_vcli = "\n\t\t\tVCLI Parameters\n\n";
static char *box_nameserver = "\n\t\t\tNameserver Parameters\n\n";
static char *box_security = "\n\t\t\tSecurity Parameters\n\n";
static char *box_time = "\n\t\t\tTime Parameters\n\n";
static char *box_syslog = "\n\t\t\tSysLog Parameters\n\n";
static char *box_lat = "\n\t\t\tLAT Parameters\n\n";
static char *box_arap = "\n\t\t\tAppleTalk Parameters\n\n";
static char *box_rip = "\n\t\t\tRouter Parameters\n\n";
static char *box_vms = "\n\t\t\tMOP and \"Login\" user Parameters\n\n";
#if NKERB > 0
static char *box_kerberos = "\n\t\t\tKerberos Security Parameters\n\n";
#endif
static char *box_ipx = "\n\t\t\tIPX Parameters\n\n";

static char *port_generic = "\n\t\t\tPort Generic Parameters\n\n";
static char *port_flow = "\n\t\t\tFlow Control and Signal Parameters\n\n";
static char *port_timers = "\n\t\t\tPort Timers and Counters\n\n";
static char *port_login = "\n\t\t\t\"Login\" User Parameters\n\n";
static char *port_security = "\n\t\t\tPort Security Parameters\n\n";
static char *port_edit = "\n\t\t\tCLI Line Editing Parameters\n\n";
static char *port_serialproto = "\n\t\t\tSerial Networking Protocol Parameters\n\n";
static char *port_slip = "\n\t\t\tSLIP Parameters\n\n";
static char *port_ppp = "\n\t\t\tPPP Parameters\n\n";
static char *port_arap = "\n\t\t\tPort AppleTalk Parameters\n\n";
static char *port_tn3270 = "\n\t\t\tPort TN3270 Parameters\n\n";
static char *port_lat = "\n\t\t\tPort LAT Parameters\n\n";

static char *sync_generic = "\n\t\t\tSynchronous Generic Parameters\n\n";
static char *sync_security = "\n\t\t\tPort Security Parameters\n\n";
static char *sync_netaddr = "\n\t\t\tSerial Networking Protocol Parameters\n\n";
static char *sync_ppp = "\n\t\t\tPPP Parameters\n\n";


static char *printer_generic = "\n\t\t\tPrinter Port Generic Parameters\n\n";
static char *interface_rip = "\n\t\t\tInterface Routing Parameters\n\n";
#endif
