#ifndef __STDIO_H__
#define __STDIO_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.75 $"
/*
*
* Copyright 1992-1996 Silicon Graphics, Inc.
* All Rights Reserved.
*
* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
* the contents of this file may not be disclosed to third parties, copied or
* duplicated in any form, in whole or in part, without the prior written
* permission of Silicon Graphics, Inc.
*
* RESTRICTED RIGHTS LEGEND:
* Use, duplication or disclosure by the Government is subject to restrictions
* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
* and Computer Software clause at DFARS 252.227-7013, and/or in similar or
* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
* rights reserved under the Copyright Laws of the United States.
*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * User-visible pieces of the ANSI C standard I/O package.
 * WARNING: this is an ANSI/POSIX/XPG4 header. Watch out for name space
 * pollution
 */
#include <standards.h>
#include <sgidefs.h>
#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

#if !defined(_SIZE_T) && !defined(_SIZE_T_)
#define _SIZE_T
#if (_MIPS_SZLONG == 32)
typedef unsigned int 	size_t;
#endif
#if (_MIPS_SZLONG == 64)
typedef unsigned long 	size_t;
#endif
#endif

#if !defined(_SSIZE_T) && !defined(_SSIZE_T_)
#define _SSIZE_T
#if (_MIPS_SZLONG == 32)
typedef int 	ssize_t;
#endif
#if (_MIPS_SZLONG == 64)
typedef long 	ssize_t;
#endif

#endif 

#if _MIPS_SIM == _ABIN32
typedef __int64_t	fpos_t;
#else
typedef long		fpos_t;
#endif

#if _NO_ANSIMODE
/* this is permitted except in strict ANSI */
#ifndef _OFF64_T
#define _OFF64_T
/* LINTED long long */
typedef	__int64_t	off64_t;	/* big byte offset type */
#endif	/* !_OFF64_T */

#ifndef _OFF_T
#define _OFF_T
#if defined(_KERNEL) && !defined(_STANDALONE)
typedef __int64_t	off_t;		/* byte offset type */
#elif defined(_STANDALONE)
typedef long		off_t;		/* byte offset type */
#elif _MIPS_SIM == _ABIN32
typedef __int64_t	off_t;		/* byte offset type */
#else
typedef long		off_t;		/* byte offset type */
#endif
#endif /* OFF_T */
#endif /* _NO_ANSIMODE */

#if (_ABIAPI || _LFAPI) && _NO_ANSIMODE
#if _MIPS_SIM == _ABI64
typedef	long		fpos64_t;
#else
/* LINTED long long - use __int64_t cause of ANSI */
typedef	__int64_t	fpos64_t;
#endif
#endif /* _ABIAPI || _LFAPI */

#endif /* defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS) */

/* Define the va_list type: */
#ifndef _VA_LIST_
#define _VA_LIST_
typedef char *va_list;
#endif /* !_VA_LIST_ */

#ifndef NULL
#define NULL		0L
#endif 


#define BUFSIZ	4096
#ifndef _NFILE
#define _NFILE	100	/* initial number of streams */
#endif	/* !_NFILE */
#define _SBFSIZ	8	/* compatibility with shared libs */

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
#define _IOFBF		0000	/* full buffered */
#define _IOLBF		0100	/* line buffered */
#define _IONBF		0004	/* not buffered */
#define _IOEOF		0020	/* EOF reached on read */
#define _IOERR		0040	/* I/O error from system */

#define _IOREAD		0001	/* currently reading */
#define _IOWRT		0002	/* currently writing */
#define _IORW		0200	/* opened for reading and writing */
#define _IOMYBUF	0010	/* stdio malloc()'d buffer */
#endif /* defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS) */

#if defined(_LANGUAGE_FORTRAN)
#define _IOFBF		'0000'x	/* full buffered */
#define _IOLBF		'0040'x	/* line buffered */
#define _IONBF		'0004'x	/* not buffered */
#define _IOEOF		'0010'x	/* EOF reached on read */
#define _IOERR		'0020'x	/* I/O error from system */
#define _IOREAD		'0001'x	/* currently reading */
#define _IOWRT		'0002'x	/* currently writing */
#define _IORW		'0080'x	/* opened for reading and writing */
#define _IOMYBUF	'0008'x	/* stdio malloc()'d buffer */
#endif	/* defined(_LANGUAGE_FORTRAN) */

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

#ifndef EOF
#   define EOF	(-1)
#endif

#define FOPEN_MAX	_NFILE
#define FILENAME_MAX    1024	/* max # of characters in a path name */

#ifndef SEEK_SET
#define SEEK_SET	0
#define SEEK_CUR	1
#define SEEK_END	2
#endif 	/* SEEK_SET */
#define TMP_MAX		17576	/* 26 * 26 * 26 */

#if _POSIX90 && _NO_ANSIMODE
#define L_ctermid	9
#define L_cuserid	9
#endif

#if _XOPEN4  && _NO_ANSIMODE
#define P_tmpdir	"/var/tmp/"
#endif

#define L_tmpnam	25	/* (sizeof(P_tmpdir) + 15) */

#define stdin	(&__iob[0])	
#define stdout	(&__iob[1])	
#define stderr	(&__iob[2])	

typedef struct	/* needs to be binary-compatible with old versions */
#if (_SGIAPI && _NO_ANSIMODE) || defined(_LANGUAGE_C_PLUS_PLUS)
/*
 * The __file_s struct tag is for backwards compat ONLY - it shouldn't be
 * used in any new headers. We would have liked to not use it in C++ since
 * it will change the name mangling of FILE *, but since it has already
 * gone out that way, we must stick to it. The safest thing is to *always*
 * define it for C++ (since a function that takes a FILE * as an argument
 * will have this tag in its mangled name).
 */
__file_s
#endif
{
#if (_MIPS_SZLONG == 32)
	int		_cnt;	/* number of available characters in buffer */
#endif
#if (_MIPS_SZLONG == 64)
	long		_cnt;	/* number of available characters in buffer */
#endif
	unsigned char	*_ptr;	/* next character from/to here in buffer */
	unsigned char	*_base;	/* the buffer */
#if _MIPS_SIM == _ABI64
	unsigned char	_reserved[2]; /* MIPS ABI */
	unsigned short	_flag;	/* the state of the stream */
	unsigned int	_file;	/* UNIX System file descriptor */
#elif _MIPS_SIM == _ABIN32
	unsigned char	_flag;	/* the state of the stream */
	unsigned char	_o_file;	/* not used */
	unsigned short	_file;	/* UNIX System file descriptor */
#else	/* _MIPS_SIM == _ABIO32 */
	unsigned char	_flag;	/* the state of the stream */
	unsigned char	_file;	/* UNIX System file descriptor */
#endif
} FILE;

extern FILE		__iob[_NFILE];	
#if _SGIAPI && _NO_ANSIMODE
extern FILE		*_lastbuf;
extern unsigned char 	*_bufendtab[];
extern unsigned char	 _sibuf[], _sobuf[];
#endif

extern int	remove(const char *);
extern int	rename(const char *, const char *);
extern FILE	*tmpfile(void);
extern char	*tmpnam(char *);
extern int	fclose(FILE *);
extern int	fflush(FILE *);
extern FILE	*fopen(const char *, const char *);
extern FILE	*freopen(const char *, const char *, FILE *);
extern void	setbuf(FILE *, char *);
extern int	setvbuf(FILE *, char *, int, size_t);
/* PRINTFLIKE2 */
extern int	fprintf(FILE *, const char *, ...);
/* SCANFLIKE2 */
extern int	fscanf(FILE *, const char *, ...);
/* PRINTFLIKE1 */
extern int	printf(const char *, ...);
/* SCANFLIKE1 */
extern int	scanf(const char *, ...);
#if _SGIAPI && _NO_ANSIMODE
/* PRINTFLIKE3 */
extern int	snprintf(char *, ssize_t, const char *, ...);
#endif
/* PRINTFLIKE2 */
extern int	sprintf(char *, const char *, ...);
/* SCANFLIKE2 */
extern int	sscanf(const char *, const char *, ...);
extern int	vfprintf(FILE *, const char *, /* va_list */ char *);
extern int	vprintf(const char *, /* va_list */ char *);
#if _SGIAPI && _NO_ANSIMODE
extern int	vsnprintf(char *, ssize_t, const char *, /* va_list */ char *);
#endif
extern int	vsprintf(char *, const char *, /* va_list */ char *);
#if defined(__INLINE_INTRINSICS) && _NO_XOPEN4
#if (defined(_COMPILER_VERSION) && (_COMPILER_VERSION >= 400))
#pragma intrinsic (printf)
#pragma intrinsic (fprintf)
#pragma intrinsic (sprintf)
#pragma intrinsic (scanf)
#pragma intrinsic (fscanf)
#pragma intrinsic (sscanf)
#endif /* COMPILER_VERSION >= 400 */
#endif /* defined(__INLINE_INTRINSICS) && _NO_XOPEN4 */
extern int	fgetc(FILE *);
extern char	*fgets(char *, int, FILE *); 
extern int	fputc(int, FILE *);
extern int	fputs(const char *, FILE *);
extern int	getc(FILE *);
extern int	getchar(void);
extern char	*gets(char *);
extern int	putc(int, FILE *);
extern int	putchar(int);
extern int	puts(const char *);
extern int	ungetc(int, FILE *);
extern size_t	fread(void *, size_t, size_t, FILE *);
#pragma int_to_unsigned fread
extern size_t	fwrite(const void *, size_t, size_t, FILE *);
#pragma int_to_unsigned fwrite
extern int	fgetpos(FILE *, fpos_t *);
extern int	fseek(FILE *, long, int);
extern int	fsetpos(FILE *, const fpos_t *);
extern long	ftell(FILE *);
extern void	rewind(FILE *);
extern void	clearerr(FILE *);
extern int	feof(FILE *);
extern int	ferror(FILE *);
extern void	perror(const char *);

extern int	__filbuf(FILE *);	
extern int	__flsbuf(int, FILE *);	

#if _POSIX90 && _NO_ANSIMODE
	/* POSIX additions */
extern FILE    *fdopen(int, const char *);
extern int	fileno(FILE *);
#endif

#if _POSIX1C && _NO_ANSIMODE
	/* POSIX 1003.1c additions */
extern void	flockfile(FILE *);
extern int	ftrylockfile(FILE *);
extern void	funlockfile(FILE *);
extern int	getc_unlocked(FILE *);
extern int	putc_unlocked(int, FILE *);
extern int	getchar_unlocked(void);
extern int	putchar_unlocked(int);
#endif /* _POSIX1C */

#if _POSIX2 && _NO_ANSIMODE
extern FILE	*popen(const char *, const char *);
extern int	pclose(FILE *);
#endif /* _POSIX2 */
#if _XOPEN4 && _NO_ANSIMODE
	/* XPG4 additions */
#include <getopt.h>
extern char	*ctermid(char *);
extern char	*cuserid(char *);
extern char	*tempnam(const char *, const char *);
extern int	getw(FILE *);
extern int	putw(int, FILE *);

#endif	/*  _XOPEN4 */

#if (_SGIAPI || _ABIAPI) && _NO_ANSIMODE
	/* non-POSIX/XOPEN/ANSI, but 'standard' unix */
extern char	*mktemp(char *);
extern int	mkstemp(char *);
extern int	setbuffer(FILE *, char *, int);
extern int	setlinebuf(FILE *);
extern int	system(const char *);
#endif /* _SGIAPI || _ABIAPI */

#if (_LFAPI || _XOPEN5) && _NO_ANSIMODE
	/* large file / xopen5 additions */
extern off_t	ftello(FILE *);
extern int	fseeko(FILE *, off_t, int);
#endif

#if _LFAPI && _NO_ANSIMODE
	/* SGI additions and large file additions */
extern int	fgetpos64(FILE *, fpos64_t *);
extern FILE	*fopen64(const char *, const char *);
extern FILE	*freopen64(const char *, const char *, FILE *);
extern int	fseek64(FILE *, __int64_t, int);
extern int	fseeko64(FILE *, off64_t, int);
extern int	fsetpos64(FILE *, const fpos64_t *);
extern __int64_t ftell64(FILE *);
extern off64_t	ftello64(FILE *);
extern FILE	*tmpfile64(void);
#endif /* _LFAPI */

#if (_SGIAPI || defined(_SGI_MP_SOURCE) || _POSIX1C) && _NO_ANSIMODE
extern int	__semputc(int, FILE *);
extern int	__semgetc(FILE *);
extern int	__us_rsthread_stdio;
#endif

#if (_SGIAPI || _REENTRANT_FUNCTIONS) && _NO_ANSIMODE
extern char	*ctermid_r(char *);
#endif

#ifndef _LINT

#if ((_SGIAPI && _NO_ABIAPI) || defined(_SGI_MP_SOURCE)) && _NO_ANSIMODE
/* NOTE:
 * The stdio macros have been modified so that they are by default 
 * multi-thread  safe.
 * For speed we have a single global that determines whether
 * threading is required.
 * There are 2 other versions of the macros:
 *	macro_locked - always use locked version
 * 	macro_unlocked - use unlocked version - these should be used
 *		with the flock/funlock-file functions
 */

/* Default versions of the macros */
#define getc(p)		getc_locked(p)
#define putc(x, p)	putc_locked((x), p)
#define getchar()	getc_locked(stdin)
#define putchar(x)	putc_locked((x), stdout)
#undef clearerr
/* the following 3 are atomic and require no locking */
#define feof(p)		feof_unlocked(p)
#define ferror(p)	ferror_unlocked(p)
#define fileno(p)	fileno_unlocked(p)

/* Explicit locked (thread-safe) versions */
#define getc_locked(p)	   	(__us_rsthread_stdio ? __semgetc(p) : \
			(--(p)->_cnt < 0 ? __filbuf(p) : (int)*(p)->_ptr++))
#define putc_locked(x, p)     	(__us_rsthread_stdio ? __semputc(x,p) : \
			(--(p)->_cnt < 0 ? __flsbuf((x), (p)) \
					  : (int)(*(p)->_ptr++ = (unsigned char)(x))))
#define getchar_locked()	getc_locked(stdin)
#define putchar_locked(x)	putc_locked((x), stdout)
#define clearerr_locked(p)	clearerr(p)
#define feof_locked(p)		feof_unlocked(p)
#define ferror_locked(p)	ferror_unlocked(p)
#define fileno_locked(p)	fileno_unlocked(p)

/* Explicit unlocked (thread-unsafe) versions */
#define getc_unlocked(p)	(--(p)->_cnt < 0 ? __filbuf(p) : (int)*(p)->_ptr++)
#define putc_unlocked(x, p)	(--(p)->_cnt < 0 ? __flsbuf((x), (p)) \
				: (int)(*(p)->_ptr++ = (unsigned char)(x)))
#define getchar_unlocked()	getc_unlocked(stdin)
#define putchar_unlocked(x)	putc_unlocked((x), stdout)
#define clearerr_unlocked(p)	((void)((p)->_flag &= ~(_IOERR | _IOEOF)))
#define feof_unlocked(p)	(((int)(p)->_flag) & _IOEOF)
#define ferror_unlocked(p)	(((int)(p)->_flag) & _IOERR)
#define fileno_unlocked(p)	((p)->_file)

#elif _POSIX1C && _NO_ANSIMODE
/*
 * POSIX1C - thread-safe by default, and no explicit 'locked' calls
 */
#define getc(p)	   	(__us_rsthread_stdio ? __semgetc(p) : \
			(--(p)->_cnt < 0 ? __filbuf(p) : (int)*(p)->_ptr++))
#define putc(x, p)     	(__us_rsthread_stdio ? __semputc(x,p) : \
			(--(p)->_cnt < 0 ? __flsbuf((x), (p)) \
				  : (int)(*(p)->_ptr++ = (unsigned char)(x))))
#define getchar()	getc(stdin)
#define putchar(x)	putc((x), stdout)

/* explicit unlocked versions */
#define getc_unlocked(p)	(--(p)->_cnt < 0 ? __filbuf(p) : (int)*(p)->_ptr++)
#define putc_unlocked(x, p)	(--(p)->_cnt < 0 ? __flsbuf((x), (p)) \
				: (int)(*(p)->_ptr++ = (unsigned char)(x)))
#define getchar_unlocked()	getc_unlocked(stdin)
#define putchar_unlocked(x)	putc_unlocked((x), stdout)

#define clearerr(p)	((void)((p)->_flag &= ~(_IOERR | _IOEOF)))
#define feof(p)		(((int)(p)->_flag) & _IOEOF)
#define ferror(p)	(((int)(p)->_flag) & _IOERR)
#define fileno(p)	((p)->_file)

#else

/*
 * Std ANSI - thread-unsafe
 */

#define getc(p)		(--(p)->_cnt < 0 ? __filbuf(p) : (int)*(p)->_ptr++)
#define putc(x, p)	(--(p)->_cnt < 0 ? __flsbuf((x), (p)) \
				: (int)(*(p)->_ptr++ = (unsigned char)(x)))
#define getchar()	getc(stdin)
#define putchar(x)	putc((x), stdout)
#define clearerr(p)	((void)((p)->_flag &= ~(_IOERR | _IOEOF)))
#define feof(p)		(((int)(p)->_flag) & _IOEOF)
#define ferror(p)	(((int)(p)->_flag) & _IOERR)

#if _POSIX90 && _NO_ANSIMODE
#define fileno(p)	((p)->_file)
#endif

#endif /* _SGI_MP_SOURCE */

#endif /* !_LINT */

#endif /* defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS) */

#ifdef __cplusplus
}
#endif
#endif /* !__STDIO_H__ */
