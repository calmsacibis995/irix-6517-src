/* wdbMbufLib.h - header file for WDB I/O buffer handling */

/* Copyright 1984-1995 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,04may95,ms  written.
*/

#ifndef __INCwdbMbufLibh
#define __INCwdbMbufLibh

/* includes */

#include "net/mbuf.h"

/* macros */

#define wdbMbufClusterInit(pMbuf, addr, nBytes, callBackRtn, callBackArg) \
    {									\
    (pMbuf)->m_next       = NULL;					\
    (pMbuf)->m_act        = NULL;					\
    (pMbuf)->m_off        = (int)(addr) - (int)(pMbuf);			\
    (pMbuf)->m_len        = nBytes;					\
    (pMbuf)->m_type       = MT_DATA;					\
    (pMbuf)->m_ctype      = MC_UCLUSTER;				\
    (pMbuf)->m_cfreeRtn   = callBackRtn;				\
    (pMbuf)->m_cbuf       = (struct mbuf *) (addr);			\
    (pMbuf)->m_refcnt     = ((unsigned char *)&(pMbuf)->m_olen + 4);	\
    *(pMbuf)->m_refcnt    = 1;						\
    (pMbuf)->m_arg1       = callBackArg;				\
    (pMbuf)->m_arg2       = 0;						\
    (pMbuf)->m_arg3       = 0;						\
    (pMbuf)->m_olen       = nBytes;					\
    }

#define wdbMbufChainFree(pMbuf)						\
    {									\
    struct mbuf *__pNextMbuf;						\
    struct mbuf *__pThisMbuf;						\
    __pThisMbuf = pMbuf;						\
    while (__pThisMbuf != NULL)						\
	{								\
	__pNextMbuf = __pThisMbuf->m_next;				\
	wdbMbufFree (__pThisMbuf);					\
	__pThisMbuf = __pNextMbuf;					\
	}								\
    }

#define wdbMbufDataGet(__pMbuf, __pDest, __len, __pLen)			\
    {									\
    int __nBytes;							\
    struct mbuf * __pThisMbuf;						\
									\
    __nBytes = 0;							\
    for (__pThisMbuf = __pMbuf;						\
         __pThisMbuf != NULL;						\
         __pThisMbuf = __pThisMbuf->m_next)				\
        {								\
        int __moreBytes = MIN (__pThisMbuf->m_len, __len - __nBytes);	\
        bcopy (mtod (__pThisMbuf, char *), __pDest + __nBytes, __moreBytes); \
        __nBytes += __moreBytes;					\
        if (__nBytes >= __len)						\
            break;							\
        }								\
									\
    *(__pLen) = __nBytes;						\
    }

/* function prototypes */

#ifdef  __STDC__

extern struct mbuf *	wdbMbufAlloc     (void);
extern void             wdbMbufFree      (struct mbuf * segId);

#else   /* __STDC__ */

extern struct mbuf *	wdbMbufAlloc     ();
extern void             wdbMbufFree      ();

#endif  /* __STDC__ */

#endif	/* __INCwdbMbufLibh */

