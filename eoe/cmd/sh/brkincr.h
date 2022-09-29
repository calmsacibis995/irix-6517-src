/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)sh:brkincr.h	1.5"
/*	3.0 SID #	1.1	*/

/* at least a 32 bit system page, no more than a 64 bit system page,
 * for the minimum break. */
#define BRKINCR 0x1000
#define BRKMAX 0x4000
