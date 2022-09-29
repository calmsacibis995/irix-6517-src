/* ecoff.h - MIPS common object file format header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01j,22sep92,rrr  added support for c++
01i,23jul92,ajm  added MAX_SCN define
01h,04jul92,jcf  cleaned up.
01g,23jun92,ajm  quiet ansi warnings
01f,26may92,rrr  the tree shuffle
01e,17may92,ajm  introduced to new.master
01d,20feb92,ajm  added little endian bitfields for swaps
01c,15jan92,ajm  got rid of #define mips sections
01c,01dec91,ajm  added mips definitions for AOUTHDR
01b,26jul90,rtp  added MIPSELMAGIC definition.
02a,28jun90,dcb  derived from coffAout.h.
01a,05feb88,ecs  derived from UniPlus+ System V Release 2 a.out.h., plus
		    all the files that it included, and the files that those
		    files, in turn, included. Now all one big happy file.
*/

#ifndef __INCecoffh
#define __INCecoffh

#ifdef __cplusplus
extern "C" {
#endif

/*		COMMON OBJECT FILE FORMAT



 	File Organization:

 	_______________________________________________    INCLUDE FILE
 	|_______________HEADER_DATA___________________|
 	|					      |
 	|	File Header			      |    "filehdr.h"
 	|.............................................|
 	|					      |
 	|	Auxilliary Header Information	      |	   "aouthdr.h"
 	|					      |
 	|_____________________________________________|
 	|					      |
 	|	".text" section header		      |	   "snnhdr.h"
 	|					      |
 	|.............................................|
 	|					      |
 	|	".data" section header		      |	      ''
 	|					      |
 	|.............................................|
 	|					      |
 	|	".bss" section header		      |	      ''
 	|					      |
 	|_____________________________________________|
 	|______________RAW_DATA_______________________|
 	|					      |
 	|	".text" section data (rounded to 4    |
 	|				bytes)	      |
 	|.............................................|
 	|					      |
 	|	".data" section data (rounded to 4    |
 	|				bytes)	      |
 	|_____________________________________________|
 	|____________RELOCATION_DATA__________________|
 	|					      |
 	|	".text" section relocation data	      |    "reloc.h"
 	|					      |
 	|.............................................|
 	|					      |
 	|	".data" section relocation data	      |       ''
 	|					      |
 	|_____________________________________________|
 	|__________LINE_NUMBER_DATA_(SDB)_____________|
 	|					      |
 	|	".text" section line numbers	      |    "linenum.h"
 	|					      |
 	|.............................................|
 	|					      |
 	|	".data" section line numbers	      |	      ''
 	|					      |
 	|_____________________________________________|
 	|________________SYMBOL_TABLE_________________|
 	|					      |
 	|	".text", ".data" and ".bss" section   |    "syms.h"
 	|	symbols				      |	   "storclass.h"
 	|					      |
 	|_____________________________________________|



 		OBJECT FILE COMPONENTS

 	HEADER FILES:	(of historical interest only)
 			/usr/include/filehdr.h
			/usr/include/aouthdr.h
			/usr/include/snnhdr.h
			/usr/include/reloc.h
			/usr/include/linenum.h
			/usr/include/syms.h
			/usr/include/storclass.h

	STANDARD FILE:
			vw/h/coffAout.h    "object file"     */

/* Unix magic numbers found in optional header */

#undef	OMAGIC		/* undefine for a.out overlaps */
#undef	NMAGIC
#undef	ZMAGIC
#undef	LIBMAGIC
#undef	N_BADMAG
#define OMAGIC  0x107
#define NMAGIC  0x108
#define ZMAGIC  0x106
#define LIBMAGIC        0x123
#define N_BADMAG(x) \
    (((x).magic)!=OMAGIC && ((x).magic)!=NMAGIC && ((x).magic)!=ZMAGIC && \
	 ((x).magic)!=LIBMAGIC)

/*
 * Coff files produced by the mips loader are guaranteed to have the raw data
 * for the sections follow the headers in this order: .text, .rdata, .data and
 * .sdata the sum of the sizes of last three is the value in dsize in the
 * optional header.  This is all done for the benefit of the programs that
 * have to load these objects so only the file header and optional header
 * have to be inspected.  The macro N_TXTOFF() takes pointers to file header
 * and optional header and returns the file offset to the start of the raw
 * data for the .text section.  The raw data for the three data sections
 * follows the start of the .text section by the value of tsize in the optional
 * header.
 *
 * Object files produced by pre 0.23 versions of the compiler had their sections
 * rounded to 8 byte boundaries.  0.23 and later versions have their sections
 * rounded to 16 (SCNROUND) byte boundaries.
 */

#undef	N_TXTOFF		/* undefined for a.out overlap */
#define N_TXTOFF(f, a) \
((a).magic == ZMAGIC || (a).magic == LIBMAGIC ? 0 : \
    ((a).vstamp < 23 ? \
    ((FILHSZ + AOUTHSZ + (f).f_nscns * SCNHSZ + 7) & 0xfffffff8) : \
    ((FILHSZ + AOUTHSZ + (f).f_nscns * SCNHSZ + SCNROUND-1) & ~(SCNROUND-1)) ) )

/*********************************************filehdr.h********************/

/*	@(#)filehdr.h	2.1	 */
struct filehdr {
	unsigned short	f_magic;	/* magic number */
	unsigned short	f_nscns;	/* number of sections */
	long		f_timdat;	/* time & date stamp */
	long		f_symptr;	/* file pointer to symtab */
	long		f_nsyms;	/* number of symtab entries */
	unsigned short	f_opthdr;	/* sizeof(optional hdr) */
	unsigned short	f_flags;	/* flags */
	};


/*
 *   Bits for f_flags:
 *
 *	F_RELFLG	relocation info stripped from file
 *	F_EXEC		file is executable  (i.e. no unresolved
 *				external references)
 *	F_LNNO		line nunbers stripped from file
 *	F_LSYMS		local symbols stripped from file
 *	F_MINMAL	this is a minimal object file (".m") output of fextract
 *	F_UPDATE	this is a fully bound update file, output of ogen
 *	F_SWABD		this file has had its bytes swabbed (in names)
 *	F_AR16WR	this file created on AR16WR machine(e.g. 11/70)
 *	F_AR32WR	this file created on AR32WR machine(e.g. vax)
 *	F_AR32W		this file created on AR32W machine (e.g. 3b,maxi)
 *	F_PATCH		file contains "patch" list in optional header
 *	F_NODF		(minimal file only) no decision functions for
 *				replaced functions
 */

#define  F_RELFLG	0000001
#define  F_EXEC		0000002
#define  F_LNNO		0000004
#define  F_LSYMS	0000010
#define  F_MINMAL	0000020
#define  F_UPDATE	0000040
#define  F_SWABD	0000100
#define  F_AR16WR	0000200
#define  F_AR32WR	0000400
#define  F_AR32W	0001000
#define  F_PATCH	0002000
#define  F_NODF		0002000


/*
 *   Magic Numbers
 */

	/* Basic-16 */

#define  B16MAGIC	0502
#define  BTVMAGIC	0503

	/* x86 */

#define  X86MAGIC	0510
#define  XTVMAGIC	0511

	/* n3b */
/*
 *   NOTE:   For New 3B, the old values of magic numbers
 *		will be in the optional header in the structure
 *		"aouthdr" (identical to old 3B aouthdr).
 */

#define  N3BMAGIC	0550
#define  NTVMAGIC	0551

	/*  XL  */
#define	 XLMAGIC	0540

	/*  MAC-32   3b-5  */

#define  FBOMAGIC	0560
#define  RBOMAGIC	0562
#define  MTVMAGIC	0561


	/* VAX 11/780 and VAX 11/750 */

			/* writeable text segments */
#define VAXWRMAGIC	0570
			/* readonly sharable text segments */
#define VAXROMAGIC	0575


	/* Motorola 68000 */
#define	MC68MAGIC	0520	/* System V/68 magic number */
#define	MC68TVMAGIC	0521	/* Bell only */
#define	M68MAGIC	0210	/* Bell only */
#define	M68TVMAGIC	0211	/* Bell only */

	/* UniSoft additions for 68000 */
#define M68NSMAGIC	0510	/* Non Shared */

	/* IBM 370 */
#define	U370WRMAGIC	0530	/* writeble text segments	*/
#define	U370ROMAGIC	0535	/* readonly sharable text segments	*/

#define	FILHDR	struct filehdr
#define	FILHSZ	sizeof(FILHDR)

	/* MIPS */
#define MIPSEBMAGIC	0x0160	/* big endian target and host */
#define MIPSELMAGIC     0x0162  /* little endian target and host */
#define SMIPSEBMAGIC	0x6001	/* big endian target, little endian host */
#define SMIPSELMAGIC    0x6201  /* little endian target, big endian host */

/*********************************************aouthdr.h********************/

/*
 * static char ID_aouth[] = "@(#)aouthdr.h	2.1	";
 */

typedef	struct aouthdr {
	short	magic;		/* see magic.h				*/
	short	vstamp;		/* version stamp			*/
	long	tsize;		/* text size in bytes, padded to FW
				   bdry					*/
	long	dsize;		/* initialized data "  "		*/
	long	bsize;		/* uninitialized data "   "		*/
	long	entry;		/* entry pt.				*/
	long	text_start;	/* base of text used for this file	*/
	long	data_start;	/* base of data used for this file	*/
	long    bss_start;      /* base of bss used for this file       */
	long    gprmask;        /* general purpose register mask        */
	long    cprmask[4];     /* co-processor register masks          */
	long    gp_value;       /* the gp value used for this object    */
} AOUTHDR;
#define AOUTHSZ sizeof(AOUTHDR)

/*********************************************snnhdr.h*********************/

/*	@(#)scnhdr.h	2.1	 */
struct scnhdr {
	char		s_name[8];	/* section name */
	long		s_paddr;	/* physical address */
	long		s_vaddr;	/* virtual address */
	long		s_size;		/* section size */
	long		s_scnptr;	/* file ptr to raw data for section */
	long		s_relptr;	/* file ptr to relocation */
	long		s_lnnoptr;	/* file ptr to line numbers */
	unsigned short	s_nreloc;	/* number of relocation entries */
	unsigned short	s_nlnno;	/* number of line number entries */
	long		s_flags;	/* flags */
	};

#define	SCNHDR	struct scnhdr
#define	SCNHSZ	sizeof(SCNHDR)
#define SCNROUND ((long)16)

/*
 * Define constants for names of "special" sections
 */

#define _TEXT ".text"
#define _INIT ".init"
#define _RDATA ".rdata"
#define _DATA ".data"
#define _LIT8 ".lit8"
#define _LIT4 ".lit4"
#define _SDATA ".sdata"
#define _BSS  ".bss"
#define _SBSS ".sbss"
#define _LIB  ".lib"
#define _UCODE ".ucode"




/*
 * The low 4 bits of s_flags is used as a section "type"
 */

#define STYP_REG	0x00		/* "regular" section:
						allocated, relocated, loaded */
#define STYP_DSECT	0x01		/* "dummy" section:
						not allocated, relocated,
						not loaded */
#define STYP_NOLOAD	0x02		/* "noload" section:
						allocated, relocated,
						 not loaded */
#define STYP_GROUP	0x04		/* "grouped" section:
						formed of input sections */
#define STYP_PAD	0x08		/* "padding" section:
 						not allocated, not relocated,
						 loaded */
#define STYP_COPY	0x10		/* "copy" section:
						for decision function used
						by field update;  not
						allocated, not relocated,
						loaded;  reloc & lineno
						entries processed normally */
#define	STYP_TEXT	0x20		/* section contains text only */
#define STYP_DATA	0x40		/* section contains data only */
#define STYP_BSS	0x80		/* section contains bss only */

/*
 *  In a minimal file or an update file, a new function
 *  (as compared with a replaced function) is indicated by S_NEWFCN
 */

#define S_NEWFCN  0x10

/*
 * In 3b Update Files (output of ogen), sections which appear in SHARED
 * segments of the Pfile will have the S_SHRSEG flag set by ogen, to inform
 * dufr that updating 1 copy of the proc. will update all process invocations.
 */

#define S_SHRSEG	0x20

/*********************************************reloc.h**********************/

/*	@(#)reloc.h	2.1	 */
struct reloc {
	long	r_vaddr;	/* (virtual) address of reference */
	unsigned	r_symndx:24,	/* index into symbol table */
			r_reserved:3,
			r_type:4,	/* relocation type */
			r_extern:1;	/* external flag */
	};

#define RELOC struct reloc
#define RELSZ sizeof(RELOC)

/* bitfields swapped for little endian conversions */

struct reloc_le {
	long	r_vaddr;	/* (virtual) address of reference */
	unsigned	r_extern:1,	/* external flag */
			r_type:4,	/* relocation type */
			r_reserved:3,
			r_symndx:24;	/* index into symbol table */
	};

#define RELOC_LE struct reloc_le
#define RELSZ_LE sizeof(RELOC_LE)

/*
 *   section numbers for relocation entries
 */

#define R_SN_TEXT 1
#define R_SN_INIT 7
#define R_SN_RDATA 2
#define R_SN_DATA 3
#define R_SN_SDATA 4
#define R_SN_SBSS 5
#define R_SN_BSS 6
#define R_SN_LIT8 8
#define R_SN_LIT4 9

/*
 *   relocation types for all products and generics
 */

/*
 * All generics
 *	reloc. already performed to symbol in the same section
 */
#define  R_ABS		0

/*
 * MIPS
 *	16 bit reference
 * 	32 bit reference
 *	26 bit jump reference
 *	hi 16 bits
 *	lo 16 bits
 *	global pointer offset
 *	literal
 */
#define	R_REFHALF	1
#define	R_REFWORD	2
#define	R_JMPADDR	3
#define R_REFHI		4
#define	R_REFLO		5
#define	R_GPREL		6
#define	R_LITERAL	7

/*
 * X86 generic
 *	8-bit offset reference in 8-bits
 *	8-bit offset reference in 16-bits
 *	12-bit segment reference
 *	auxiliary relocation entry
 */
#define	R_OFF8		07
#define R_OFF16		010
#define	R_SEG12		011
#define	R_AUX		013

/*
 * B16 and X86 generics
 *	16-bit direct reference
 *	16-bit "relative" reference
 *	16-bit "indirect" (TV) reference
 */
#define  R_DIR16	01
#define  R_REL16	02
#define  R_IND16	03

/*
 * 3B generic
 *	24-bit direct reference
 *	24-bit "relative" reference
 *	16-bit optimized "indirect" TV reference
 *	24-bit "indirect" TV reference
 *	32-bit "indirect" TV reference
 */
#define  R_DIR24	04
#define  R_REL24	05
#define  R_OPT16	014
#define  R_IND24	015
#define  R_IND32	016

/*
 * 3B and M32 generics
 *	32-bit direct reference
 */
#define  R_DIR32	06

/*
 * M32 generic
 *	32-bit direct reference with bytes swapped
 */
#define  R_DIR32S	012

/*
 * DEC Processors  VAX 11/780 and VAX 11/750
 *
 */

#define R_RELBYTE	017
#define R_RELWORD	020
#define R_RELLONG	021
#define R_PCRBYTE	022
#define R_PCRWORD	023
#define R_PCRLONG	024



/*
 * Motorola 68000
 *
 * ... uses R_RELBYTE, R_RELWORD, R_RELLONG, R_PCRBYTE and R_PCRWORD as for
 * DEC machines above.
 */

	/* Definition of a "TV" relocation type */

/*********************************************linenum.h********************/

/*	@(#)linenum.h	2.1	 */
/*  There is one line number entry for every
    "breakpointable" source line in a section.
    Line numbers are grouped on a per function
    basis; the first entry in a function grouping
    will have l_lnno = 0 and in place of physical
    address will be the symbol table index of
    the function name.
*/
struct lineno
{
	union
	{
		long	l_symndx ;	/* sym. table index of function name
						iff l_lnno == 0      */
		long	l_paddr ;	/* (physical) address of line number */
	}		l_addr ;
	unsigned short	l_lnno ;	/* line number */
} ;

#define	LINENO	struct lineno
#define	LINESZ	6	/* sizeof(LINENO) */

/*****************************************syms.h/storclass.h***********/

/*	@(#)syms.h	2.1	 */
/*		Storage Classes are defined in storclass.h  */
/*	@(#)storclass.h	2.1	 */
/*
 *   STORAGE CLASSES
 */

#define  C_EFCN          -1    /* physical end of function */
#define  C_NULL          0
#define  C_AUTO          1     /* automatic variable */
#define  C_EXT           2     /* external symbol */
#define  C_STAT          3     /* static */
#define  C_REG           4     /* register variable */
#define  C_EXTDEF        5     /* external definition */
#define  C_LABEL         6     /* label */
#define  C_ULABEL        7     /* undefined label */
#define  C_MOS           8     /* member of structure */
#define  C_ARG           9     /* function argument */
#define  C_STRTAG        10    /* structure tag */
#define  C_MOU           11    /* member of union */
#define  C_UNTAG         12    /* union tag */
#define  C_TPDEF         13    /* type definition */
#define  C_USTATIC       14    /* undefined static */
#define  C_ENTAG         15    /* enumeration tag */
#define  C_MOE           16    /* member of enumeration */
#define  C_REGPARM       17    /* register parameter */
#define  C_FIELD         18    /* bit field */
#define  C_BLOCK         100   /* ".bb" or ".eb" */
#define  C_FCN           101   /* ".bf" or ".ef" */
#define  C_EOS           102   /* end of structure */
#define  C_FILE          103   /* file name */

        /*
         * The following storage class is a "dummy" used only by STS
         * for line number entries reformatted as symbol table entries
         */

#define  C_LINE          104
#define  C_ALIAS         105   /* duplicate tag */
#define  C_HIDDEN        106   /* special storage class for external */
                               /* symbols in dmert public libraries  */

/*		Number of characters in a symbol name */
#define  SYMNMLEN	8
/*		Number of characters in a file name */
#define  FILNMLEN	14
/*		Number of array dimensions in auxiliary entry */
#define  DIMNUM		4


struct syment
{
	union
	{
		char		_n_name[SYMNMLEN];	/* old COFF version */
		struct
		{
			long	_n_zeroes;	/* new == 0 */
			long	_n_offset;	/* offset into string table */
		} _n_n;
		char		*_n_nptr[2];	/* allows for overlaying */
	} _n;
	long			n_value;	/* value of symbol */
	short			n_scnum;	/* section number */
	unsigned short		n_type;		/* type and derived type */
	char			n_sclass;	/* storage class */
	char			n_numaux;	/* number of aux. entries */
};

#define n_name		_n._n_name
#define n_nptr		_n._n_nptr[1]
#define n_zeroes	_n._n_n._n_zeroes
#define n_offset	_n._n_n._n_offset

/*
   Relocatable symbols have a section number of the
   section in which they are defined.  Otherwise, section
   numbers have the following meanings:
*/
        /* undefined symbol */
#define  N_UNDEF	0
        /* value of symbol is absolute */
#define  N_ABSLT	-1
        /* special debugging symbol -- value of symbol is meaningless */
#define  N_DEBUG	-2
	/* indicates symbol needs transfer vector (preload) */
#define  N_TV		(unsigned short)-3

	/* indicates symbol needs transfer vector (postload) */

#define  P_TV		(unsigned short)-4

/*
   The fundamental type of a symbol packed into the low
   4 bits of the word.
*/

#define  _EF	".ef"

#define  T_NULL     0
#define  T_ARG      1          /* function argument (only used by compiler) */
#define  T_CHAR     2          /* character */
#define  T_SHORT    3          /* short integer */
#define  T_INT      4          /* integer */
#define  T_LONG     5          /* long integer */
#define  T_FLOAT    6          /* floating point */
#define  T_DOUBLE   7          /* double word */
#define  T_STRUCT   8          /* structure  */
#define  T_UNION    9          /* union  */
#define  T_ENUM     10         /* enumeration  */
#define  T_MOE      11         /* member of enumeration */
#define  T_UCHAR    12         /* unsigned character */
#define  T_USHORT   13         /* unsigned short */
#define  T_UINT     14         /* unsigned integer */
#define  T_ULONG    15         /* unsigned long */

/*
 * derived types are:
 */

#define  DT_NON      0          /* no derived type */
#define  DT_PTR      1          /* pointer */
#define  DT_FCN      2          /* function */
#define  DT_ARY      3          /* array */

/*
 *   type packing constants
 */

#define  N_BTMASK     017
#define  N_TMASK      060
#define  N_TMASK1     0300
#define  N_TMASK2     0360
#define  N_BTSHFT     4
#define  N_TSHIFT     2

/*
 *   MACROS
 */

	/*   Basic Type of  x   */

#define  BTYPE(x)  ((x) & N_BTMASK)

	/*   Is  x  a  pointer ?   */

#define  ISPTR(x)  (((x) & N_TMASK) == (DT_PTR << N_BTSHFT))

	/*   Is  x  a  function ?  */

#define  ISFCN(x)  (((x) & N_TMASK) == (DT_FCN << N_BTSHFT))

	/*   Is  x  an  array ?   */

#define  ISARY(x)  (((x) & N_TMASK) == (DT_ARY << N_BTSHFT))

	/* Is x a structure, union, or enumeration TAG? */

#define ISTAG(x)  ((x)==C_STRTAG || (x)==C_UNTAG || (x)==C_ENTAG)

#define  INCREF(x) ((((x)&~N_BTMASK)<<N_TSHIFT)|(DT_PTR<<N_BTSHFT)|(x&N_BTMASK))

#define  DECREF(x) ((((x)>>N_TSHIFT)&~N_BTMASK)|((x)&N_BTMASK))

/*
 *	AUXILIARY ENTRY FORMAT
 */

union auxent
{
	struct
	{
		long		x_tagndx;	/* str, un, or enum tag indx */
		union
		{
			struct
			{
				unsigned short	x_lnno;	/* declaration line number */
				unsigned short	x_size;	/* str, union, array size */
			} x_lnsz;
			long	x_fsize;	/* size of function */
		} x_misc;
		union
		{
			struct			/* if ISFCN, tag, or .bb */
			{
				long	x_lnnoptr;	/* ptr to fcn line # */
				long	x_endndx;	/* entry ndx past block end */
			} 	x_fcn;
			struct			/* if ISARY, up to 4 dimen. */
			{
				unsigned short	x_dimen[DIMNUM];
			} 	x_ary;
		}		x_fcnary;
		unsigned short  x_tvndx;		/* tv index */
	} 	x_sym;
	struct
	{
		char	x_fname[FILNMLEN];
	} 	x_file;
        struct
        {
                long    x_scnlen;          /* section length */
                unsigned short  x_nreloc;  /* number of relocation entries */
                unsigned short  x_nlinno;  /* number of line numbers */
        }       x_scn;

	struct
	{
 		long		x_tvfill;	/* tv fill value */
		unsigned short	x_tvlen;	/* length of .tv */
		unsigned short	x_tvran[2];	/* tv range */
	}	x_tv;	/* info about .tv section (in auxent of symbol .tv)) */
};

#define	SYMENT	struct syment
#define	SYMESZ	18	/* sizeof(SYMENT) */

#define	AUXENT	union auxent
#define	AUXESZ	18	/* sizeof(AUXENT) */

/*	Defines for "special" symbols   */

#define _ETEXT	"etext"
#define _EDATA	"edata"
#define _END	"end"

#ifdef _START
#undef _START
#define _START	"_start"
#endif	/* _START */

#define _TVORIG	"_tvorig"
#define _TORIGIN	"_torigin"
#define _DORIGIN	"_dorigin"

#define _SORIGIN	"_sorigin"

/*
 * Simple values for n_type.  VxWorks symbol defines.
 */

#define	N_UNDF	0x0		/* undefined */
#define	N_ABS	0x2		/* absolute */
#define	N_TEXT	0x4		/* text */
#define	N_DATA	0x6		/* data */
#define	N_BSS	0x8		/* bss */
#define	N_COMM	0x12		/* common (internal to ld) */
#define	N_FN	0x1f		/* file name symbol */

#define	N_EXT	01		/* external bit, or'ed in */
#define	N_TYPE	0x1e		/* mask for all the type bits */

#define MAX_SCNS        11
#define NO_SCNS         0

/*
 * Sdb entries have some of the N_STAB bits set.
 * These are given in <stab.h>
 */

#define	N_STAB	0xe0		/* if any of these bits set, a SDB entry */

/******************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* __INCecoffh */
