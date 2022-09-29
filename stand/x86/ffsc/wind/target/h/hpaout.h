/* hpAout.h - HP/UX a.out header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01h,22sep92,rrr  added support for c++
01g,18sep92,jcf  cleaned up some more.
01f,04jul92,jcf  cleaned up.
01e,26may92,rrr  the tree shuffle
01d,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
01c,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01b,12dec87,gae  added modification history; included HP/UX's magic.h.
01a,12dec87,hin  borrowed from HP/UX a.out.h file.
*/

#ifndef __INChpAouth
#define __INChpAouth

#ifdef __cplusplus
extern "C" {
#endif

/* See WRS: comments where changed */

/* HPUX_ID: @(#)a.out.h	27.5     85/06/27  */

/*
 *	Layout of a.out file :
 *	_________________________
 *	| header		|
 *	|_______________________|
 *	| text section		|
 *	|_______________________|
 *	| data section		|
 *	|_______________________|
 *	| pascal section	|
 *	|_______________________|
 *	| symbol table		|
 *	|_______________________|
 *	| debug name table	|
 *	|_______________________|
 *	| source line table	|
 *	|_______________________|
 *	| value table		|
 *	|_______________________|
 *	| text relocation area	|
 *	|_______________________|
 *	| data relocation area	|
 *	|_______________________|
 *
 *	header:			0
 *	text:			sizeof(header)
 *	data:			sizeof(header)+textsize
 *	MIS:			sizeof(header)+textsize+datasize
 *	LEST:			sizeof(header)+textsize+datasize+
 *				MISsize
 *	DNTT			sizeof(header)+textsize+datasize+
 *				MISsize+LESTsize
 *	SLT			sizeof(header)+textsize+datasize+
 *				MISsize+LESTsize+DNTTsize
 *	VT			sizeof(header)+textsize+datasize+
 *				MISsize+LESTsize+DNTTsize+SLTsize
 *	text relocation:	sizeof(header)+textsize+datasize+
 *				MISsize+LESTsize+DNTTsize+SLTsize+
 *				VTsize
 *	data relocation:	sizeof(header)+textsize+datasize+
 *				MISsize+LESTsize+DNTTsize+SLTsize+
 *				VTsize+rtextsize
 *
 *	NOTE - header, data, and text are padded to a multiple of
 *	       EXEC_PAGESIZE bytes (using EXEC_ALIGN macro) in
 *	       demand-load files.
 *
 */

/* header of a.out files */

/*#include <magic.h>*/
/*#include <nlist.h>	included for all machines */

/*------------------------start-of-magic.h------------------------------------*/
/* @(#) $Revision: 1.1 $ */

/*
   magic.h:  info about HP-UX "magic numbers"
*/

/* where to find the magic number in a file and what it looks like: */
#define MAGIC_OFFSET    0L

struct magic
         {
          unsigned short int system_id;
          unsigned short int file_type;
         };
typedef struct magic MAGIC;

/* predefined (required) file types: */
#define RELOC_MAGIC     0x106           /* relocatable only */
#define EXEC_MAGIC      0x107           /* normal executable */
#define SHARE_MAGIC     0x108           /* shared executable */

#define AR_MAGIC        0xFF65

/* optional (implementation-dependent) file types: */
#define DEMAND_MAGIC    0x10B           /* demand-load executable */

/* predefined HP-UX target system machine IDs */
#define HP9000_ID       0x208
#define HP98x6_ID       0x20A
#define HP9000S200_ID   0x20C
/*--------------------------end-of-magic.h------------------------------------*/

/* WRS: munge name to not conflict with usual a.out.h */
struct execHP {
/*  0 */	MAGIC	a_magic;		/* magic number */
/*  4 */	short	a_stamp;		/* version id */
/*  6 */	short	a_unused;
/*  8 */	long	a_sparehp;
/* 12 */	long	a_text;			/* size of text segment */
/* 16 */	long	a_data;			/* size of data segment */
/* 20 */	long	a_bss;			/* size of bss segment */
/* 24 */	long	a_trsize;		/* text relocation size */
/* 28 */	long	a_drsize;		/* data relocation size */
/* 32 */	long	a_pasint;		/* pascal interface size */
/* 36 */	long	a_lesyms;		/* symbol table size */
/* 40 */	long	a_dnttsize;		/* debug name table size */
/* 44 */	long	a_entry;		/* entry point */
/* 48 */	long	a_sltsize;		/* source line table size */
/* 52 */	long	a_vtsize;		/* value table size */
/* 56 */	long	a_spare3;
/* 60 */	long	a_spare4;
};

#define	EXEC_PAGESIZE	4096	/* not always the same as the MMU page size */
#define	EXEC_PAGESHIFT	12	/* log2(EXEC_PAGESIZE) */
#define	EXEC_ALIGN(bytes)	(((bytes)+EXEC_PAGESIZE-1) & ~(EXEC_PAGESIZE-1))

# define TEXT_OFFSET(hdr)	((hdr).a_magic.file_type == DEMAND_MAGIC ? \
				EXEC_ALIGN(sizeof(hdr)) : sizeof(hdr))

# define DATA_OFFSET(hdr) 	((hdr).a_magic.file_type == DEMAND_MAGIC ? \
				EXEC_ALIGN(sizeof(hdr)) +        \
					EXEC_ALIGN((hdr).a_text) : \
				sizeof(hdr) + (hdr).a_text)

# define MODCAL_OFFSET(hdr)	((hdr).a_magic.file_type == DEMAND_MAGIC ? \
				EXEC_ALIGN(sizeof(hdr)) +        \
					EXEC_ALIGN((hdr).a_text) + \
					EXEC_ALIGN((hdr).a_data) : \
				sizeof(hdr) + (hdr).a_text + (hdr).a_data)

# define LESYM_OFFSET(hdr)     MODCAL_OFFSET(hdr) + (hdr).a_pasint
# define DNTT_OFFSET(hdr)  LESYM_OFFSET(hdr) + (hdr).a_lesyms
# define SLT_OFFSET(hdr)   DNTT_OFFSET(hdr) + (hdr).a_dnttsize
# define VT_OFFSET(hdr)    SLT_OFFSET(hdr) + (hdr).a_sltsize
# define RTEXT_OFFSET(hdr) VT_OFFSET(hdr) + (hdr).a_vtsize
# define RDATA_OFFSET(hdr) RTEXT_OFFSET(hdr) + (hdr).a_trsize
# define DNTT_SIZE(hdr)  (hdr).a_dnttsize
# define VT_SIZE(hdr)    (hdr).a_vtsize
# define SLT_SIZE(hdr)   (hdr).a_sltsize


/* macros which define various positions in file based on an exec: filhdr */
#define TEXTPOS		TEXT_OFFSET(filhdr)
#define DATAPOS 	DATA_OFFSET(filhdr)
#define MODCALPOS	MODCAL_OFFSET(filhdr)
#define LESYMPOS	LESYM_OFFSET(filhdr)
#define DNTTPOS		DNTT_OFFSET(filhdr)
#define SLTPOS		SLT_OFFSET(filhdr)
#define VTPOS		VT_OFFSET(filhdr)
#define RTEXTPOS	RTEXT_OFFSET(filhdr)
#define RDATAPOS	RDATA_OFFSET(filhdr)

/* symbol management */
/* WRS: munge name to not conflict with usual a.out.h */
struct nlistHP {
	long	n_value;
	unsigned char	n_type;
	unsigned char	n_length;	/* length of ascii symbol name */
	short	n_almod;
	short	n_unused;
};

#define	A_MAGIC1	FMAGIC       	/* normal */
#define	A_MAGIC2	NMAGIC       	/* read-only text */

/* relocation commands */
/* WRS: munge name to not conflict with usual a.out.h */
struct r_infoHP {	/* mit= reloc{rpos,rsymbol,rsegment,rsize,rdisp} */
	long r_address;		/* position of relocation in segment */
	short r_symbolnum;	/* id of the symbol of external relocations */
	char r_segment;		/* RTEXT, RDATA, RBSS, REXTERN, or RNOOP */
	char r_length;		/* RBYTE, RWORD, or RLONG */
};

/* symbol types */
#define	EXTERN	040	/* = 0x20 */
#define ALIGN	020	/* = 0x10 */	/* special alignment symbol type */
#define	UNDEF	00
#define	ABS	01
#define	TEXT	02
#define	DATA	03
#define	BSS	04
#define	COMM	05	/* internal use only */
#define REG	06

/* relocation regions */
#define	RTEXT	00
#define	RDATA	01
#define	RBSS	02
#define	REXT	03
#define	RNOOP	077	/* no-op relocation  record, does nothing */

/* relocation sizes */
#define RBYTE	00
#define RWORD	01
#define RLONG	02
#define RALIGN	03	/* special reloc flag to support .align symbols */

	/* values for type flag */
#define	N_UNDF_	0	/* undefined */
#define	N_TYPE_	037
#define	N_FN_	037	/* file name symbol */
#define	N_ABS_	01	/* absolute */
#define	N_TEXT_	02	/* text symbol */
#define	N_DATA_	03	/* data symbol */
#define	N_BSS_	04	/* bss symbol */
#define	N_REG_	024	/* register name */
#define	N_EXT_	040	/* external bit, or'ed in */
#define	FORMAT_	"%06o"	/* to print a value */
#define SYMLENGTH_	255		/* maximum length of a symbol */

#ifdef	WRS_UNWANTED

/* WRS: the following is not needed (and possibly conflicts) */

/* These suffixes must also be maintained in the cc shell file */

#define OUT_NAME "a.out"
#define OBJ_SUFFIX ".o"
#define C_SUFFIX ".c"
#define ASM_SUFFIX ".s"

#define N_BADTYPE(x) (((x).a_magic.file_type)!=EXEC_MAGIC&&\
	((x).a_magic.file_type)!=SHARE_MAGIC&&\
	((x).a_magic.file_type)!=DEMAND_MAGIC&&\
	((x).a_magic.file_type)!=RELOC_MAGIC)

#define N_BADMACH(x) ((x).a_magic.system_id!=HP9000S200_ID&&\
	(x).a_magic.system_id!=HP98x6_ID)
#define N_BADMAG(x)  (N_BADTYPE(x) || N_BADMACH(x))

#endif	/* WRS_UNWANTED */

#ifdef __cplusplus
}
#endif

#endif /* __INChpAouth */
