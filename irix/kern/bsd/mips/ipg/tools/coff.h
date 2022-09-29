/*
 * "COFF.H" -- structure definitions for Common Object-File Format.
 * created:  6/24/87	J. Howe
 */

/*  File Header and related information */

struct filehdr
	{
    unsigned short f_magic;			/* magic number */
    unsigned short f_nscns;			/* number of sections */
    long		   f_timdat;		/* time & date stamp */
    long		   f_symptr;		/* file pointer to synbol table */
    long		   f_nsyms;			/* number of symtab entries */
    unsigned short f_opthdr;		/* size of optional header */
    unsigned short f_flags;			/* flags */
	};

#define FILHDR	struct filehdr
#define FILHSZ	sizeof (FILHDR)

/* Magic NUmbers */

#define SIPFBOMAGIC 0572		/* Am29000 (Byte 0 is MSB) */
#define SIPRBOMAGIC 0605		/* Am29000 (Byte 0 is LSB) */

/* File Header Flags to be used by Am29000 */

#define F_RELFLG	00001		/* reloc info stipped form file */
#define F_EXEC		00002		/* File is executable (no unres externs) */
#define F_LNNO		00004		/* line numbers stripped from file */
#define F_LSYMS		00010		/* local symbols stripped from file */
#define F_AR32WR	00400		/* File has 32 bits/word, least sig byte 1st */
#define F_AR32W		01000		/* File has 32 bits/word, most sig byte 1st */

/* Optional Header (a.out) */

typedef struct aouthdr
	{
    short magic;				/* magic number */
    short vstamp;				/* version stamp */
    long  tsize;				/* text size in bytes */
    long  dsize;				/* initialized data size */
    long  bsize;				/* uninitialized data size */
    long  entry;				/* entry point */
    long  text_start;			/* base of text used for this file */
    long  data_start;			/* base of data used for this file */
	}  AOUTHDR;

#define OPTHSZ		sizeof (AOUTHDR)
#define A_VSTAMP	1
#define A_MAGIC		0407

/* Section Header and related information */

struct scnhdr
	{
    char			s_name[8];	/* section name */
    long			s_paddr;	/* physical address */
    long			s_vaddr;	/* virtual address */
    long			s_size;		/* section size */
    long			s_scnptr;	/* file ptr to raw data for section */
    long			s_relptr;	/* file ptr to relocation data */
    long			s_lnnoptr;	/* file ptr to line numbers */
    unsigned short	s_nreloc;	/* number of relocation entries */
    unsigned short	s_nlnno;	/* number of line number entries */
    long			s_flags;	/* flags */
	};

#define SCNHDR  struct scnhdr
#define SCNHSZ  sizeof (SCNHDR)

/* Section types */

#define STYP_REG	0x0000		/* regular section -- allocated, relocated, */
								/* loaded */
#define STYP_DSECT	0x0001		/* Dummy section,not alloc,reloc,notloaded */
#define STYP_NOLOAD	0x0002		/* Noload section, alloc, reloc, not loaded */
#define STYP_GROUP	0x0004		/* grouped section fromed from input sects */
#define STYP_PAD	0x0008		/* padded section,not alloc, notreloc,loaded */
#define STYP_COPY	0x0010		/* Copy section, for a decision function used */

				/* in updating fields; not alloc, not reloc */
				/* loaded; reloc and line nums processed norm */

#define STYP_TEXT	0x0020		/* section contains executable text */
#define STYP_DATA	0x0040		/* section contains initialized data */
#define STYP_BSS	0x0080		/* section contains only uninit data */
#define STYP_INFO	0x0200		/* Comment sect,not alloc,not reloc,not load */
#define STYP_LIB	0x0800		/* for .lib section, treated like STYP_INFO */
#define STYP_LIT	0x8020		/* literal data - like STYP_TEXT */
#define STYP_ABS	0x4000		/* absolute,alloc,not reloc,loaded */
#define STYP_BSSREG	0x1200		/* section contains only uninitialized    */
#define STYP_ENVIR	0x2200		/* environment like STYP_INFO */

/*  Relocation information and related definitions */

struct reloc
	{
    long			r_vaddr;	/* virtual address of reference */
    long			r_symndx;	/* index into symbol table */
    unsigned short	r_type;		/* relocations type */
	};

#define RELOC	struct reloc
#define RELSZ	10				/* sizeof (RELOC) */

/* Relocation Types for the Am29000 */

#define R_ABS		0		/* reference is absolute */
#define R_IREL		030		/* instruction relative (jmp/call) */
#define R_IABS		031		/* instruction absolute (jmp/call) */
#define R_ILOHALF	032		/* instruxtion low half (const) */
#define R_IHIHALF	033		/* instruction high half (consth) part 1 */
#define R_IHCONST	034		/* instruction high half (consth) part 2 */
#define R_BYTE		035		/* relocatable byte value */
#define R_HWORD		036		/* relocatable halfword value */
#define R_WORD		037		/* relocatable word value */
#define R_IGLBLRC	040		/* instruction global reg RC */
#define R_IGLBLRA	041		/* instruction global reg RA */
#define R_IGLBLRB	042		/* instruction global reg RB */

/* Line number entry declaration and related definitions */

struct lineno
	{
    union
    	{
		long l_symndx;		/* sym table index of function name */
		long l_paddr;		/* physical address of line number */
    	} l_addr;
    unsigned short l_lnno;	/* line number */
	};

#define ll_symndx	l_addr.l_symndx
#define ll_paddr	l_addr.l_paddr
#define LINENO		struct lineno
#define LINESZ		6		/* sizeof (LINENO) */

/* symbol table entry declaration and related definitions */

#define SYMNMLEN	8		/* number of characters in symbol name */

struct syment
	{
    union
    	{
		char _n_name[SYMNMLEN];	/* name */
		struct
			{
	    	long _n_zeroes;		/* symbol name */
		    long _n_offset;		/* offset into string table */
			} _n_n;
		char *_n_nptr[2];		/* allows for overlaying */
    	} _n;
    long			n_value;	/* value of symbol */
    short			n_scnum;	/* section number */
    unsigned short	n_type;		/* type and derived type */
    char			n_sclass;	/* storage class */
    char			n_numaux;	/* number of aux entries */
	};

#define n_name	 _n._n_name
#define n_nptr	 _n._n_ptr[1]
#define n_zeroes _n._n_n._n_zeroes
#define n_offset _n._n_n._n_offset

#define SYMENT	struct syment
#define SYMESZ	18

/* storage class definitions */

#define C_EFCN		-1		/* physical end of function */
#define C_NULL		 0		/* - */
#define C_AUTO		 1		/* automatic varibale */
#define C_EXT		 2		/* external symbol */
#define C_STAT		 3		/* static */
#define C_REG		 4		/* local register variable */
#define C_EXTDEF	 5		/* external definition */
#define C_LABEL		 6		/* label */
#define C_ULABEL	 7		/* undefined lablel */
#define C_MOS		 8		/* member of structure */
#define C_ARG		 9		/* function argument */
#define C_STRTAG	10		/* structure tag */
#define C_MOU		11		/* member of union */
#define C_UNTAG		12		/* union tag */
#define C_TPDEF		13		/* type definition */
#define C_USTATIC	14		/* uninitialized static */
#define C_ENTAG		15		/* enumeration tag */
#define C_MOE		16		/* member of enumeration */
#define C_REGPARM	17		/* register parameter */
#define C_FIELD		18		/* bit field */
#define C_GLBLREG	19		/* global register */
#define C_EXTREG	20		/* external global register */
#define C_DEFREG	21		/* ext. def. of gloal register */
#define C_BLOCK	   100		/* beginning and end of bl;ock */
#define C_FCN	   101		/* beginning and end of function */
#define C_EOS	   102		/* end of structure */
#define C_FILE	   103		/* file name */
#define C_LINE	   104		/* used only by utility programs */
#define C_ALIAS	   105		/* duplicated tag */
#define C_HIDDEN   106		/* like static; use to avoid aname  conflicts */

/* Special section number definitions used in symbol entries */

#define N_DEBUG		-2		/* special symbolic debugging symbol */
#define N_ABS		-1		/* absolute symbol */
#define N_UNDEF		 0		/* undefined external symbol */
#define N_SCNUM		1-65535	/* section number where symbol defined */

/* Fundamental symbol types */

#define T_NULL		 0		/* type not assigned */
#define T_VOID		 1		/* void */
#define T_CHAR		 2		/* character */
#define T_SHORT		 3		/* short integer */
#define T_INT		 4		/* integer */
#define T_LONG		 5		/* long integer */
#define T_FLOAT		 6		/* floatinf pt */
#define T_DOUBLE	 7		/* double word */
#define T_UCHAR		12		/* unsigned char */
#define T_USHORT	13		/* unsigned short */
#define T_UINT		14		/* unsigned integer */
#define T_ULONG		15		/* unsigned long */

/* Derived symbol types */

#define DT_NON		0		/* no derived type */
#define DT_PTR		1		/* pointer */
#define DT_FCN		2		/* function */
#define DT_ARY		3		/* array */

/* Auxiliary symbol table entry declarations and related defs */

#define FILNMLEN   14		/* number of chars in file name */
#define DIMNUM		4		/* number of array dimensions in aux entry */

union auxent
	{
    struct
    	{
		long x_tagndx;					/* str,un or enum tag index */
		union
			{
	    	struct
	    		{
				unsigned short x_lnno;	/* declaration line no */
				unsigned short x_size;	/* str,union,enum size */
	    		} x_lnsz;
	    	long x_fsize;				/* size of function */
			} x_misc;
		union
			{
	    	struct
			    {
				long x_lnnoptr;			/* ptr to fcn line no. */
				long x_endndx;			/* entry ndx past blck end */
			    } x_fcn;
	    	struct						/* if ISARY, up to 4 dimen */
			    {
				unsigned short x_dimen[DIMNUM];
			    } x_ary;
			} x_fcnary;
		unsigned short x_tvndx;			/* tv index */
    	} x_sym;
    struct
    	{
		char x_fname[FILNMLEN];
	    } x_file;
    struct
    	{
		long		   x_scnlen;		/* section length */
		unsigned short x_nreloc;		/* num of reloc entries */
		unsigned short x_nlnno;			/* num of line nums */
    	} x_scn;
    struct
    	{
		long		   x_tvfill;		/* tv fill value */
		unsigned short x_tvlen;			/* length of tv */
		unsigned short x_tvrna[2];		/* tv range */
    	} x_tv;					/* info about tv sect (in auxent of sym tv */
	};

#define xx_tagndx	x_sym.x_tagndx
#define xx_lnno		x_sym.x_misc.x_lnsz.x_lnno
#define xx_size		x_sym.x_misc.x_lnsz.x_size
#define xx_fsize	x_sym.x_misc.x_fsize
#define xx_lnnoptr	x_sym.x_fcnary.x_fcn.x_lnnoptr
#define xx_endndx	x_sym.x_fcnary.x_fcn.x_endndx
#define xx_dimen	x_sym.x_fcnary.x_ary.x_dimen
#define xx_scnlen	x_scn.x_scnlen
#define xx_nreloc	x_scn.x_nreloc
#define xx_nlinno	x_scn.x_nlnno
#define xx_fname	x_file.x_fname

#define AUXENT		union auxent
#define AUXESZ		sizeof(AUXENT)

/* Archive file header structure */

struct ar_hdr
	{
	char ar_name[16];		/* '/' terminated file member name */
	char ar_date[12];		/* file member date */
	char ar_uid[6];			/* user identification */
	char ar_gid[6];			/* group id */
	char ar_mode[8];		/* file member mode (octal) */
	char ar_size[10];		/* file member size */
	char ar_fmag[2];		/* header trailer string */
	};
#define ARMAG	"!<arch>\n"
#define ARFMAG	"`\n"
#define SARMAG	8
