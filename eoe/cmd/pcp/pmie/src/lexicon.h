/*
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

/***********************************************************************
 * lexicon.h - lexical scanner
 ***********************************************************************/
/* $Id: lexicon.h,v 1.1 1999/04/28 10:06:17 kenmcd Exp $ */

#ifndef LEXICON_H
#define LEXICON_H

/***********************************************************************
 * ONLY FOR USE BY: lexicon.c and syntax.c
 ***********************************************************************/

#define LEX_MAX 254			/* max length of token */

/* scanner input context stack entry */
typedef struct lexin {
    struct lexin *prev;                 /* calling context on stack */
    FILE         *stream;               /* rule input stream */
    char         *macro;                /* input from macro definition */
    char         *name;                 /* file/macro name */
    int          lno;                   /* current line number */
    int          cno;                   /* current column number */
    int          lookin;                /* lookahead buffer input index */
    int          lookout;               /* lookahead buffer output index */
    signed char  look[LEX_MAX + 2];     /* lookahead ring buffer */
} LexIn;

extern LexIn    *lin;                   /* current input context */


/***********************************************************************
 * public
 ***********************************************************************/

/* initialize scan of new input file */
int lexInit(char *);

/* finalize scan of input stream */
void lexFinal(void);

/* not end of input stream? */
int lexMore(void);

/* discard input until ';' or EOF */
void lexSync(void);

/* scanner main function */
int yylex(void);

/* yacc parser */
int yyparse(void);

#endif /* LEXICON_H */

