/* vxmIfLibP.h  - VxM interface library header file */

/* Copyright 1992 Wind River Systems, Inc. */

/*
modification history
---------------------
01d,27feb93,jcf  added new monitor features.
01c,22sep92,rrr  added support for c++
01b,23aug92,jcf  added VXM_IF_OPS typedef and macros for vector functions.
01a,30mar92,del  written.
*/

#ifndef __INCvxmIfLibPh
#define __INCvxmIfLibPh

#ifdef __cplusplus
extern "C" {
#endif

#ifndef	_ASMLANGUAGE

typedef struct		/* VXM_IF_OPS */
    {
    FUNCPTR	vxmTblGet;	/* function to access all other funcs */
    FUNCPTR	vxmIntVecSet;	/* set an interrupt vector via VXM */
    FUNCPTR	vxmIntVecGet;	/* get an interrupt vector via VXM */
    FUNCPTR	vxmBufRead;	/* read a buffer from the host */
    FUNCPTR	vxmBufWrite;	/* write a buffer to the host */
    FUNCPTR	vxmWrtBufFlush;	/* flush any pending data to the host */
    FUNCPTR	vxmHostQuery;	/* query the host for data */
    FUNCPTR	vxmClbkAdd;	/* add a calback to the callback table */
    FUNCPTR	vxmClbkState;	/* set the state of a callback */
    FUNCPTR	vxmClbkQuery;	/* query the host for callbacks events */
    } VXM_IF_OPS;

typedef struct		/* VXM_IF_ANCHOR */
    {
    UINT32	ifMagic;	/* magic number */
    FUNCPTR 	ifGetFunc;	/* func to access shared functions */
    } VXM_IF_ANCHOR;


enum VXM_IF_FUNCTIONS		/* reserved interface function numbers */
    {
    VXM_IF_BUF_WRT_FUNC = 0x01,		/* data transfer functions */
    VXM_IF_WRTBUF_FLUSH_FUNC,
    VXM_IF_BUF_RD_FUNC,
    VXM_IF_QUERY_FUNC,
    VXM_IF_INT_VEC_GET_FUNC,		/* get interrupt vector function */
    VXM_IF_INT_VEC_SET_FUNC,		/* set interrupt vector function */
    VXM_IF_FLT_VEC_SET_FUNC,		/* set/get fault vector (i960 only) */
    VXM_IF_CALLBACK_ADD_FUNC,		/* callback interface functions */
    VXM_IF_CALLBACK_STATE_FUNC,
    VXM_IF_CALLBACK_QUERY_FUNC,
    VXM_IF_ENTER_HOOK_SET_FUNC,		/* mon. enter/exit hook set funcs */
    VXM_IF_EXIT_HOOK_SET_FUNC
    };

enum VXM_IF_CALLBACKS		/* callback function numbers */
    {
    VXM_IF_CALLBACK_DBG,		/* reserved: vxMon/uWorks dbg */
    VXM_IF_CALLBACK_1,
    VXM_IF_CALLBACK_2,
    VXM_IF_CALLBACK_3,
    VXM_IF_CALLBACK_4,
    VXM_IF_CALLBACK_5,
    VXM_IF_CALLBACK_6,
    VXM_IF_CALLBACK_7,
    VXM_IF_CALLBACK_8,
    VXM_IF_CALLBACK_9,
    VXM_IF_CALLBACK_10
    };


/* misc. interface defines */

#define	VXM_IF_MAGIC			0xfeedface
#define	VXM_IF_TBL_MAX_FUNCS		0x10
#define	VXM_IF_MAX_CALLBACKS		0x10
#define VXM_IF_CALLBACK			0x7f000000
#define	VXM_IF_CALLBACK_ALL		-1
#define VXM_IF_CALLBACK_READY		0x01
#define	VXM_IF_QUERY 			0x01


/* monitor interface macros */

#define	VXM_IF_VEC_SET(vec,func)				\
        ((vxmIfOps.vxmIntVecSet == NULL) ? (ERROR) :		\
	 (* vxmIfOps.vxmIntVecSet) ((vec), (func)))

#define	VXM_IF_VEC_GET(vec)					\
        ((vxmIfOps.vxmIntVecGet == NULL) ? ((FUNCPTR) NULL) :	\
	 ((FUNCPTR) (* vxmIfOps.vxmIntVecGet) (vec)))

/* variable declarations */

extern VXM_IF_OPS vxmIfOps;

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern	STATUS 	vxmIfInit (VXM_IF_ANCHOR *pAnchor);
extern	BOOL 	vxmIfInstalled (void);
extern	STATUS 	vxmIfVecSet (FUNCPTR *vec, FUNCPTR func);
extern	FUNCPTR	vxmIfVecGet (FUNCPTR *vec);
extern	int 	vxmIfHostRead (char * pDest);
extern	void 	vxmIfHostWrite (char *pSrc, int	len);
extern	BOOL 	vxmIfHostQuery (void);
extern	void 	vxmIfWrtBufFlush (void);
extern	int 	vxmIfBufRead (char *pBuf, int nBytes);
extern	int 	vxmIfBufWrite (char * pBuf, int nBytes);
extern	STATUS 	vxmIfCallbackAdd (int funcNo, FUNCPTR func, UINT32 arg,
				  UINT32 maxargs, UINT32 state);
extern	STATUS	vxmIfCallbackExecute (int funcNo, int nArgs, ... );
extern	STATUS 	vxmIfCallbackReady (int funcNo);
extern	STATUS 	vxmIfCallbackQuery (int funcNo);

#else	/* __STDC__ */

extern	STATUS 	vxmIfInit ();
extern	BOOL 	vxmIfInstalled ();
extern	STATUS 	vxmIfVecSet ();
extern	FUNCPTR	vxmIfVecGet ();
extern	int 	vxmIfHostRead ();
extern	void 	vxmIfHostWrite ();
extern	BOOL 	vxmIfHostQuery ();
extern	void 	vxmIfWrtBufFlush ();
extern	int 	vxmIfBufRead ();
extern	int 	vxmIfBufWrite ();
extern	STATUS 	vxmIfCallbackAdd ();
extern	STATUS	vxmIfCallbackExecute ();
extern	STATUS 	vxmIfCallbackReady ();
extern	STATUS 	vxmIfCallbackQuery ();
#endif	/* __STDC__ */

#endif /*_ASMLANGUAGE*/

#ifdef __cplusplus
}
#endif

#endif /* __INCvxmIfLibPh */
