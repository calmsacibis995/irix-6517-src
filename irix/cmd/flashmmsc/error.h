/*
 * Error routines -header file.
 * 
 * $Id: error.h,v 1.1 1997/12/11 03:03:02 jfd Exp $
 */

#ifndef _error_h
#define _error_h

extern	char   *sys_errlist[];
extern	int	errno;

#define STRERR	sys_errlist[errno]

void	warning(char *fmt, ...);
void	fatal(char *fmt, ...);
void	debug(char *fmt, ...);
void	perr(char *fmt, ...);
void	debug_enable(char *filename);	/* NULL for disable */
void	silent_enable(int flag);

#endif /* _error_h */
