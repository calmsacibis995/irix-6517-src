/************************************************************************** 
 *									  *
 * 		 Copyright (C) 1993 Silicon Graphics, Inc.	          *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.3 $"
	
#include <asm.h>
#include <sys/regdef.h>
#include <sys/sbd.h>

		.text
		.set 	noreorder
		.set	at

/*
 * The power-on vector table starts here.  It is important to ensure
 * that this file is loaded at 0x900000001fc00000 so that the vectors
 * appear in the memory locations expected by the R10000.
 */

#define	HANDLER(_handler)	j	_handler; nop
#define	INVALID()		HANDLER(notimplemented)

LEAF(start)
	HANDLER(entry);			HANDLER(bevRestartMaster);
	HANDLER(bevRestartSlave);	HANDLER(bevPodMode)
	HANDLER(bevRestartMasterEPC);	HANDLER(bevFlashLeds)
	HANDLER(bevRePod);		HANDLER(bevIP25monExit);
	INVALID();			INVALID(); /* 0x40 */
        INVALID();			INVALID();	
	INVALID();			INVALID();
        INVALID();			INVALID();	
	INVALID();			INVALID(); /* 0x80 */
        INVALID();			INVALID();	
	INVALID();			INVALID();
        INVALID();			INVALID();	
	INVALID();			INVALID(); /* 0xc0 */
        INVALID();			INVALID();	
	INVALID();			INVALID();
        INVALID();			INVALID();		
	HANDLER(bevECC);		INVALID(); /* 0x100 */
        INVALID();			INVALID();		
	INVALID();			INVALID();
        INVALID();			INVALID();		
	INVALID();			INVALID(); /* 0x140 */
        INVALID();			INVALID();	
	INVALID();			INVALID();
        INVALID();			INVALID();	
	INVALID();			INVALID(); /* 0x180 */
        INVALID();			INVALID();	
	INVALID();			INVALID();
        INVALID();			INVALID();	
	INVALID();			INVALID(); /* 0x1c0 */
        INVALID();			INVALID();	
	INVALID();			INVALID();
        INVALID();			INVALID();	
	INVALID();			INVALID(); /* 0x200 */
        INVALID();			INVALID();	
	INVALID();			INVALID();
        INVALID();			INVALID();		
	INVALID();			INVALID(); /* 0x240 */
        INVALID();			INVALID();	
	INVALID();			INVALID();
        INVALID();			INVALID();	
	HANDLER(bevXtlbRefill);		INVALID(); /* 0x280 */
        INVALID();			INVALID();	
	INVALID();			INVALID();
        INVALID();			INVALID();	
	INVALID();			INVALID(); /* 0x2c0 */
        INVALID();			INVALID();	
	INVALID();			INVALID();
        INVALID();			INVALID();	
	HANDLER(bevCache);		INVALID(); /* 0x300 */
        INVALID();			INVALID();	
	INVALID();			INVALID();
        INVALID();			INVALID();		
	INVALID();			INVALID(); /* 0x340 */
        INVALID();			INVALID();	
	INVALID();			INVALID();
	INVALID();			INVALID();
        HANDLER(bevGeneral);		INVALID(); /* 0x380 */
	INVALID();			INVALID(); 
        INVALID();			INVALID();	
        INVALID();			INVALID();	
	INVALID();			INVALID(); /* 0x3c0 */
        INVALID();			INVALID();	
	INVALID();			INVALID();
	INVALID();			INVALID();
						   /* 0x400 */
	END(start)
