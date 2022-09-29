/* Copyright (C) 1994, 1989 Transarc Corporation - All rights reserved */
/*
 * HISTORY
 * $Log: cm_chunk.h,v $
 * Revision 65.3  1999/07/21 19:01:14  gwehrman
 * Defined SGIMIPS for this development header.  Fix for bug 657352.
 *
 * Revision 65.2  1998/03/19 23:47:23  bdr
 * Misc 64 bit file changes and/or code cleanup for compiler warnings/remarks.
 *
 * Revision 65.1  1997/10/20  19:17:23  jdoak
 * *** empty log message ***
 *
 * $EndLog$
 */

#ifndef SGIMIPS
#define SGIMIPS
#endif /* !SGIMIPS */
/*
 * Chunk number/offset computations
 */

#define cm_chunkoffset(offset) 			\
    (((offset) < cm_firstcsize) ? (offset) :	\
    (((offset) - cm_firstcsize) & (cm_othercsize - 1)))

#define cm_chunk(offset)			\
    (((offset) < cm_firstcsize) ? 0 :		\
    ((((offset) - cm_firstcsize) >> cm_logchunk) + 1))

#define cm_chunkbase(offset)			\
    (((offset) < cm_firstcsize) ? 0 :		\
    ((((offset) - cm_firstcsize) & ~(cm_othercsize - 1)) + cm_firstcsize))

#define cm_chunksize(offset)			\
    (((offset) < cm_firstcsize) ? cm_firstcsize : cm_othercsize)

#ifdef SGIMIPS
#define cm_chunktobase(chunk)			\
    (((chunk) == 0) ? (afs_hyper_t) 0 : 	\
    (((afs_hyper_t) cm_firstcsize) + ((((afs_hyper_t)(chunk)) - 1) \
    << ((afs_hyper_t) cm_logchunk))))
#else  /* SGIMIPS */
#define cm_chunktobase(chunk)			\
    (((chunk) == 0) ? 0 : (cm_firstcsize + (((chunk) - 1) << cm_logchunk)))
#endif /* SGIMIPS */

#define cm_chunktosize(chunk)			\
    (((chunk) == 0) ? cm_firstcsize : cm_othercsize)
