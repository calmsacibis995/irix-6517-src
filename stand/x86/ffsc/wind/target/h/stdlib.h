/* stdlib.h - standard library header file */

/* Copyright 1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02i,15oct93,cd   added #ifndef _ASMLANGUAGE.
02h,05feb93,smb  corrected prototype for strtoul
02g,22sep92,rrr  added support for c++
02f,21sep92,smb  reordered prototype list.
02e,24jul92,smb  replaced types.
02d,24jul92,smb  added prototypes for div_r and ldiv_r
		 removed the ifndef for types div_t, ldiv_t and wchar_T
02c,20jul92,smb  replaced modification history.
02b,19jul92,smb  rewritten
02a,04jul92,jcf  cleaned up.
01f,26may92,rrr  the tree shuffle
01e,25nov91,llk  added ansi definitions EXIT_FAILURE, EXIT_SUCCESS, RAND_MAX,
                   MB_CUR_MAX.
                 included more function prototypes.  Some are commented out.
01d,04oct91,rrr  passed through the ansification filter
                  -fixed #else and #endif
                  -changed copyright notice
01c,10jun91.del  added pragma for gnu960 alignment.
01b,19oct90,shl  fixed typo in modhist 01a.
01a,05oct90,dnw  created based on Marc Ullman's version
*/

#ifndef __INCstdlibh
#define __INCstdlibh

#ifdef __cplusplus
extern "C" {
#endif

#include "types/vxansi.h"	/* includes types div_t, ldiv_t, wchar_t */

#ifndef _ASMLANGUAGE

#define EXIT_FAILURE	_PARM_EXIT_FAILURE
#define EXIT_SUCCESS	_PARM_EXIT_SUCCESS
#define MB_CUR_MAX	_PARM_MB_CUR_MAX
#define RAND_MAX	_PARM_RAND_MAX

#ifndef NULL
#define NULL	((void *) 0)
#endif

#ifdef _TYPE_div_t
_TYPE_div_t;
#undef _TYPE_div_t
#endif

#ifdef _TYPE_ldiv_t
_TYPE_ldiv_t;
#undef _TYPE_ldiv_t
#endif

#ifdef _TYPE_wchar_t
_TYPE_wchar_t;
#undef _TYPE_wchar_t
#endif

#ifdef _TYPE_size_t
_TYPE_size_t;
#undef _TYPE_size_t
#endif


typedef struct {		/* used by mblen and mbtowc */
	unsigned char __state;
	unsigned short __wchar;
	} _Mbsave;

#if defined(__STDC__) || defined(__cplusplus)

extern void	abort (void);
extern int	abs (int __i);
extern int	atexit (void (*__func)(void));
extern double	atof (const char *__s);
extern int	atoi (const char *__s);
extern long	atol (const char *__s);
extern void *	bsearch (const void *__key, const void *__base,
		         size_t __nelem, size_t __size,
		         int  (*__cmp)(const void *__ck, const void *__ce));
extern div_t	div (int __numer, int __denom);
extern long	labs (long __i);
extern ldiv_t	ldiv (long __numer, long __denom);
extern int	mblen (const char *__s, size_t __n);
extern size_t	mbstowcs (wchar_t *__wcs, const char *__s, size_t __n);
extern int	mbtowc (wchar_t *__pwc, const char *__s, size_t __n);
extern void	qsort (void *__base, size_t __nelem, size_t __size,
		       int  (*__cmp)(const void *__e1, const void *__e2));
extern int	rand (void);
extern void *	srand (unsigned int __seed);
extern double	strtod (const char *__s, char **__endptr);
extern long	strtol (const char *__s, char **__endptr, int __base);
extern unsigned long strtoul (const char *__s, char **__endptr, int __base);
extern int	system (const char *__s);
extern size_t	wcstombs (char *__s, const wchar_t *__wcs, size_t __n);
extern int	wctomb (char *__s, wchar_t __wchar);

extern void *	calloc (size_t __nelem, size_t __size);
extern void	exit (int __status);
extern void	free (void *__ptr);
extern char *	getenv (const char *__name);
extern void *	malloc (size_t __size);
extern void *	realloc (void *__ptr, size_t __size);

#if _EXTENSION_WRS

extern void     div_r (int numer, int denom, div_t * divStructPtr);
extern void     ldiv_r (long numer, long denom, ldiv_t * divStructPtr);

#endif

#else   /* __STDC__ */

extern void     abort ();
extern int      abs ();
extern int      atexit ();
extern double   atof ();
extern int      atoi ();
extern long     atol ();
extern void *   bsearch ();
extern div_t    div ();
extern long     labs ();
extern ldiv_t   ldiv ();
extern int      mblen ();
extern size_t   mbstowcs ();
extern int      mbtowc ();
extern void     qsort ();
extern int      rand ();
extern void *   srand ();
extern double   strtod ();
extern long     strtol ();
extern unsigned long strtoul ();
extern int      system ();
extern size_t   wcstombs ();
extern int      wctomb ();

extern void *   calloc ();
extern void     exit ();
extern void     free ();
extern char *   getenv ();
extern void *   malloc ();
extern void *   realloc ();

#if _EXTENSION_WRS

extern void     div_r ();
extern void     ldiv_r ();

#endif

#endif /* _ASMLANGUAGE */

#endif  /* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCstdlibh */
