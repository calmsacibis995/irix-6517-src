/* dll.h - doubly linked list library */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,11oct95,ms  removed "static __inline__" (SPR #4500)
01b,05apr95,ms  cleanup + fixes for when !define(__GNUC__).
01a,10oct94,rrr written.
*/

#ifndef _INCdllh
#define _INCdllh

typedef struct __dll
    {
    struct __dll *__dll_next;
    struct __dll *__dll_prev;
    } dll_t;

#define __dll_init(d)	((d)->__dll_next = (d)->__dll_prev = (d))
#define __dll_remove(d) ((d)->__dll_prev->__dll_next = (d)->__dll_next, \
			 (d)->__dll_next->__dll_prev = (d)->__dll_prev)
#define __dll_head(d)	((d)->__dll_next)
#define __dll_tail(d)	((d)->__dll_prev)
#define __dll_next(d)	((d)->__dll_next)
#define __dll_prev(d)	((d)->__dll_prev)
#define __dll_end(d)	(d)
#define __dll_empty(d)  ((d)->__dll_next == (d))
#define __dll_insert(pNode, pPrev) {            \
    __dll_next(pNode) = __dll_next(pPrev);      \
    __dll_prev(pNode) = (pPrev);                \
    __dll_next(pPrev) = (pNode);                \
    __dll_prev(__dll_next(pNode)) = (pNode);    \
    }

#define dll_empty(pList)	__dll_empty(pList)
#define dll_insert(pNode, pPrev) __dll_insert(pNode, pPrev)
#define dll_init(pNode)		__dll_init(pNode)
#define dll_remove(pNode)	__dll_remove(pNode)
#define dll_head(pNode)		__dll_head(pNode)
#define dll_tail(pNode)		__dll_tail(pNode)
#define dll_next(pNode)		__dll_next(pNode)
#define dll_prev(pNode)		__dll_prev(pNode)
#define dll_end(pNode)		__dll_end(pNode)

#endif	/* _INCdllh */
