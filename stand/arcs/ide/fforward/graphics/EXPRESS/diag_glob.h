/* $Header: /proj/irix6.5.7m/isms/stand/arcs/ide/fforward/graphics/EXPRESS/RCS/diag_glob.h,v 1.7 1991/08/14 16:39:41 vimal Exp $ */

/*********************************************************************
*
*	Global variable declarations for GE7.
*
*********************************************************************/


/* the values of these literals are the locations in memory */
#define	CONSTMEM	0	/* base of constants 256 words */
#define	MAXCONS		255	/* last constant */


#define	VARBASE		0
#define	VARSIZE		258

#define COLMEMORY	8100
#define SCRATCHMEM	8192
/*---------------------------------------------------------------
* 	GL RE register shadows & display modes.			 
*
*	Note:
*
*	    For the sake of the poor little micro assemblers which
*           have space sensitive parsers. Please make sure all address
*	    offset calculations are of the following form:
*
*	#define JUNK	VARBASE	+ 0	correct 
*	#define JUNK	VARBASE+10	Assembler cant Parse
*
*	    To add a new variable in the middle, omit the offset value and
*	    add the variable as follows :
*
*	#define JUNK	VARBASE +
*
*	    Then run the 'var.awk' script on the file to adjust all the
*	    offsets following it. Please note that the '+' MUST be preceeded
*	    AND followed by a space.
*
*
*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/
/* 	GL RE register shadows & display modes.			 */
/*---------------------------------------------------------------*/
#define	GE_STATUS	VARBASE + 0	/* Base address of current context */

#define	FILLMODE	VARBASE + 100	/* fill mode mask */
#define	DMAMODE		VARBASE + 101	/* mode bits for DMA */
#define	CHARVALID	VARBASE + 102	/* current text valid flag & position */
#define	CPOSX		VARBASE + 103	/* current character position */
#define	CPOSY		VARBASE + 104
#define	CPOSZ		VARBASE + 105

#define TEMP1		VARBASE + 1000
#define TEMP2		VARBASE + 1001
#define TEMP3		VARBASE + 1002

#define CTXSHRAM	VARBASE + 256
#define DIAG_XLOC	VARBASE + 257
#define DIAG_YLOC	VARBASE + 258
#define DIAG_ZLOC	VARBASE + 259
#define DIAG_WLOC	VARBASE + 260

/* DMA shadow regs */
#define DMAREGS		VARBASE + 4096
#define DMA_XYSTART	VARBASE + 4096
#define DMA_XSIZE	VARBASE + 4097
#define DMA_XINIT	VARBASE + 4098
#define DMA_XRESET	VARBASE + 4099
#define DMA_TXSIZE	VARBASE + 4100
#define DMA_YSIZE	VARBASE + 4101
#define DMA_YINIT	VARBASE + 4102
#define DMA_YRESET	VARBASE + 4103
#define DMA_TYSIZE	VARBASE + 4104
#define DMA_XYORIGIN	VARBASE + 4105

/* Dump loc of ctx 29 words */
#define CTXDUMP		VARBASE + 512

/* Nect loc 4135 */
