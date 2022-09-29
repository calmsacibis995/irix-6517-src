/* va-i960.h - Gnu C variable arguments header file for Intel i960 */

/* Copyright 1992 Wind River Systems, Inc. */

/*
modification history
01c,09jul95,jmb	 Add Cygnus version of va-i960.h
01b,29jul95,kkk	 fixed warning at end of #ifndef. fixed problem with va_start()
	         (SPR# 3268)
01a,17nov92,rrr	 created from gcc 2.2 version.
*/

#ifndef __INCva_i960h
#define __INCva_i960h


	/* NOTE:
	 *  The comments in the following file are from the
	 *  original Gnu version of va-i960.h.  The file names
	 *  and symbols mentioned are not necessarily
	 *  present in Wind River Systems headers or modules.
	 */

#ifdef __GCC960_VER

/* GNU C varargs support for the Intel 80960.  */

/* Define __gnuc_va_list.  */

#ifndef __GNUC_VA_LIST
#define __GNUC_VA_LIST
/* The first element is the address of the first argument.
   The second element is the number of bytes skipped past so far.  */
typedef unsigned __gnuc_va_list[2];	
#endif /* not __GNUC_VA_LIST */

/* If this is for internal libc use, don't define anything but
   __gnuc_va_list.  */
#if defined (_STDARG_H) || defined (_VARARGS_H)

/* In GCC version 2, we want an ellipsis at the end of the declaration
   of the argument list.  GCC version 1 can't parse it.  */

#if __GNUC__ > 1
#define __va_ellipsis ...
#else
#define __va_ellipsis
#endif

#if 1

/* The stack size of the type t.  */
#define ___vsiz(T)   (((sizeof (T) + 3) / 4) * 4)

/* The stack alignment of the type t.  */
#define ___vali(T)   (__alignof__ (T) >= 4 ? __alignof__ (T) : 4)

/* The offset of the next stack argument after one of type t at offset i.  */
#define ___vpad(I, T) ((((I) + ___vali (T) - 1) / ___vali (T)) \
		       * ___vali (T) + ___vsiz (T))

#endif

/* Avoid errors if compiling GCC v2 with GCC v1.  */
#if __GNUC__ == 1
#define __extension__
#endif

#ifdef _STDARG_H
#define va_start(AP, LASTARG)				\
__extension__						\
({ __asm__ ("st	g14,%0" : "=m" (*(AP)));		\
   (AP)[1] = (unsigned) __builtin_next_arg () - *AP; })
#else
extern int __builtin_argsize();
extern void *__builtin_argptr();

#define	va_alist __builtin_va_alist
#define	va_dcl	 char *__builtin_va_alist; __va_ellipsis
#define	va_start(AP) ((AP)[1] = (unsigned) __builtin_argsize(), 	\
		       *(AP) = (unsigned) __builtin_argptr())
#endif

/* We cast to void * and then to TYPE * because this avoids
   a warning about increasing the alignment requirement.  */
#define	va_arg(AP, T)							\
(									\
  (									\
    ((AP)[1] <= 48 && (___vpad ((AP)[1], T) > 48 || ___vsiz (T) > 16))	\
      ? ((AP)[1] = 48 + ___vsiz (T))					\
      : ((AP)[1] = ___vpad ((AP)[1], T))					\
  ),									\
									\
  *((T *) (void *) ((char *) *(AP) + (AP)[1] - ___vsiz (T)))		\
)

void va_end (__gnuc_va_list);		/* Defined in libgcc.a */
#define	va_end(AP)

#endif /* defined (_STDARG_H) || defined (_VARARGS_H) */

#else    /* not __GCC960_VER, it's Cygnus instead */

/* GNU C varargs support for the Intel 80960.  */

/* Define __gnuc_va_list.  */

#ifndef __GNUC_VA_LIST
#define __GNUC_VA_LIST
/* The first element is the address of the first argument.
   The second element is the number of bytes skipped past so far.  */
typedef unsigned __gnuc_va_list[2];	
#endif /* not __GNUC_VA_LIST */

/* If this is for internal libc use, don't define anything but
   __gnuc_va_list.  */
#if defined (_STDARG_H) || defined (_VARARGS_H)

/* In GCC version 2, we want an ellipsis at the end of the declaration
   of the argument list.  GCC version 1 can't parse it.  */

#if __GNUC__ > 1
#define __va_ellipsis ...
#else
#define __va_ellipsis
#endif

/* The stack size of the type t.  */
#define __vsiz(T)   (((sizeof (T) + 3) / 4) * 4)
/* The stack alignment of the type t.  */
#define __vali(T)   (__alignof__ (T) >= 4 ? __alignof__ (T) : 4)
/* The offset of the next stack argument after one of type t at offset i.  */
#define __vpad(I, T) ((((I) + __vali (T) - 1) / __vali (T)) \
		       * __vali (T) + __vsiz (T))

/* Avoid errors if compiling GCC v2 with GCC v1.  */
#if __GNUC__ == 1
#define __extension__
#endif

#ifdef _STDARG_H
/* Call __builtin_next_arg even though we aren't using its value, so that
   we can verify that firstarg is correct.  */
#define va_start(AP, LASTARG)				\
__extension__						\
({ __builtin_next_arg (LASTARG);			\
   __asm__ ("st	g14,%0" : "=m" (*(AP)));		\
   (AP)[1] = (__builtin_args_info (0) + __builtin_args_info (1)) * 4; })

#else

#define	va_alist __builtin_va_alist
#define	va_dcl	 char *__builtin_va_alist; __va_ellipsis
#define	va_start(AP) \
__extension__						\
({ __asm__ ("st	g14,%0" : "=m" (*(AP)));		\
   (AP)[1] = (__builtin_args_info (0) + __builtin_args_info (1)) * 4; })
#endif

/* We cast to void * and then to TYPE * because this avoids
   a warning about increasing the alignment requirement.  */
#define	va_arg(AP, T)							\
(									\
  (									\
    ((AP)[1] <= 48 && (__vpad ((AP)[1], T) > 48 || __vsiz (T) > 16))	\
      ? ((AP)[1] = 48 + __vsiz (T))					\
      : ((AP)[1] = __vpad ((AP)[1], T))					\
  ),									\
									\
  *((T *) (void *) ((char *) *(AP) + (AP)[1] - __vsiz (T)))		\
)

#ifndef va_end
void va_end (__gnuc_va_list);		/* Defined in libgcc.a */
#endif
#define	va_end(AP)

#endif /* defined (_STDARG_H) || defined (_VARARGS_H) */

#endif  /* ifdef __GCC960_VER */

#endif /* __INCva_i960h */

