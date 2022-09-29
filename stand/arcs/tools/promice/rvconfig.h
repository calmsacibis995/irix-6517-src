/*	rvconfig.h - Edit 0.2

	RomView Version 1.1a
	Copyright (C) 1990-2 Grammar Engine, Inc.
	All rights reserved
	
	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

/*	 - RomView configuration parameters
*/
#ifdef PIRV
#define	RVC_NUM		16			/* max numeric items in script */
#define	RVC_LST		16			/* max size of numeric list */
#define	RVC_CL		128			/* command line input */
#define	RVC_NC		32			/* number of configurations */
#define	RVC_WS		16			/* max # of ROMs in word */
#define	RVC_BS		256			/* bigbuffer size */
#define	RVC_DS		63			/* default dump size */
#define	RVC_XC		32			/* over the link xfer count */
#define	RVC_STKP	0x800		/* default stack pointer */
#define	RVC_STKS	64			/* default stack size */
#define RVC_NBRK	64			/* number of break points */

#define	RVC_JUMP	0x020000	/* mask for jump */
#define	RVC_CALL	0x120000	/* mask for call */
#define	RVC_PCTS	0x00C000	/* 'pc to stack' */
#define	RVC_STPC	0x00D0E0	/* 'stack to pc' */
#define	RVC_MTAC	0x00E500	/* move something to ACC */
#define	RVC_ACTM	0x00F500	/* move ACC to something */
#endif
