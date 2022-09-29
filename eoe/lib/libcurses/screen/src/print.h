/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/print.h	1.2"

/* externs from iexpand.c, cexpand.c */
extern void tpr(FILE *, char *);
extern int cpr(FILE *, char *);
extern char *cexpand(char *), *iexpand(char *); 
extern char *cconvert(char *), *rmpadding(char *, char *, int *);
extern char *infotocap(char *, int *);

/* externs from print.c */
enum printtypes
    {
    pr_none,
    pr_terminfo,		/* print terminfo listing */
    pr_cap,			/* print termcap listing */
    pr_longnames		/* print C variable name listing */
    };

extern void pr_onecolumn(int);
extern void pr_caprestrict(int);
extern void pr_width(int);
extern void pr_init(enum printtypes);
extern void pr_heading(char *, char *);
extern void pr_bheading(void);
extern void pr_boolean(char *, char *, char *, int);
extern void pr_bfooting(void);
extern void pr_nheading(void);
extern void pr_number(char *, char *, char *, int);
extern void pr_nfooting(void);
extern void pr_sheading(void);
extern void pr_string(char *, char *, char *, char *);
extern void pr_sfooting(void);
