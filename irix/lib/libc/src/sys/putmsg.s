/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
/*  putmsg.s 1.1 */

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

#if defined(_LIBC_ABI) || defined(_LIBC_NOMP)
SYSCALL(putmsg)
	RET(putmsg)
#else
#define SYS__putmsg SYS_putmsg
SYSCALL(_putmsg)
	RET(_putmsg)
#endif
