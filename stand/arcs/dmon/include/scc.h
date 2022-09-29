#ident	"$Id: scc.h,v 1.1 1994/07/20 22:55:08 davidl Exp $"
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


#ifdef IP24
#define	SCC_BASE	0x1fbd9830|0xa0000000	/* Duart base address */
#endif

#if	!defined(IP24) & defined(IP22)
#define	SCC_BASE	0x1fbd9830|0xa0000000	/* Duart base address */
#endif

#define SCC_PTRB	8		/**/
#define SCC_DATAB	12		/* port B data register*/
#define SCC_PTRA	0		/* status register port A */
#define SCC_DATAA	4		/* Data register port A */

#define	NOP_DELAY \
				nop; \
				nop; \
				nop; \
				nop; \
				nop; \
				nop; \
				nop; \
				nop; \
				nop; \
				nop
/*
 * to determine the time constant:
 *      TC = ( (MHZ {the clock rate} / (16 * 2 * BR)) - 2 )
 *              for a 16x clock
 */
#define	MHZ     10000000        /* base pclock for the scc part */

#define	BCONST  16              /* use this if you wish to change the divisor*/

#define	B_CONST(speed)  ( (MHZ/(2*BCONST*speed)) - 2 )
