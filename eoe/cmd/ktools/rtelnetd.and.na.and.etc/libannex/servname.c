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
 * Module Description::
 *
 *	BSD getservbyname routines for hosts that are lacking
 *
 * Original Author:  John R LoVerso		Created on: 03/17/89
 *
 * Module Reviewers:
 *	%$(reviewers)$%
 *
 * Revision Control Information:
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/servname.c,v 1.3 1996/10/04 12:08:58 cwilson Exp $
 *
 * This file created by RCS from
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/servname.c,v $
 *
 * Revision History:
 * $Log: servname.c,v $
 * Revision 1.3  1996/10/04 12:08:58  cwilson
 * latest rev
 *
 * Revision 1.7  1993/06/21  19:53:41  reeve
 * Added include for netdb.h.
 *
 * Revision 1.6  1991/06/21  09:49:35  barnes
 * changing _xxxx function names to xylo_xxxx
 *
 * Revision 1.5  91/04/24  19:41:56  emond
 * Don't try to include socket.h when using TLI.
 * 
 * Revision 1.4  89/10/16  17:29:59  loverso
 * Add "printer" service to give greater functionallity to broken SysV hosts
 * 
 * Revision 1.3  89/05/23  11:58:17  loverso
 * Add timserver
 * 
 * Revision 1.2  89/04/27  13:43:09  loverso
 * Add alias "name" for "nameserver" (IEN-116)
 * 
 * Revision 1.1  89/04/10  23:37:35  loverso
 * Initial revision
 * 
 * This file is currently under revision by: $Locker:  $
 *
 *****************************************************************************
 */

/*
 *	Include Files
 */

#include "../inc/config.h"
#include "netdb.h"

#ifndef TLI
#include <sys/socket.h>
#endif
#include <netinet/in.h>

#include <ctype.h>
#include <stdio.h>

/*
 *	External Definitions
 */


/*
 *	Defines and Macros
 */


/*
 *	Structure and Typedef Definitions
 */


/*
 *	Forward Function Definitions
 */


/*
 *	Global Data Declarations
 */


/*
 *	Static Data Declarations
 */

static struct servent servtable[] = {
	{ "time",	0,  37,	"udp"}, /* simple time service */
	{ "timserver",	0,  37,	"udp"}, /* simple time service alias */
	{ "nameserver",	0,  42,	"udp"}, /* IEN116 nameserver */
	{ "name",	0,  42,	"udp"}, /* IEN116 nameserver alias */
	{ "erpc",	0, 121,	"udp"}, /* Annex Expedited RPC listener */
	{ "printer",	0, 515,	"tcp"}, /* BSD/Annex print service */
	0,
};

/*
 * replacement version of the BSD getservbyname() C library function
 * for systems lacking it
 */
struct servent *
xylo_getservbyname(name,proto)
	char *name, *proto;
{
	register struct servent *p = servtable;
	static struct servent ret;

	for (; p->s_name; p++)
		if (strcmp(name, p->s_name) == 0 &&
		    (proto == 0 || strcmp(p->s_proto, proto) == 0))
			break;
	if (p->s_name) {
		/* don't rely on structure copy */
		ret.s_name = p->s_name;
		ret.s_aliases = p->s_aliases;
		ret.s_port = htons(p->s_port);
		ret.s_proto = p->s_proto;
		return &ret;
	} else
		return (struct servent *)0;
}
