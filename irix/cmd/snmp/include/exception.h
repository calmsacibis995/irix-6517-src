#ifndef EXCEPTION_H
#define EXCEPTION_H
/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Exception handling declarations.
 *
 * To raise an exception, call exc_raise(errno, format, ...) where errno is
 * zero or a system error number (cf. intro(2)) and format is a printf-like
 * format string.  If exc_raise returns, then exc_error contains zero or the
 * system error number associated with the current exception and exc_message
 * points to a dynamically allocated string.
 *
 * To enable automatic program failure on exception, set exc_autofail to 1.
 * To disable a function from raising exceptions which the caller wishes to
 * raise, set exc_defer to the maximum number of exc_raise calls made by the
 * subroutine.  Use exc_openlog to route exc_perror output to syslogd rather
 * than to stderr.
 */
#ifdef _VA_LIST_
void		exc_vraise(int error, const char *format, va_list ap);
#endif
void		exc_raise(int, const char *, ...);
void		exc_perror(int, const char *, ...);
void		exc_openlog(char *, int, int);
void		exc_errlog(int, int, const char *, ...);

extern char	*exc_progname;		/* program name message prefix */
extern int	exc_autofail;		/* if true, fail on undeferred raise */
extern int	exc_defer;		/* times to defer raising exception */
extern int	exc_error;		/* raised error number */
extern int	exc_level;		/* syslog-like error level */
extern char	*exc_message;		/* raised formatted message */
extern void	(*exc_vperr)();		/* varargs interface to exc_perror */
extern void	(*exc_vplog)();		/* varargs interface to exc_errlog */

#endif
