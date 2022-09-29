/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)v7.local.h	5.2 (Berkeley) 9/19/85
 */

/*
 * Declarations and constants specific to an installation.
 *
 * Vax/Unix version 7.
 */

#define	GETHOST				/* System has gethostname syscall */
#ifdef	GETHOST
#define	LOCAL		EMPTYID		/* Dynamically determined local host */
#else
#define	LOCAL		'j'		/* Local host id */
#endif	/* GETHOST */

#define	MAIL		"/bin/rrmail"	/* Name of mail sender */
#define SENDMAIL	"/usr/lib/sendmail"
					/* Name of classy mail deliverer */
#if defined(SVR3) || defined(_SVR4_SOURCE)
#define	EDITOR		"/usr/bin/ex"	/* Name of text editor */
#define	VISUAL		"/usr/bin/vi"	/* Name of display editor */
#else
#define	EDITOR		"/bin/ex"	/* Name of text editor */
#define	VISUAL		"/bin/vi"	/* Name of display editor */
#endif
#define	SHELL		"/bin/csh"	/* Standard shell */
#if defined(SVR3) || defined(_SVR4_SOURCE)
#define	MORE		"/usr/bsd/more"	/* Standard output pager */
#else
#define	MORE		"/bin/more"	/* Standard output pager */
#endif
#define	HELPFILE	"/usr/lib/Mail.help"
					/* Name of casual help file */
#define	THELPFILE	"/usr/lib/Mail.help.~"
#define	POSTAGE		"/usr/adm/maillog"
					/* Where to audit mail sending */
					/* Name of casual tilde help */
#define	UIDMASK		0177777		/* Significant uid bits */
#define	MASTER		"/usr/lib/Mail.rc"
#define	APPEND				/* New mail goes to end of mailbox */
#define CANLOCK				/* Locking protocol actually works */
#define	UTIME				/* System implements utime(2) */

#define TMPDIR		"/tmp"		/* Default location of temp files */

#ifndef _SVR4_SOURCE
#ifndef SVR3
#ifndef VMUNIX
#include "sigretro.h"			/* Retrofit signal defs */
#endif /* VMUNIX */
#endif /* SVR3 */
#endif /* _SVR4_SOURCE */
