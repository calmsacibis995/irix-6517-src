/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/fakewin.h,v 1.2 1996/07/06 17:42:58 nn Exp $"

#ifndef _FAKEWIN_H
#define	_FAKEWIN_H

/*
 * This file defines appropriate macros so that
 * we can use the same codebase for Unix, DOS, and Windows.
 */

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _WINDOWS
#include <tklib.h>

#define	malloc		_fmalloc
#define	calloc		_fcalloc
#define	free		_ffree
#define	strdup		_fstrdup
#define	strcpy		_fstrcpy
#define	strcmp		_fstrcmp
#define	strchr		_fstrchr
#define	sprintf		wsprintf
#define	vsprintf	wvsprintf
#define	memcpy		_fmemcpy
#define	strlen		_fstrlen
#else
#define	LPSTR	char *
#endif

#if !defined(_WINDOWS) && !defined(_MSDOS)
#define	_TKFAR
#endif

#ifndef	_WINDOWS
#define	_TKPASCAL
#define	__export
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* !_FAKEWIN_H */
