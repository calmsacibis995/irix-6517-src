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

#ident	"$Revision: 1.23 $"

#include <asm.h>
#include <sys/regdef.h>
#include <sys/SN/SN0/ip27config.h>

	.text
	.set	noreorder
	.set	at

/*
 * The power-on vector table starts here.  It is important to ensure
 * that the text segment of this file gets physically loaded into
 * 0xffffffffbfc00000 (same as 0x900000001fc00000) so that the vectors
 * appear in the memory locations expected by the R10000.  It's also
 * important that this file is linked at an address containing the
 * 0x1fc00000 bits so that the segment for absolute jumps is correct.
 */

#define	HANDLER(_handler)		j	_handler; nop
#define	INVALID()			jal	notimplemented; nop

LEAF(start)
	/*
	 * Offset 0x00 through 0x5f: PROM entry points
	 */

	HANDLER(entry);			HANDLER(bevRestartMaster);
	HANDLER(bevSlaveLoop);		HANDLER(bevPodMode)
	HANDLER(bevRestartMasterEPC);	HANDLER(bevFlashLeds)
	HANDLER(bevRePod);		HANDLER(bevLaunchSlave);
	HANDLER(bevWaitSlave);		HANDLER(bevPollSlave);
	HANDLER(bevPrintf);		INVALID();

	/*
	 * Offset 0x60 through 0xff:  R10000 mode bits for IP27, and
	 * other PROM configuration data.  For some reason the XLEAF
	 * is required or the .word's will move to the data section.
	 */

XLEAF(ip27config)	/* KEEP IN SYNC WITH ip27config.h */

	.word		CONFIG_TIME_CONST	/* Must be offset 0x60 */
	.word		CONFIG_CPU_MODE

	.dword		CONFIG_MAGIC

	.dword		CONFIG_FREQ_CPU
	.dword		CONFIG_FREQ_HUB
	.dword		CONFIG_FREQ_RTC

	.word		CONFIG_ECC_ENABLE
	.word		CONFIG_FPROM_CYC

	.word		CONFIG_MACH_TYPE
	.word		CONFIG_CHECK_SUM_ADJ 

	.word		CONFIG_DEFAULT_FLASH_COUNT 
	.word		CONFIG_FPROM_WR

	.word		PVERS_VERS
	.word		PVERS_REV

	.word		0:22			/* Alignment pad space */

	/*
	 * Offset 0x100 through 0x3ff:  Remainder of exception vectors.
	 */

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
	HANDLER(bevTlbRefill);		INVALID(); /* 0x200 */
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
