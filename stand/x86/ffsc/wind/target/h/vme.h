/* vme.h - VMEbus constants header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02b,22sep92,rrr  added support for c++
02a,04jul92,jcf  cleaned up.
01g,26may92,rrr  the tree shuffle
01f,13jan92,ccc  fixed VME_AM_IS_XXX macro for 16 and 24 bit AM's.
01e,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
01d,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01c,04may90,gae  added VME_AM_IS_XXX test macros for 16, 24, & 32 bit AM's.
01b,08nov89,shl  added ifdef to prevent inclusion of vxWorks.h more than once.
01a,02jun87,dnw  written.
*/

#ifndef __INCvmeh
#define __INCvmeh

#ifdef __cplusplus
extern "C" {
#endif

/* address modifier codes */

#define VME_AM_STD_SUP_ASCENDING	0x3f
#define VME_AM_STD_SUP_PGM		0x3e
#define VME_AM_STD_SUP_DATA		0x3d

#define VME_AM_STD_USR_ASCENDING	0x3b
#define VME_AM_STD_USR_PGM		0x3a
#define VME_AM_STD_USR_DATA		0x39

#define VME_AM_SUP_SHORT_IO		0x2d
#define VME_AM_USR_SHORT_IO		0x29

#define VME_AM_EXT_SUP_ASCENDING	0x0f
#define VME_AM_EXT_SUP_PGM		0x0e
#define VME_AM_EXT_SUP_DATA		0x0d

#define VME_AM_EXT_USR_ASCENDING	0x0b
#define VME_AM_EXT_USR_PGM		0x0a
#define VME_AM_EXT_USR_DATA		0x09

#define	VME_AM_IS_SHORT(addr)	((addr & 0xf0) == 0x20)
#define	VME_AM_IS_STD(addr)	((addr & 0xf0) == 0x30)
#define	VME_AM_IS_EXT(addr)	((addr & 0xf0) == 0x00)

#ifdef __cplusplus
}
#endif

#endif /* __INCvmeh */
