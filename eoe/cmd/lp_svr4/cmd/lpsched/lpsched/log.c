/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 252.227-7013.  |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Avenue                               |
 * |         Sunnyvale, California 94088-3650, USA             |
 * |-----------------------------------------------------------|
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/cmd/lpsched/lpsched/RCS/log.c,v 1.1 1992/12/14 11:33:37 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#define	WHO_AM_I	I_AM_LPSCHED
#include "oam.h"

#if	defined(__STDC__)
#include "stdarg.h"
#else
#include "varargs.h"
#endif

#include "lpsched.h"
#include "debug.h"

#include <locale.h>

#if	defined(__STDC__)
static void		log ( char * , va_list );
static void		lplog ( int, int, long int, va_list );
#else
static void		log();
static void		lplog();
#endif

/**
 ** open_logfile() - OPEN FILE FOR LOGGING MESSAGE
 ** close_logfile() - CLOSE SAME
 **/

FILE *
#if	defined(__STDC__)
open_logfile (
	char *			name
)
#else
open_logfile (name)
	char			*name;
#endif
{
	ENTRY ("open_logfile")

	register char		*path;

	register FILE		*fp;


#if	defined(MALLOC_3X)
	/*
	 * Don't rely on previously allocated pathnames.
	 */
#endif
	path = makepath(Lp_Logs, name, (char *)0);
	fp = fopen(path, "a");
	Free (path);
	return (fp);
}

void
#if	defined(__STDC__)
close_logfile (
	FILE *			fp
)
#else
close_logfile (fp)
	FILE			*fp;
#endif
{
	ENTRY ("close_logfile")

	fclose (fp);
	return;
}

/**
 ** fail() - LOG MESSAGE AND EXIT (ABORT IF DEBUGGING)
 **/

/*VARARGS1*/
void
#if	defined(__STDC__)
fail (
	char *			format,
	...
)
#else
fail (format, va_alist)
	char			*format;
	va_dcl
#endif
{
	ENTRY ("fail")

	va_list			ap;
    
#if	defined(__STDC__)
	va_start (ap, format);
#else
	va_start (ap);
#endif
	log (format, ap);
	va_end (ap);

#if	defined(DEBUG)
	if (debug & DB_ABORT)
		abort ();
	else
#endif
		exit (1);
	/*NOTREACHED*/
}

/**
 ** lpfail() - LOG INTERNATIONALIZED MESSAGE AND EXIT (ABORT IF DEBUGGING)
 **/

/*VARARGS1*/
void
#if	defined(__STDC__)
lpfail (
        int                     severity,
        int                     seqnum,
        long int                arraynum,
	...
)
#else
lpfail (severity, seqnum, arraynum, va_alist)
        int                     severity;
        int                     seqnum;
        long int                arraynum;
	va_dcl
#endif
{
	DEFINE_FNNAME (lpfail)

	va_list			ap;
    
#if	defined(__STDC__)
	va_start (ap, arraynum);
#else
	va_start (ap);
#endif
	lplog (severity, seqnum, arraynum, ap);
	va_end (ap);

#if	defined(DEBUG)
	if (debug & DB_ABORT)
		abort ();
	else
#endif
		exit (1);
	/*NOTREACHED*/
}

/**
 ** note() - LOG MESSAGE
 **/

/*VARARGS1*/
void
#if	defined(__STDC__)
note (
	char *			format,
	...
)
#else
note (format, va_alist)
	char			*format;
	va_dcl
#endif
{
	ENTRY ("note")

	va_list			ap;

#if	defined(__STDC__)
	va_start (ap, format);
#else
	va_start (ap);
#endif
	log (format, ap);
	va_end (ap);
	return;
}

/**
 ** lpnote() - LOG AN INTERNATIONALIZED MESSAGE
 **/

/*VARARGS1*/
void
#if	defined(__STDC__)
lpnote (
        int                     severity,
        int                     seqnum,
        long int                arraynum,
	...
)
#else
lpnote (severity, seqnum, arraynum, va_alist)
        int                     severity;
        int                     seqnum;
        long int                arraynum;
	va_dcl
#endif
{
	DEFINE_FNNAME (lpnote)

	va_list			ap;

#if	defined(__STDC__)
	va_start (ap, arraynum);
#else
	va_start (ap);
#endif
	lplog (severity, seqnum, arraynum, ap);
	va_end (ap);
	return;
}

/**
 ** schedlog() - LOG MESSAGE IF IN DEBUG MODE
 **/

/*VARARGS1*/
void
#if	defined(__STDC__)
schedlog (
	char *			format,
	...
)
#else
schedlog (format, va_alist)
	char			*format;
	va_dcl
#endif
{
	ENTRY ("schedlog")

	va_list			ap;

#if	defined(DEBUG)
	if (debug & DB_SCHEDLOG) {

#if	defined(__STDC__)
		va_start (ap, format);
#else
		va_start (ap);
#endif
		log (format, ap);
		va_end (ap);

	}
#endif
	return;
}

/**
 ** mallocfail() - COMPLAIN ABOUT MEMORY ALLOCATION FAILURE
 **/

void
#if	defined(__STDC__)
mallocfail (
	void
)
#else
mallocfail ()
#endif
{
	ENTRY ("mallocfail")

	lpfail (ERROR, E_SCH_MEMFAILED);
	/*NOTREACHED*/
}

/**
 ** log() - LOW LEVEL ROUTINE THAT LOGS MESSSAGES
 **/

static void
#if	defined(__STDC__)
log (
	char *			format,
	va_list			ap
)
#else
log (format, ap)
	char			*format;
	va_list			ap;
#endif
{
	ENTRY ("log")

	int			close_it;

	FILE			*fp;

	static int		nodate	= 0;


	if (!am_in_background) {
		fp = stdout;
		close_it = 0;
	} else {
		if (!(fp = open_logfile("lpsched")))
			return;
		close_it = 1;
	}

	if (am_in_background && !nodate) {
		long			now;

		time (&now);
		fprintf (fp, "%24.24s: ", ctime(&now));
	}
	nodate = 0;

	vfprintf (fp, format, ap);
	if (format[strlen(format) - 1] != '\n')
		nodate = 1;

	if (close_it)
		close_logfile (fp);
	else
		fflush (fp);

	return;
}

/**
 ** lplog() - LOW LEVEL ROUTINE THAT LOGS INTERNATIONALIZED MESSSAGES
 **/

static void
#if	defined(__STDC__)
lplog (
        int			severity,
	int			seqnum,
	long int		arraynum,
	va_list			ap
)
#else
lplog (severity, seqnum, arraynum, ap)
        int			severity;
	int			seqnum;
	long int		arraynum;
	va_list			ap;
#endif
{
	DEFINE_FNNAME (lplog)

	int			close_it;

	FILE			*fp;

	static int		nodate	= 0;

        char                    buf[MSGSIZ];
        char                    text[MSGSIZ];
        char                    msg_text[MSGSIZ];

	if (!am_in_background) {
		fp = stdout;
		close_it = 0;
	} else {
		if (!(fp = open_logfile("lpsched")))
			return;
		close_it = 1;
	}

	if (am_in_background && !nodate) {
		long			now;

		time (&now);
		fprintf (fp, "%24.24s: ", ctime(&now));
	}
	nodate = 0;
        (void)setlocale(LC_ALL, "");
        setcat("uxlp");
        setlabel(who_am_i);
        sprintf(msg_text,":%d:%s\n",seqnum,agettxt(arraynum,buf,MSGSIZ));
        vpfmt(fp,severity,msg_text,ap);
	if (msg_text[strlen(msg_text) - 1] != '\n')
		nodate = 1;
        sprintf(text,"%s",agettxt(arraynum + 1,buf,MSGSIZ));
        if (strncmp(text, "", 1) != 0)  {
           sprintf(msg_text,":%d:%s\n",seqnum + 1,text);
           pfmt(fp,MM_ACTION,msg_text);
        }

	if (close_it)
		close_logfile (fp);
	else
		fflush (fp);

	return;
}
   
/**
 ** execlog()
 **/

/*VARARGS1*/
void
#if	defined(__STDC__)
execlog (
	char *			format,
	...
)
#else
execlog (format, va_alist)
	char			*format;
	va_dcl
#endif
{
	ENTRY ("execlog")

	va_list			ap;

#if	defined(DEBUG)
	FILE			*fp	= open_logfile("exec");

	time_t			now = time((time_t *)0);

	char			buffer[BUFSIZ];

	EXEC *			ep;

	static int		nodate	= 0;

#if	defined(__STDC__)
	va_start (ap, format);
#else
	va_start (ap);
#endif
	if (fp) {
		setbuf (fp, buffer);
		if (!nodate)
			fprintf (fp, "%24.24s: ", ctime(&now));
		nodate = 0;
		if (!STREQU(format, "%e")) {
			vfprintf (fp, format, ap);
			if (format[strlen(format) - 1] != '\n')
				nodate = 1;
		} else switch ((ep = va_arg(ap, EXEC *))->type) {
		case EX_INTERF:
			fprintf (
				fp,
				"      EX_INTERF %s %s\n",
				ep->ex.printer->printer->name,
				ep->ex.printer->request->secure->req_id
			);
			break;
		case EX_SLOWF:
			fprintf (
				fp,
				"      EX_SLOWF %s\n",
				ep->ex.request->secure->req_id
			);
			break;
		case EX_ALERT:
			fprintf (
				fp,
				"      EX_ALERT %s\n",
				ep->ex.printer->printer->name
			);
			break;
		case EX_FALERT:
			fprintf (
				fp,
				"      EX_FALERT %s\n",
				ep->ex.form->form->name
			);
			break;
		case EX_PALERT:
			fprintf (
				fp,
				"      EX_PALERT %s\n",
				ep->ex.pwheel->pwheel->name
			);
			break;
		case EX_NOTIFY:
			fprintf (
				fp,
				"      EX_NOTIFY %s\n",
				ep->ex.request->secure->req_id
			);
			break;
		default:
			fprintf (fp, "      EX_???\n");
			break;
		}
		close_logfile (fp);
	}
#endif
	return;
}
