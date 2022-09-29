/* bufLib.h - header file for remote debug server */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,05apr95,ms  new data types.
01a,20sep94,ms  written.
*/

#ifndef __INCbufLibh
#define __INCbufLibh

#ifdef	__cplusplus
extern "C" {
#endif

/* data types */

typedef struct
    {
    void *              pClass;         /* class pointer */
    char *              pBufs;          /* array of buffers */
    int                 numBufs;        /* number of buffers in the array */
    int                 bufSize;        /* size of each buffer */
    char *		pFreeBufs;	/* free list */
    } BUF_POOL;

/* function prototypes */


#if	defined(__STDC__) || defined(__cplusplus)

void	bufPoolInit	(BUF_POOL *pBufPool, char *pBufs, int numBufs,
			 int bufSize);
char *	bufAlloc	(BUF_POOL *pBufPool);
void	bufFree		(BUF_POOL *pBufPool, char *pBuf);

#else	/* defined(__STDC__) || defined(__cplusplus) */

void	bufPoolInit	();
char *	bufAlloc	();
void	bufFree		();

#endif	/* defined(__STDC__) || defined(__cplusplus) */

#ifdef	__cplusplus
}
#endif

#endif /* __INCbufLibh */

