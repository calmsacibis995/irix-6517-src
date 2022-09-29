#ifndef DIAGCMDS
#define DIAGCMDS
/* $Header: /proj/irix6.5.7m/isms/stand/arcs/ide/fforward/graphics/EXPRESS/RCS/diagcmds.h,v 1.19 1993/09/11 00:44:37 shannon Exp $ */
/**************************************************************************
 *								 	  *
 * 		 Copyright (C) 1990, Silicon Graphics, Inc.		  *
 *								 	  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *								 	  *
 **************************************************************************/

/*********************************************************************
 *	This is the offical DIAGNOSTICS GE7 token definition list.
 *
 *********************************************************************/
#define	DIAG_USEV		(0x200)
#define	DIAG_LOADV		(0x400)
#define	DIAG_ITOF		(0x4000)
#define	DIAG_CONVV3		(0x800)
#define	DIAG_CONVV2		(0x1000)
#define	DIAG_CONVC3		(0x1800)
#define	DIAG_CONVC4		(0x2000)
#define	DIAG_CONVCP		(0x2800)
#define	DIAG_CONVC1		(0x3000)
#define	DIAG_DATA		(0)
#define	DIAG_DATAF		(0 | DIAG_ITOF)

/* COMMAND TOKENS */

#define	DIAG_FINISH2	(1)	/* sets finish flag 2 */
#define	DIAG_GERAM12	(2)	/* tests GE7 internal RAM */
#define	DIAG_GESHRAM	(3)	/* tests GE7 to shram datapath */
#define	DIAG_SEEDROM	(4)	/* tests GE7 seed ROM */
#define	DIAG_SQROM	(5)	/* teste GE7 Square root ROM */
#define	DIAG_REINIT	(6)	/* Initialize RE3 registers */
#define	DIAG_GEBUS	(7)	/* Tests gebug between GE7s */
#define	DIAG_GEFIFO	(8)	/* tests GE7 to RE3 data path */
#define	DIAG_REDMA	(9)	/* sets up DMA from Host to RE3 */
#define	DIAG_SHRAMDMA	(10)	/* sets up DMA from Host to Shram */
#define	DIAG_SHRAMREDMA	(11)	/* sets up DMA from shram to RE3 */
#define	DIAG_GESEQ	(12)	/* tests GE7 sequencer commands */
#define DIAG_ATTR	(13)
#define DIAG_ATTR_ALL	(14)
#define DIAG_ACTATTR	(15)
#define DIAG_RELVCTR	(16)
#define DIAG_GECTR	(17)
#define DIAG_BIGTEST	(18)
#define DIAG_S4F	(19)
#define DIAG_S4I	(20)
#define DIAG_S3F	(21)
#define DIAG_S3I	(22)
#define DIAG_S2I	(23) 
#define DIAG_S2F	(24)
#define DIAG_C4I	(25) 
#define DIAG_C4F	(26)
#define DIAG_C3I	(27)
#define DIAG_C3F	(28)
#define DIAG_CPACK	(29)
#define DIAG_CINDEX	(30)
#define DIAG_CFLT	(31)
#define DIAG_LOOPY	(32)
#define DIAG_ONE_QUAD   (33)
#define DIAG_FOUR_QUAD  (34)
#define DIAG_CLEAR_SCREEN (35)
#define DIAG_ONE_QUAD_SERIAL   (36)
#define DIAG_SMALL_QUAD (37)
#define DIAG_CLEAR_NOGRANT (38)
#define DIAG_SMALL_QUAD_NOGRANT (39)
#define DIAG_STRIDEDMA (47)
#define DIAG_SIMDRECTW (48)
#define DIAG_SETCTX (49)
#define DIAG_CMOV (50)
#define DIAG_DRAWCHAR (51)
#define DIAG_ZDRAW	(52)
#define DIAG_READSOURCE	(53)
#define DIAG_RDVTX	(54)
#define DIAG_VTX	(55)
#define DIAG_V4F	(DIAG_VTX | DIAG_USEV)
#define DIAG_V4I	(DIAG_V4F | DIAG_ITOF)
#define DIAG_V3F	(DIAG_V4F | DIAG_CONVV3)
#define DIAG_V3I	(DIAG_V3F | DIAG_ITOF)
#define DIAG_V2F	(DIAG_V4F | DIAG_CONVV2)
#define DIAG_V2I	(DIAG_V2F | DIAG_ITOF)
#define	DIAG_REDMA_ZB	(56)	/* sets up DMA from Host to RE3 */
#define	DIAG_FPUCOND	(57)
#define	DIAG_SPFCOND	(58)
#define	DIAG_FLOATOPS	(59)
#define	DIAG_NORM_QUAD	(60)
#define DIAG_AUXPLANE 	(67)
#define DIAG_CIDPLANE	(68)
#define DIAG_PASS32	(69)
#define DIAG_SIMV3f	(70)
#define DIAG_CTXSW	(511)
#endif /* DIAGCMDS */

void time_delay(unsigned int ticks);
