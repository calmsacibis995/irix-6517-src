#ifndef __MEMORY_H__
#define __MEMORY_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.8 $"
/*
*
* Copyright 1992, Silicon Graphics, Inc.
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


/* from stddef.h */
#if !defined(_SIZE_T) && !defined(_SIZE_T_)
#define _SIZE_T
#if (_MIPS_SZLONG == 32)
typedef unsigned int	size_t;
#endif
#if (_MIPS_SZLONG == 64)
typedef unsigned long	size_t;
#endif
#endif

#if defined(__cplusplus) && \
     defined(_MIPS_SIM) && _MIPS_SIM != _MIPS_SIM_ABI32 && \
     defined(_COMPILER_VERSION) && (_COMPILER_VERSION >= 720) && \
     defined(__LIBC_OVERLOAD__) && __LIBC_OVERLOAD__

#define __cpp_string_h
#else
#undef __cpp_string_h
#endif 

extern void *memccpy(void *, const void *, int, size_t);
#ifndef __cpp_string_h
extern void *memchr(const void *, int, size_t);
#endif /* __cpp_string_h */
extern void *memcpy(void *, const void *, size_t);
extern void *memset(void *, int, size_t);
extern int memcmp(const void *, const void *, size_t);

#ifdef __INLINE_INTRINSICS
/* The functions made intrinsic here can be activated by adding the
** option -D__INLINE_INTRINSICS
*/
#if (defined(_COMPILER_VERSION) && (_COMPILER_VERSION >= 721))
#pragma intrinsic (memcpy)
#pragma intrinsic (memset)
#endif /* COMPILER_VERSION >= 721 */
#endif /* __INLINE_INTRINSICS */


#ifdef __cplusplus
}
#endif

#if defined(__cpp_string_h) && !defined(__sgi_cpp_memchr_defined)
#define __sgi_cpp_memchr_defined

/* In C++, five of the functions from the C string library, one of
 * which is memchr, are replaced by overloaded pairs.  (Overloaded on
 * const.)  In each case, there is no semantic difference between the
 * C function and the C++ functions.  We handle this by declaring the
 * C version in an internal namespace; each C++ version just turns
 * around and calls the C version, performing whatever const
 * manipulations are necessary.  Note that this trick only works
 * because a namespace doesn't appear in the mangled name of an extern
 * "C" function.  Extern "C" names aren't mangled.
 */

namespace __sgilib {
  extern "C" {
    extern void *memchr(const void *, int, size_t);
  } 
} 

inline const void* memchr(const void* s, int c, size_t n) {
  return __sgilib::memchr(s, c, n);
}

inline void* memchr(void* s, int c, size_t n) {
  return __sgilib::memchr(s, c, n);
}

#endif /* __cpp_string_h && !__sgi_cpp_memchr_defined */

#undef __cpp_string_h
#endif /* !__MEMORY_H__ */
