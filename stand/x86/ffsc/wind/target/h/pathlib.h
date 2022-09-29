/* pathLib.h - path library header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01j,22sep92,rrr  added support for c++
01i,04jul92,jcf  cleaned up.
01h,26may92,rrr  the tree shuffle
01g,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed VOID to void
		  -changed copyright notice
01f,05oct90,dnw  removed private functions.
01e,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
                 added copyright notice.
01d,30jul90,dnw  added pathLastNamePtr() and changed pathLastName() to void.
01c,01may90,llk  changed pathBuild() and pathCat() to return STATUS.
01b,01apr90,llk  deleted pathArrayFree().
01a,06jul89,llk  written.
*/

#ifndef __INCpathLibh
#define __INCpathLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vwmodnum.h"


/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	pathBuild (char ** nameArray, char ** nameArrayEnd,
			   char *destString);
extern STATUS 	pathCat (char *dirName, char *fileName, char *result);
extern char *	pathLastNamePtr (char *pathName);
extern void 	pathCondense (char *pathName);
extern void 	pathLastName (char *pathName, char ** pLastName);
extern void 	pathParse (char *longName, char ** nameArray, char *nameBuf);
extern void 	pathSplit (char *fullFileName, char *dirName, char *fileName);

#else	/* __STDC__ */

extern STATUS 	pathBuild ();
extern STATUS 	pathCat ();
extern char *	pathLastNamePtr ();
extern void 	pathCondense ();
extern void 	pathLastName ();
extern void 	pathParse ();
extern void 	pathSplit ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCpathLibh */
