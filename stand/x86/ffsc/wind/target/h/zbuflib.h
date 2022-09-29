/* zbufLib.h - zeroCopy buffer interface library header */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,08nov94,dzb  written.
*/

#ifndef __INCzbufLibh
#define __INCzbufLibh

#ifdef __cplusplus
extern "C" {
#endif

/* includes */

#include "limits.h"
#include "private/semlibp.h"

/* typedefs */

/* HIDDEN */

typedef struct				/* ZBUF_FUNC */
    {
    FUNCPTR	createRtn;		/* zbufCreate()		*/
    FUNCPTR	deleteRtn;		/* zbufDelete()		*/
    FUNCPTR	insertRtn;		/* zbufInsert()		*/
    FUNCPTR	insertBufRtn;		/* zbufInsertBuf()	*/
    FUNCPTR	insertCopyRtn;		/* zbufInsertCopy()	*/
    FUNCPTR	extractCopyRtn;		/* zbufExtractCopy()	*/
    FUNCPTR	cutRtn;			/* zbufCut()		*/
    FUNCPTR	splitRtn;		/* zbufSplit()		*/
    FUNCPTR	dupRtn;			/* zbufDup()		*/
    FUNCPTR	lengthRtn;		/* zbufLength()		*/
    FUNCPTR	segFindRtn;		/* zbufSegFind()	*/
    FUNCPTR	segNextRtn;		/* zbufSegNext()	*/
    FUNCPTR	segPrevRtn;		/* zbufSegPrev()	*/
    FUNCPTR	segDataRtn;		/* zbufSegData()	*/
    FUNCPTR	segLengthRtn;		/* zbufSegLength()	*/
    } ZBUF_FUNC;

/* Pools not used */

typedef struct zbufBlockId	/* ZBUF_BLOCK_ID */
    {
    int				length;
    void *			segFree;
    struct zbufPoolId *		zbufPoolId;
    struct zbufBlockId *	blockNext;
    } *ZBUF_BLOCK_ID;

typedef struct zbufPoolId	/* ZBUF_POOL_ID */
    {
    int				use;
    SEMAPHORE			poolSem;
    SEMAPHORE			waitSem;
    struct zbufBlockId *	blockHead;
    } *ZBUF_POOL_ID;

typedef void *			ZBUF_ID;
typedef void *			ZBUF_SEG;

/* END_HIDDEN */

/* defines */

#define	ZBUF_BEGIN		-INT_MAX	/* shortcut for prepending */
#define	ZBUF_END		INT_MAX		/* shortcut for appending */
#define	ZBUF_NONE		((ZBUF_SEG) NONE) /* cut past zbuf */

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS	zbufLibInit (FUNCPTR libInitRtn);
extern ZBUF_ID	zbufCreate (void);
extern STATUS	zbufDelete (ZBUF_ID zbufId);
extern ZBUF_SEG	zbufInsert (ZBUF_ID zbufId1, ZBUF_SEG zbufSeg, int offset,
                    ZBUF_ID zbufId2);
extern ZBUF_SEG	zbufInsertBuf (ZBUF_ID zbufId, ZBUF_SEG zbufSeg, int offset,
		    caddr_t buf, int len, VOIDFUNCPTR freeRtn, int freeArg);
extern ZBUF_SEG	zbufInsertCopy (ZBUF_ID zbufId, ZBUF_SEG zbufSeg,
		    int offset, caddr_t buf, int len);
extern int	zbufExtractCopy (ZBUF_ID zbufId, ZBUF_SEG zbufSeg,
		    int offset, caddr_t buf, int len);
extern ZBUF_SEG	zbufCut (ZBUF_ID zbufId, ZBUF_SEG zbufSeg, int offset, int len);
extern ZBUF_ID	zbufSplit (ZBUF_ID zbufId, ZBUF_SEG zbufSeg, int offset);
extern ZBUF_ID	zbufDup (ZBUF_ID zbufId, ZBUF_SEG zbufSeg, int offset, int len);
extern int	zbufLength (ZBUF_ID zbufId);
extern ZBUF_SEG	zbufSegFind (ZBUF_ID zbufId, ZBUF_SEG zbufSeg, int *pOffset);
extern ZBUF_SEG	zbufSegNext (ZBUF_ID zbufId, ZBUF_SEG zbufSeg);
extern ZBUF_SEG	zbufSegPrev (ZBUF_ID zbufId, ZBUF_SEG zbufSeg);
extern caddr_t	zbufSegData (ZBUF_ID zbufId, ZBUF_SEG zbufSeg);
extern int	zbufSegLength (ZBUF_ID zbufId, ZBUF_SEG zbufSeg);

#else	/* __STDC__ */

extern STATUS	zbufLibInit ();
extern ZBUF_ID	zbufCreate ();
extern STATUS	zbufDelete ();
extern ZBUF_SEG	zbufInsert ();
extern ZBUF_SEG	zbufInsertBuf ();
extern ZBUF_SEG	zbufInsertCopy ();
extern int	zbufExtractCopy ();
extern ZBUF_SEG	zbufCut ();
extern ZBUF_ID	zbufSplit ();
extern ZBUF_ID	zbufDup ();
extern int	zbufLength ();
extern ZBUF_SEG	zbufSegFind ();
extern ZBUF_SEG	zbufSegNext ();
extern ZBUF_SEG	zbufSegPrev ();
extern caddr_t	zbufSegData ();
extern int	zbufSegLength ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCzbufLibh */
