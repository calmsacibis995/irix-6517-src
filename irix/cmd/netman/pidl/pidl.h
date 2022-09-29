#ifndef PIDL_H
#define PIDL_H
/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 * PIDL global declarations.
 */
#include <stdio.h>
#include "exception.h"

/*
 * Parser/scanner exports.
 */
extern void	addimportdir(char *);		/* string arg must be saved */
extern void	pushinput(FILE *, char *, int);	/* string arg must be saved */
extern int	yyparse();
extern void	yyerror(char *, ...);

extern int	yylineno;		/* current line number */
extern char	*yyfilename;		/* current file name in safe store */

/*
 * Main driver exports.
 */
typedef struct pragma {
	unsigned int	p_refcnt;	/* reference count */
	struct symbol	*p_sym;		/* symbol to which pragma applies */
	char		*p_name;	/* pragma name */
	char		*p_value;	/* and string value */
	struct pragma	*p_next;	/* others for the same symbol */
} Pragma;

#define	holdpragma(p)	((p)->p_refcnt++, (p))

extern Pragma	*addpragma(char *);
extern char	*haspragma(struct symbol *, char *);
extern void	deletepragma(Pragma *);
extern char	*basename(char *, char *);
extern char	*dirname(char *);
extern void	terminate(int);

/*
 * Global variables.
 */
extern struct scope	*innermost;	/* innermost scope during parse */
extern struct scope	strings;	/* string table */
extern struct scope	imports;	/* imported module table */
extern unsigned short	lastfid;	/* last field identifier */
extern struct treenode	*parsetree;	/* tree built by yyparse */
extern int		status;		/* exit status, bumped on error */
extern FILE		*ptfile;	/* pass-through file pointer */
extern char		ptname[];	/* pass-through tmp file name */

#endif
