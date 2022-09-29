/* ASM29.H -- MAIN AM29000 ASSEMBLER HEADER FILE                */

/* history:                                                     */

/* n/a       E. Wilner   --  created                            */
/*  6/23/87  J. Howe     --  COFF modifications                 */

/*  8/25/88  RJK         --  rev 0.16                           */
/*  9/12/88  RJK         --  rev 0.17                           */
/* 10/25/88  RJK         --  rev 0.18                           */
/* 11/22/88  RJK         --  rev 0.19                           */
/* 12/13/88  RJK         --  rev 0.20                           */
/*  2/28/89  RJK         --  rev 0.21 (1.0e0)                   */
/*  5/22/89  RJK         --  rev 1.0f0                          */

/* ------------------------------------------------------------ */


/* misc. configuration flags    */

#define CPP_HASH    1
#define CS_IF       1
#define SKIP_PATH   1

/* ------------------------------------------------------------ */

/* defining     --  has the following effect:                   */
/*                                                              */
/* CPP_HASH     --  recognize "# <line> <file>"                 */
/* CS_IF        --  enable C-style "ifdef"'s                    */
/* SKIP_PATH    --  ".file" directive skips path component      */

/* ------------------------------------------------------------ */

/* standard header files        */

#include <ctype.h>
#include <setjmp.h>
#include <stdio.h>

#include <string.h>
#include <bstring.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/param.h>

/* ------------------------------------------------------------ */

/* local header file(s)         */

#include "coff.h"

/* ------------------------------------------------------------ */

/* miscellaneous characters     */

#define BSLASH      '\\'        /* backslash                    */
#define COLON       ':'         /* colon                        */
#define COMMA       ','         /* comma                        */
#define CR          015         /* carriage return              */
#define DQ          '"'         /* double quote                 */
#define DS          '$'         /* dollar sign                  */
#define EOS         '\0'        /* null (end of string)         */
#define HASH        '#'         /* hash                         */
#define LF          012         /* linefeed                     */
#define NL          '\n'        /* local newline                */
#define SEMIC       ';'         /* semi-colon                   */
#define SLASH       '/'         /* forward slash                */
#define SPACE       ' '         /* space                        */
#define SQ          '\''        /* single quote                 */
#define USC         '_'         /* underscore                   */

/* ------------------------------------------------------------ */

/* line-length parameters       */

#define LLEN        1023        /* maximum line length          */
#define MSLOP        512        /* argument expansion allowance */

/* "LLEN" here does  not include a  newline character or a null */
/* byte terminator.                                             */

/* ------------------------------------------------------------ */

/* miscellaneous parameters     */

#define IFNEST       120        /* max# levels of "if" nesting  */
#define MAXBODY    15000        /* max macro body size (bytes)  */
#define MAXERRS      200        /* max# of errors (per pass)    */
#define MAXMRECUR    100        /* max# recursive macro calls   */
#define MAXNAME       63        /* maximum symbol length        */
#define MAXRARGS      50        /* max# of repeat arguments     */
#define REPNEST       15        /* max# levels repeat nesting   */

/* ------------------------------------------------------------ */

/* miscellaneous definitions    */

#define ushort      unsigned short
#define ulong       unsigned long

#define BMASK       0xFF        /* byte mask                    */
#define BPB         8           /* bits per byte                */
#define FALSE       0           /* false                        */
#define NO_BSL      (1 << BPB)  /* see "strchar()"              */
#define NS          nullstr     /* null string                  */
#define REVISION    "1.0f0"     /* revision string              */
#define S_LEN       SYMNMLEN
#define TRUE        1           /* true                         */

                                /* description for header       */
#define DESCR       "IPT/Am29000 Assembler"

/* ------------------------------------------------------------ */

/* reserved-symbol table entry  */

struct res
{
    char        rs_name[9];    /* symbol name includes mnemonics and
                                 assembler directives and pseudo ops */
    char        rs_format;     /* F_xxx   --  pneumonic format */
    char        rs_flags;      /* R_xxx   --  line format or other attributes */

    short       rs_opcode;
};

/* ------------------------------------------------------------ */

/* bubonic formats:  the format of the 32 bits making up the feumonic        */
/*                      or the type of reserved symbol                       */
/*     The general format for every instruction fleamonic is:                */
/*            bits  31  .... 24 | 23  ....  16 | 15  ....  8 | 7  ....  0    */
/*                   OP      X      RC/VN/addr        RA        RB/I/addr    */
/*                           X = M or A or part of OP                        */
/*    M == selects RB or constant I                                          */
/*    A == selects address + PC  or  30 bit address                          */
/* see Am29000 manual for more details                                       */

/* ------------------------------------------------------------ */

#define F_PSOP          0       /* pseudo-op or directive       */
#define F_MCAB          1       /*   OP  M  RC  RA  RB          */
#define F_CAB           2       /*   OP     RC  RA  RB          */
#define F_MVAB          3       /*   OP  M  VN  RA  RB          */
#define F_VAB           4       /*   OP     VN  RA  RB          */
#define F_AB            5       /*   OP         RA  RB          */
#define F_CA            6       /*   OP     RC  RA              */
#define F_SB            7       /*   OP         SA  RB          */
#define F_CS            8       /*   OP     RC  SA              */
#define F_B             9       /*   OP             RB          */
#define F_A2A          10       /*   OP  A 17-10 RA  9-2        */
#define F_0A           11       /*   OP    15-08 RA  7-0        */
#define F_0S           12       /*   OP    15-08 SA  7-0        */
#define F_MCB          13       /*   OP  M  RC      RB          */
#define F_MEAB         14       /*   OP  M CTRL RA  RB          */
#define F_             15       /*   OP                         */
#define F_NOP          16       /*   NOP (ASEQ 40#h,gr1,gr1)    */

#define F_CALLX        17       /*   pseudo-macro for near/far calls */
#define F_CACA         18       /*   OP     RC  RA RND FC FA   - convert inst */

#define OP_CONSTH   0x0200  /* opcode field as read from reserved table */
#define OP_CONSTN   0x0100

#define R_LABEL     0x01    /* must have a label */
#define R_NOLAB     0x02    /* mustn't have one */
#define R_PASSLAB   0x04    /* pass lebel through without actually defining */
#define R_NOGEN     0x08    /* does not generate code */
#define R_UNCOND    0x10    /* process even if .if'ed out */

/* ------------------------------------------------------------ */

extern char _namech[];

extern struct res restab[]; /* the reserved symbol table itself */
                            /* consists of instruct. mneumonics and */
                            /* assembler directives */

extern int nres;            /* number of reserved symbols */

/* ------------------------------------------------------------ */

/* cross reference structure    */

#define XR_PACK     10          /* xrefer packing factor */

struct xrefer
{
    short    xr_cnt;            /* # of entries (1 .. XR_PACK) */
    struct
    {
        short  xr_line;         /* line number where referenced */
        short  xr_flags;        /* XR_xxx */
    } xr_list[XR_PACK];         /* the entries */
    struct xrefer *xr_link;     /* link to next list */
};

/* xrefer flags */

/* ------------------------------------------------------------ */

/* The symbol table entry structure
   If this is a segment-table entry, 'val' points to the seglist structure,
   and 'size' is the local segment number */

struct sym
{
    struct syment se;
    long    sy_index;          /* index into alphabet list of symbols */
                               /* needed for relocatable items  */
    short   sy_flags;          /* symbol flags                  */
    short   sy_f2;             /* additional flags              */
    struct sym *sy_llink, *sy_rlink;
                               /* left and right links */
    short sy_defline;          /* line # where defined (main file only) */
    struct xrefer *sy_xref;    /* ptr to list of references */
    char        *sy_name;
};

#define SY_NULL     ((struct sym *) NULL)

/* ------------------------------------------------------------ */

/* first set of flag bits       */

#define F_LOCSYM        0x8000      /* local symbol for debugger */
#define F_PUBLIC        0x4000      /* public symbol */
#define F_EXTERN        0x2000      /* external symbol */
#define F_UNDEF         0x1000
#define F_MULDEF        0x0800
#define F_RELOC         0x0400      /* used to force reloc of non-reloc sym */
#define F_SETSYM        0x0200      /* sym defined via .set directive */
#define F_REGSYM        0x0100      /* register symbol: predefined or .reg */
#define F_SPREG         0x0080
#define F_SECTION       0x0040      /* a dummy entry that provides for reloc */
#define F_VAL           0x0008      /* a dummy entry that provides for reloc */
                                    /* local label references relative to    */
                                    /* section base address */
#define F_ASMGEN        0x0020      /* assembler generated symbol */
#define F_REF           0x0010      /* referenced symbol - for spregs */
#define F_DBGSYM        0x0001      /* Debug symbol */

/* ------------------------------------------------------------ */

/* second set of flag bits      */

#define F2_DEFP         0x0001		/* defined in current pass */
#define F2_DDEF		0x0002		/* defined with -D */

/* ------------------------------------------------------------ */

#define sy_segx     sy_defline  /* segment-mark field           */
#define SEGX_MARK   (-42)       /* segment-mark flag value      */

/* The "sy_segx" field is set equal to  SEGX_MARK to identify a */
/* segment symbol.                                              */

/* This convention facilitates some precautionary measures.     */

/* ------------------------------------------------------------ */

#ifdef NOTDEF
struct symlist
{
    struct sym     * sl_sym;
    struct symlist * sl_link;
};
#endif

/* ------------------------------------------------------------ */

/* define the pseudo-ops: */

#define PS_IDENT    1   /* .ident <string. */
#define PS_INCLUDE  2   /* .include <string>   */
#define PS_END      3   /* .end   */
#define PS_OPTIONS  4   /* .options  <op list> */
#define PS_ERR      5   /*  .err  <string> */
#define PS_IF       6   /*  .if <absx>  -- assemble if value not zero */
#define PS_IFDEF    7   /*  .ifdef  <var> -- assemble if variable is defined */
#define PS_IFNDEF   8   /*  .ifnotdef  <var> --  */
#define PS_IFEQS    9   /*  .ifeqs <string>,<string> -- ass if strings equal */
#define PS_IFNES    10  /*  .ifnes <string>,<string> -- ass if strings != */
#define PS_IFNUM    11
#define PS_IFNOTNUM 12
#define PS_ELSE     13  /* .else   --  alternate case */
#define PS_ENDIF    14  /* .endif  */
#define PS_LIST     15  /* .list  -- enable listing */
#define PS_NOLIST   16  /* .nolist -- disable listing */
#define PS_PRINT    17  /* .print <string>  -- print string to standard out */
#define PS_EJECT    18  /* .page  -- advance to top of page */
#define PS_SPACE    19  /* .space <absx>,<absx> - space N lines,leave K */
#define PS_TITLE    20  /* .title <string>  -- set listing title */
#define PS_SUBTTL   21  /* .subttl <string>  -- set listing subtitle */
#define PS_GLOBAL   22	/* .global <var list> - make sym global to all mods */
#define PS_EXTERN   23  /* .extern <var list>  --  declare symbols external */
#define PS_SECT     24  /* .seg <seg type>  --- declare a new segment */
#define PS_USE      25  /* .use <var>  --  use a declared segment */
#define PS_EQU      26  /* .equ <absx> - set symbol to value,unlimited scope */
#define PS_SET      27  /* .set <absx>  -- set symbol to value, limited scope */

#define PS_REG      28  /*  .reg <register> -- set symbol to register name */
#define PS_MACRO    29  /* .macro <var list> -- declare a macrro */
#define PS_EXITM    31  /* .exitm  -- terminate macro expansion */
#define PS_ENDM     32  /* .endm  -- end macro declaration */
#define PS_PURGEM   33  /* .purgem <var list>  -- delete all macros listed */
#define PS_REP      34  /* .rep <absx>  -- repeat N times */
#define PS_IREP     35  /* .irep <list> -- repeat for each item in list */
#define PS_IREPC    36  /* .irepc <string> -- repeat for each char in str */
#define PS_ENDR     37  /*  .endr  -- end repeat block */
#define PS_ASCII    38  /*  .ascii " ... "   --  intialize ascii string */
#define PS_BYTE     39  /*  .byte  <expr list> -- initialize bytes */
#define PS_HWORD    40  /*  .hword <expr list>  --  initialize half-words */
#define PS_WORD     41  /*  .word  <expr list>  -- initialize words */
#define PS_FLOAT    42  /*  .float  <float list> - init single prec values */
#define PS_DOUBLE   43  /*  .double <float list> -- init double prec values */
#define PS_ALIGN    44  /*  .align  <absx>  -- force word alignment */
#define PS_BLOCK    45  /*  .block  <expr>   -- reserves bytes in bss segment */
#define PS_EXTEND   46  /*  .extend <float list> -- init extended prec float */

#define PS_DEF      47  /*  .def <name> - define COFF debugging symbol  */
#define PS_VAL      48  /*  .val - assign value to symbol named in last .def */
#define PS_SIZE     49  /*  .size <expr> - ass size for last .def symbol */
#define PS_TYPE     50  /*  .type <expr> - ass type for last .def sym */
#define PS_SCL      51  /*  .scl <expr>  - ass storage class for last .def */
#define PS_TAG      52  /*  .tag <name>  - ass name of str,un,en of last .def */

#define PS_LINE     53  /*  .line <expr> - ass line number to last .def sym */
#define PS_DIM      54  /*  .dim  <dim1,dim2,dim3,dim4> */
#define PS_ENDEF    55  /*  .endef - end of COFF symbol definition */
#define PS_LN       56  /*  .ln <absx><expr>- create a line no. entry in COFF */
#define PS_FILE     57  /*  .file <string>  -   create COFF .file symbol */
#define PS_COMM     58  /*  .comm <var><absx> declare undefined external */
                        /*       with value equal to absx bytes */
#define PS_TEXT     59  /*  .text   -  use .text segment */
#define PS_DATA     60  /*  .data   -  use .data segment */
#define PS_LFLAGS   62  /*  .lflags <flaglist> - listing flags */
#define PS_LCOMM    63  /*  .lcomm */
#define PS_DSECT    64  /*  .dsect  -  start dummy section,only labels
                                and .blocks allowed */

/* ------------------------------------------------------------ */

/* entry for set symbol attribute directives; for debuggers */

struct debug_sym
{
    struct syment   db_cf;		/* a coff symbol */
    char	    db_name[64];		/* its full name */
    struct debug_sym *db_nextag;	/* ptr to next entry that is a tag */
    unsigned long   db_index;		/* index of this entry in table */
    struct debug_sym *db_link;		/* ptr to next entry */
    union auxent    *db_auxp;		/* ptr to aux entry */
};

/* ------------------------------------------------------------ */

#ifndef GLOBAL
#define GLOBAL      extern
#endif


GLOBAL struct debug_sym *dbugtab;      /* the start of the table */
GLOBAL struct debug_sym *lsttag;       /* ptr to last entry made */
GLOBAL struct debug_sym *lastdbp;

#define D1MASK  0x00030
#define MAXBLOCKS   100         /* max # of nested structures or blocks */

/* ------------------------------------------------------------ */

struct arg {            /* macro formal/actual argument pair */
    char      * arg_val;
    char      * arg_name;
};

/* ------------------------------------------------------------ */

struct macro {          /* a latent macro */
    char      * m_body;           /* null-terminated body */
    short       m_rcnt;           /* recursion cntr */
    short       m_argc;           /* # of arguments expected */
    struct arg  m_args[1];        /* dummy */
};

/* ------------------------------------------------------------ */

struct include {        /* an include file or active macro */
    struct include * if_link;      /* parent source file */
    FILE      * if_fp;
    FILE        if_fs;             /* for sopen of macro body */
    struct macro *if_mac;          /* ptr to macro struct; for dec recur cnt */
    char      * if_name;           /* file or macro name */
    short       if_lineno;         /* line number in file */
    char        if_argc;           /* # of arguments present */
    char        if_type;           /* ' ' = main; 'I' = include; 'M' = macro */
    char        if_sp;           /* ifsp at .INCLUDE */
    char        if_foo[3];
    struct arg  if_args[1];        /* dummy */
};

/* ------------------------------------------------------------ */

/* a value (possibly relocatable): */

struct value
{
    short       v_seg;            /* local seg # */
    char        v_reloc;          /* relocation type: REL_xxx (see COFF.H) */
    char        v_flags;          /* VF_xxx */
    long        v_val;
    struct sym *v_ext;            /* external symbol, if any */
};

/* ------------------------------------------------------------ */

#define     VF_SIZE     0x07
#define     VF_BYTE     0x00
#define     VF_WORD     0x01
#define     VF_HWORD    0x02

/* ------------------------------------------------------------ */

/* Relocation types defined in 'coff.h' */

#define NRELOC      8           /* number of reloc types */

/* ------------------------------------------------------------ */

/* the local segments are kept in a list, in order... */

struct seglist
{
    struct seglist  *sg_link;      /* next in list */
    struct scnhdr   sg_head;       /* the segment header for object file */
    short           sg_p1nrel;     /* # reloc items during pass 1 */
/* >>><<< these are duplicated from header info ; probably not nec */
    long            sg_faddr;      /* address in outf */
    long            sg_faddrel;    /* ditto, of reloc data */
    struct sym     *sg_symptr;     /* each section has a dummy symbol in the */
                           /* sym table for relocating local references */
};

/* ------------------------------------------------------------ */

GLOBAL struct seglist *dsection;           /* dummy section entry */
GLOBAL struct sym *dummy;                  /* the symbol that points to it */

/* ------------------------------------------------------------ */

union dblflt
{
    double  d;
    long    l[2];
};

/* ------------------------------------------------------------ */

GLOBAL long viSegs;             /* Index of First Section symbol */
GLOBAL int endafile;            /* flag to indicate end of source file */

GLOBAL long pc;                 /* current location counter */
GLOBAL long lpc;                /* pc @ beginning of line */

GLOBAL int nsegs;               /* # of local segs defined */
GLOBAL int curseg;              /* seg # of current seg */
GLOBAL struct seglist * cursegp; /* pointer to current seg's header */
GLOBAL int prevseg;             /* number of section last used */
GLOBAL struct seglist *prevsegp;   /* ptr to section last used */

GLOBAL struct seglist * sectlist; /* linear list of local segments */

GLOBAL struct sym *symtab,      /* the symbol table */
                  *mactab,      /* the macro table */
                  *segtab;      /* segment table */

#ifdef NOTDEF
GLOBAL struct symlist *extlist; /* list of externals */
#endif

GLOBAL int ndbsyms;             /* number of debug symbol entries originating
                                    from symbol attribute directive */
GLOBAL long dbfiles[100];       /* allocated array of indices of .file
                                    symbols in Coff symtab - more than 100? */
GLOBAL int ndbfils;             /* number of .file entries */
GLOBAL struct debug_sym dbugsym;    /* a place to put sym att directive syms */
GLOBAL union  auxent  auxsym;      /* its aux entry when nec. */
GLOBAL struct debug_sym *saptr;
GLOBAL struct debug_sym *block[MAXBLOCKS];
                                  /* array of ptrs to sym w/endndx entry */
GLOBAL int nblocks;               /* index into above array */

                                /* input line buffer            */
GLOBAL char srcline[LLEN+MSLOP+1];

GLOBAL char *lptr;              /* pointer to srcline during parsing */

GLOBAL int pass;                /* current assembler pass */
GLOBAL int list;                /* T if listing else F */
GLOBAL int llist;               /* T if local listing else F */
GLOBAL int pr_reloc;            /* non zero if reloc sym to be printed */
GLOBAL int lineno;              /* source file line number */
#ifdef sgi
GLOBAL int listpc;		/* listing the PC is interesting */
GLOBAL int pcprec;		/* digits given the PC in the listing */
GLOBAL int pcmask;
GLOBAL int listovr;		/* list this macro output */
#endif
GLOBAL int omit_warn;
GLOBAL int pageline;            /* current line on listing page */
GLOBAL int pglength;            /* max length of listing page - default 60 */
GLOBAL int pgcolumns;		/* # of columns on listing - default 80 */
GLOBAL int errcnt;              /* number of errors encountered */
GLOBAL int warncnt;             /* number of warnings encountered */
GLOBAL int srctype;             /* type of current input file */

GLOBAL int target_order;        /* = 0 if MSB 1st;  = 1 if LSB 1st */
GLOBAL int host_order;          /*   "  "       "    "    "    "   */
GLOBAL int psyms;               /* T if print symbols   */
GLOBAL int pasyms;              /* T if print assembler generated symbols */
GLOBAL int listalld;            /* T if list all data storage bytes */
GLOBAL int xref;                /* T if print cross references  */
GLOBAL int textonly;            /* T if assemble all into .text section */
GLOBAL int ttyout;
GLOBAL int debugon;             /* T if print during pass 1 */

GLOBAL int listmac, listinc, midpass, inmac;
GLOBAL int listcond, listuncond, listsym ;

GLOBAL int sym_index;           /* alphabetical index into symbol table */
GLOBAL int lab_cnt;             /* count during each pass of number of */
                                /* non local labels encountered */

GLOBAL struct value lcode[50];      /* code for this line for listing */
GLOBAL int lwords;                  /* # of entries */

GLOBAL FILE *outf;              /* object file */
GLOBAL struct include *src;     /* current input file/macro (linked list) */
GLOBAL struct include *mainsrc; /* the main source file */

GLOBAL int ifsp;            /* IF stack pointer or index */
GLOBAL int ifstk[IFNEST];   /* IF file stack for conditional assemb.  */
GLOBAL int m_ifsp;          /* save nesting level during macro exp */

GLOBAL int rsp;             /* REPEAT block stack pointer or index */
GLOBAL int rep_num[REPNEST]; /* REPEAT stack of counters */
GLOBAL int rep_cnt[REPNEST]; /* REPEAT block count - to keep track of locals */
GLOBAL long rep_fpos[REPNEST];  /* fpos at start of block       */
GLOBAL int rep_lastype[REPNEST]; /* rep stack of previous src->if_type */
GLOBAL int rep_lineno[REPNEST]; /* lineno of current repeat block */
GLOBAL char *rep_formal[REPNEST];   /* repeat block formal arg */

#ifdef OLD
GLOBAL struct value *rep_args[REPNEST][MAXRARGS];  /* actual argument array */
#else
GLOBAL char *rep_args[REPNEST][MAXRARGS];  /* actual argument array */
GLOBAL char rep_type[REPNEST];  /* repeat-block type            */
#endif

GLOBAL struct sym *tag;                 /* this line's label */
GLOBAL char        tagname[MAXNAME+1];  /* the name thereof */

GLOBAL struct res *inst;        /* pointer to instr mneumonic entry  */

GLOBAL struct filehdr head;     /* COFF object file fileheader */
GLOBAL long stringptr;          /* offset of string table in COFF file */
GLOBAL long cur_strgoff;        /* offset of current string in COFF file */

GLOBAL char dstname[100];       /* output (object) file name    */
GLOBAL long ext_float[4];       /* extended float               */
GLOBAL jmp_buf jmp_fltp;        /* floating-point jump vector   */
GLOBAL int lcflag;              /* "-L" (L* local symbols) flag */
GLOBAL int no_align;            /* don't-align STYP_XXXX mask   */
GLOBAL int no_args;             /* argument-suppression flag    */
GLOBAL char nullstr[1];         /* null string                  */
GLOBAL int silent;              /* "silent" (-s) flag           */

/* ------------------------------------------------------------ */

/* misc global functions        */

extern void		    align(int);
extern long                 bcheck(long,long,long);
extern void		    checkfor(int, int);
extern void		    ck_align(int);
extern int		    cmpstr(char*, char*);
extern int		    comma(int);
extern unsigned long        dbfindtag(char*);
extern void		    deblank(void);
#define dir_sep(c)	    ((c) == SLASH)
extern char                 *e_alloc(unsigned int, unsigned int);
extern void		    endseg(int);
extern void		    eprintf(char *, ...);
extern int		    eol(void);
extern void		    error(int);
extern void		    fatal(int);
extern struct debug_sym	    *finddbugsym(char*);
extern struct sym	    *findsym(char*, struct sym*);
extern char                 *fname(char*);
extern void		    genbyte(int);
extern void		    genrel(long, char);
extern void		    genvbyte(struct value*);
extern void		    genvhword(struct value*);
extern void		    genvrel(struct value*);
extern void		    genvword(struct value*);
extern double               getdouble(void);
extern float                getfloat(void);
extern long                 getint(void);
extern long		    getintb(long,long);
extern void		    getintval(struct value*, long, long);
extern int		    getline(void);
extern void		    getval(struct value*);
extern int		    get_name(char*);
extern int		    get_rname(char*, int);
extern int		    getreg(struct value*, int);
extern void		    ieee(double, char*);
extern void		    ieeed(double, char*);
extern void		    ieeex(double, char*);
extern int		    inclose(int);
extern int		    infopen(char*, int, char*);
extern struct sym	    *insertsym(char*, struct sym*);
extern char                 *m_alloc(unsigned int);
extern char                 *nam_str(char *);
extern struct scnhdr *	    num_hdr(int);
extern int		    newpage(void);
extern void		    panic(char*);
#ifdef SWAP_COFF
extern void		    putswap(long, char*, int);
#endif
extern void		    readline(FILE*, char*);
extern char                 *savestring(char*);
extern struct sym           *searchsym(char *);
extern void		    skip_eol(void);
extern void		    s_fatal(char*);
extern void		    s_open(char*, int, FILE*);
extern void		    s_seek(FILE*, long, int);
extern long                 s_tell(FILE*);
extern int		    strchar(int);
extern void		    strdelete(char*, int);
extern char                 *tempext(void);
extern void		    wmsg(char*, char*, ...);
extern void		    x_error(char*, ...);
extern void		    xmsg(char*, char*, ...);

/* ------------------------------------------------------------ */

/* misc library functions       */

#include <stdlib.h>
#include <unistd.h>

/* ------------------------------------------------------------ */

#define namech(c) _namech[c]

#define zap(p)      bzero(p,sizeof(*p))
#define isodigit(c) (((c)>='0') && ((c)<'8'))
#define sgn(x)      (((x) < 0) ? -1 : (((x) > 0) ? 1 : 0 ))

/* ------------------------------------------------------------ */

/* define the error codes */

#define e_phase          1      /* phase error */
#define e_missing        2      /* bad line format; missing character */
#define e_operand        3      /* malformed operand */
#define e_range          4      /* operand out of range */
#define e_size           5      /* illegal size */
#define e_muldef         6      /* multiply defined symbol */
#define e_undef          7      /* undefined symbol */
#define e_extra          8      /* extra stuff on line */
#define e_nolbl          9      /* missing label */

#define e_nooperand     10      /* missing operand */
#define e_bigmac        11      /* macro too big */
#define e_endm          12      /* missing or extra .endm */
#define e_notcond       13      /* not in conditional code */
#define e_condnest      14      /* excessive condition nesting */
#define e_endcond       15      /* inconsistent else/endif      */
#define e_notabs        16      /* expression must be absolute */
#define e_noendc        17      /* missing .endc */
#define e_num           18      /* bad number */
#define e_lbl           19      /* label useless */

#define e_reloc         20      /* misc. relocation error */
#define e_opr           21      /* illegal operand */
#define e_cparen        22      /* ) expected */
#define e_nsseg         23      /* no such segment exists */
#define e_badopt        24      /* bad option field */
#define e_confopt       25      /* conflicting options */
#define e_index         26      /* index specifier expected (+/- nn) */
#define e_instr         27      /* unknown instruction */
#define e_quote         28      /* missing close-quote */
#define e_text          29      /* bad .text delimiter */

#define e_spreg         30      /* not a special purpose reg */
#define e_unalign       31      /* target address not word aligned */
#define e_user          32      /* user generated error */
#define e_notinmac      33
#define e_repnest       34      /* repeat block nesting error */
#define e_symatt        35      /* symbol attribute directive err */
#define e_recur         36
#define e_bss_op        37      /* illegal in bss section       */
#define e_badlabel      38      /* illegal chars or missing colon in lbl */
#define e_illdigit      39      /* illegal digit */

#define e_illreg        40      /* illegal register */
#define e_toolong       41      /* source line too long */
#define e_local         42      /* illegal local label */
#define e_mult          43      /* not valid multiplier for storage dir */
#define e_notag         44      /* tag name required for directive */
#define e_divzero       45      /* divide by zero */
#define e_dsect         46      /* illegal operation in dsect */
#define e_noalign       47      /* warning for .hword/.word  */
#define e_nofile        48      /* include file does not exist */
#define e_nofunc        49      /* bad $func(thing) */

#define e_smflt         50      /* fltp value is too small      */
#define e_lgflt         51      /* fltp value is too large      */
#define e_badchar       52      /* illegal character(s) in line */
#define e_2type         53      /* section attribute conflict   */
#define e_notype        54      /* section type not specified   */
#define e_longname      55      /* name longer than MAXNAME     */
#define e_too_op        56      /* too many operands            */
#define e_exov          57      /* expression overflow          */

#define MAX_ERROR_NO    57      /* maximum error number         */

/* ------------------------------------------------------------ */

#define w_fsize        (-10)    /* ".file" size warning flag    */

/* ------------------------------------------------------------ */

/* Register Types */

#define REG_GP      0               /* general purpose */
#define REG_GLOB    1               /* global */
#define REG_LOC     2               /* local */
#define REG_NAME    3               /* register symbol */

/* ------------------------------------------------------------ */

/* misc */

#define M_BIT       0x01000000      /* M bit of instruction code selects
                                        8 bit constant or register */
#define A_BIT       0x01000000      /* A bit of instr code selects absolute
                                        or PC relative addressing */
#define CE_BIT      0x00800000      /* CE bit of load/store instr selects
                                        coprocessor or external access */
#define REQUIRED    1
#define NOTREQRD    0

#define DSECT      (-1)

#define MSB         0
#define LSB         1

/* values for pr_reloc => print ! or %% on listing */

#define R_SEG       1           /* set when R_BYTE reloc is local */
#define R_EXT       2           /* set when R_BYTE relocation is external */

/* END "ASM29.H"                                                */

