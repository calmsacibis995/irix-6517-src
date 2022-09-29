/* strxfrmP.h - header file for string */

/* Copyright 1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,22sep92,rrr  added support for c++
01b,11jul92,smb  fixed __STDC__ ifdef.
01a,08jul92,smb  written and documented.
*/

/*
DESCRIPTION

SEE ALSO: American National Standard X3.159-1989
*/

#include "string.h"
#include "private/strstatep.h"


#ifndef __INCstrxfrmPh
#define __INCstrxfrmPh

#ifdef __cplusplus
extern "C" {
#endif

/* A data object of type __cosave saves the state information between
 * calls to __strxfrm.
 */
typedef struct
    {
    uchar_t	__state;
    ushort_t	__wchar;
    } __cosave;


/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern size_t __strxfrm (char *sout, const uchar_t **ppsin, size_t size,
    		         __cosave *ps);
#else	/* __STDC__ */

extern size_t __strxfrm ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCstrxfrmPh */
