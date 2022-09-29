/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.2 $"
/* LINTLIBRARY */

/*
 *  data.c
 *
 *	This file contains global data for the library "liboam".
 *
 *	FILE   *oam_devtab	The open file descriptor for the current
 *				device table.
 *	FILE   *oam_dgroup	The open file descriptor for the current
 *				device-group table.
 */


/*
 *  Header files needed:
 *	<stdio.h>	Standard I/O definitions
 */

#include	<stdio.h>



FILE	       *oam_devtab = (FILE *) NULL;
FILE	       *oam_dgroup = (FILE *) NULL;
