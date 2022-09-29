#ifndef __STRING_H__
#define __STRING_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.41 $"
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
/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * WARNING - this is an ANSI/POSIX/XPG4 header. Watch for name space pollution
 * Note that ANSI/POSIX/XPG permits any function name beginning with
 * 'str', 'mem', or 'wcs'.
 */
#include <standards.h>

#if !defined(_SIZE_T) && !defined(_SIZE_T_)
#define _SIZE_T
#if (_MIPS_SZLONG == 32)
typedef unsigned int	size_t;
#endif
#if (_MIPS_SZLONG == 64)
typedef unsigned long	size_t;
#endif
#endif

#ifndef NULL
#define NULL	0L
#endif

#if defined(__cplusplus) && \
     defined(_MIPS_SIM) && _MIPS_SIM != _MIPS_SIM_ABI32 && \
     defined(_COMPILER_VERSION) && (_COMPILER_VERSION >= 720) && \
     defined(__LIBC_OVERLOAD__) && __LIBC_OVERLOAD__

#define __cpp_string_h
#else
#undef __cpp_string_h
#endif 

/*
 * Std ANSI functions
 */
extern void *memcpy(void *, const void *, size_t);
extern void *memmove(void *, const void *, size_t);
extern char *strcpy(char *, const char *);
extern char *strncpy(char *, const char *, size_t);
extern char *strcat(char *, const char *);
extern char *strncat(char *, const char *, size_t);
extern void *memccpy(void *, const void *, int, size_t);
extern int memcmp(const void *, const void *, size_t);
extern int strcmp(const char *, const char *);
extern int strcoll(const char *, const char *);
extern int strncmp(const char *, const char *, size_t);
extern size_t strxfrm(char *, const char *, size_t);
#ifndef __cpp_string_h
extern void *memchr(const void *, int, size_t);
extern char *strchr(const char *, int);
#endif /* __cpp_string_h */
extern size_t strcspn(const char *, const char *);
#pragma int_to_unsigned strcspn
#ifndef __cpp_string_h
extern char *strpbrk(const char *, const char *);
extern char *strrchr(const char *, int);
#endif /* __cpp_string_h */
extern size_t strspn(const char *, const char *);
#pragma int_to_unsigned strspn
#ifndef __cpp_string_h
extern char *strstr(const char *, const char *);
#endif /* __cpp_string_h */
extern char *strtok(char *, const char *);
extern void *memset(void *, int, size_t);
extern char *strerror(int);
extern size_t strlen(const char *);
#pragma int_to_unsigned strlen

#if _SGIAPI && _NO_ANSIMODE
extern int ffs(int);
/* Case-insensitive comparision routines from 4.3BSD. */
extern int strcasecmp(const char *, const char *);
extern int strncasecmp(const char *, const char *, size_t);
#endif	/* _SGIAPI */

#if (_XOPEN4UX || _XOPEN5) && _NO_ANSIMODE
extern char *strdup(const char *);
#endif

#if (_POSIX1C || _XOPEN5) && _NO_ANSIMODE
extern char *strtok_r(char *, const char *, char **);
#endif

#ifdef __INLINE_INTRINSICS
/* The functions made intrinsic here can be activated by the driver
** passing -D__INLINE_INTRINSICS to cfe.
*/
#ifdef _CFE
#pragma intrinsic (strcpy) /* Only effective if second arg is string const */
#endif
#if (defined(_COMPILER_VERSION) && (_COMPILER_VERSION >= 400))
#pragma intrinsic (strcpy) /* Only effective if second arg is string const */
#pragma intrinsic (strcmp) /* Only effective if args are string const */
#pragma intrinsic (strlen) /* Only effective if arg is string const */
#endif /* COMPILER_VERSION >= 400 */
#if (defined(_COMPILER_VERSION) && (_COMPILER_VERSION >= 721))
#pragma intrinsic (memcpy)
#pragma intrinsic (memmove)
#pragma intrinsic (memset)
#endif /* COMPILER_VERSION >= 721 */
#endif /* __INLINE_INTRINSICS */

#ifdef __cplusplus
}
#endif

#ifdef __cpp_string_h

/*
 * In C++, five of the functions from the C string library are replaced
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
extern void *memchr(const void *, int, size_t);
extern char *strchr(const char *, int);
extern char *strpbrk(const char *, const char *);
extern char *strrchr(const char *, int);
extern char *strstr(const char *, const char *);
} /* Close extern "C" */
} /* Close namespace __sgilib. */

#ifndef __sgi_cpp_memchr_defined
#define __sgi_cpp_memchr_defined

inline const void* memchr(const void* s, int c, size_t n) {
  return __sgilib::memchr(s, c, n);
}

inline void* memchr(void* s, int c, size_t n) {
  return __sgilib::memchr(s, c, n);
}
#endif /* __sgi_cpp_memchr_defined */

inline const char* strchr(const char* s, int c) {
  return __sgilib::strchr(s, c);
}

inline char* strchr(char* s, int c) {
  return __sgilib::strchr(s, c);
}

inline const char* strpbrk(const char* s1, const char* s2) {
  return __sgilib::strpbrk(s1, s2);
}

inline char* strpbrk(char* s1, const char* s2) {
  return __sgilib::strpbrk(s1, s2);
}

inline const char* strrchr(const char* s, int c) {
  return __sgilib::strrchr(s, c);
}
    
inline char* strrchr(char* s, int c) {
  return __sgilib::strrchr(s, c);
}

inline const char* strstr(const char* s1, const char* s2) {
  return __sgilib::strstr(s1, s2);
}

inline char* strstr(char* s1, const char* s2) {
  return __sgilib::strstr(s1, s2);
}

#endif /* __cpp_string_h */

#undef __cpp_string_h
#endif /* !__STRING_H__ */
