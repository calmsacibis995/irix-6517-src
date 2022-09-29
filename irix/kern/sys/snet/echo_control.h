#ident "@(#)echo_control.h	1.3	11/13/92"

/******************************************************************
 *
 *  SpiderTCP Interface Primitives
 *
 *  Copyright (c) 1991  Spider Systems Limited
 *
 *  This Source Code is furnished under Licence, and may not be
 *  copied or distributed without express written agreement.
 *
 *  All rights reserved.
 *
 *  Written by 		Nick Felisiak, Ian Heavens, Peter Reid, 
 *			Gavin Shearer, Mark Valentine 
 *
 *  ECHO_CONTROL.H
 *
 *  ECHO Streams ioctl primitives for TCP/IP on V.3 Streams               
 *
 ******************************************************************/

/*
 *	 /projects/common/PBRAIN/SCCS/pbrainF/dev/src/include/sys/snet/0/s.echo_control.h
 *	@(#)echo_control.h	1.3
 *
 *	Last delta created	16:49:23 1/14/91
 *	This file extracted	14:53:19 11/13/92
 *
 *	Modifications:
 *
 *	PW	13 Feb 1989	Added TY_ECHO, htons and ntohs.
 *
 *
 */

/*
 * primitive for ECHO driver registration
 */
#define ECHO_TYPE	'E'
#define TY_ECHO		0x9000
#define	htons(x)	((((x) >> 8) & 0x00FF) | (((x) << 8) & 0xFF00))
#define	ntohs(x)	htons(x)
