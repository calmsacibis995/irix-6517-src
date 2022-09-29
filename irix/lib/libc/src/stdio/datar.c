/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:stdio/iob.c	2.14"

#include "synonyms.h"
#include <stdio.h>
#include "stdiom.h"

/*
 * amount endbuf adjusted for ungetc's
 *
 * Note: this array must be initialized.  it is referenced but often not
 * written by stdio.  If it is in BSS/COMMON, it is possible to create a
 * non-shared demand-fill not-dirty page (wasted in system daemons).
 */
Uchar _bufendadj[_NFILE+1] = {0};

/*
 * Flags to indicate whether an ungetc has been done to the buffer, so
 * that a flush is required on short, relative fseeks.
 */
Uchar _bufdirtytab[_NFILE+1] = {0};

