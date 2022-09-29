/************************************************************************/
/*	                  COPYRIGHT (C) NOTICE				*/
/*									*/
/*	Copyright (C) Solutions Technology Pty Ltd, 1989.		*/
/*	All Rights Reserved						*/
/*									*/
/*	This   document  contains  confidential   and   proprietary	*/
/*	information of Solutions Technology Pty Ltd.  No disclosure	*/
/*	or use of any portion  of the contents  of this file or any	*/
/*	information,  data, or  program  generated in any  way from	*/
/*	the information  presented  after this  notice may  be used	*/
/*	without the express written consent of Solutions Technology	*/
/*	Pty Ltd (Incorporated in N.S.W. Australia).			*/
/*									*/
/************************************************************************/

/*
 * IN_PROC.H
 *
 * PRUPOSE:
 *	Header for IN_PROC.C
 */

/* =========================== CONFIGURABLE STUFF ===================== */


/* the function 'free' on some implementations of C return a value
 * indicating an error. You need to define INX_FREE_CHECK to 1
 * to make IN_PROC check the return value. You may also
 * need to modify "INX_BAD_FREE" which is the condition
 * that is compared to the return value of free to indicate 
 * if there is an error.
 * i.e. if ( INX_BAD_FREE free( ptr ) ) { error( "!@#$%" ); }
 */

#ifndef INX_FREE_CHECK
/* check memory allocation function "free" or not */
#define INX_FREE_CHECK	0
#define INX_BAD_FREE	0 ==	/* free bad condition check */
#endif


#define	INX_ERRFILE	stdout		/* which file ( stderr ) or ( stdout )
					 * for printing error messages
					 * you may wish to open your own */

typedef char	INX_BOOL;	/* boolean values are of this type */

/* ============================ INX_POSI =========================== */
#ifndef INX_FEEDBACK
/* if feedback (macro substitution is required) set to 1 */
#define INX_FEEDBACK 0
#endif

typedef struct {
	char		* inx_filename;	/* the file name */
	long		  inx_line;	/* the line number */
	short		  inx_colm;	/* position in line */
	long		  inx_offs;	/* offset into file for current line */
	char		  inx_fdb;	/* was a MACRO expansion */
} inx_posi;

/* ======================== INX_TYPE =========================== */
/* This data structure is to maintain the information
 * about the files being read
 */

typedef struct inx_type {
	struct inx_type	* inx_next;	/* the next file */
	inx_posi	  inx_place[1];	/* the file position */
	FILE		* inx_fs;	/* the file stream */
} inx_type;

/* ======================== INX_LISTTYPE ======================= */
/* This data structure is to maintain the file names
 * of all the files that have been processed.  These are kept in
 * a linked list so that the 'inx_posi' structures are consistent
 * until the linked list is deleted. ( i.e. even after all the
 * files have been closed)
 */

typedef struct inx_listtype {
	struct inx_listtype	* inx_nextname;	  /* the next file */
	char			  inx_dummy[ 1 ]; /* where the filename starts */
} inx_listtype;

extern inx_type		* inx_ptr;	/* the input stream data structure */
extern inx_listtype	* inx_listp;    /* the file name linked list table */
extern inx_posi		  inx_last[];	/* the last non white space posi */

extern 			  inx_getc();	/* the function to get a character */

extern void		* inx_alloc( size_t ); /* memory allocation function */
extern void		  inx_free();	/* memory deallocation function */

#define INX_CURR	(inx_ptr->inx_place)
					/* Where the pointer is currently */

/* ============================= ERROR MESSAGE STUFF ================== */

#define	INX_MAXLINE	0x100		/* the buffer size to read in the line
					 * for error message lines */
#define INX_ABORT	0x0040		/* abort after printing error message
					 * and error statistics */
#define INX_NOLINE	0x0000		/* do not print line */
#define INX_LINE	0x0020		/* print an error line */
#define INX_EXTRA	0x0010		/* an extra parameter exists */


/* ======================== ERROR STATISTICS ========================= */
/* The inx_error function can collate errors.
 * The errors are defined at present to be of four classes. The bottom
 * nibble has been allocated to this purpose so this can easily
 * be expanded to 16 types.
 *
 * 1. Plain ERROR
 * 2. Warnings
 * 3. SEVERE
 * 4. Informational
 * 4. Internal
 * The macro "INX_TALLY" can be used to get to these tallies
 * quickly.
 */

#define INX_ERMASK	0x0007		/* The 3 error bits */
#define INX_ERROR	0x0000		/* there has been an error */
#define INX_WARN	0x0001		/* this is a warning */
#define INX_SEVERE	0x0002		/* This is a severe error */
#define INX_INFORM	0x0003		/* This is an informational
					 * message only */
#define INX_MDEBUG	0x0004		/* Debug message
					 */
#define INX_INTERNAL	INX_ERMASK	/* This error is internal to the
					 * program */
#define INX_ERTYPES	(INX_ERMASK+1)	/* the number of error types */

					/* Macro to easily access the 
					 * number of errors for a type */

extern	long	  inx_tally[];		/* where error stats are actually
					 * stored */

extern	char	* inx_statnames[];	/* Textual description of the
					 * statistics */

#define INX_TALLY( x )		inx_errtable[ ((x) & INX_ERMASK) ].inx_tally

#define INX_STATNAMES( x )	inx_errtable[ ((x) & INX_ERMASK) ].inx_name

#define INX_SUPPRESS( x )	inx_errtable[ ((x) & INX_ERMASK) ].inx_suppress

#define INX_MAXERR( x )		inx_errtable[ ((x) & INX_ERMASK) ].inx_maxerr

	/* ----------------- INX_ERRTYPE ------------------ */
	/* This structure contains the information relevant to
	 * error handling */

typedef struct {
    long	  inx_tally;		/* how many of these errors
    					 * have occurred */
    char	* inx_name;		/* the name of this type of
    					 * error */
    int		  inx_maxerr;		/* Maximum number of errors of
    					 * this type to allow ( 0 = infin )
					 * if more than this many errors
					 * occur abort will occur */
    INX_BOOL	  inx_suppress;		/* Suppress this type of error
    					 * from output (BOOLEAN) */
    INX_BOOL	  inx_major;		/* does this error constitue a major
					 * type */
} inx_errtype;

extern inx_errtype inx_errtable[];	/* where the error information
					 * is kept */

extern int inx_error(
inx_posi	 * place,		/* the place that the error occured */
char		** table,		/* the table of error messages */
int		   err,			/* the error number */
unsigned int	   flags,		/* what to actually do */
		    ...
);

extern int inx_clear( void );		/* clear error statistics */
extern int inx_iserror( void);		/* are there any major errors */
extern int inx_stats( void );		/* print error statistics */


/* ========================= IN_PROC SPECIFIC ======================= */

#define INXE_OFFSET	0x100		/* error number offset */
extern char	* inx_table[];		/* table for IN_PROC.C errors only */

#define INX_TABLE 	inx_table	/* which error table */

/* ===================== MACRO EXPANSION FEEDBACK OPTION =========== */
/* FDB_GET is the 'function' called when 
 */

					/* macro feedback function */
#define FDB_GET		exit( printf("MACRO UNAVALABLE"));


/* MICROSOFT 'C's handling of text files fails miserably when
 * trying "fseek" files in text mode.  They are opened as
 * binary files instead.
 */

#ifdef MICROSOFT
#define INX_MODE	"rb"
#define INX_CNTRLZ	0x1a
#else
#define INX_MODE	"r"
#define INX_CNTRLZ	0x1a
#endif

