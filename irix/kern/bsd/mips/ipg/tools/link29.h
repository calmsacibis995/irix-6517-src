static char _link29_h[]="@(#)link29.h	1.6 89/05/26, AMD";
/* LINK29.H -- MAIN Am29000 LINKER HEADER FILE                  */

/* history:                                                     */

/*  7/22/87  J. Howe     --  created                            */

/* 10/26/88  RJK         --  rev 0.18                           */
/* 11/22/88  RJK         --  rev 0.19                           */
/*  1/10/89  RJK         --  rev 0.20                           */
/*  2/28/89  RJK         --  rev 0.21 (1.0e0)                   */

/* ------------------------------------------------------------ */
/*                 system-dependent definitions                 */
/* ------------------------------------------------------------ */

#ifdef HIF
#undef unix
#undef MSDOS
#define READ        "r"
#define WRITE       "w"
#define WR_RD       "w+"
#define RDBIN       "rb"
#define WRBIN       "wb"
#define RDWRBIN     "w+b"
#endif

/* ------------------------------------------------------------ */

#ifdef MSDOS
#undef unix
#undef HIF
#define READ        "r"
#define WRITE       "w"
#define WR_RD       "w+"
#define RDBIN       "rb"
#define WRBIN       "wb"
#define RDWRBIN     "w+b"
#endif

/* ------------------------------------------------------------ */

#ifdef unix
#define READ        "r"
#define WRITE       "w"
#define WR_RD       "w+"
#define RDBIN       "r"
#define WRBIN       "w"
#define RDWRBIN     "w+"
#endif

/* ------------------------------------------------------------ */
/*                     general definitions                      */
/* ------------------------------------------------------------ */

/* miscellaneous header file(s) */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "coff.h"

/* ------------------------------------------------------------ */

/* fundamental constants        */

#define ZERO        0
#define ONE         1

#define FALSE       0
#define TRUE        1

/* ------------------------------------------------------------ */

/* miscellaneous characters     */

#define DOT         '.'         /* period                       */
#define EOS         '\0'        /* null (end of string)         */

/* ------------------------------------------------------------ */

/* misc. configuration flags    */

#define ALGN_COMM   1

/* ------------------------------------------------------------ */

/* defining     --  has the following effect:                   */
/*                                                              */
/* ALGN_COMM    --  ".comm" common blocks are quadword-aligned  */

/* ------------------------------------------------------------ */

/* miscellaneous definitions    */

#define S_LEN       SYMNMLEN
#define S_TYPES     (STYP_ABS | STYP_BSS | STYP_DATA | STYP_TEXT)
#define STATIC      static

#define uchar       unsigned char
#define ulong       unsigned long
#define ushort      unsigned short

/* ------------------------------------------------------------ */

/* miscellaneous constants      */

#define OBJFILE     1
#define LIBFILE     2
#define TXTFILE     4

#define MSB         0   /* native byte order of machine MSB 1st */
#define LSB         1   /* native byte order of machine LSB 1st */

#define REVISION    "1.0e0"     /* revision string              */

#define ORDERMASK   (F_AR32WR | F_AR32W)

/* ------------------------------------------------------------ */

/* (re)location modes           */

#ifdef MOVE_BSS
#define ABSLOC      1           /* absolute sections            */
#define BSSLOC      2           /* relocatable BSS     sections */
#define RELLOC      3           /* relocatable non-BSS sections */
#else
#define ABSLOC      1           /* absolute    sections         */
#define RELLOC      2           /* relocatable sections         */
#endif

/* ------------------------------------------------------------ */

/* general error codes          */

#define e_stype     1   /* combining different section types */
#define e_exlinks   2   /* trying to combine same sect twice */

/* ------------------------------------------------------------ */
/*                          structures                          */
/* ------------------------------------------------------------ */

/* a local or global section */

struct scninfo
{
    struct scnhdr sc_header;    /* section header from coff object file */
    unsigned long sc_baddr;     /* base address of section */
    unsigned long sc_tfileloc;  /* location of section in temporary file */
    unsigned long sc_offset;    /* offset from base of combined section */
    struct mem_config *sc_mem;  /* index into memory configuration array */
    struct lineno *sc_lno;      /* ptr to array of lineno entries */
    struct module *sc_mod;      /* ptr to module it came from */
    short sc_lnkstat;           /* T if already linked, else F */
    struct scninfo *sc_group;   /* ptr to group that osect is in */
    struct scninfo *sc_hlink;   /* horizontal link to next group */
    struct scninfo *sc_vlink;   /* vertical link to next output section */
    struct scninfo *sc_next;    /* next piece of this output section */
};

/* ------------------------------------------------------------ */

/* module table entry           */

struct module
{
    struct filehdr m_header;    /* coff file header form object file */
    struct scninfo *m_scns;     /* ptr to array of local sections */
    struct module *m_link;      /* next module in list */
    struct sym **m_ixtab;       /* array of indices into symbol table */
    char m_name[2];
};

/* ------------------------------------------------------------ */

/* internal symbol table entry structure */

struct sym
{
    short sy_section;           /* section number (or zero)     */
    long sy_val;                /* symbol value                 */
    long sy_flags;              /* internal symbol flags        */
    unsigned short sy_type;     /* symbol type                  */
    char sy_class;              /* symbol class                 */

    char sy_numaux;         /* number of auxiliary entries */
    union auxent *sy_auxp;  /* ptr to auxiliary entry if numaux = 1 */
    long sy_index;          /* index into alpha list of symbols */
    struct module *sy_mod;  /* ptr to module from which symbol came */
    struct sym *sy_realsyp; /* used as actual ptr; an undefined may get
                               redirected to a defined symbol */

    struct sym *sy_rlink,*sy_llink;  /* table links for alphabetical access */
    char sy_name[2];        /* dummy for 1..63 + NULL */
};

/* ------------------------------------------------------------ */

/* symbol flags                 */

#define F_UNDEF     0x01        /* undefined         symbol     */
#define F_MULDEF    0x02        /* multiply-defined  symbol     */
#define F_GLOBSYM   0x04        /* global            symbol     */
#define F_COMMON    0x08        /* common-block      symbol     */
#define F_ESECT     0x10        /* "e<section>"      symbol     */

/* ------------------------------------------------------------ */

/* memory configuration structure holds data fr 1 chunk of memory */
/* (if no MEMORY directives; assume all memory available in 1 chunk ) */

struct mem_config
{
    unsigned long   mm_start;   /* start address of chunk */
    unsigned long   mm_length;  /* length of chunk */
    unsigned long   mm_alloc;   /* amount already allocated */
    struct mem_config *mm_link; /* ptr to next available chunk */
    char mm_name[12];           /* name of chunk, if any */
};

/* ------------------------------------------------------------ */

/* linked section structure for holding output sections in order requested */

struct osection_list
{
    struct scninfo *o_sect;      /* ptr to output section */
    struct osection_list *o_link;
};

/* ------------------------------------------------------------ */
/*                       global variables                       */
/* ------------------------------------------------------------ */

#ifdef RESOLVE
#define GLOBAL
#else
#define GLOBAL      extern
#endif

/* ------------------------------------------------------------ */

GLOBAL struct mem_config *memory;   /* start of array to be allocated later */
GLOBAL struct mem_config *curmemp;     /* ptr to current memory segment */

GLOBAL struct osection_list *order;

/* ------------------------------------------------------------ */

/* symbol tables */

GLOBAL struct sym *symtab;          /* external defined symbols table */
GLOBAL struct sym *usymtab;         /* undefined symbols table */
GLOBAL struct sym *modtab;          /* the module name table */
GLOBAL struct sym *osectab;         /* table of output section names */

GLOBAL struct scninfo *groups;     /* ptr to ordered,linked list of output
                                      sections */

/* ------------------------------------------------------------ */

/* the temporary file where all raw data goes */

GLOBAL FILE *tfile;
GLOBAL FILE *dbfile;                   /* temp file for debug syms */
GLOBAL FILE *strfile;                  /* temp file for string table */

/* ------------------------------------------------------------ */

/* the output file */

GLOBAL FILE *ofile;             /* output file pointer          */
GLOBAL char ofname[80];         /* output file name             */

/* ------------------------------------------------------------ */

/* the list of modules */

GLOBAL struct module *modlist;  /* head of list                 */
GLOBAL struct module *modapp;   /* tail (for appending)         */

/* ------------------------------------------------------------ */

/* miscellaneous ("-z") options */

GLOBAL long zo;                 /* "-z" option bits             */

#define U1FLAG      0x00000001  /* "e<section>" single-'_' flag */
#define D2FLAG      0x00000002  /* merge-sections flag          */
#define LEGAL_Z     0x00000003  /* all legal "-z" option bits   */

/* ------------------------------------------------------------ */

/* misc variables */

GLOBAL long maxsegsize;         /* largest local segment size   */

GLOBAL char *segbuf;            /* ptr to buffer for holding object code */
GLOBAL struct filehdr ofhead;   /* file header for output file  */
GLOBAL AOUTHDR opthdr;          /* optional "a.out" header      */
GLOBAL short map;               /* T if map listing requested */
GLOBAL short symap;             /* T if symbol listing requested */
GLOBAL short inmap;             /* T if include input modules in map */

GLOBAL unsigned short of_optflags; /* command line flags for stripping output */
GLOBAL char entry_sym[20];
GLOBAL long loc_ctr;               /* location counter of output data */
GLOBAL long alloc_start;           /* allocation start address */
GLOBAL long alloc_stop;
GLOBAL int location;               /* type of current section location */
GLOBAL long nextscnhead;           /* loc in output file of next section head */
GLOBAL long nextscnbody;           /* loc of next section code/data in output
                                      file */
GLOBAL long nextscnlnno;           /* loc nxt sect lineno tabl in outp file */
GLOBAL long nextscnreloc;          /* loc of next reloc item in output file */

GLOBAL long stringptr;             /* used by loadfile to locate string table */
GLOBAL long strgtab_ptr;           /*beginning of string table */
GLOBAL long strgtab_off;           /* loc in output file for next string table
                                      entry */

GLOBAL long sym_index;             /* running index of symbols in sym table */
GLOBAL int nsects;                 /* number of output sections */
GLOBAL int lastfile;               /* keeps track inter module .file links */

GLOBAL int host_order;             /* byte order of host machine */
GLOBAL int target_order;           /* byte order of machine processing output
                                        of linker */

GLOBAL char pname[100];
GLOBAL int pgroups;
GLOBAL int pblock;
GLOBAL int pflags;
GLOBAL char pskip;
GLOBAL long pval;
GLOBAL int psections;
GLOBAL int ngroups;
GLOBAL struct scninfo *curgroup;
GLOBAL struct scninfo *curosect;
GLOBAL struct module *curmodp;
GLOBAL unsigned long cur_loc;

GLOBAL long last_pc;

/* ------------------------------------------------------------ */
/*             miscellaneous function declarations              */
/* ------------------------------------------------------------ */

/* library functions            */

extern char *calloc();
extern char *malloc();

extern long ftell();
extern long time();

#ifndef M_I86
#if !HIF
extern char *memcpy();
#else
extern void *memcpy();
#endif
#endif

/* ------------------------------------------------------------ */

/* linker functions             */

extern char *c_alloc();
extern char *m_alloc();

extern unsigned long reverse();

extern struct scninfo *add_group();
extern struct scninfo *add_osect();

extern struct sym *findsym();
extern struct sym *findosym();
extern struct sym *insertsym();
extern struct sym *insertosym();

extern FILE *tmp_file();

#define exit    l_exit

/* END OF "LINK29.H"                                            */

