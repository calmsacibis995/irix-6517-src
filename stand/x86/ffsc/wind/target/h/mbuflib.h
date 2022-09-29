/* mbufLib.h - mbuf interface library header */

/* Copyright 1984-1995 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,13mar95,dzb  added MBUF_VALID, and changed macros to use (SPR #4066).
01a,08nov94,dzb  written.
*/

#ifndef __INCmbufLibh
#define __INCmbufLibh

#ifdef __cplusplus
extern "C" {
#endif

/* includes */

#include "vwmodnum.h"
#include "intlib.h"
#include "limits.h"
#include "net/mbuf.h"

/* typedefs */

/* definition of mbuf ID */

typedef struct mbufId		/* MBUF_ID */
    {
    union
	{
        struct mbuf *		head;		/* head of mbuf chain */
        struct mbufId *		idNext;		/* next ID in free chain */
	} uId;

    u_char			type;		/* mbuf ID type */
    } *MBUF_ID;

/* definition of mbuf desc */

typedef struct mbufDesc		/* MBUF_DESC */
    {
    caddr_t			buf;		/* user buffer address */
    union
	{
        u_char			refCnt;		/* share counter */
        struct mbufDesc *	descNext;	/* next desc in free chain */
	} uDesc;
    } *MBUF_DESC;

typedef struct mbuf *		MBUF_SEG;

extern	struct mbufId *		_mbufIdHead;

/* defines */

#define	mbufHead		uId.head
#define	mbufIdNext		uId.idNext
#define	mbufRefCnt		uDesc.refCnt
#define	mbufDescNext		uDesc.descNext

/* status codes */

#define	S_mbufLib_ID_INVALID		(M_mbufLib | 1)
#define	S_mbufLib_ID_EMPTY		(M_mbufLib | 2)
#define	S_mbufLib_SEGMENT_NOT_FOUND	(M_mbufLib | 3)
#define	S_mbufLib_LENGTH_INVALID	(M_mbufLib | 4)
#define	S_mbufLib_OFFSET_INVALID	(M_mbufLib | 5)

#define	MBUF_ID_INC		50		/* increment for ID alloc */
#define	MBUF_DESC_INC		50		/* increment for desc alloc */
#define	MBUF_BEGIN		ZBUF_BEGIN	/* start of chain */
#define	MBUF_END		ZBUF_END	/* end of chain */
#define	MBUF_NONE		((MBUF_SEG) NONE) /* cut past chain */
#define	MBUF_VALID		0x5e		/* validate off magic number */

#define MBUF_ID_CREATE(mbufId)						\
    {									\
    int lockKey = intLock ();						\
    if ((mbufId = _mbufIdHead) != NULL)					\
        {								\
        _mbufIdHead = mbufId->mbufIdNext;				\
        intUnlock (lockKey);						\
        mbufId->type = MBUF_VALID;					\
        mbufId->mbufHead = NULL;					\
        }								\
    else 								\
        {								\
        intUnlock (lockKey);						\
	mbufId = _mbufCreate ();					\
        }								\
    }

#define	MBUF_ID_DELETE_EMPTY(mbufId)					\
    {									\
    int lockKey;							\
    mbufId->type = MT_FREE;						\
    lockKey = intLock ();						\
    mbufId->mbufIdNext = _mbufIdHead;					\
    _mbufIdHead = mbufId;						\
    intUnlock (lockKey);						\
    }

#define	MBUF_ID_DELETE(mbufId)						\
    {									\
    if (mbufId->mbufHead != NULL)					\
        m_freem (mbufId->mbufHead);					\
    MBUF_ID_DELETE_EMPTY(mbufId);					\
    }

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern void *	_mbufLibInit (void);
extern MBUF_ID	_mbufCreate (void);
extern STATUS	_mbufDelete (MBUF_ID mbufId);
extern MBUF_SEG	_mbufInsert (MBUF_ID mbufId1, MBUF_SEG mbufSeg, int offset,
                    MBUF_ID mbufId2);
extern MBUF_SEG	_mbufInsertBuf (MBUF_ID mbufId, MBUF_SEG mbufSeg, int offset,
                    caddr_t buf, int len, VOIDFUNCPTR freeRtn, int freeArg);
extern MBUF_SEG	_mbufInsertCopy (MBUF_ID mbufId, MBUF_SEG mbufSeg,
                    int offset, caddr_t buf, int len);
extern int	_mbufExtractCopy (MBUF_ID mbufId, MBUF_SEG mbufSeg,
                    int offset, caddr_t buf, int len);
extern MBUF_SEG	_mbufCut (MBUF_ID mbufId, MBUF_SEG mbufSeg, int offset,
		    int len);
extern MBUF_ID	_mbufSplit (MBUF_ID mbufId, MBUF_SEG mbufSeg, int offset);
extern MBUF_ID	_mbufDup (MBUF_ID mbufId, MBUF_SEG mbufSeg, int offset,
		    int len);
extern int	_mbufLength (MBUF_ID mbufId);
extern MBUF_SEG	_mbufSegFind (MBUF_ID mbufId, MBUF_SEG mbufSeg, int *pOffset);
extern MBUF_SEG	_mbufSegNext (MBUF_ID mbufId, MBUF_SEG mbufSeg);
extern MBUF_SEG	_mbufSegPrev (MBUF_ID mbufId, MBUF_SEG mbufSeg);
extern caddr_t	_mbufSegData (MBUF_ID mbufId, MBUF_SEG mbufSeg);
extern int	_mbufSegLength (MBUF_ID mbufId, MBUF_SEG mbufSeg);

#else	/* __STDC__ */

extern void *	_mbufLibInit ();
extern MBUF_ID	_mbufCreate ();
extern STATUS	_mbufDelete ();
extern MBUF_SEG	_mbufInsert ();
extern MBUF_SEG	_mbufInsertBuf ();
extern MBUF_SEG	_mbufInsertCopy ();
extern int	_mbufExtractCopy ();
extern MBUF_SEG	_mbufCut ();
extern MBUF_ID	_mbufSplit ();
extern MBUF_ID	_mbufDup ();
extern int	_mbufLength ();
extern MBUF_SEG	_mbufSegFind ();
extern MBUF_SEG	_mbufSegNext ();
extern MBUF_SEG	_mbufSegPrev ();
extern caddr_t	_mbufSegData ();
extern int	_mbufSegLength ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCmbufLibh */
