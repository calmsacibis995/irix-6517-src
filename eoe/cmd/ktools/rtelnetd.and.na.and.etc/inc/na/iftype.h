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
 *	Interpretation of load/dump sequence bytes
 *	(condensed from inc/rom/hw_per_annex.h)
 *
 * Original Author: Paul Mattes		Created on: 29. December 1987
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/na/RCS/iftype.h,v 1.2 1996/10/04 12:04:18 cwilson Exp $
 *
 * This file created by RCS from:
 *
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/na/RCS/iftype.h,v $
 *
 * Revision History:
 *
 * $Log: iftype.h,v $
 * Revision 1.2  1996/10/04 12:04:18  cwilson
 * latest rev
 *
 * Revision 1.4  1991/12/20  15:43:01  raison
 * added flash as a new type of load_dump_seq
 *
 * Revision 1.3  91/02/15  09:01:32  raison
 * changed IFTYPE_MASK to allow 6 bits to distinguish ports (now allows 64 ports).
 * 
 * Revision 1.2  89/04/05  12:42:03  loverso
 * Changed copyright notice
 * 
 * Revision 1.1  87/12/29  11:45:11  mattes
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *  DATE:	$Date: 1996/10/04 12:04:18 $
 *  REVISION:	$Revision: 1.2 $
 *
 ****************************************************************************
 */

#define PDL_IS_SIZ	4	/* How many bytes in the sequence */
#define IFTYPE_MASK	0xc0	/* Mask for I/F type part of seq bytes */
#define IFTYPE_SLIP	0x80	/* SL/IP-type interface */
#define IFTYPE_FLASH	0x40	/* FLASH-type interface */
#define IFBYTE_NONE	0xff	/* No interface at all (filler) */
