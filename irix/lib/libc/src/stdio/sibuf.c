/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:stdio/sibuf.c	2.14"
/*LINTLIBRARY*/
#include "synonyms.h"
#include <stdio.h>
#include "stdiom.h"

/*
 * File must be last to save wasted space in Xsetalign linker code!!!
 *
 * Ptrs to start of preallocated buffers for stdin, stdout.
 * Some slop is allowed at the end of the buffers in case an upset in
 * the synchronization of _cnt and _ptr (caused by an interrupt or other
 * signal) is not immediately detected.
 *
 * Note: _smbuf should come first since it is much more likely to be used,
 *	with _sobuf next and _sibuf last (least likely to be used)
 */
Uchar _sibuf[BUFSIZ + _SMBFSZ];
