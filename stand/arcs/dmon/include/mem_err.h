#ident	"$Id: mem_err.h,v 1.1 1994/07/20 22:54:58 davidl Exp $"
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
#define	MEM_ERR		1		/* stuck at one or zero fault    */
#define	MEM_AINA_ERR	9		/* address error	*/

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

