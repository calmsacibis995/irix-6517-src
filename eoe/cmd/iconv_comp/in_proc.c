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
 * IN_PROC.C
 *		Written by G. Mariani Jan-1988
 *
 * PRUPOSE:
 *	This module contains routines to manipulate input from source
 * files and to send error messages showing the line and pointing to
 * the error.
 *	It will allow multiple input files and will keep track of the
 * current file position.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "in_proc.h"
#define TRUE 1
#define FALSE 0

inx_posi	  inx_nullplace[] = {{
	"No File",	/* char		* inx_filename;	* the file name */
	0,		/* long		  inx_line;	* the line number */
	0,		/* short	  inx_colm;	* position in line */
	0		/* long		  inx_offs;	* offset into file */
			/* 				* for current line */
}};

inx_type	* inx_ptr = 0;	    /* the input stream data structure */
inx_listtype	* inx_listp = 0;    /* the file name linked list table */
inx_posi	  inx_last[1];	    /* the last non white space position */

/* ============================= INX_TABLE ========================== */
/* IN_PROC - error message table
 */
char	* inx_table[] = {
        ( char * ) INXE_OFFSET,
#define					INXE_NOERR	( INXE_OFFSET + 1 )
	"No error",
#define					INXE_ICLERR	( INXE_OFFSET + 2 )
	"Input file close failure",
#define					INXE_ALLOC	( INXE_OFFSET + 3 )
	"Out of heap memory",
#define					INXE_DEALLOC	( INXE_OFFSET + 4 )
	"Heap memory trashed",
#define					INXE_OPERR	( INXE_OFFSET + 5 )
	"Failure to open file",
#define					INXE_SKERR	( INXE_OFFSET + 6 )
	"Failure to seek on file",
#define					INXE_GETSERR	( INXE_OFFSET + 7 )
	"Failure to 'gets' on file",
#define					INXE_LIMIT	( INXE_OFFSET + 8 )
	"Error limit reached",
};

/* ============================= INX_GETC =========================== */
/* get a character from the stream
 */
int inx_getc()
{
	register int		  c;
	register inx_type 	* p = inx_ptr;

redo:
	/* check to see if there is a file ! */
	if ( !p ) {

#if 0
	    icc_find_next();
	    p = inx_ptr;
#endif

	    if ( ! p ) {
		return EOF;	/* there is no data */
	    }
	}
	
	switch ( c = fgetc( p->inx_fs ) ) {
	    case '\n' : {
		p->inx_place->inx_line ++;
		p->inx_place->inx_colm = 0;
		p->inx_place->inx_offs = ftell( p->inx_fs );
		return c;
	    }
	    case '\t' : {
		/* get the column right for tabs */
		p->inx_place->inx_colm +=
		    8 - ((p->inx_place->inx_colm) & 0x7)
		;
		return c;
	    }
	    case '\r' : {
		goto redo;
	    }
	    case EOF : case INX_CNTRLZ : {
		/* end of file */
		if ( fclose( p->inx_fs ) == EOF ) {

		    /* report an error message */
		    inx_error(
			inx_ptr->inx_place,	/* the place where the error occured */
			INX_TABLE,		/* which error table */
			INXE_ICLERR,		/* close error */
			INX_NOLINE | INX_WARN	/* abort and print line */
		    );

		}
		p = p->inx_next;		/* pop off the file info */
		inx_free( inx_ptr );		/* free the memory */
		inx_ptr = p;			/* point to new place */
		goto redo;			/* try again from this file */
	    }
	    default : {
						/* must have a single char */
		p->inx_place->inx_colm ++;
		/* get last non white space position */
		if ( c != ' ' ) {
		    * inx_last = * p->inx_place;
		}
		return c;			/* return the character */
	    }
	} /* switch */	
} /* inx_getc */

/* ============================ INX_SETFILE ============================ */
/* This will set the file for the next input.  There are two ways to
 * use this function.
 *
 *	1. Send a filename and a file stream already opened;
 *
 *	2. Send a filename and a NIL file pointer
 *	   In this case the file is opened in setfile and linked
 *	   into the list.  If it fails to open the file the
 *	   function returns false otherwise true.  An error
 *	   message will be printed also if there are any problems.
 *
 * 	If there is a problem with heap space allocation the function
 *	will abort and give error messages.
 *
 * File names are allocated memory and kept in a linked list.  These names
 * will actually last through out the life of excecution or if they
 * are actually free'ed prematurly.  This means that file name pointers
 * can be assumed to be valid for error messages etc, even though the
 * file is closed.
 */

int inx_setfile( filename, fs )
char	* filename;
FILE	* fs;
{

    inx_type		* i;	/* the input stream data structure */
    inx_listtype	* x;	/* The list pointer type */

    /* see if the file needs opening */
    if ( ! fs ) {	

	/* open it up */
	if ( !( fs = fopen( filename, INX_MODE ) ) ) {
	    /* failure to open file */

	    inx_error(
		inx_last,		/* the place where the error occured */
		INX_TABLE,		/* which error table */
		INXE_OPERR,		/* open error */
		INX_LINE | INX_ERROR	/* print line */
	    );

	    return 0;			/* return zero signifying that there
    					 * was a failure */
	}
    }

    /* get some memory for the file control stuff */
    i = (inx_type *) inx_alloc( sizeof( inx_type ) );

    /* allocate the new filename memory */
    /* note that strlen does not include the null however the 
     * inx_listtype reseves one character for it - if the compiler
     * performs word alignment on structures then it may actually
     * reserve two characters - There is heaps of room for the null */

    x = ( inx_listtype * )
	inx_alloc( strlen( filename ) + sizeof( inx_listtype ) )
    ;

    /* link it into the list */
    x->inx_nextname = inx_listp;
    inx_listp = x;

    /* put the filename into the memory */
    strcpy( x->inx_dummy, filename );

    /* link the file information into the file list */
    i->inx_next = inx_ptr;
    inx_ptr = i;

    /* link the filename to the file information */
    i->inx_place->inx_filename = x->inx_dummy;

    /* initialize the rest */
    i->inx_place->inx_line = 1;
    i->inx_place->inx_colm = 0;
    i->inx_place->inx_offs = 0;
    i->inx_place->inx_fdb  = 0;
    i->inx_fs = fs;

    return 1;
    
}

/* ============================ INX_ALLOC ============================= */
/* allocate memory - on failure report error and abort
 */

void	* inx_alloc( size_t size )
{

    register void * t = malloc( size );		/* allocate some stuff */

    /* t is 0 if allocation failed */
    if ( !t ) {
	/* print a pretty error message and abort */
	inx_error(
	    inx_last,			/* the place where the error occured */
	    INX_TABLE,			/* which error table */
	    INXE_ALLOC,			/* allocate error */
	    INX_LINE | INX_ABORT |	/* abort and print line */
	    INX_SEVERE
	);
    }

    /* return the address */
    return t;
    
}

/* ============================ INX_FREE ============================= */
/* deallocate memory - on failure report error and abort
 * Some implementations of 'malloc' and 'free' detect heap rubbishing.
 * Some don't. If the symbol INX_FREE_CHECK is set the checking will
 * be performed.
 */

void inx_free( pointer )
char	* pointer;
{

#if INX_FREE_CHECK

    if ( INX_BAD_FREE free( pointer ) ) {
	inx_error(
	    inx_last,			/* the place where the error occured */
	    INX_TABLE,			/* which error table */
	    INXE_DEALLOC,		/* deallocate error */
	    INX_LINE | INX_ABORT |	/* abort and print line */
	    INX_INTERNAL
	);
    }

#else

    free( pointer );

#endif
    
}

/* ============================ INX_PRINTLINE ========================= */
/* this function prints a string given and expands tabs
 */

void inx_printline( s, col )
register char	* s;		/* the buffer */
register int	  col;		/* the starting column number */
{
		
    while ( * s ) {
	switch ( * s ) {
	    case '\r' : {
		break;
	    }
	    case '\t' : {
		col ++;
		fputc( ' ', INX_ERRFILE );
		while ( col & 0x7 ) {
		    col ++;
		    fputc( ' ', INX_ERRFILE );
		}
		break;
	    }
	    case '\n' : {
		fputc( '\n', INX_ERRFILE );
		col = 0;
		break;
	    }
	    default : {
		fputc( * s, INX_ERRFILE );
		col ++;
	    }
	}
	s ++;
    }
}

/* ============================== INX_ERROR =========================== */
/* The error printing routine. This function will print
 * an error message and if required it will print the line at which
 * it occurred with an arrow at the position of the error.
 * The function can be also made to abort with the error.
 */

inx_posi	inx_lastplace = { 0 };	/* the last position record given */

short		inx_flag;

/* store the last place a line was printed so we don't print the
 * one line many times on different errors
 */
static inx_posi	lastplace[] = {{
	0,		/* char		* inx_filename;	* the file name */
	0,		/* long		  inx_line;	* the line number */
	0,		/* short	  inx_colm;	* position in line */
	0		/* long		  inx_offs;	* offset into file */
			/* 				* for current line */
}};


int inx_error(
inx_posi	 * place,		/* the place that the error occured */
char		** table,		/* the table of error messages */
int		   err,			/* the error number */
unsigned int	   flags,		/* what to actually do */
		    ...
) {
    va_list	ap;
    char	* extra = flags & INX_EXTRA ? va_start(ap, flags), va_arg(ap, char *) : 0;
    int		i;
    int		j;
    char	buff[ INX_MAXLINE ];	/* the buffer to read in the line */
    long	offs;

    INX_TALLY( flags )++;		/* increment the number of times
					 * this type of error occurs */
	    
    /* is this type of messages suppressed */
    if ( INX_SUPPRESS( flags ) ) {
	goto chkabort;
    }

    i = ( int ) table[ 0 ];		/* this contains an error offset */

    /* error table and number convention
     * 1. The first element in the table contains an error number offset
     * 2. The second element contains a "No Error" message.
     * 3. Error numbers are consistent with the table such that
     *	  the error number minus the offset gives an index into
     *	  the error table for the text to that message.
     * If an abort request is given the "err" number is given to exit
     * except when it is a "No Error", and that time it is given 0.
     * This allows it to be used with such things as MAKE etc.
     */

    /* do we not require to print an error message or can't we get a line */
    if ( !( flags & INX_LINE ) || ( !inx_ptr )) {

	if ( ! place ) {
	    place = inx_nullplace;
	}

other_type:
	/* print a terse unix type "filename : 36 Syntax" message ! */
	fprintf(
	    INX_ERRFILE,		/* which file ( stderr ) or ( stdout ) */
	    "%s : %3ld %c: %s",
	    place->inx_filename,	/* ! */
	    place->inx_line,		/* ! */
	    * INX_STATNAMES( flags ),	/* The first character of the error
	    				 * type */
	    table[ err - i ]
	);

	/* is there extra information */
	if ( flags & INX_EXTRA ) {
	    fprintf( INX_ERRFILE, " - %s\n", extra );
	} else {
	    fputc( '\n', INX_ERRFILE );
	}

    } else {
	/* print the verbose type of message */

	/* is this place valid */
	if ( !place ) {
	    /* no it isn't. go do the other type */
	    goto other_type;
	}

	/* check to see if we have not already printed the last line */
	if ( /* are the lines the same and files the same */
	    ( place->inx_line != lastplace->inx_line ) ||
	    ( place->inx_filename != lastplace->inx_filename )
	) {
	
	    /* first check to see if the file is available ! */

	    if ( inx_ptr->inx_place->inx_filename != place->inx_filename ) {
		/* no it isn't. go do the other type */
		goto other_type;
	    }

	    /* save the file position */
	    offs = ftell( inx_ptr->inx_fs );

	    /* place the file at the position it was when the error occurred */
	    if ( fseek( inx_ptr->inx_fs, place->inx_offs, 0 ) == EOF ) {
		inx_error(
		    inx_ptr->inx_place,	/* the place where the error occured */
		    INX_TABLE,		/* which error table */
		    INXE_SKERR,		/* seek error */
		    INX_NOLINE		/* abort and print line */
		);
	    
reseek:
		fseek( inx_ptr->inx_fs, offs, 0 );	/* try to go back */
		goto other_type;
	    }

	    /* we could not read the string */
	    if ( !fgets( buff, INX_MAXLINE -1, inx_ptr->inx_fs ) ) {

		inx_error(
		    inx_ptr->inx_place,	/* the place where the error occured */
		    INX_TABLE,		/* which error table */
		    INXE_GETSERR,	/* seek error */
		    INX_NOLINE		/* abort and print line */
		);

		goto reseek;
	    }

	    /* we are now set ! */

	    /* print the line the error occured on */
	    fprintf(
		INX_ERRFILE,		/* which file ( stderr ) or ( stdout ) */
		"%s : %3ld : ",
		place->inx_filename,	/* ! */
		place->inx_line		/* ! */
	    );
	    inx_printline( buff, 0 );

	} else {
	    offs = -1;
	}

	fprintf(
	    INX_ERRFILE,		/* which file ( stderr ) or ( stdout ) */
	    "%s : %3ld %c",
	    place->inx_filename,	/* ! */
	    place->inx_line,		/* ! */
	    * INX_STATNAMES( flags )	/* The first character of the error
	    				 * type */
	);

	/* see if we can print the error before or after */
	if ( strlen( table[ err - i ] ) > place->inx_colm - 3 ) {
	    /* must be on the other side */
	    /* i.e. x.c : 5 : main(argc,argv,) */
	    /*      x.c : 5 :---------------^- unexpected '(' */

	    for ( j = 0; j < place->inx_colm; j ++ ) {
		fputc( '-', INX_ERRFILE );
	    }

	    fprintf(
		INX_ERRFILE,		/* which file ( stderr ) or ( stdout ) */
		"^- %s",		/* the last bit */
		table[ err - i ]	/* the error message */
	    );

	} else {
	    /* must be on the other side */
	    /* i.e. x.c : 5 : main( argc, argv, ) */
	    /*      x.c : 5 : Unexpected ')'----^ */

	    fprintf(
		INX_ERRFILE,		/* which file ( stderr ) or ( stdout ) */
		" %s ",			/* the last bit */
		table[ err - i ]	/* the error message */
	    );
	    
	    for ( j = strlen( table[ err - i ] ) + 2; j < place->inx_colm; j ++ ) {
		fputc( '-', INX_ERRFILE );
	    }

	    fputs( "^-", INX_ERRFILE );

	}

	/* is there extra information */
	if ( flags & INX_EXTRA ) {
	    fprintf( INX_ERRFILE, " %s\n", extra );
	} else {
	    fputc( '\n', INX_ERRFILE );
	}

	/* restore offset only if changed */
	if ( offs != -1 ) {
	    /* now seek back to where we came from */
	    if ( EOF == fseek( inx_ptr->inx_fs, offs, 0 ) ) {	/* try to go back */
		fprintf( stderr, "Cannot seek on this file - abort\n" );
		perror( place->inx_filename );
		exit( -1 );
	    }
	}
	 
	/* store where we were last */
	* lastplace = * place;

    }

chkabort:	/* see if an abort is required */

    /* has an abort been requested or is there too  many errors */
    if ( 
	(flags & INX_ABORT)
    ) {
	/* print statistics first */
	inx_stats();
	exit( err );
    }

    /* has an abort been requested or is there too  many errors */
    if ( 
	(INX_MAXERR( flags ) && ( INX_MAXERR( flags ) <= INX_TALLY( flags ) ))
    ) {
	
	inx_error(
	    place,			/* the place where the error occured */
	    INX_TABLE,			/* which error table */
	    INXE_LIMIT,			/* error limit */ 
	    INX_LINE | INX_ABORT	/* abort and print line */
	    | INX_SEVERE
	);

	exit( err );			/* Should not get here */
    }

    return 0;
}

/* ============================ INX_STATS ======================== */
/* This function prints error statistics on the ERRFILE.
 *
 */

/* STATNAMES contains strings that describe the message types
 * The first column of the types is used to print out on
 * the actual error messages
 */

/*   v---- Make messages such that the first column is is	    *
 *   v     not repeated in the messages - this is used for message  *
 *   v	   output 						    *
 *  "Error",				 * Keep these texts singular
 *  "Warning",				 * so that when more that
 *  "Severe Error",			 * one error occurs an "s"
 *  "Informatiional Message",		 * can be appended to make it
 *  "Debug message",			 * plural *
 *  "5 Type error",
 *  "6 Type error",
 *  "Compiler internal error",
 */


inx_errtype inx_errtable[] = {			/* where the error information */
  {						/* is kept */
    0,			/* long	 inx_tally;	 how many of these errors
    			 *			 have occurred */
    "Error",		/* char	* inx_name;	 the name of this type of
    			 *			 error */
    50,			/* int	inx_maxerr;	 Maximum number of errors of
    			 *			 this type to allow (0=infin)
    			 *			 if more than this many errors
			 * 			 occur abort will happen */
    0,			/* INX_BOOL inx_suppress;Suppress this type of error
    			 *			 from output (BOOLEAN) */
    1,			/* INX_BOOL inx_major;	 does this error constitue
    			 *			 a major error type */
  },
  {
    0,			/* long	 inx_tally;	 how many of these errors
    			 *			 have occurred */
    "Warning",		/* char	* inx_name;	 the name of this type of
    			 *			 error */
    0,			/* int	inx_maxerr;	 Maximum number of errors of
    			 *			 this type to allow (0=infin)
    			 *			 if more than this many errors
			 * 			 occur abort will happen */
    0,			/* INX_BOOL inx_suppress;Suppress this type of error
    			 *			 from output (BOOLEAN) */
    0,			/* INX_BOOL inx_major;	 does this error constitue
    			 *			 a major error type */
  },
  {	
    0,			/* long	 inx_tally;	 how many of these errors
    			 *			 have occurred */
    "Severe Error",	/* char	* inx_name;	 the name of this type of
    			 *			 error */
    10,			/* int	inx_maxerr;	 Maximum number of errors of
    			 *			 this type to allow (0=infin)
    			 *			 if more than this many errors
			 * 			 occur abort will happen */
    0,			/* INX_BOOL inx_suppress;Suppress this type of error
    			 *			 from output (BOOLEAN) */
    1,			/* INX_BOOL inx_major;	 does this error constitue
    			 *			 a major error type */
  },
  {	
    0,			/* long	 inx_tally;	 how many of these errors
    			 *			 have occurred */
    "Informatiional Message",
			/* char	* inx_name;	 the name of this type of
    			 *			 error */
    0,			/* int	inx_maxerr;	 Maximum number of errors of
    			 *			 this type to allow (0=infin)
    			 *			 if more than this many errors
			 * 			 occur abort will happen */
    0,			/* INX_BOOL inx_suppress;Suppress this type of error
    			 *			 from output (BOOLEAN) */
    0,			/* INX_BOOL inx_major;	 does this error constitue
    			 *			 a major error type */
  },
  {	
    0,			/* long	 inx_tally;	 how many of these errors
    			 *			 have occurred */
    "Debug Message",	/* char	* inx_name;	 the name of this type of
    			 *			 error */
    0,			/* int	inx_maxerr;	 Maximum number of errors of
    			 *			 this type to allow (0=infin)
    			 *			 if more than this many errors
			 * 			 occur abort will happen */
    0,			/* INX_BOOL inx_suppress;Suppress this type of error
    			 *			 from output (BOOLEAN) */
    0,			/* INX_BOOL inx_major;	 does this error constitue
    			 *			 a major error type */
  },
  {	
    0,			/* long	 inx_tally;	 how many of these errors
    			 *			 have occurred */
    "5 Type error",	/* char	* inx_name;	 the name of this type of
    			 *			 error */
    1,			/* int	inx_maxerr;	 Maximum number of errors of
    			 *			 this type to allow (0=infin)
    			 *			 if more than this many errors
			 * 			 occur abort will happen */
    0,			/* INX_BOOL inx_suppress;Suppress this type of error
    			 *			 from output (BOOLEAN) */
    1,			/* INX_BOOL inx_major;	 does this error constitue
    			 *			 a major error type */
  },
  {	
    0,			/* long	 inx_tally;	 how many of these errors
    			 *			 have occurred */
    "6 Type error",	/* char	* inx_name;	 the name of this type of
    			 *			 error */
    1,			/* int	inx_maxerr;	 Maximum number of errors of
    			 *			 this type to allow (0=infin)
    			 *			 if more than this many errors
			 * 			 occur abort will happen */
    0,			/* INX_BOOL inx_suppress;Suppress this type of error
    			 *			 from output (BOOLEAN) */
    1,			/* INX_BOOL inx_major;	 does this error constitue
    			 *			 a major error type */
  },
  {	
    0,			/* long	 inx_tally;	 how many of these errors
    			 *			 have occurred */
    "Compiler internal error",
			/* char	* inx_name;	 the name of this type of
    			 *			 error */
    1,			/* int	inx_maxerr;	 Maximum number of errors of
    			 *			 this type to allow (0=infin)
    			 *			 if more than this many errors
			 * 			 occur abort will happen */
    0,			/* INX_BOOL inx_suppress;Suppress this type of error
    			 *			 from output (BOOLEAN) */
    1,			/* INX_BOOL inx_major;	 does this error constitue
    			 *			 a major error type */
  },
};

int inx_stats( void )
{
    int		i;
    int		printed = 0;
    long	total = 0;
    
    /* go through all the error types */
    for ( i = 0; i < INX_ERTYPES; i ++ ) {

	/* if there is any of these errors */

	if ( INX_TALLY( i ) ) {

	    /* check to see if we need to print a heading */
	    if ( !printed ) {
		fprintf(
		    INX_ERRFILE,
		    "----- Error statistics ----------------\n"
		);
	    }

	    printed ++;		/* increment the number of lines printed */

	    total += INX_TALLY( i );	/* perform total */
	    
	    /* Messages are of the form
	     *    30 Informational messages
	     * or
	     *    30 Suppressed Informational messages
	     */

	    fprintf(			/* print the message */
		INX_ERRFILE,
		"%5ld %s%s%s\n",
		INX_TALLY( i ),
		( INX_SUPPRESS( i ) ? "Suppressed " : "" ),
		INX_STATNAMES( i ),
		( INX_TALLY( i ) > 1 ? "s" : "" )
	    );

	}
    }

    /* if more than one error message was printed */
    if ( printed > 1 ) {
	fprintf(
	    INX_ERRFILE,
	    "%5ld TOTAL\n",
	    total
	);
    }

    return 0;
}

/* =========================== INX_ISERROR ========================== */
/* This function returns true if there was major errors
 * or false if there was only informational messages ( or warings )
 *  This is taken from the data in the 'inx_errtable' array
 * that identifies what is a "major" error
 * 
 */

int inx_iserror( void )
{
    int		i;
    
    /* go through all the error types */
    for ( i = 0; i < INX_ERTYPES; i ++ ) {
	/* if a mojor error has a non zero tally */
	if ( INX_TALLY( i ) && inx_errtable[ i ].inx_major ) {
	    /* there is a major error */
	    return 1;
	}
    }
    
    /* having looked at all the errors there is obviosly
     * no major errors - return false */
    
    return 0;
}

/* =========================== INX_CLEAR ========================== */
/* Clears error tallies.
 * 
 */

int inx_clear( void )
{
    int		i;
    
    for ( i = 0; i < INX_ERTYPES; i ++ ) {
	INX_TALLY( i ) = 0;
    }

    return 0;
}

