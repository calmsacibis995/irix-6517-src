/*	piconfig.h - Edit 6

	LoadICE Version 3
	Copyright (C) 1990-95 Grammar Engine, Inc.
	All rights reserved
	
	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

/*	 - LoadICE configuration parameters
*/

#define	PIC_ARM	30			/* alarm period is 30 seconds */
#define	PIC_SCR	84			/* max entries in table in pisynf.c */
#define	PIC_NUM 16			/* max numeric items in script */
#define	PIC_LST 16			/* max size of numeric list */
#define	PIC_TRY	10			/* reset after this many autobaud chars */
#define	PIC_TOT	5			/* just a second, or two. NOT! */
#define	PIC_NF	32			/* number of files */
#define	PIC_NR	64			/* number of ROMs */
#define	PIC_NC	32			/* number of configurations */
#define	PIC_WS	16			/* max # of ROMs in word */
#define	PIC_KK	24			/* function key count */
#ifdef UNIX
#define	PIC_BS	65536		/* bigbuffer size */
#else
#define	PIC_BS	4096		/* bigbuffer size */
#endif
#define	PIC_XC	32			/* over the link xfer count */
#define	PIC_SN	32			/* serial device name size */
#define	PIC_PN	32			/* parallel device name size */
#define	PIC_HN	128			/* fastport host name */
#define	PIC_FN	128			/* filename size */
#define	PIC_SS	32			/* number of bytes to search */
#define	PIC_CL	512			/* length of the command line */
#define	PIC_MP	259			/* Max packet size */
#define	PIC_MD	256			/* maximum data in packet */
#define	PIC_DS	63			/* default dump size */
#define	PIC_RT	55			/* reset time default is 55*9 ms */
#define	PIC_TI	48			/* default target intrpt length (*2+4 mics) */
#define	PIC_MM	0x1000000	/* 24-bit address sapce */
#define	PIC_MX	0x10000		/* maximum transfer per unit before pilop() */
#define	PIC_LOCK	-1		/* response to 'mode' if unit is locked */
#define	PIC_XCODE	0x96	/* AITTY mode code */
#define	PIC_DLY	100000		/* loop count before checking for time-out */
#define	PIC_PPD0	10		/* loop count for fast host strobe strech */
#define	PIC_PPD1	10		/* loop count for fast host strobe2 strech */
#define	PIC_PPD2	10		/* loop count for fast host b_ack strech */

/*	- and other stuff */

#ifdef	MACAPPL		/* Mac Application (not MPW tool) */
#ifndef	VECDEF
#define VECDEF
extern	int		(*printfVec)();
extern	int		(*getsVec)();
#define	printf	(*printfVec)
#define	gets	(*getsVec)
#define	fgets	(*getsVec)
#endif
#endif

#ifndef	VMS
#define	PI_SUCCESS	0			/* mission successfull */
#define	PI_FAILURE	1			/* didn't make it */
#else
#define	PI_SUCCESS	1			/* and for VMS - */
#define	PI_FAILURE	0			/*  - things are backwards */
#endif
