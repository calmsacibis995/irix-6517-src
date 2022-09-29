/* rngLib.h - ring buffer subroutine library header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02b,22sep92,rrr  added support for c++
02a,04jul92,jcf  cleaned up.
01k,26may92,rrr  the tree shuffle
01j,04oct91,rrr  passed through the ansification filter
		  -changed VOID to void
		  -changed copyright notice
01i,06apr91,gae  added NOMANUAL to avoid fooling mangen.
01h,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
                 added copyright notice.
01g,10aug90,dnw  added declaration of rngFlush().
01f,07aug90,shl  moved function declarations to end of file.
01e,22aug88,gae  removed incorrect comment about using 'shorts'
		 for optimization.
01d,30may88,dnw  changed to v4 names.
01c,10nov87,dnw  changed ring pointers from shorts to ints.
		 rings can now be > 32K bytes.
01b,15aug84,dnw  removed several macros, now routines in rngLib.cs;
		   only rngGetC and rngPutC remain as macros.
01a,06jun84,dnw  culled from old drvLib.h
*/

#ifndef __INCrngLibh
#define __INCrngLibh

#ifdef __cplusplus
extern "C" {
#endif


/* HIDDEN */

/* typedefs */

typedef struct		/* RING - ring buffer */
    {
    int pToBuf;		/* offset from start of buffer where to write next */
    int pFromBuf;	/* offset from start of buffer where to read next */
    int bufSize;	/* size of ring in bytes */
    char *buf;		/* pointer to start of buffer */
    } RING;

/* END_HIDDEN */

typedef RING *RING_ID;


/*
 * The following macros are designed to do various operations on
 * the RING object.  By using them, users can avoid having to know
 * the structure of RING.  However they are intended to be very
 * efficient and this is why they are macros in the first place.
 * In general the parameters to them should be register variables
 * for maximum efficiency.
 */

/*******************************************************************************
*
* RNG_ELEM_GET - get one character from a ring buffer
*
* This macro gets a single character from the specified ring buffer.
* Must supply temporary variable (register int) 'fromP'.
*
* RETURNS: 1 if there was a char in the buffer to return, 0 otherwise
*
* NOMANUAL
*/

#define RNG_ELEM_GET(ringId, pCh, fromP)		\
    (						\
    fromP = (ringId)->pFromBuf,			\
    ((ringId)->pToBuf == fromP) ?		\
	0 					\
    :						\
	(					\
	*pCh = (ringId)->buf[fromP],		\
	(ringId)->pFromBuf = ((++fromP == (ringId)->bufSize) ? 0 : fromP), \
	1					\
	)					\
    )

/*******************************************************************************
*
* RNG_ELEM_PUT - put one character into a ring buffer
*
* This macro puts a single character into the specified ring buffer.
* Must supply temporary variable (register int) 'toP'.
*
* RETURNS: 1 if there was room in the buffer for the char, 0 otherwise
*
* NOMANUAL
*/

#define RNG_ELEM_PUT(ringId, ch, toP)		\
    (						\
    toP = (ringId)->pToBuf,			\
    (toP == (ringId)->pFromBuf - 1) ?		\
	0 					\
    :						\
	(					\
    	(toP == (ringId)->bufSize - 1) ?	\
	    (					\
	    ((ringId)->pFromBuf == 0) ?		\
		0				\
	    :					\
		(				\
		(ringId)->buf[toP] = ch,	\
		(ringId)->pToBuf = 0,		\
		1				\
		)				\
	    )					\
	:					\
	    (					\
	    (ringId)->buf[toP] = ch,		\
	    (ringId)->pToBuf++,			\
	    1					\
	    )					\
	)					\
    )

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern BOOL 	rngIsEmpty (RING_ID ringId);
extern BOOL 	rngIsFull (RING_ID ringId);
extern RING_ID 	rngCreate (int nbytes);
extern int 	rngBufGet (RING_ID rngId, char *buffer, int maxbytes);
extern int 	rngBufPut (RING_ID rngId, char *buffer, int nbytes);
extern int 	rngFreeBytes (RING_ID ringId);
extern int 	rngNBytes (RING_ID ringId);
extern void 	rngDelete (RING_ID ringId);
extern void 	rngFlush (RING_ID ringId);
extern void 	rngMoveAhead (RING_ID ringId, int n);
extern void 	rngPutAhead (RING_ID ringId, char byte, int offset);

#else	/* __STDC__ */

extern BOOL 	rngIsEmpty ();
extern BOOL 	rngIsFull ();
extern RING_ID 	rngCreate ();
extern int 	rngBufGet ();
extern int 	rngBufPut ();
extern int 	rngFreeBytes ();
extern int 	rngNBytes ();
extern void 	rngDelete ();
extern void 	rngFlush ();
extern void 	rngMoveAhead ();
extern void 	rngPutAhead ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCrngLibh */
