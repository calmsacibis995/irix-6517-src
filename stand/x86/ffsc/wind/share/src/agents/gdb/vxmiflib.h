/* vxmIfLib.c  - VxM interface library header file */

/*
modification history
---------------------
01h,01aug93,jcf added vxmIfBreakQuery().
01g,28jul93,jcf fixed VXM_IF_MAX_VALID.
01f,27jul93,jcf added vxmIfTtyNum and vxmIfIntAckSet.
01e,29mar93,del added vxmIfExit support.
01d,01feb93,del added support to allow UAC to set monitor enter/exit
		hooks, as well as interrupt lock level to use when
		in monitor.
01c,05jan93,del added VXM_IF_CALLBACK_DBG for task specific dbg mechanism.
01b,27sep92,del added VXM_IF_INT_VEC_GET and VXM_IF_FAULT_VEC_SET
		plus clean up of obsolete ty stuff.
01a,30mar92,del written.
*/

#ifndef INCvxmIfLibh
#define INCvxmIfLibh

#ifndef	_ASMLANGUAGE

typedef struct 
    {
    UINT32	ifMagic;		/* magic number */
    FUNCPTR 	ifGetFunc;		/* func to access shared functions */
    } VXM_IF_ANCHOR;

/* reserved interface function numbers */

enum VXM_IF_FUNCTIONS 
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
    VXM_IF_EXIT_HOOK_SET_FUNC,
    VXM_IF_INT_LVL_SET_FUNC,		/* interrupt level to use while in mon*/
    VXM_IF_EXIT_FUNC,			/* exit function */
    VXM_IF_INT_ACK_SET_FUNC,		/* update interrupt acknowledge table */
    VXM_IF_TTY_NUM_FUNC,		/* get tty channel used by VxMon */
    VXM_IF_BREAK_QUERY_FUNC,		/* query host for ctrl-c func */
    /* insert new function numbers BEFORE VXM_IF_MAX_VALID */
    VXM_IF_MAX_VALID
    };

/* XXX change this if fields are added to VXM_IF_FUNCIONS */
#define VXM_IF_MAX_VALID	VXM_IF_BREAK_QUERY_FUNC

#define VXM_IF_CALLBACK_0 	0	/* XXX string. hack */

/* callback function numbers */
enum VXM_IF_CALLBACKS
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
#define	VXM_IF_TBL_MAX_FUNCS		0x18
#define	VXM_IF_MAX_CALLBACKS		0x10
#define VXM_IF_CALLBACK			0x7f000000
#define	VXM_IF_CALLBACK_ALL		-1
#define VXM_IF_CALLBACK_READY		0x01
#define	VXM_IF_QUERY 			0x01


#ifndef __STDC__
IMPORT	FUNCPTR	vxmIfEnterHookSet ();
IMPORT	FUNCPTR	vxmIfExitHookSet ();
IMPORT	BOOL	vxmIfBrkIgnore ();
IMPORT	int 	vxmIfBufRead ();
IMPORT	int 	vxmIfBufWrite ();
IMPORT	STATUS 	vxmIfCallbackAdd ();
IMPORT	STATUS	vxmIfCallbackExecute ();
IMPORT	STATUS 	vxmIfCallbackReady ();
IMPORT	STATUS 	vxmIfCallbackQuery ();
IMPORT	STATUS 	vxmIfInit ();
IMPORT	BOOL 	vxmIfInstalled ();
IMPORT	BOOL 	vxmIfHostQuery ();
IMPORT	int 	vxmIfHostRead ();
IMPORT	void 	vxmIfHostWrite ();
IMPORT	STATUS 	vxmIfVecSet ();
IMPORT	STATUS 	vxmIfWrtBufFlush ();
IMPORT	STATUS 	vxmIfIntAckSet ();
IMPORT	int 	vxmIfTtyNum ();
IMPORT	int 	vxmIfBreakQuery ();

#else /* __STDC__ */
IMPORT	FUNCPTR	vxmIfEnterHookSet (FUNCPTR hookFunc, UINT32 hookArg);
IMPORT	FUNCPTR	vxmIfExitHookSet (FUNCPTR hookFunc, UINT32 hookArg);
IMPORT	BOOL	vxmIfBrkIgnore (int tid);
IMPORT	int 	vxmIfBufRead (char *pBuf, int nBytes);
IMPORT	int 	vxmIfBufWrite (char * pBuf, int nBytes);
IMPORT	STATUS 	vxmIfCallbackAdd (int funcNo, FUNCPTR func, UINT32 arg, 
				  UINT32 maxargs, UINT32 state);
IMPORT	STATUS	vxmIfCallbackExecute (int funcNo, int nArgs, ... );
IMPORT	STATUS 	vxmIfCallbackReady (int funcNo);
IMPORT	STATUS 	vxmIfCallbackQuery (int funcNo);
IMPORT	STATUS 	vxmIfInit (VXM_IF_ANCHOR *pAnchor);
IMPORT	BOOL 	vxmIfInstalled ();
IMPORT	BOOL 	vxmIfHostQuery ();
IMPORT	int 	vxmIfHostRead (char * pDest);
IMPORT	void 	vxmIfHostWrite (char *pSrc, int	len);
IMPORT	STATUS 	vxmIfVecSet (FUNCPTR *vec, FUNCPTR func);
IMPORT	STATUS 	vxmIfWrtBufFlush ();
IMPORT	STATUS 	vxmIfIntAckSet (UINT *ackTable);
IMPORT	int 	vxmIfTtyNum ();
IMPORT	STATUS 	vxmIfBreakQuery (FUNCPTR breakFunc);
#endif /* __STDC__ */

#endif /*_ASMLANGUAGE*/
#endif /* INCvxmIfLibh */
