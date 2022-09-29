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

#ident "$Id: pmlc.h,v 2.7 1997/09/12 01:08:24 markgw Exp $"

#ifndef _PMLC_H
#define _PMLC_H

extern int pmDebug;

/* config file parser states (bit field values) */
#define GLOBAL  	0
#define INSPEC  	1
#define INSPECLIST	2

/* timezone to use when printing status */
#define TZ_LOCAL	0
#define TZ_LOGGER	1
#define TZ_OTHER	2

/* timezone variables */
extern char	*tz;			/* for -Z timezone */
extern int	tztype;			/* timezone for status cmd */

/* generic error message buffer */
extern char     emess[];

/* parse summary back from yacc to main */
extern char	*hostname;
extern int	state;
extern int	control;
extern int	mynumb;
extern int	mystate;

extern void yyerror(char *);
extern void yywarn(char *);
extern int yylex(void);
extern int yyparse(void);
extern void beginmetrics(void);
extern void endmetrics(void);
extern void beginmetgrp(void);
extern void endmetgrp(void);
extern void addmetric(const char *);
extern void addinst(char *, int);
extern int connected(void);
extern int still_connected(int);
extern int metric_cnt;
#ifdef PCP_DEBUG
extern void dumpmetrics(FILE *);
#endif

/* lex I/O routines */
extern char input(void);
extern void unput(char);
extern char lastinput(void);

/* connection routines */
extern int ConnectLogger(char *, int *, int *);
extern int ConnectPMCD(void);

/* information about an instance domain */
typedef struct {
    pmInDom	indom;
    int		n_insts;
    int		*inst;
    char	**name;
} indom_t;

/* a metric plus an optional list of instances */
typedef struct {
    char		*name;
    pmID		pmid;
    int			indom;		/* index of indom (or -1) */
    int			n_insts;	/* number of insts for this metric */
    int			*inst;		/* list of insts for this metric */
    struct {
	unsigned	selected : 1,	/* apply instances to metric? */
			new : 1,	/* new in current PMNS subtree */
			has_insts : 1;	/* free inst list? */
    } status;
} metric_t;

#endif /* _PMLC_H */
