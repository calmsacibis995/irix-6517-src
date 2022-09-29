/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/widec.h	1.1"

#include <sgidefs.h>
#include <stdio.h>

/* declaration of a wide character data type			*/

#ifndef _WCHAR_T
#define _WCHAR_T
 typedef __int32_t  wchar_t;
#endif

/* Character based input and output functions			*/
#ifdef	__STDC__
extern	int		fgetwc(FILE *), fputwc(wchar_t, FILE *); 
extern	int		ungetwc(wchar_t, FILE *);
extern	wchar_t		*getws(wchar_t *), *fgetws(wchar_t *, int, FILE *);
extern	int		putws(wchar_t *), fputws(wchar_t *, FILE *);
#else
extern	int		fgetwc(), fputwc(), ungetwc();
extern	wchar_t		*getws(), *fgetws();
extern	int		putws(), fputws();
#endif
#define	getwc(p)	fgetwc((p))
#define	putwc(x, p)	fputwc((x), (p))
#define getwchar()	getwc(stdin)
#define putwchar(x)	putwc((x), stdout)

/* wchar_t string operation functions				*/

#ifdef	__STDC__
extern wchar_t
	*wscpy(wchar_t *, wchar_t *),
	*wsncpy(wchar_t *, wchar_t *, int),
	*wscat(wchar_t *, wchar_t *),
	*wsncat(wchar_t *, wchar_t *, int),
	*wschr(wchar_t *, int),
	*wsrchr(wchar_t *, int),
	*wspbrk(wchar_t *, wchar_t *),
	*wstok(wchar_t *, wchar_t *);
extern int
	wscmp(wchar_t *, wchar_t *),
	wsncmp(wchar_t *, wchar_t *, int),
	wslen(wchar_t *),
	wsspn(wchar_t *, wchar_t *),
	wscspn(wchar_t *, wchar_t *);
extern char
	*wstostr(char *, wchar_t *);
extern	wchar_t
	*strtows(wchar_t *, char *);
#else
extern wchar_t
	*wscpy(),
	*wsncpy(),
	*wscat(),
	*wsncat(),
	*wschr(),
	*wsrchr(),
	*wspbrk(),
	*wstok();
extern int
	wscmp(),
	wsncmp(),
	wslen(),
	wsspn(),
	wscspn();
extern char
	*wstostr();
extern	wchar_t
	*strtows();
#endif
