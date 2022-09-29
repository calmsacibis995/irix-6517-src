/* string.h - string library header file */

/* Copyright 1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02f,22sep92,rrr  added support for c++
02e,14sep92,smb  added const to the prototype of bcopy.
02d,07sep92,smb  removed macros for memcpy and memset.
02c,30jul92,smb  added macros for memcpy and memset.
02b,09jul92,smb  removed extra memmove declarations.
02a,04jul92,jcf  cleaned up.
01b,03jul92,smb  macro override for strerror added.
01a,29jul91,rrr  written.
*/

#ifndef __INCstringh
#define __INCstringh

#ifdef __cplusplus
extern "C" {
#endif

#include "types/vxansi.h"

#ifndef NULL
#define NULL	((void *) 0)
#endif

#ifdef _TYPE_size_t
_TYPE_size_t;
#undef _TYPE_size_t
#endif

#if defined(__STDC__) || defined(__cplusplus)

extern void *	memchr (const void *__s, int __c, size_t __n);
extern int 	memcmp (const void *__s1, const void *__s2, size_t __n);
extern void *	memcpy (void *__s1, const void *__s2, size_t __n);
extern void *	memmove (void *__s1, const void *__s2, size_t __n);
extern void *	memset (void *__s, int __c, size_t __n);

extern char *	strcat (char *__s1, const char *__s2);
extern char *	strchr (const char *__s, int __c);
extern int 	strcmp (const char *__s1, const char *__s2);
extern int 	strcoll (const char *__s1, const char *__s2);
extern char *	strcpy (char *__s1, const char *__s2);
extern size_t 	strcspn (const char *__s1, const char *__s2);
extern size_t 	strlen (const char *__s);
extern char *	strncat (char *__s1, const char *__s2, size_t __n);
extern int 	strncmp (const char *__s1, const char *__s2, size_t __n);
extern char *	strncpy (char *__s1, const char *__s2, size_t __n);
extern char *	strpbrk (const char *__s1, const char *__s2);
extern char *	strrchr (const char *__s, int __c);
extern size_t 	strspn (const char *__s1, const char *__s2);
extern char *	strstr (const char *__s1, const char *__s2);
extern char *	strtok (char *__s, const char *__sep);
extern size_t 	strxfrm (char *__s1, const char *__s2, size_t __n);
extern char *	strerror(int __errcode);

#if _EXTENSION_POSIX_REENTRANT		/* undef for ANSI */
extern char *	strtok_r (char *__s, const char *__sep, char **__ppLast);
#endif

#if _EXTENSION_WRS		 	/* undef for ANSI */
extern int	strerror_r (int __errcode, char *__buf);
extern void 	bcopy (const char *source, char *dest, int nbytes);
extern void 	bcopyBytes (char *source, char *dest, int nbytes);
extern void 	bcopyWords (char *source, char *dest, int nwords);
extern void 	bcopyLongs (char *source, char *dest, int nlongs);
extern void 	bfill (char *buf, int nbytes, int ch);
extern void 	bfillBytes (char *buf, int nbytes, int ch);
extern void 	bzero (char *buffer, int nbytes);
extern int 	bcmp (char *buf1, char *buf2, int nbytes);
extern void 	binvert (char *buf, int nbytes);
extern void 	bswap (char *buf1, char *buf2, int nbytes);
extern void 	uswab (char *source, char *destination, int nbytes);
extern void 	swab (char *source, char *dest, int nbytes);
extern char *	index (const char *s, int c);
extern char *	rindex (const char *s, int c);
#endif

#else	/* __STDC__ */

extern void *	memchr ();
extern int 	memcmp ();
extern void *	memcpy ();
extern void *	memmove ();
extern void *	memset ();

extern char *	strcat ();
extern char *	strchr ();
extern int 	strcmp ();
extern int 	strcoll ();
extern char *	strcpy ();
extern size_t 	strcspn ();
extern size_t 	strlen ();
extern char *	strncat ();
extern int 	strncmp ();
extern char *	strncpy ();
extern char *	strpbrk ();
extern char *	strrchr ();
extern size_t 	strspn ();
extern char *	strstr ();
extern char *	strtok ();
extern size_t 	strxfrm ();
extern char *	strerror ();

#if _EXTENSION_POSIX_REENTRANT		/* undef for ANSI */
extern char *	strtok_r ();
#endif

#if _EXTENSION_WRS		 	/* undef for ANSI */
extern int	strerror_r ();
extern void 	bcopy ();
extern void 	bcopyBytes ();
extern void 	bcopyWords ();
extern void 	bcopyLongs ();
extern void 	bfill ();
extern void 	bfillBytes ();
extern void 	bzero ();
extern int 	bcmp ();
extern void 	binvert ();
extern void 	bswap ();
extern void 	uswab ();
extern void 	swab ();
extern char *	index ();
extern char *	rindex ();
#endif

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCstringh */
