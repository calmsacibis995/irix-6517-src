#ifndef _WCHAR_H
#define _WCHAR_H
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.21 $"
/*
*
* Copyright 1995, Silicon Graphics, Inc.
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
#include <sgidefs.h>
#include <standards.h>
#include <locale_attr.h>
/*
 * This is an XPG4 header
 */

#if !defined(_SIZE_T) && !defined(_SIZE_T_)
#define _SIZE_T
#if (_MIPS_SZLONG == 32)
typedef unsigned int	size_t;
#endif
#if (_MIPS_SZLONG == 64)
typedef unsigned long	size_t;
#endif
#endif

#ifndef _WCHAR_T
#define _WCHAR_T
#if (_MIPS_SZLONG == 32)
typedef long wchar_t;
#endif
#if (_MIPS_SZLONG == 64)
typedef __int32_t wchar_t;
#endif
#endif

#ifndef _WUCHAR_T
#   define _WUCHAR_T
#if (_MIPS_SZLONG == 32)
	typedef unsigned long	wuchar_t;
#endif
#if (_MIPS_SZLONG == 64)
	typedef __uint32_t	wuchar_t;
#endif
#endif

#ifndef _WINT_T
#   define _WINT_T
#if (_MIPS_SZLONG == 32)
	typedef long	wint_t;
#endif
#if (_MIPS_SZLONG == 64)
	typedef __int32_t wint_t;
#endif
#endif

#ifndef _WCTYPE_T
#   define _WCTYPE_T
#if (_MIPS_SZLONG == 32)
	typedef unsigned long	wctype_t;
#endif
#if (_MIPS_SZLONG == 64)
	typedef __uint32_t	wctype_t;
#endif
#endif

#ifndef _MBSTATE_T
#   define _MBSTATE_T
	typedef char	mbstate_t;
#endif

#ifndef NULL
#   define NULL	0L
#endif

#ifndef WEOF
#   define WEOF	(-1)
#endif

#include <stdio.h>	/* it requires FILE to be completed! */
#include <ctype.h>
#include <time.h>	/* for struct tm */

#if defined(__cplusplus) && \
     defined(_MIPS_SIM) && _MIPS_SIM != _MIPS_SIM_ABI32 && \
     defined(_COMPILER_VERSION) && (_COMPILER_VERSION >= 720) && \
     defined(__LIBC_OVERLOAD__) && __LIBC_OVERLOAD__
#define __cpp_string_h
#else
#undef __cpp_string_h
#endif 

extern int	iswalnum(wint_t);
extern int	iswalpha(wint_t);
extern int	iswcntrl(wint_t);
extern int	iswdigit(wint_t);
extern int	iswgraph(wint_t);
extern int	iswlower(wint_t);
extern int	iswprint(wint_t);
extern int	iswpunct(wint_t);
extern int	iswspace(wint_t);
extern int	__iswblank(wint_t);
extern int	iswupper(wint_t);
extern int	iswxdigit(wint_t);
extern int	iswctype(wint_t, wctype_t);
extern wctype_t	wctype(const char *);
extern wint_t	towlower(wint_t);
extern wint_t	towupper(wint_t);

extern wint_t	fgetwc(FILE *);
extern wchar_t	*fgetws(wchar_t *, int, FILE *);
extern wint_t	fputwc(wint_t, FILE *);
extern int	fputws(const wchar_t *, FILE *);
extern wint_t	getwc(FILE *);
extern wint_t	getwchar(void);
extern wint_t	putwc(wint_t, FILE *);
extern wint_t	putwchar(wint_t);
extern wint_t	ungetwc(wint_t, FILE *);
#define getwchar()	getwc(stdin)
#define putwchar(x)	putwc((x), stdout)

extern wchar_t	*wcscat(wchar_t *, const wchar_t *);
#ifndef __cpp_string_h
extern wchar_t	*wcschr(const wchar_t *, wint_t);
#endif /* __cpp_string_h */
extern int	wcscmp(const wchar_t *, const wchar_t *);
extern int	wcscoll(const wchar_t *, const wchar_t *);
extern size_t	wcsxfrm(wchar_t *, const wchar_t *, size_t);
extern wchar_t	*wcscpy(wchar_t *, const wchar_t *);
extern size_t	wcscspn(const wchar_t *, const wchar_t *);
extern size_t	wcslen(const wchar_t *);
extern wchar_t	*wcsncat(wchar_t *, const wchar_t *, size_t);
extern int	wcsncmp(const wchar_t *, const wchar_t *, size_t);
extern wchar_t	*wcsncpy(wchar_t *, const wchar_t *, size_t);
#ifndef __cpp_string_h
extern wchar_t	*wcspbrk(const wchar_t *, const wchar_t *);
extern wchar_t	*wcsrchr(const wchar_t *, wchar_t);
#endif /* __cpp_string_h */
extern size_t	wcsspn(const wchar_t *, const wchar_t *);
extern wchar_t	*wcstok(wchar_t *, const wchar_t *);
#ifndef __cpp_string_h
extern wchar_t	*wcsstr(const wchar_t *, const wchar_t *);
#endif /* __cpp_string_h */
extern size_t	wcsftime(wchar_t *, size_t, const char *, const struct tm *);

extern int	wcwidth(wchar_t);
extern int	wcswidth(const wchar_t *, size_t);
extern wchar_t	*wcswcs(const wchar_t *, const wchar_t *);

extern double	wcstod(const wchar_t *, wchar_t **);
extern long	wcstol(const wchar_t *, wchar_t **, int);
extern unsigned long	wcstoul(const wchar_t *, wchar_t **, int);

#if _ABIAPI || _SGIAPI
extern __int64_t wcstoll(const wchar_t *, wchar_t **, int);
extern __uint64_t wcstoull(const wchar_t *, wchar_t **, int);
#endif

extern int __iswctype(wint_t, wctype_t);
extern wint_t	__trwctype(wint_t, wctype_t);

#define	_E1	0x00000100	/* phonogram (international use) */
#define	_E2	0x00000200	/* ideogram (international use) */
#define	_E3	0x00000400	/* English (international use) */
#define	_E4	0x00000800	/* number (international use) */
#define	_E5	0x00001000	/* special (international use) */
#define	_E6	0x00002000	/* other characters (international use) */

#define _ISwalpha	_ISalpha
#define	_ISwupper	_ISupper
#define	_ISwlower	_ISlower
#define	_ISwdigit	_ISdigit
#define	_ISwxdigit	_ISxdigit
#define	_ISwalnum	_ISalnum
#define	_ISwspace	_ISspace
#define _ISwblank	_ISblank
#define	_ISwpunct	_ISpunct
#define	_ISwprint	(_ISprint|_E1|_E2|_E5|_E6)
#define	_ISwgraph	(_ISgraph|_E1|_E2|_E5|_E6)
#define	_ISwcntrl	_IScntrl

#define	_ISwphonogram	_E1	/* phonogram (international use) */
#define	_ISwideogram	_E2	/* ideogram (international use) */
#define	_ISwenglish	_E3	/* English (international use) */
#define	_ISwnumber	_E4	/* number (international use) */
#define	_ISwspecial	_E5	/* special (international use) */
#define	_ISwother	_E6	/* other characters (international use) */

#if _SGIAPI
#define iscodeset0(c) ( _IS_EUC_LOCALE ? ( !((c) & ~0xff) )    : (*__libc_attr._euc_func._iscodeset)(0,c) )
#define iscodeset1(c) ( _IS_EUC_LOCALE ? ( ((c) >> 28) == 0x3) : (*__libc_attr._euc_func._iscodeset)(1,c) )
#define iscodeset2(c) ( _IS_EUC_LOCALE ? ( ((c) >> 28) == 0x1) : (*__libc_attr._euc_func._iscodeset)(2,c) )
#define iscodeset3(c) ( _IS_EUC_LOCALE ? ( ((c) >> 28) == 0x2) : (*__libc_attr._euc_func._iscodeset)(3,c) )

extern int	iswascii(wint_t);
extern int	isphonogram(wint_t);
extern int	isideogram(wint_t);
extern int	isenglish(wint_t);
extern int	isnumber(wint_t);
extern int	isspecial(wint_t);
extern wchar_t	*wcstok_r(wchar_t *, const wchar_t *, wchar_t **);
#endif /* _SGIAPI */




#if _UNSAFE_WCTYPE
/*
 * Due to ANSI/XPG rules, macro arguments may be only evaluated once.
 * Standard macro definitions of iswprint et.al. require evaluation
 * of their argument twice. So, there are 3 options:
 * 1) no macros
 * 2) stick evaluated argument in a static
 * 3) provide 'unsafe' macros that evaluate the argument twice.
 *
 * Std SVR4 provides 2) - this is really bad for multi-threaded apps
 * as well as signal handlers etc..
 * We opt for 1 and 3 - for applications that explicitly need speed
 * and can guarantee that multiple evaluation is ok, the following are
` * provided.
 */
#define	iswalpha(c)	( ( _IS_EUC_LOCALE && (c) <= 255) ? isalpha(c) : __iswctype(c, _ISwalpha) )
#define	iswupper(c)	( ( _IS_EUC_LOCALE && (c) <= 255) ? isupper(c) : __iswctype(c, _ISwupper) )
#define	iswlower(c)	( ( _IS_EUC_LOCALE && (c) <= 255) ? islower(c) : __iswctype(c, _ISwlower) )
#define	iswdigit(c)	( ( _IS_EUC_LOCALE && (c) <= 255) ? isdigit(c) : __iswctype(c, _ISwdigit) )
#define	iswxdigit(c)	( ( _IS_EUC_LOCALE && (c) <= 255) ? isxdigit(c) : __iswctype(c, _ISwxdigit) )
#define	iswalnum(c)	( ( _IS_EUC_LOCALE && (c) <= 255) ? isalnum(c) : __iswctype(c, _ISwalnum) )
#define	iswspace(c)	( ( _IS_EUC_LOCALE && (c) <= 255) ? isspace(c) : __iswctype(c, _ISwspace) )
#define __iswblank(c)	( ( _IS_EUC_LOCALE && (c) <= 255) ? __isblank(c) : __iswctype(c, _ISwblank) )
#define	iswpunct(c)	( ( _IS_EUC_LOCALE && (c) <= 255) ? ispunct(c) : __iswctype(c, _ISwpunct) )
#define	iswprint(c)	( ( _IS_EUC_LOCALE && (c) <= 255) ? isprint(c) : __iswctype(c, _ISwprint) )
#define	iswgraph(c)	( ( _IS_EUC_LOCALE && (c) <= 255) ? isgraph(c) : __iswctype(c, _ISwgraph) )
#define	iswcntrl(c)	( ( _IS_EUC_LOCALE && (c) <= 255) ? iscntrl(c) : __iswctype(c, _ISwcntrl) )
#define	iswascii(c)	(!((c) & ~0177))
#define	towupper(c)	( ( _IS_EUC_LOCALE && (c) <= 255) ? toupper(c) : _trwctype(c, _L) )
#define	towlower(c)	( ( _IS_EUC_LOCALE && (c) <= 255) ? tolower(c) : _trwctype(c, _U) )

#endif /* _UNSAFE_WCTYPE */

#ifdef __cplusplus
}
#endif

#ifdef __cpp_string_h

/*
 * In C++, five of the functions from the C wide string library are replaced
 * by overloaded pairs.  (Overloaded on const.)  In each case, there is
 * no semantic difference between the C function and the C++ functions.
 * We handle this by declaring the C version in an internal namespace;
 * each C++ version just turns around and calls the C version, performing
 * whatever const manipulations are necessary.  Note that this trick only
 * works because a namespace doesn't appear in the mangled name of an
 * extern "C" function.  Extern "C" names aren't mangled.
 */

namespace __sgilib {
extern "C" {
extern wchar_t	*wcschr(const wchar_t *, wint_t);
extern wchar_t	*wcspbrk(const wchar_t *, const wchar_t *);
extern wchar_t	*wcsrchr(const wchar_t *, wchar_t);
extern wchar_t	*wcsstr(const wchar_t *, const wchar_t *);
} /* Close extern "C" */
} /* Close namespace __sgilib. */

inline const wchar_t* wcschr(const wchar_t* s, wint_t c) {
  return __sgilib::wcschr(s, c);
}

inline wchar_t* wcschr(wchar_t* s, wint_t c) {
  return __sgilib::wcschr(s, c);
}

inline const wchar_t* wcspbrk(const wchar_t* s1, const wchar_t* s2) {
  return __sgilib::wcspbrk(s1, s2);
}

inline wchar_t* wcspbrk(wchar_t* s1, const wchar_t* s2) {
  return __sgilib::wcspbrk(s1, s2);
}

inline const wchar_t* wcsrchr(const wchar_t* s, wchar_t c) {
  return __sgilib::wcsrchr(s, c);
}

inline wchar_t* wcsrchr(wchar_t* s, wchar_t c) {
  return __sgilib::wcsrchr(s, c);
}

inline const wchar_t* wcsstr(const wchar_t* s1, const wchar_t* s2) {
  return __sgilib::wcsstr(s1, s2);
}

inline wchar_t* wcsstr(wchar_t* s1, const wchar_t* s2) {
  return __sgilib::wcsstr(s1, s2);
}

#endif /* __cpp_string_h */

#undef __cpp_string_h
#endif /*_WCHAR_H*/
