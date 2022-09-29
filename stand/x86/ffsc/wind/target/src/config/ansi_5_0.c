/* ansi_5_0.c - 5.0 compatible library initialization file */

/* Copyright 1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,19sep92,smb  written.
*/

/*
DESCRIPTION
This file is used to include all 5.0 ANSI C library routines in the 
VxWorks build. The routines are only included when this file is 
included by usrConfig.c
*/

#ifndef __INCansi_5_0c
#define __INCansi_5_0c


#include "vxworks.h"
#include "ctype.h"
#include "math.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"


#undef isalnum			/* #undef needed for the MIPS compiler */
#undef isalpha
#undef iscntrl
#undef isdigit
#undef isgraph
#undef islower
#undef isprint
#undef ispunct
#undef isspace
#undef isupper
#undef isxdigit
#undef tolower
#undef toupper

VOIDFUNCPTR Ansi5_0Files[] =
    {
    /* ctype */
    (VOIDFUNCPTR) isalnum,
    (VOIDFUNCPTR) isalpha,
    (VOIDFUNCPTR) iscntrl,
    (VOIDFUNCPTR) isdigit,
    (VOIDFUNCPTR) isgraph,
    (VOIDFUNCPTR) islower,
    (VOIDFUNCPTR) isprint,
    (VOIDFUNCPTR) ispunct,
    (VOIDFUNCPTR) isspace,
    (VOIDFUNCPTR) isupper,
    (VOIDFUNCPTR) isxdigit,
    (VOIDFUNCPTR) tolower,
    (VOIDFUNCPTR) toupper,

    /* math */
    (VOIDFUNCPTR) acos,
    (VOIDFUNCPTR) asin,
    (VOIDFUNCPTR) atan,
    (VOIDFUNCPTR) atan2,
    (VOIDFUNCPTR) ceil,
    (VOIDFUNCPTR) cos,
    (VOIDFUNCPTR) cosh,
    (VOIDFUNCPTR) exp,
    (VOIDFUNCPTR) fabs,
    (VOIDFUNCPTR) floor,
    (VOIDFUNCPTR) fmod,
    (VOIDFUNCPTR) frexp,
    (VOIDFUNCPTR) ldexp,
    (VOIDFUNCPTR) log,
    (VOIDFUNCPTR) log10,
    (VOIDFUNCPTR) modf,
    (VOIDFUNCPTR) pow,
    (VOIDFUNCPTR) sin,
    (VOIDFUNCPTR) sinh,
    (VOIDFUNCPTR) sqrt,
    (VOIDFUNCPTR) tan,
    (VOIDFUNCPTR) tanh,

#undef clearerr			/* #undef needed for the MIPS compiler */
#undef feof
#undef ferror
#undef getchar
#undef getc
#undef putchar
#undef putc

    /* stdio */
    (VOIDFUNCPTR) clearerr,
    (VOIDFUNCPTR) fclose,
    (VOIDFUNCPTR) feof,
    (VOIDFUNCPTR) ferror,
    (VOIDFUNCPTR) fflush,
    (VOIDFUNCPTR) fgetc,
    (VOIDFUNCPTR) fgets,
    (VOIDFUNCPTR) fopen,
    (VOIDFUNCPTR) fprintf,
    (VOIDFUNCPTR) fputc,
    (VOIDFUNCPTR) fputs,
    (VOIDFUNCPTR) fread,
    (VOIDFUNCPTR) freopen,
    (VOIDFUNCPTR) fscanf,
    (VOIDFUNCPTR) fseek,
    (VOIDFUNCPTR) ftell,
    (VOIDFUNCPTR) fwrite,
    (VOIDFUNCPTR) getc,
    (VOIDFUNCPTR) getchar,
    (VOIDFUNCPTR) gets,
    (VOIDFUNCPTR) perror,
    (VOIDFUNCPTR) putc,
    (VOIDFUNCPTR) putchar,
    (VOIDFUNCPTR) puts,
    (VOIDFUNCPTR) rewind,
    (VOIDFUNCPTR) scanf,
    (VOIDFUNCPTR) setbuf,
    (VOIDFUNCPTR) ungetc,
#if _EXTENSION_POSIX_1003		/* undef for ANSI */
    (VOIDFUNCPTR) fdopen,
    (VOIDFUNCPTR) fileno,
#endif /* _EXTENSION_POSIX_1003 */
#if _EXTENSION_WRS			/* undef for ANSI */
    (VOIDFUNCPTR) getw,
    (VOIDFUNCPTR) putw,
    (VOIDFUNCPTR) setbuffer,
    (VOIDFUNCPTR) stdioShow,
#endif /* _EXTENSION_WRS */

    /* stdlib */
    (VOIDFUNCPTR) abs,
    (VOIDFUNCPTR) atexit,
    (VOIDFUNCPTR) bsearch,
    (VOIDFUNCPTR) qsort,
    (VOIDFUNCPTR) system,

    /* string */
    (VOIDFUNCPTR) memcmp,
    (VOIDFUNCPTR) memcpy,
    (VOIDFUNCPTR) memset,
    (VOIDFUNCPTR) memmove,
    (VOIDFUNCPTR) strcat,
    (VOIDFUNCPTR) strchr,
    (VOIDFUNCPTR) strcmp,
    (VOIDFUNCPTR) strcpy,
    (VOIDFUNCPTR) strlen,
    (VOIDFUNCPTR) strncat,
    (VOIDFUNCPTR) strncmp,
    (VOIDFUNCPTR) strncpy,
    (VOIDFUNCPTR) strrchr
    };

#endif /* __INCansi_5_0c */
