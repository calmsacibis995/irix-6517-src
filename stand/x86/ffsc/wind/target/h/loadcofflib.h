/* loadCoffLib.h - coff object module header */

/* Copyright 1984-1990 Wind River Systems, Inc. */

/*
modification history
--------------------
01i,11mar94,pme  added bootCoffInit() prototype (SPR# 2947).
01h,09jul93,pme  added Am29K family support.
01g,22sep92,rrr  added support for c++
01f,18sep92,jcf  removed nested comment.
01e,11sep92,ajm  changed OFFSET_MASK to OFFSET24_MASK due to b.out redefines,
		 fixed #ifndef define
01d,29jul92,rrr  removed moduleLib.h if no CPU is defined.
01c,28jul92,jmm  removed CPU_FAMILY = I960 ifdefs
                 changed return of ldCoffModAtSym to MODULE_ID
01b,17jul92,rrr  added extern to functions.
01a,21may92,wmd  received from Intel.

*/

#ifndef __INCloadCoffLibh
#define __INCloadCoffLibh

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CPU
#include "symlib.h"
#include "modulelib.h"
#endif

/******************************************************************************
 *
 * This file describes a COFF object file.
 *
 ******************************************************************************/

#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 1			/* tell gcc960 not to optimize alignments */
#endif /* CPU_FAMILY==I960 */

/******************************* FILE HEADER *******************************/

struct filehdr {
	unsigned short	f_magic;	/* magic number			*/
	unsigned short	f_nscns;	/* number of sections		*/
	long		f_timdat;	/* time & date stamp		*/
	long		f_symptr;	/* file pointer to symtab	*/
	long		f_nsyms;	/* number of symtab entries	*/
	unsigned short	f_opthdr;	/* sizeof(optional hdr)		*/
	unsigned short	f_flags;	/* flags			*/
};


/* Bits for f_flags:
 *	F_RELFLG	relocation info stripped from file
 *	F_EXEC		file is executable (no unresolved external references)
 *	F_LNNO		line numbers stripped from file
 *	F_LSYMS		local symbols stripped from file
 *	F_AR32WR	file has byte ordering of an AR32WR machine (e.g. vax)
 *	F_PIC		file contains position-independent code
 *	F_PID		file contains position-independent data
 *	F_LINKPID	file is suitable for linking w/pos-indep code or data
 */
#define F_RELFLG	0x0001
#define F_EXEC		0x0002
#define F_LNNO		0x0004
#define F_LSYMS		0x0008
#define F_AR32WR	0x0010
#define F_PIC		0x0040
#define F_PID		0x0080
#define F_LINKPID	0x0100

#define F_CCINFO	0x0800
	/* FOR GNU/960 VERSION ONLY!
	 *
	 * Set only if cc_info data (for 2-pass compiler optimization) is
	 * appended to the end of the object file.
	 *
	 * Since cc_info data is removed when a file is stripped, we can assume
	 * that its presence implies the presence of a symbol table in the file,
	 * with the cc_info block immediately following.
	 *
	 * A COFF symbol table does not necessarily require a string table.
	 * When cc_info is present we separate it from the symbol info with a
	 * delimiter word of 0xfffffff, which simplifies testing for presence
	 * of a string table:  if the word following the symbol table is not
	 * 0xffffffff, it is the string table length; the string table must be
	 * skipped over before the delimiter (and cc_info) can be read.
	 *
	 * The format/meaning of the cc_data block are known only to the
	 * compiler (and, to a lesser extent, the linker) except for the first
	 * 4 bytes, which contain the length of the block (including those 4
	 * bytes).  This length is stored in a machine-independent format, and
	 * can be retrieved with the CI_U32_FM_BUF macro in cc_info.h .
	 */


/*	Intel 80960 (I960) processor flags.
 *	F_I960TYPE == mask for processor type field.
 */
#define	F_I960TYPE	0xf000
#define	F_I960CORE	0x1000
#define	F_I960KB	0x2000
#define	F_I960SB	0x2000
#define	F_I960MC	0x3000
#define	F_I960XA	0x4000
#define	F_I960CA	0x5000
#define	F_I960KA	0x6000
#define	F_I960SA	0x6000

/*
 * i80960 Magic Numbers
 */
#define I960ROMAGIC	0x160	/* read-only text segments	*/
#define I960RWMAGIC	0x161	/* read-write text segments	*/

#define I960BADMAG(x) (((x).f_magic!=I960ROMAGIC) && ((x).f_magic!=I960RWMAGIC))

/*
 * Am29K Magic Numbers
 */
#define AM29KBIGMAGIC		0572	/* Am29K big endian */
#define AM29KLITTLEMAGIC	0573	/* Am29K little endian */

#define AM29KBADMAG(x)   (((x).f_magic!=AM29KBIGMAGIC) && \
			  ((x).f_magic!=AM29KLITTLEMAGIC))

#define	FILHDR	struct filehdr
#define	FILHSZ	sizeof(FILHDR)

#undef  N_TXTOFF                /* undefined for a.out overlap */
#define N_TXTOFF(f) \
    ((f).f_opthdr == 0) ? \
    (FILHSZ + (f).f_nscns * SCNHSZ ) : \
    (FILHSZ + AOUTSZ + (f).f_nscns * SCNHSZ )

/************************* AOUT "OPTIONAL HEADER" *************************/

#define OMAGIC	0407
#define NMAGIC	0410

typedef	struct aouthdr {
	short		magic;	/* type of file				*/
	short		vstamp;	/* version stamp			*/
	unsigned long	tsize;	/* text size in bytes, padded to FW bdry*/
	unsigned long	dsize;	/* initialized data "  "		*/
	unsigned long	bsize;	/* uninitialized data "   "		*/
	unsigned long	entry;	/* entry pt.				*/
	unsigned long	text_start;	/* base of text used for this file */
	unsigned long	data_start;	/* base of data used for this file */
#if (CPU_FAMILY == I960)
	unsigned long	tagentries;	/* number of tag entries to follow */
					/*	ALWAYS 0 FOR 960           */
#endif /* (CPU_FAMILY == I960) */
} AOUTHDR;

#define AOUTSZ (sizeof(AOUTHDR))


/****************************** SECTION HEADER ******************************/

struct scnhdr {
	char		s_name[8];	/* section name			*/
	long		s_paddr;	/* physical address, aliased s_nlib */
	long		s_vaddr;	/* virtual address		*/
	long		s_size;		/* section size			*/
	long		s_scnptr;	/* file ptr to raw data for section */
	long		s_relptr;	/* file ptr to relocation	*/
	long		s_lnnoptr;	/* file ptr to line numbers	*/
	unsigned short	s_nreloc;	/* number of relocation entries	*/
	unsigned short	s_nlnno;	/* number of line number entries*/
	long		s_flags;	/* flags			*/
#if (CPU_FAMILY == I960)
	unsigned long	s_align;	/* section alignment		*/
#endif /* (CPU_FAMILY == I960) */
};

/*
 * names of "special" sections
 */
#define _TEXT	".text"
#define _DATA	".data"
#define _BSS	".bss"

/*
 * s_flags "type"
 */
				/* TYPE    ALLOCATED? RELOCATED? LOADED? */
				/* ----    ---------- ---------- ------- */
#define STYP_REG	0x0000	/* regular    yes        yes       yes   */
#define STYP_DSECT	0x0001	/* dummy      no         yes       no    */
#define STYP_NOLOAD	0x0002	/* noload     yes        yes       no    */
#define STYP_GROUP	0x0004	/* grouped  <formed from input sections> */
#define STYP_PAD	0x0008	/* padding    no         no        yes   */
#define STYP_COPY	0x0010	/* copy       no         no        yes   */
#define STYP_INFO	0x0200	/* comment    no         no        no    */

#define STYP_TEXT	0x0020	/* section contains text */
#define STYP_DATA	0x0040	/* section contains data */
#define STYP_BSS	0x0080	/* section contains bss  */
#define STYP_LIT	0x8000	/* section contains literal */

/*
 * additional section type for global registers which will 
 * be relocatable for the Am29K.
 */
#define STYP_BSSREG     0x1200  /* Global register area (like STYP_INFO) */
#define STYP_ENVIR      0x2200  /* Environment (like STYP_INFO) */
#define STYP_ABS        0x4000  /* Absolute (allocated, not reloc, loaded) */

#define	SCNHDR	struct scnhdr
#define	SCNHSZ	sizeof(SCNHDR)


/******************************* LINE NUMBERS *******************************/

/* 1 line number entry for every "breakpointable" source line in a section.
 * Line numbers are grouped on a per function basis; first entry in a function
 * grouping will have l_lnno = 0 and in place of physical address will be the
 * symbol table index of the function name.
 */
struct lineno{
	union {
		long l_symndx;	/* function name symbol index, iff l_lnno == 0*/
		long l_paddr;	/* (physical) address of line number	*/
	} l_addr;
	unsigned short	l_lnno;	/* line number		*/
	char padding[2];	/* force alignment	*/
};

#define	LINENO	struct lineno
#define	LINESZ	sizeof(LINENO)


/********************************** SYMBOLS **********************************/

#define SYMNMLEN	8	/* # characters in a symbol name	*/
#define FILNMLEN	14	/* # characters in a file name		*/
#define DIMNUM		4	/* # array dimensions in auxiliary entry */

#if (CPU_FAMILY == I960)
struct syment {
	union {
		char	_n_name[SYMNMLEN];	/* old COFF version	*/
		struct {
			long	_n_zeroes;	/* new == 0		*/
			long	_n_offset;	/* offset into string table */
		} _n_n;
		char	*_n_nptr[2];	/* allows for overlaying	*/
	} _n;
	long		n_value;	/* value of symbol		*/
	short		n_scnum;	/* section number		*/
	unsigned short	n_flags;	/* copy of flags from filhdr	*/
	unsigned long	n_type;		/* type and derived type	*/
	char		n_sclass;	/* storage class		*/
	char		n_numaux;	/* number of aux. entries	*/
	char		pad2[2];	/* force alignment		*/
};

#define	SYMENT	struct syment
#define	SYMESZ	sizeof(SYMENT)
#else /* (CPU_FAMILY == I960) */

struct syment {
	union {
		char	_n_name[SYMNMLEN];	/* old COFF version	*/
		struct {
			long	_n_zeroes;	/* new == 0		*/
			long	_n_offset;	/* offset into string table */
		} _n_n;
		char	*_n_nptr[2];	/* allows for overlaying	*/
	} _n;
	long		n_value;	/* value of symbol		*/
	short		n_scnum;	/* section number		*/
	unsigned short	n_type;		/* type and derived type	*/
	char		n_sclass;	/* storage class		*/
	char		n_numaux;	/* number of aux. entries	*/
};

#define	SYMENT	struct syment
#define	SYMESZ	 sizeof(SYMENT) /* don't use sizeof since some compilers */
				/* may add padding in the structure      */
#endif /* (CPU_FAMILY == I960) */

#define n_name		_n._n_name
#define n_ptr		_n._n_nptr[1]
#define n_zeroes	_n._n_n._n_zeroes
#define n_offset	_n._n_n._n_offset


/*
 * Relocatable symbols have number of the section in which they are defined,
 * or one of the following:
 */
#define N_UNDEF ((short)0)  /* undefined symbol */
#define N_ABS   ((short)-1) /* value of symbol is absolute */
#define N_DEBUG ((short)-2) /* debugging symbol -- value is meaningless */
#define N_TV    ((short)-3) /* indicates symbol needs preload transfer vector */
#define P_TV    ((short)-4) /* indicates symbol needs postload transfer vector*/

/*
 * Symbol storage classes
 */
#define C_EFCN		-1	/* physical end of function	*/
#define C_NULL		0
#define C_AUTO		1	/* automatic variable		*/
#define C_EXT		2	/* external symbol		*/
#define C_STAT		3	/* static			*/
#define C_REG		4	/* register variable		*/
#define C_EXTDEF	5	/* external definition		*/
#define C_LABEL		6	/* label			*/
#define C_ULABEL	7	/* undefined label		*/
#define C_MOS		8	/* member of structure		*/
#define C_ARG		9	/* function argument		*/
#define C_STRTAG	10	/* structure tag		*/
#define C_MOU		11	/* member of union		*/
#define C_UNTAG		12	/* union tag			*/
#define C_TPDEF		13	/* type definition		*/
#define C_USTATIC	14	/* undefined static		*/
#define C_ENTAG		15	/* enumeration tag		*/
#define C_MOE		16	/* member of enumeration	*/
#define C_REGPARM	17	/* register parameter		*/
#define C_FIELD		18	/* bit field			*/
#if (CPU_FAMILY == AM29XXX)
#define C_GLBLREG       19	/* global register		*/
#define C_EXTREG        20	/* external global register	*/
#define C_DEFREG        21	/* ext. def. of global register	*/
#else /* (CPU_FAMILY == AM29XXX) */
#define C_AUTOARG	19	/* auto argument		*/
#endif /* (CPU_FAMILY == AM29XXX) */
#define C_BLOCK		100	/* ".bb" or ".eb"		*/
#define C_FCN		101	/* ".bf" or ".ef"		*/
#define C_EOS		102	/* end of structure		*/
#define C_FILE		103	/* file name			*/
#define C_LINE		104	/* line # reformatted as symbol table entry */
#define C_ALIAS	 	105	/* duplicate tag		*/
#define C_HIDDEN	106	/* ext symbol in dmert public lib */
#define C_SCALL		107	/* Procedure reachable via system call	*/
#define C_LEAFEXT	108	/* Global leaf procedure, "call" via BAL */
#define C_LEAFSTAT	113	/* Static leaf procedure, "call" via BAL */


/*
 * Type of a symbol, in low 5 bits of n_type
 */
#define T_NULL		0
#define T_VOID		1	/* function argument (only used by compiler) */
#define T_CHAR		2	/* character		*/
#define T_SHORT		3	/* short integer	*/
#define T_INT		4	/* integer		*/
#define T_LONG		5	/* long integer		*/
#define T_FLOAT		6	/* floating point	*/
#define T_DOUBLE	7	/* double word		*/
#define T_STRUCT	8	/* structure 		*/
#define T_UNION		9	/* union 		*/
#define T_ENUM		10	/* enumeration 		*/
#define T_MOE		11	/* member of enumeration*/
#define T_UCHAR		12	/* unsigned character	*/
#define T_USHORT	13	/* unsigned short	*/
#define T_UINT		14	/* unsigned integer	*/
#define T_ULONG		15	/* unsigned long	*/
#define T_LNGDBL	16	/* long double		*/

/*
 * derived types, in n_type
 */
#define DT_NON		0	/* no derived type	*/
#define DT_PTR		1	/* pointer		*/
#define DT_FCN		2	/* function		*/
#define DT_ARY		3	/* array		*/

#if (CPU_FAMILY == AM29XXX)
#define N_BTMASK        0xf
#define N_TMASK         0x30
#define N_BTSHFT        4
#define N_TSHIFT        2
#else /* (CPU_FAMILY == AM29XXX) */
#define N_BTMASK	0x1f
#define N_TMASK		0x60
#define N_BTSHFT	5
#define N_TSHIFT	2
#endif /* (CPU_FAMILY == AM29XXX) */

#define BTYPE(x)	((x) & N_BTMASK)

#define ISPTR(x)	(((x) & N_TMASK) == (DT_PTR << N_BTSHFT))
#define ISFCN(x)	(((x) & N_TMASK) == (DT_FCN << N_BTSHFT))
#define ISARY(x)	(((x) & N_TMASK) == (DT_ARY << N_BTSHFT))
#define ISTAG(x)	((x)==C_STRTAG||(x)==C_UNTAG||(x)==C_ENTAG)


#define DECREF(x) ((((x)>>N_TSHIFT)&~N_BTMASK)|((x)&N_BTMASK))

union auxent {
	struct {
		long x_tagndx;	/* str, un, or enum tag indx */
		union {
			struct {
			    unsigned short x_lnno; /* declaration line number */
			    unsigned short x_size; /* str/union/array size */
			} x_lnsz;
			long x_fsize;	/* size of function */
		} x_misc;
		union {
			struct {		/* if ISFCN, tag, or .bb */
			    long x_lnnoptr;	/* ptr to fcn line # */
			    long x_endndx;	/* entry ndx past block end */
			} x_fcn;
			struct {		/* if ISARY, up to 4 dimen. */
			    unsigned short x_dimen[DIMNUM];
			} x_ary;
		} x_fcnary;
		unsigned short x_tvndx;		/* tv index */
	} x_sym;

	union {
		char x_fname[FILNMLEN];
		struct {
			long x_zeroes;
			long x_offset;
		} x_n;
	} x_file;

	struct {
		long x_scnlen;			/* section length */
		unsigned short x_nreloc;	/* # relocation entries */
		unsigned short x_nlinno;	/* # line numbers */
	} x_scn;

	struct {
		long		x_tvfill;	/* tv fill value */
		unsigned short	x_tvlen;	/* length of .tv */
		unsigned short	x_tvran[2];	/* tv range */
	} x_tv;		/* info about .tv section (in auxent of symbol .tv)) */

#if (CPU_FAMILY == I960)
	/******************************************
	 *  I960-specific *2nd* aux. entry formats
	 ******************************************/
	struct {
		long		x_stindx;	/* sys. table entry */
	} x_sc;					/* system call entry */

	struct {
		unsigned long	x_balntry;	/* BAL entry point */
	} x_bal;	                        /* BAL-callable function */

	struct {
		unsigned long	x_timestamp;	/* time stamp */
		char 	x_idstring[20];	        /* producer identity string */
	} x_ident;				/* Producer ident info */

	char a[sizeof(struct syment)];	/* force auxent/syment sizes to match */
#endif /* (CPU_FAMILY == I960) */
};

#define	AUXENT	union auxent
#define	AUXESZ	sizeof(AUXENT)

#define _ETEXT	"_etext"

/********************** RELOCATION DIRECTIVES **********************/

struct reloc {
	long r_vaddr;		/* Virtual address of reference */
	long r_symndx;		/* Index into symbol table	*/
	unsigned short r_type;	/* Relocation type		*/
#if (CPU_FAMILY == I960)
	char pad[2];		/* Unused			*/
#endif /* (CPU_FAMILY == I960) */
};

/* Only values of r_type GNU/960 cares about */
#define R_RELLONG	17	/* Direct 32-bit relocation		*/
#define R_IPRMED	25	/* 24-bit ip-relative relocation	*/
#define R_OPTCALL	27	/* 32-bit optimizable call (leafproc/sysproc) */
#define R_OPTCALLX	28	/* 64-bit optimizable call (leafproc/sysproc) */

/* Only values of r_type GNU/29k cares about */
#define R_ABS           0       /* reference is absolute */

#define R_IREL          030     /* instruction relative (jmp/call) */
#define R_IABS          031     /* instruction absolute (jmp/call) */
#define R_ILOHALF       032     /* instruction low half  (const)  */
#define R_IHIHALF       033     /* instruction high half (consth) part 1 */
#define R_IHCONST       034     /* instruction high half (consth) part 2 */
				/* constant offset of R_IHIHALF relocation */
#define R_BYTE          035     /* relocatable byte value */
#define R_HWORD         036     /* relocatable halfword value */
#define R_WORD          037     /* relocatable word value */

#define R_IGLBLRC       040     /* instruction global register RC */
#define R_IGLBLRA       041     /* instruction global register RA */
#define R_IGLBLRB       042     /* instruction global register RB */


#define RELOC struct reloc
#define RELSZ sizeof(RELOC)

#define SYMS_ABSOLUTE	2		/* to flag absolute symbols */
#define	BAL_ENTRY	0x10		/* I80960 BAL entry point to function */
#define OFFSET24_MASK	0x00ffffff	/* mask for 24bit offsets */
#define BAL_OPCODE	0x0b000000	/* opcode for branch and link */
#define BALXG14_OPCODE  0x85f03000      /* opcode for balx absolute addr, g14 */
#define CALL_OPCODE	0x09000000	/* opcode for call instruction */
#define B_OPCODE	0x08000000	/* opcode for branch instruction */
#define CALLS_OPCODE    0x66000000      /* opcode for calls instruction */

/* 29K instructions */

#define ASSERT_OPCODE	0x50000000	/* opcode for assert except eq, neq */
#define ASEQ_OPCODE	0x70000000	/* opcode for assert eq */
#define ASNEQ_OPCODE	0x72000000	/* opcode for assert neq */
#define EMULATE_OPCODE	0xd7000000	/* opcode for emulate */

#define REG_MASK	0xff		/* register bits in opcode */
#define REG_A_OFFSET	8		/* offset for register A in opcode */
#define REG_B_OFFSET	0		/* offset for register B in opcode */
#define REG_C_OFFSET	16		/* offset for register C in opcode */

/* status codes */

#define S_loadLib_FILE_READ_ERROR               (M_loadCoffLib | 1)
#define S_loadLib_REALLOC_ERROR                 (M_loadCoffLib | 2)
#define S_loadLib_JMPADDR_ERROR                 (M_loadCoffLib | 3)
#define S_loadLib_NO_REFLO_PAIR                 (M_loadCoffLib | 4)
#define S_loadLib_GPREL_REFERENCE               (M_loadCoffLib | 5)
#define S_loadLib_UNRECOGNIZED_RELOCENTRY       (M_loadCoffLib | 6)
#define S_loadLib_REFHALF_OVFL                  (M_loadCoffLib | 7)
#define S_loadLib_FILE_ENDIAN_ERROR             (M_loadCoffLib | 8)
#define S_loadLib_UNEXPECTED_SYM_CLASS          (M_loadCoffLib | 9)
#define S_loadLib_UNRECOGNIZED_SYM_CLASS        (M_loadCoffLib | 10)
#define S_loadLib_HDR_READ                      (M_loadCoffLib | 11)
#define S_loadLib_OPTHDR_READ                   (M_loadCoffLib | 12)
#define S_loadLib_SCNHDR_READ                   (M_loadCoffLib | 13)
#define S_loadLib_READ_SECTIONS                 (M_loadCoffLib | 14)
#define S_loadLib_LOAD_SECTIONS                 (M_loadCoffLib | 15)
#define S_loadLib_RELOC_READ                    (M_loadCoffLib | 16)
#define S_loadLib_SYMHDR_READ                   (M_loadCoffLib | 17)
#define S_loadLib_EXTSTR_READ                   (M_loadCoffLib | 18)
#define S_loadLib_EXTSYM_READ                   (M_loadCoffLib | 19)


/* function declarations */

#ifdef CPU
#if defined(__STDC__) || defined(__cplusplus)
extern STATUS    loadCoffInit ();
extern STATUS    bootCoffInit ();
extern MODULE_ID ldCoffModAtSym (int fd, int symFlag, char **ppText,
				 char **ppData, char **ppBss, SYMTAB_ID symTbl);

#else   /* __STDC__ */
extern STATUS    loadCoffInit ();
extern STATUS    bootCoffInit ();
extern MODULE_ID ldCoffModAtSym ();

#endif  /* __STDC__ */
#endif


#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 0			/* turn off alignment requirement */
#endif /* CPU_FAMILY==I960 */

#ifdef __cplusplus
}
#endif

#endif /* __INCloadCoffLibh */
