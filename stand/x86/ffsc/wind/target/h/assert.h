/* assert.h - ANSI standard assert functions header */

/* Copyright 1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01e,13nov92,smb  fixed assert macro to not generate warnings
01d,22sep92,rrr  added support for c++
01c,20jul92,smb  added __assert extern.
01b,04jul92,jcf  cleaned up.
01a,03jul92,smb  written.
*/

#ifdef __cplusplus
extern "C" {
#endif

#include "types/vxansi.h"

#undef assert
#ifdef NDEBUG
#define assert(ignore)	((void) 0)
#else /* turn debugging on */

#define _ASSERT_STR(z) _ASSERT_TMP(z)
#define _ASSERT_TMP(z) #z

#if defined(__STDC__) || defined(__cplusplus)
extern void __assert (const char *msg);
#else
extern void __assert ();
#endif

#define assert(test) ((void) \
		      ((test) ? ((void) 0) : \
		       __assert("Assertion failed: "#test", file " 	\
                                __FILE__ ", line "_ASSERT_STR(__LINE__)"\n")))
#endif	/* NDEBUG */

#ifdef __cplusplus
}
#endif
