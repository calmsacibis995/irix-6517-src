
/* .ident	"@(#)libc-m32:m4.def	1.4" */
#ident	"$Header: /proj/irix6.5.7m/isms/irix/lib/librestart/src/RCS/mprotect.s,v 1.1 1988/12/05 10:11:14 eva Exp $"
/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
/*  mprotect.s 1.1 */

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

SYSCALL(mprotect)
	RET(mprotect)
