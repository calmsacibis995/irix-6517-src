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
 *
 *	Common literal names for Encore Annex version of NA
 *
 * Original Author: %$(author)$%	Created on: %$(created-on)$%
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/na/RCS/names.h,v 1.2 1996/10/04 12:04:50 cwilson Exp $
 *
 * This file created by RCS from:
 *
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/na/RCS/names.h,v $
 *
 * Revision History:
 *
 * $Log: names.h,v $
 * Revision 1.2  1996/10/04 12:04:50  cwilson
 * latest rev
 *
 * Revision 1.4  1993/02/22  17:56:19  wang
 * Added reset interface command for NA.
 *
 * Revision 1.3  92/02/03  09:37:41  carlson
 * SPR 519 -- added matching name length definition.
 * 
 * Revision 1.2  92/01/31  22:53:48  raison
 * added printer_set and printer_speed parm
 * 
 * Revision 1.1  91/12/12  14:02:41  russ
 * Initial revision
 * 
 * Revision 1.2  89/04/05  12:47:01  loverso
 * New copyright message
 * 
 * Revision 1.1  87/09/28  11:50:48  mattes
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *  DATE:	$  $
 *  REVISION:	$  $
 *
 ****************************************************************************
 */

#define BOX		"annex"
#define Box		"Annex"
#define BOXES		"annexes"
#define BOX_ES		"annex(es)"
#define BOX_PARAMETER	"annex parameter"
#define BOX_PARM_NAME	"annex parameter name"
#define NO_BOX		"missing annex identifier"
#define NO_BOXES	"default annexes have not been specified"
#define BAD_BOX		"invalid annex identifier: "
#define WHAT_BOX	"unknown annex: "
#define NONYMOUS_BOX	"zero annex identifier not valid here"
#define LONG_BOX	"maximum annex name length is 40 characters"
#define BOX_LMAX	40	/* Should match above message */
#define PRINTER		"printer"
#define INTERFACES	"interface"
