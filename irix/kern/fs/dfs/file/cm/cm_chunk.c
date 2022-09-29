/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: cm_chunk.c,v 65.4 1998/03/23 16:24:17 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/* Copyright (C) 1994, 1989 Transarc Corporation - All rights reserved */

#include <dcedfs/param.h>			/* Should be always first */
#include <dcedfs/sysincludes.h>		/* Standard vendor system headers */
#include <cm.h>				/* Cm-based standard headers */

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_chunk.c,v 65.4 1998/03/23 16:24:17 gwehrman Exp $")

/*
 * Chunk module: INCOMPLETE...
 */

#ifdef SGIMIPS
int cm_chunkoffset(long offset)
#else  /* SGIMIPS */
int cm_chunkoffset(offset)
    long offset;
#endif /* SGIMIPS */
{
    if (offset < cm_firstcsize) 
	return offset;
    else
	return ((offset - cm_firstcsize) & (cm_othercsize - 1));
}


#ifdef SGIMIPS
int cm_chunk(long offset)
#else  /* SGIMIPS */
int cm_chunk(offset)
    long offset;
#endif /* SGIMIPS */
{
    if (offset < cm_firstcsize)
	return 0;
    else
	return (((offset - cm_firstcsize) >> cm_logchunk) + 1);
}


#ifdef SGIMIPS
int cm_chunkbase(int offset)
#else  /* SGIMIPS */
int cm_chunkbase(offset)
    int offset;
#endif /* SGIMIPS */
{
    if (offset < cm_firstcsize)
	return 0;
    else
	return (((offset - cm_firstcsize) & ~(cm_othercsize - 1)) + cm_firstcsize);
}


#ifdef SGIMIPS
int cm_chunksize(long offset)
#else  /* SGIMIPS */
int cm_chunksize(offset)
    long offset;
#endif /* SGIMIPS */
{
    if (offset < cm_firstcsize)
	return cm_firstcsize;
    else
	return cm_othercsize;
}


#ifdef SGIMIPS
int cm_chunktobase(long chunk)
#else  /* SGIMIPS */
int cm_chunktobase(chunk)
    long chunk;
#endif /* SGIMIPS */
{
    if (chunk == 0)
	return 0;
    else 
	return (cm_firstcsize + ((chunk - 1) << cm_logchunk));
}


#ifdef SGIMIPS
int cm_chunktosize(long chunk)
#else  /* SGIMIPS */
int cm_chunktosize(chunk)
    long chunk;
#endif /* SGIMIPS */
{
    if (chunk == 0)
	return cm_firstcsize;
    else
	return cm_othercsize;
}
