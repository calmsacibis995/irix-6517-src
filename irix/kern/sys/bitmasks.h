/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)kern-port:sys/bitmasks.h	10.1"*/
#ident	"$Revision: 3.4 $"

/*	setmask[i] has the low order i bits set.  For example,
 *	setmask[5] == 0x1F.
 */

extern int setmask[33];

/*	sbittab[i] has bit number i set.  For example,
 *	sbittab[5] == 0x20.
 */

extern int sbittab[];

/*	cbittab[i] has all bits on except bit i which is off.  For example,
 *	cbittab[5] == 0xFFFFFFDF.
 */

extern int cbittab[];
