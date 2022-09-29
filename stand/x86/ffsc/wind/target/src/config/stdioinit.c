/* stdioLibInit.c - stdio library initialization */

/* Copyright 1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,13oct92,jcf  removed stdioShow reference.
01b,24sep92,smb  removed POSIX extensions.
01a,05sep92,smb  written.
*/

/*
DESCRIPTION
This file is used to include the stdio ANSI C library routines in the 
VxWorks build. The routines are only included when this file is 
included by usrConfig.c.

NOMANUAL
*/

#ifndef  __INCstdioLibInitc
#define  __INCstdioLibInitc


#include "vxworks.h"
#include "stdio.h"

#undef clearerr			/* #undef needed for the MIPS compiler */
#undef feof
#undef ferror
#undef getchar
#undef getc
#undef putchar
#undef putc

VOIDFUNCPTR stdioFiles[] =
    {
    (VOIDFUNCPTR) clearerr,
    (VOIDFUNCPTR) fclose,
    (VOIDFUNCPTR) feof,
    (VOIDFUNCPTR) ferror,
    (VOIDFUNCPTR) fflush,
    (VOIDFUNCPTR) fgetc,
    (VOIDFUNCPTR) fgetpos,
    (VOIDFUNCPTR) fgets,
    (VOIDFUNCPTR) fopen,
    (VOIDFUNCPTR) fprintf,
    (VOIDFUNCPTR) fputc,
    (VOIDFUNCPTR) fputs,
    (VOIDFUNCPTR) fread,
    (VOIDFUNCPTR) freopen,
    (VOIDFUNCPTR) fscanf,
    (VOIDFUNCPTR) fseek,
    (VOIDFUNCPTR) fsetpos,
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
    (VOIDFUNCPTR) setvbuf,
    (VOIDFUNCPTR) tmpfile,
    (VOIDFUNCPTR) tmpnam,
    (VOIDFUNCPTR) ungetc,
    (VOIDFUNCPTR) vfprintf,
    (VOIDFUNCPTR) fdopen,
    (VOIDFUNCPTR) fileno,
#if _EXTENSION_WRS			/* undef for ANSI */
    (VOIDFUNCPTR) getw,
    (VOIDFUNCPTR) putw,
    (VOIDFUNCPTR) setbuffer,
#endif /* _EXTENSION_WRS */
    };

#endif /* __INCstdioLibInitc */
