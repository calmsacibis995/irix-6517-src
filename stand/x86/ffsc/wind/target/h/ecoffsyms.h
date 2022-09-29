/* ecoffSyms.h - MIPS ECOFF symbols definitions header */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01i,04nov94,kdl  merge cleanup - added missing comment marks.
01h,30sep93,caf  removed AUXINT() macro and associated global variable.
01g,22sep92,rrr  added support for c++
01f,04jul92,jcf  cleaned up.
01e,23jun92,ajm  quiet ansi warnings
01d,26may92,rrr  the tree shuffle
01c,17may92,ajm  introduced to new.master
01b,20feb92,ajm  added little endian bitfield defs
01a,15jan92,ajm  written from mips syms.h, symconst.h, sym.h, stsupport.h
*/

#ifndef __INCecoffSymsh
#define __INCecoffSymsh

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------- */
/* | Copyright (c) 1986, 1989 MIPS Computer Systems, Inc.  | */
/* | All Rights Reserved.                                  | */
/* --------------------------------------------------------- */

/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Defines for "special" symbols   */

#define _ETEXT	"etext"
#define	_EDATA	"edata"
#define	_END	"end"

/*
 * The displacement of the gp is from the start of the small data section
 * is GP_DISP.  The GP_PAD is the padding of the gp area so small negitive
 * offset from gp relocation values can allways be used.
 */

#define GP_PAD		16
#define GP_DISP		(32768 - GP_PAD)
#define GP_SIZE 	(GP_DISP+32767)

/* "special" symbols for starts of sections */

#define	_FTEXT		"_ftext"
#define	_FDATA		"_fdata"
#define	_FBSS		"_fbss"
#define	_GP		"_gp"
#define	_PROCEDURE_TABLE	"_procedure_table"
#define	_PROCEDURE_TABLE_SIZE	"_procedure_table_size"
#define	_PROCEDURE_STRING_TABLE	"_procedure_string_table"
#define	_COBOL_MAIN		"_cobol_main"
#define _START			"__start"

/* --------------------------------------------------------- */
/* | Copyright (c) 1986, 1989 MIPS Computer Systems, Inc.  | */
/* | All Rights Reserved.                                  | */
/* --------------------------------------------------------- */

/* (C) Copyright 1984 by Third Eye Software, Inc.
 *
 * Third Eye Software, Inc. grants reproduction and use rights to
 * all parties, PROVIDED that this comment is maintained in the copy.
 *
 * Third Eye makes no claims about the applicability of this
 * symbol table to a particular use.
 */

/* glevels for field in FDR */
#define GLEVEL_0	2
#define GLEVEL_1	1
#define GLEVEL_2	0	/* for upward compat reasons. */
#define GLEVEL_3	3

/* magic number fo symheader */
#define magicSym	0x7009

/* Language codes */
#define langC		0
#define langPascal	1
#define langFortran	2
#define	langAssembler	3	/* one Assembley inst might map to many mach */
#define langMachine	4
#define langNil		5
#define langAda		6
#define langPl1		7
#define langCobol	8
#define langMax		9	/* maximun allowed 32 -- 5 bits */

/* The following are value definitions for the fields in the SYMR */

/*
 * Storage Classes
 */

#define scNil		0
#define scText		1	/* text symbol */
#define scData		2	/* initialized data symbol */
#define scBss		3	/* un-initialized data symbol */
#define scRegister	4	/* value of symbol is register number */
#define scAbs		5	/* value of symbol is absolute */
#define scUndefined	6	/* who knows? */
#define scCdbLocal	7	/* variable's value is IN se->va.?? */
#define scBits		8	/* this is a bit field */
#define scCdbSystem	9	/* variable's value is IN CDB's address space */
#define scDbx		9	/* overlap dbx internal use */
#define scRegImage	10	/* register value saved on stack */
#define scInfo		11	/* symbol contains debugger information */
#define scUserStruct	12	/* address in struct user for current process */
#define scSData		13	/* load time only small data */
#define scSBss		14	/* load time only small common */
#define scRData		15	/* load time only read only data */
#define scVar		16	/* Var parameter (fortran,pascal) */
#define scCommon	17	/* common variable */
#define scSCommon	18	/* small common */
#define scVarRegister	19	/* Var parameter in a register */
#define scVariant	20	/* Variant record */
#define scSUndefined	21	/* small undefined(external) data */
#define scInit		22	/* .init section symbol */
#define scBasedVar	23	/* Fortran or PL/1 ptr based var */
#define scMax		32


/*
 *   Symbol Types
 */

#define stNil		0	/* Nuthin' special */
#define stGlobal	1	/* external symbol */
#define stStatic	2	/* static */
#define stParam		3	/* procedure argument */
#define stLocal		4	/* local variable */
#define stLabel		5	/* label */
#define stProc		6	/*     "      "	 Procedure */
#define stBlock		7	/* beginnning of block */
#define stEnd		8	/* end (of anything) */
#define stMember	9	/* member (of anything	- struct/union/enum */
#define stTypedef	10	/* type definition */
#define stFile		11	/* file name */
#define stRegReloc	12	/* register relocation */
#define stForward	13	/* forwarding address */
#define stStaticProc	14	/* load time only static procs */
#define stConstant	15	/* const */
#define stStaParam	16	/* Fortran static parameters */
    /* Psuedo-symbols - internal to debugger */
#define stStr		60	/* string */
#define stNumber	61	/* pure number (ie. 4 NOR 2+2) */
#define stExpr		62	/* 2+2 vs. 4 */
#define stType		63	/* post-coersion SER */
#define stMax		64

/* definitions for fields in TIR */

/* type qualifiers for ti.tq0 -> ti.(itqMax-1) */
#define tqNil	0	/* bt is what you see */
#define tqPtr	1	/* pointer */
#define tqProc	2	/* procedure */
#define tqArray 3	/* duh */
#define tqFar	4	/* longer addressing - 8086/8 land */
#define tqVol	5	/* volatile */
#define tqMax	8

/* basic types as seen in ti.bt */
#define btNil		0	/* undefined */
#define btAdr		1	/* address - integer same size as pointer */
#define btChar		2	/* character */
#define btUChar		3	/* unsigned character */
#define btShort		4	/* short */
#define btUShort	5	/* unsigned short */
#define btInt		6	/* int */
#define btUInt		7	/* unsigned int */
#define btLong		8	/* long */
#define btULong		9	/* unsigned long */
#define btFloat		10	/* float (real) */
#define btDouble	11	/* Double (real) */
#define btStruct	12	/* Structure (Record) */
#define btUnion		13	/* Union (variant) */
#define btEnum		14	/* Enumerated */
#define btTypedef	15	/* defined via a typedef, isymRef points */
#define btRange		16	/* subrange of int */
#define btSet		17	/* pascal sets */
#define btComplex	18	/* fortran complex */
#define btDComplex	19	/* fortran double complex */
#define btIndirect	20	/* forward or unnamed typedef */
#define btFixedDec	21	/* Fixed Decimal */
#define btFloatDec	22	/* Float Decimal */
#define btString	23	/* Varying Length Character String */
#define btBit		24	/* Aligned Bit String */
#define btPicture	25	/* Picture */
#define btVoid		26	/* void */
#define btMax		64

#if (MFG == MIPS)
/* optimization type codes */
#define otNil		0
#define otReg		1	/* move var to reg */
#define otBlock		2	/* begin basic block */
#define	otProc		3	/* procedure */
#define otInline	4	/* inline procedure */
#define otEnd		5	/* whatever you started */
#define otMax		6	/* KEEP UP TO DATE */
#endif /* (MFG == MIPS) */

/* --------------------------------------------------------- */
/* | Copyright (c) 1986, 1989 MIPS Computer Systems, Inc.  | */
/* | All Rights Reserved.                                  | */
/* --------------------------------------------------------- */

/* (C) Copyright 1984 by Third Eye Software, Inc.
 *
 * Third Eye Software, Inc. grants reproduction and use rights to
 * all parties, PROVIDED that this comment is maintained in the copy.
 *
 * Third Eye makes no claims about the applicability of this
 * symbol table to a particular use.
 */

/*
 * This file contains the definition of the Third Eye Symbol Table.
 *
 * Symbols are assumed to be in 'encounter order' - i.e. the order that
 * the things they represent were encountered by the compiler/assembler/loader.
 * EXCEPT for globals!	These are assumed to be bunched together,
 * probably right after the last 'normal' symbol.  Globals ARE sorted
 * in ascending order.
 *
 * -----------------------------------------------------------------------
 * A brief word about Third Eye naming/use conventions:
 *
 * All arrays and index's are 0 based.
 * All "ifooMax" values are the highest legal value PLUS ONE. This makes
 * them good for allocating arrays, etc. All checks are "ifoo < ifooMax".
 *
 * "isym"	Index into the SYMbol table.
 * "ipd"	Index into the Procedure Descriptor array.
 * "ifd"	Index into the File Descriptor array.
 * "iss"	Index into String Space.
 * "cb"		Count of Bytes.
 * "rgPd"	array whose domain is "0..ipdMax-1" and RanGe is PDR.
 * "rgFd"	array whose domain is "0..ifdMax-1" and RanGe is FDR.
 */


/*
 * Symbolic Header (HDR) structure.
 * As long as all the pointers are set correctly,
 * we don't care WHAT order the various sections come out in!
 *
 * A file produced solely for the use of CDB will probably NOT have
 * any instructions or data areas in it, as these are available
 * in the original.
 */

typedef struct {
	short	magic;		/* to verify validity of the table */
	short	vstamp;		/* version stamp */
	long	ilineMax;	/* number of line number entries */
	long	cbLine;		/* number of bytes for line number entries */
	long	cbLineOffset;	/* offset to start of line number entries*/
	long	idnMax;		/* max index into dense number table */
	long	cbDnOffset;	/* offset to start dense number table */
	long	ipdMax;		/* number of procedures */
	long	cbPdOffset;	/* offset to procedure descriptor table */
	long	isymMax;	/* number of local symbols */
	long	cbSymOffset;	/* offset to start of local symbols*/
	long	ioptMax;	/* max index into optimization symbol entries */
	long	cbOptOffset;	/* offset to optimization symbol entries */
	long	iauxMax;	/* number of auxillary symbol entries */
	long	cbAuxOffset;	/* offset to start of auxillary symbol entries*/
	long	issMax;		/* max index into local strings */
	long	cbSsOffset;	/* offset to start of local strings */
	long	issExtMax;	/* max index into external strings */
	long	cbSsExtOffset;	/* offset to start of external strings */
	long	ifdMax;		/* number of file descriptor entries */
	long	cbFdOffset;	/* offset to file descriptor table */
	long	crfd;		/* number of relative file descriptor entries */
	long	cbRfdOffset;	/* offset to relative file descriptor table */
	long	iextMax;	/* max index into external symbols */
	long	cbExtOffset;	/* offset to start of external symbol entries*/
	/* If you add machine dependent fields, add them here */
	} HDRR, *pHDRR;
#define cbHDRR sizeof(HDRR)
#define hdrNil ((pHDRR)0)

/*
 * The FDR and PDR structures speed mapping of address <-> name.
 * They are sorted in ascending memory order and are kept in
 * memory by CDB at runtime.
 */

/*
 * File Descriptor
 *
 * There is one of these for EVERY FILE, whether compiled with
 * full debugging symbols or not.  The name of a file should be
 * the path name given to the compiler.	 This allows the user
 * to simply specify the names of the directories where the COMPILES
 * were done, and we will be able to find their files.
 * A field whose comment starts with "R - " indicates that it will be
 * setup at runtime.
 */
typedef struct fdr {
	unsigned long	adr;	/* memory address of beginning of file */
	long	rss;		/* file name (of source, if known) */
	long	issBase;	/* file's string space */
	long	cbSs;		/* number of bytes in the ss */
	long	isymBase;	/* beginning of symbols */
	long	csym;		/* count file's of symbols */
	long	ilineBase;	/* file's line symbols */
	long	cline;		/* count of file's line symbols */
	long	ioptBase;	/* file's optimization entries */
	long	copt;		/* count of file's optimization entries */
	short	ipdFirst;	/* start of procedures for this file */
	short	cpd;		/* count of procedures for this file */
	long	iauxBase;	/* file's auxiliary entries */
	long	caux;		/* count of file's auxiliary entries */
	long	rfdBase;	/* index into the file indirect table */
	long	crfd;		/* count file indirect entries */
	unsigned lang: 5;	/* language for this file */
	unsigned fMerge : 1;	/* whether this file can be merged */
	unsigned fReadin : 1;	/* true if it was read in (not just created) */
	unsigned fBigendian : 1;/* if set, was compiled on big endian machine */
				/*	aux's will be in compile host's sex */
	unsigned glevel : 2;	/* level this file was compiled with */
	unsigned reserved : 22;  /* reserved for future use */
	long	cbLineOffset;	/* byte offset from header for this file ln's */
	long	cbLine;		/* size of lines for this file */
	} FDR, *pFDR;
#define cbFDR sizeof(FDR)
#define fdNil ((pFDR)0)
#define ifdNil -1
#define ifdTemp 0
#define ilnNil -1


/*
 * Procedure Descriptor
 *
 * There is one of these for EVERY TEXT LABEL.
 * If a procedure is in a file with full symbols, then isym
 * will point to the PROC symbols, else it will point to the
 * global symbol for the label.
 */

typedef struct pdr {
	unsigned long	adr;	/* memory address of start of procedure */
	long	isym;		/* start of local symbol entries */
	long	iline;		/* start of line number entries*/
	long	regmask;	/* save register mask */
	long	regoffset;	/* save register offset */
	long	iopt;		/* start of optimization symbol entries*/
	long	fregmask;	/* save floating point register mask */
	long	fregoffset;	/* save floating point register offset */
	long	frameoffset;	/* frame size */
	short	framereg;	/* frame pointer register */
	short	pcreg;		/* offset or reg of return pc */
	long	lnLow;		/* lowest line in the procedure */
	long	lnHigh;		/* highest line in the procedure */
	long	cbLineOffset;	/* byte offset for this procedure from the fd base */
	} PDR, *pPDR;
#define cbPDR sizeof(PDR)
#define pdNil ((pPDR) 0)
#define ipdNil	-1

/*
 * The structure of the runtime procedure descriptor created by the loader
 * for use by the static exception system.
 */
typedef struct runtime_pdr {
	unsigned long	adr;	/* memory address of start of procedure */
	long	regmask;	/* save register mask */
	long	regoffset;	/* save register offset */
	long	fregmask;	/* save floating point register mask */
	long	fregoffset;	/* save floating point register offset */
	long	frameoffset;	/* frame size */
	short	framereg;	/* frame pointer register */
	short	pcreg;		/* offset or reg of return pc */
	long	irpss;		/* index into the runtime string table */
	long	reserved;
	struct exception_info *exception_info;/* pointer to exception array */
} RPDR, *pRPDR;
#define cbRPDR sizeof(RPDR)
#define rpdNil ((pRPDR) 0)

/*
 * Line Numbers
 *
 * Line Numbers are segregated from the normal symbols because they
 * are [1] smaller , [2] are of no interest to your
 * average loader, and [3] are never needed in the middle of normal
 * scanning and therefore slow things down.
 *
 * By definition, the first LINER for any given procedure will have
 * the first line of a procedure and represent the first address.
 */

typedef	long LINER, *pLINER;
#define lineNil ((pLINER)0)
#define cbLINER sizeof(LINER)
#define ilineNil	-1



/*
 * The Symbol Structure		(GFW, to those who Know!)
 */

typedef struct {
	long	iss;		/* index into String Space of name */
	long	value;		/* value of symbol */
	unsigned st : 6;	/* symbol type */
	unsigned sc  : 5;	/* storage class - text, data, etc */
	unsigned reserved : 1;	/* reserved */
	unsigned index : 20;	/* index into sym/aux table */
	} SYMR, *pSYMR;
/*
 *  Now SYMR with Little Endian bitfields for swaps
 */

typedef struct {
	long	iss;		/* index into String Space of name */
	long	value;		/* value of symbol */
	unsigned index : 20;	/* index into sym/aux table */
	unsigned reserved : 1;	/* reserved */
	unsigned sc  : 5;	/* storage class - text, data, etc */
	unsigned st : 6;	/* symbol type */
	} SYMR_LE, *pSYMR_LE;

#define symNil ((pSYMR)0)
#define cbSYMR sizeof(SYMR)
#define isymNil -1
#define indexNil 0xfffff
#define issNil -1
#define issNull 0


/* The following converts a memory resident string to an iss.
 * This hack is recognized in SbFIss, in sym.c of the debugger.
 */
#define IssFSb(sb) (0x80000000 | ((unsigned long)(sb)))

/* E X T E R N A L   S Y M B O L  R E C O R D
 *
 *	Same as the SYMR except it contains file context to determine where
 *	the index is.
 */
typedef struct {
	unsigned jmptbl:1;	/* symbol is a jump table entry for shlibs */
	unsigned cobol_main:1;	/* symbol is a cobol main procedure */
	unsigned reserved:14;	/* reserved for future use */
	short	ifd;		/* where the iss and index fields point into */
	SYMR	asym;		/* symbol for the external */
	} EXTR, *pEXTR;
/*
 *
 *  Now EXTR with Litte Endian bit fields for swaps
 *
 */

typedef struct {
	unsigned reserved:14;	/* reserved for future use */
	unsigned cobol_main:1;	/* symbol is a cobol main procedure */
	unsigned jmptbl:1;	/* symbol is a jump table entry for shlibs */
	short	ifd;		/* where the iss and index fields point into */
	SYMR	asym;		/* symbol for the external */
	} EXTR_LE, *pEXTR_LE;

#define extNil ((pEXTR)0)
#define cbEXTR sizeof(EXTR)


/* A U X I L L A R Y   T Y P E	 I N F O R M A T I O N */

/*
 * Type Information Record
 */
typedef struct {
	unsigned fBitfield : 1; /* set if bit width is specified */
	unsigned continued : 1; /* indicates additional TQ info in next AUX */
	unsigned bt  : 6;	/* basic type */
	unsigned tq4 : 4;
	unsigned tq5 : 4;
	/* ---- 16 bit boundary ---- */
	unsigned tq0 : 4;
	unsigned tq1 : 4;	/* 6 type qualifiers - tqPtr, etc. */
	unsigned tq2 : 4;
	unsigned tq3 : 4;
	} TIR, *pTIR;
#define cbTIR sizeof(TIR)
#define tiNil ((pTIR)0)
#define itqMax 6

/*
 * Relative symbol record
 *
 * If the rfd field is 4095, the index field indexes into the global symbol
 *	table.
 */

typedef struct {
	unsigned	rfd : 12;    /* index into the file indirect table */
	unsigned	index : 20; /* index int sym/aux/iss tables */
	} RNDXR, *pRNDXR;
#define cbRNDXR sizeof(RNDXR)
#define rndxNil ((pRNDXR)0)

/* dense numbers or sometimes called block numbers are stored in this type,
 *	a rfd of 0xffffffff is an index into the global table.
 */
typedef struct {
	unsigned long	rfd;    /* index into the file table */
	unsigned long	index; 	/* index int sym/aux/iss tables */
	} DNR, *pDNR;
#define cbDNR sizeof(DNR)
#define dnNil ((pDNR)0)



/*
 * Auxillary information occurs only if needed.
 * It ALWAYS occurs in this order when present.

	    isymMac		used by stProc only
	    TIR			type info
	    TIR			additional TQ info (if first TIR was not enough)
	    rndx		if (bt == btStruct,btUnion,btEnum,btSet,btRange,
				    btTypedef):
				    rsym.index == iaux for btSet or btRange
				    else rsym.index == isym
	    dimLow		btRange, btSet
	    dimMac		btRange, btSet
	    rndx0		As many as there are tq arrays
	    dimLow0
	    dimHigh0
	    ...
	    rndxMax-1
	    dimLowMax-1
	    dimHighMax-1
	    width in bits	if (bit field), width in bits.
 */
#define cAuxMax (6 + (idimMax*3))

/* a union of all possible info in the AUX universe */
typedef union {
	TIR	ti;		/* type information record */
	RNDXR	rndx;		/* relative index into symbol table */
	long	dnLow;		/* low dimension */
	long	dnHigh;		/* high dimension */
	long	isym;		/* symbol table index (end of proc) */
	long	iss;		/* index into string space (not used) */
	long	width;		/* width for non-default sized struc fields */
	long	count;		/* count of ranges for variant arm */
	} AUXU, *pAUXU;
#define cbAUXU sizeof(AUXU)
#define auxNil ((pAUXU)0)
#define iauxNil -1


/*
 * Optimization symbols
 *
 * Optimization symbols contain some overlap information with the normal
 * symbol table. In particular, the proc information
 * is somewhat redundant but necessary to easily find the other information
 * present.
 *
 * All of the offsets are relative to the beginning of the last otProc
 */

typedef struct {
	unsigned ot: 8;		/* optimization type */
	unsigned value: 24;	/* address where we are moving it to */
	RNDXR	rndx;		/* points to a symbol or opt entry */
	unsigned long	offset;	/* relative offset this occured */
	} OPTR, *pOPTR;
#define optNil	((pOPTR) 0)
#define cbOPTR sizeof(OPTR)
#define ioptNil -1

/*
 * File Indirect
 *
 * When a symbol is referenced across files the following procedure is used:
 *	1) use the file index to get the File indirect entry.
 *	2) use the file indirect entry to get the File descriptor.
 *	3) add the sym index to the base of that file's sym table
 *
 */

typedef long RFDT, *pRFDT;
#define cbRFDT sizeof(RFDT)
#define rfdNil	-1

/*
 * The file indirect table in the mips loader is known as an array of FITs.
 * This is done to keep the code in the loader readable in the area where
 * these tables are merged.  Note this is only a name change.
 */
typedef long FIT, *pFIT;
#define cbFIT	sizeof(FIT)
#define ifiNil	-1
#define fiNil	((pFIT) 0)

/* Dense numbers
 *
 * Rather than use file index, symbol index pairs to represent symbols
 *	and globals, we use dense number so that they can be easily embeded
 *	in intermediate code and the programs that process them can
 *	use direct access tabls instead of hash table (which would be
 *	necesary otherwise because of the sparse name space caused by
 *	file index, symbol index pairs. Dense number are represented
 *	by RNDXRs.
 */

/*
 * The following table defines the meaning of each SYM field as
 * a function of the "st". (scD/B == scData OR scBss)
 *
 * Note: the value "isymMac" is used by symbols that have the concept
 * of enclosing a block of related information.	 This value is the
 * isym of the first symbol AFTER the end associated with the primary
 * symbol. For example if a procedure was at isym==90 and had an
 * isymMac==155, the associated end would be at isym==154, and the
 * symbol at 155 would probably (although not necessarily) be the
 * symbol for the next procedure.  This allows rapid skipping over
 * internal information of various sorts. "stEnd"s ALWAYS have the
 * isym of the primary symbol that started the block.
 *

ST		SC	VALUE		INDEX
--------	------	--------	------@0@a,04jul92,jcf   cleaned up.
stFile		scText	address		isymMac
stLabel		scText	address		---
stGlobal	scD/B	address		iaux
stStatic	scD/B	address		iaux
stParam		scAbs	offset		iaux
stLocal		scAbs	offset		iaux
stProc		scText	address		iaux	(isymMac is first AUX)
stStaticProc	scText	address		iaux	(isymMac is first AUX)

stMember	scNil	ordinal		---	(if member of enum)
stMember	scNil	byte offset	iaux	(if member of struct/union)
stMember	scBits	bit offset	iaux	(bit field spec)

stBlock		scText	address		isymMac (text block)
stBlock		scNil	cb		isymMac (struct/union member define)
stBlock		scNil	cMembers	isymMac (enum member define)

stEnd		scText	address		isymStart
stEnd		scNil	-------		isymStart (struct/union/enum)

stTypedef	scNil	-------		iaux
stRegReloc	sc???	value		old register number
stForward	sc???	new address	isym to original symbol

stConstant	scInfo	value		--- (scalar)
stConstant	scInfo	iss		--- (complex, e.g. string)

 *
 */

/* --------------------------------------------------------- */
/* | Copyright (c) 1986, 1989 MIPS Computer Systems, Inc.  | */
/* | All Rights Reserved.                                  | */
/* --------------------------------------------------------- */
/* Revision 2.1.1.1  1993/06/07  15:55:11  wise
 * project.initialize
 *
 * Revision 1.1  1993/06/07  11:11:14  wise
 */
/*
 * Author	Mark I. Himelstein
 * Date Started	5/15/85
 * Purpose	provide support to uc to produce mips symbol tables.
 */

#if	FALSE

/* removed because global variable in header file causes compile problems */

AUXU _auxtemp;
#define AUXINT(c) ((_auxtemp.isym = c), _auxtemp)

#endif	/* FALSE */

/* the following struct frames the FDR dtructure and is used at runtime
 *	to access the objects in the FDR with pointers (since the FDR
 *	only has indeces.
 */
typedef struct {
	pFDR	pfd;		/* file descriptor for this file */
	pSYMR	psym;		/* symbols for this file */
	long	csymMax;	/* max allocated */
	pAUXU	paux;		/* auxiliaries for this file */
	long	cauxMax;	/* max allocated */
	char	*pss;		/* strings space for this file */
	long	cbssMax;	/* max bytes allowed in ss */
	pOPTR	popt;		/* optimization table for this file */
	long	coptMax;	/* max allocated */
	pLINER	pline;		/* lines for this file */
	long	clineMax;	/* max allocated */
	pRFDT	prfd;		/* file indirect for this file */
	long	crfdMax;	/* max allocated */
	pPDR	ppd;		/* procedure descriptor tables */
	long	cpdMax;		/* max allocated */
	long	freadin;	/* set if read in */
	} CFDR, *pCFDR;
#define cbCFDR sizeof (CFDR)
#define cfdNil ((pCFDR) 0)
#define icfdNil -1


/* the following struct embodies the HDRR dtructure and is used at runtime
 *	to access the objects in the HDRR with pointers (since the HDRR
 *	only has indeces.
 */
typedef struct {
	long	fappend;	/* are we appending to this beast ? */
	pCFDR	pcfd;		/* the compile time file descriptors */
	pFDR	pfd;		/* all of the file descriptors in this cu */
	long	cfd;		/* count of file descriptors */
	long	cfdMax;		/* max file descriptors */
	pSYMR	psym;		/* the symbols for this cu */
	pEXTR	pext;		/* externals for this cu */
	long	cext;		/* number of externals */
	long	cextMax;	/* max currently allowed */
	char	*pssext;	/* string space for externals */
	long	cbssext;	/* # of bytes in ss */
	long	cbssextMax;	/* # of bytes allowed in ss */
	pAUXU	paux;		/* auxiliaries for this cu */
	char	*pss;		/* string space for this cu */
	pDNR	pdn;		/* dense number table for this cu */
	long	cdn;		/* number of dn's */
	long	cdnMax;		/* max currently allowed */
	pOPTR	popt;		/* optimization table for this cu */
	pLINER	pline;		/* lines for this cu */
	pRFDT	prfd;		/* file indirect for this cu */
	pPDR	ppd;		/* procedure descriptor tables */
	int	flags;		/* which has been read in already */
	int	fswap;		/* do dubtables need to be swapped */
	HDRR	hdr;		/* header from disk */
	} CHDRR, *pCHDRR;
#define cbCHDRR sizeof (CHDRR)
#define chdrNil ((pCHDRR) 0)
#define ichdrNil -1

/* constants and macros */

#define ST_FILESINIT	25 	/* initial number of files */
#define ST_STRTABINIT	512 	/* initial number of bytes in strring space */
#define ST_EXTINIT	32	/* initial number of symbols/file */
#define ST_SYMINIT	64	/* initial number of symbols/file */
#define ST_AUXINIT	64	/* initial number of auxiliaries/file */
#define ST_LINEINIT	512	/* initial number of auxiliaries/file */
#define ST_PDINIT	32	/* initial number of procedures in one file */
#define ST_DNINIT	128	/* initial # of dense numbers */
#define ST_FDISS	1	/* we expect a fd's iss to be this */
#define ST_IDNINIT	2	/* start the dense num tab with this entry */
#define ST_PROCTIROFFSET 1	/* offset from aux of proc's tir */
#define ST_RELOC	1	/* this sym has been reloced already */

#define ST_EXTIFD	0x7fffffff	/* ifd for externals */
#define ST_RFDESCAPE	0xfff	/* rndx.rfd escape says next aux is rfd */
#define ST_ANONINDEX	0xfffff /* rndx.index for anonymous names */
#define ST_PEXTS	0x01	/* mask, if set externals */
#define ST_PSYMS	0x02	/* mask, if set symbols */
#define ST_PLINES	0x04	/* mask, if set lines */
#define ST_PHEADER	0x08	/* mask, if set headers */
#define ST_PDNS		0x10	/* mask, if set dense numbers */
#define ST_POPTS	0x20	/* mask, if set optimization entries */
#define ST_PRFDS	0x40	/* mask, if set file indirect entries */
#define ST_PSSS		0x80	/* mask, if set string space */
#define ST_PPDS		0x100	/* mask, if set proc descriptors */
#define ST_PFDS		0x200	/* mask, if set file descriptors */
#define ST_PAUXS	0x400	/* mask, if set auxiliaries */
#define ST_PSSEXTS	0x800	/* mask, if set external string space */

#define ST_FCOMPLEXBT(bt) ((bt == btStruct) || (bt == btUnion) || (bt == btTypedef) || (bt == btEnum))
#define valueNil	0
#define export

/*
 * Constants to describe aux types used in swap_aux( , ,type);
 */
#define ST_AUX_TIR	0
#define ST_AUX_RNDXR	1
#define ST_AUX_DNLOW	2
#define ST_AUX_DNMAC	3
#define ST_AUX_ISYM	4
#define ST_AUX_ISS	5
#define ST_AUX_WIDTH	6

/* Revision 2.1.1.1  1993/06/07  15:55:11  wise
 * project.initialize
 *
 * Revision 1.1  1993/06/07  11:11:14  wise
 */

#ifdef __cplusplus
}
#endif

#endif /* __INCecoffSymsh */
