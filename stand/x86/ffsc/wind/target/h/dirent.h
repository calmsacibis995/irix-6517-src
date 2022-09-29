/* dirent.h - POSIX directory handling definitions */

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
01f,10jun91.del  added pragma for gnu960 alignment.
01e,05oct90,dnw  added rewinddir() and closedir().
01d,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
                 added copyright notice.
01c,07aug90,shl  added IMPORT type to function declarations.
01b,25may90,dnw  moved NAME_MAX to limits.h
		 added copyright
01a,17apr90,kdl  written.
*/


#ifndef __INCdirenth
#define __INCdirenth

#ifdef __cplusplus
extern "C" {
#endif

#include "limits.h"


#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 1			/* tell gcc960 not to optimize alignments */
#endif	/* CPU_FAMILY==I960 */

/* Directory entry */

struct dirent		/* dirent */
    {
    char	d_name [NAME_MAX + 1];	/* file name, null-terminated */
    };


/* HIDDEN */
/* Directory descriptor */

typedef struct		/* DIR */
    {
    int		  dd_fd;		/* file descriptor for open directory */
    int		  dd_cookie;		/* filesys-specific marker within dir */
    struct dirent dd_dirent;		/* obtained directory entry */
    } DIR;

/* END_HIDDEN */

#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 0			/* turn off alignment requirement */
#endif	/* CPU_FAMILY==I960 */

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern DIR *	opendir (char *dirName);
extern STATUS 	closedir (DIR *pDir);
extern struct 	dirent *readdir (DIR *pDir);
extern void 	rewinddir (DIR *pDir);

#else	/* __STDC__ */

extern DIR *	opendir ();
extern STATUS 	closedir ();
extern struct 	dirent *readdir ();
extern void 	rewinddir ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCdirenth */
