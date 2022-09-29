/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident "$Revision: 3.12 $"

/*
 * Sysmips() system call commands.
 */

#define SETNAME		1	/* rename the system */
#define STIME		2	/* set time */
#define FLUSH_CACHE	3	/* flush caches */
#define	MIPS_FPSIGINTR	5	/* generate a SIGFPE on every fp interrupt */
#define	MIPS_FPU	6	/* turn on/off the fpu */
#define MIPS_FIXADE	7	/* fix address error (unaligned) */
#define	POSTWAIT	8	/* post wait driver for Oracle */
#define	PPHYSIO		9	/* parallel IO */
#define	PPHYSIO64	10	/* parallel IO - big offsets */
