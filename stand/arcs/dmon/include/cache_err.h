#ident	"$Id: cache_err.h,v 1.1 1994/07/20 22:54:38 davidl Exp $"
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/



/*------------------------------------------------------------------------+
| failure code.                                                           |
+------------------------------------------------------------------------*/
#define	DCACHE_ERR	1		/* stuck at one or zero fault    */
#define ICACHE_ERR	2		/* stuck at one or zero fault	 */
#define DTAG_ERR	3		/* data tag stuck at fault */
#define	ITAG_ERR	4		/* instr tag stuck at fault */
#define	DTAG_PAR_ERR	5		/* data tag parity error         */
#define	AINA_ERR	9		/* address error	*/

#define	WBACK_ERR	10		/* write back error.             */
#define	K0MOD_ERR	11		/* write to kseg1 changed cache  */
#define	K1MOD_ERR	12		/* write to cache changed memory */
#define	FILL_ERR	13		/* line fill error.              */

#define DCACHE_SIZE_ERR	20		/* data cache size err */
#define ICACHE_SIZE_ERR	21		/* instruction cache size err */


/*------------------------------------------------------------------------+
| macro to print number of passes.                                        |
+------------------------------------------------------------------------*/
#define	PrintPasses(pmsg, count)		\
	la	a0, pmsg;			\
	jal	puts;				\
	nop;					\
	and	a0, count, 0x7fff;		\
	jal	putdec;				\
	move	a1, zero

