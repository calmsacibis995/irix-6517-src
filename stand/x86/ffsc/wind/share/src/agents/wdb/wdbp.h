/* wdbP.h - WDB protocol extentions for the RPC backend */

/* Copyright 1984-1995 Wind River Systems, Inc. */

/*
modification history
--------------------
01d,25oct95,ms  changed WDB_VERSION to "1.0"
01c,15jun95,tpr added xdr_CHECKSUM prototype.
01b,23may95,ms	added WIN32 prototypes.
01a,04apr95,ms	derived from work by tpr (and pme and ms).
		  merged wdb.h, wdbtypes.h, comtypes.h, and  xdrwdb.h
		  removed #ifdef UNIX
		  made all data types of the form wdbXxx_t.
		  added types WDB_STRING_T, WDB_OPQ_DATA_T.
		  required types TGT_ADDR_T and UINT32 be defined in host.h.
		  made most event data an array of ints.
		  removed obsolete data types.
*/

#ifndef __INCwdbPh
#define __INCwdbPh

#ifdef __cplusplus
extern "C" {
#endif

/*
DESCRIPTION

This header file describes the backend of the tgtsvr which is specific
to the RPC based WDB agent.

The WDB agent has a fixed UDP port number in order to remove the complexity
of having a portmapper (which adds significant overhead if the
agent is configured as a ROM-monitor).

Every RPC request sent to the agent has a four byte sequence number
appended to the begining of the parameters. The two most significant
bytes are the ID of the host.

Every RPC reply from the agent has a four byte error status appended
to the begining of the reply. One of the bits of the error status
is reserved to mean that events are pending on the target. The rest
of the error status is either OK (on success) or the reason
for failure. If a procedure fails, then the rest of the reply is
not decoded (e.g., it contains garbage).
*/

/* includes */

#include "wdb.h"

/* session information */

#define WDBPORT (u_short) 0x4321		/* UDP port to connect */
#define WDBPROG	(u_long)  0x55555555		/* RPC program number */
#define WDBVERS	(u_long)  1			/* RPC version number */
#define WDB_VERSION_STR	  "1.0-alpha"

/* message cores */

#define WDB_EVENT_NOTIFY	0x8000		/* notify bit in errCode */
#define WDB_HOST_ID_MASK	0xffff0000	/* hostId in seqNum */

/* data types */

typedef struct wdb_param_wrapper
    {
    void *	pParams;	/* real parameters */
    xdrproc_t	xdr;		/* XDR filter for the real params */
    UINT32	seqNum;		/* sequence number */
    } WDB_PARAM_WRAPPER;

typedef struct wdb_reply_wrapper
    {
    void *	pReply;		/* real reply */
    xdrproc_t	xdr;		/* XDR filter for the real reply */
    UINT32	errCode;	/* error status */
    } WDB_REPLY_WRAPPER;

/* function prototypes */

#if defined(__STDC__) || defined(__cplusplus) || defined(WIN32_COMPILER)

extern BOOL	xdr_UINT32		(XDR *xdrs, UINT32 *objp);
extern BOOL	xdr_TGT_ADDR_T		(XDR *xdrs, TGT_ADDR_T *objp);
extern BOOL	xdr_TGT_INT_T		(XDR *xdrs, TGT_INT_T *objp);
extern BOOL	xdr_WDB_STRING_T		(XDR *xdrs, WDB_STRING_T *objp);
extern BOOL	xdr_WDB_OPQ_DATA_T	(XDR *xdrs, WDB_OPQ_DATA_T *objp,
					 UINT32 len);
extern BOOL	xdr_ARRAY		(XDR *xdrs, char **, TGT_INT_T *,
					 TGT_INT_T, TGT_INT_T,
					 xdrproc_t elproc);
extern BOOL	xdr_CHECKSUM		(XDR *xdrs, UINT32 xdrCksumVal,
					 UINT32 xdrStreamSize,
					 UINT32 xdrCksumValPos,
					 UINT32 xdrStreamSizePos);

extern BOOL	xdr_WDB_PARAM_WRAPPER	(XDR *xdrs, WDB_PARAM_WRAPPER *objp);
extern BOOL	xdr_WDB_REPLY_WRAPPER	(XDR *xdrs, WDB_REPLY_WRAPPER *objp);
extern BOOL	xdr_WDB_TGT_INFO 	(XDR *xdrs, WDB_TGT_INFO *objp);
extern BOOL	xdr_WDB_MEM_REGION	(XDR *xdrs, WDB_MEM_REGION *objp);
extern BOOL	xdr_WDB_MEM_XFER	(XDR *xdrs, WDB_MEM_XFER *objp);
extern BOOL	xdr_WDB_MEM_SCAN_DESC	(XDR *xdrs, WDB_MEM_SCAN_DESC *objp);
extern BOOL	xdr_WDB_CTX		(XDR *xdrs, WDB_CTX *objp);
extern BOOL	xdr_WDB_CTX_CREATE_DESC	(XDR *xdrs, WDB_CTX_CREATE_DESC *objp);
extern BOOL	xdr_WDB_CTX_STEP_DESC	(XDR *xdrs, WDB_CTX_STEP_DESC *objp);
extern BOOL	xdr_WDB_REG_READ_DESC	(XDR *xdrs, WDB_REG_READ_DESC *objp);
extern BOOL	xdr_WDB_REG_WRITE_DESC	(XDR *xdrs, WDB_REG_WRITE_DESC *objp);
extern BOOL	xdr_WDB_EVTPT_ADD_DESC	(XDR *xdrs, WDB_EVTPT_ADD_DESC *objp);
extern BOOL	xdr_WDB_EVTPT_DEL_DESC	(XDR *xdrs, WDB_EVTPT_DEL_DESC *objp);
extern BOOL	xdr_WDB_EVT_DATA	(XDR *xdrs, WDB_EVT_DATA *objp);

#else	/* __STDC__ */

extern BOOL	xdr_UINT32		();
extern BOOL	xdr_BOOL		();
extern BOOL	xdr_TGT_ADDR_T		();
extern BOOL	xdr_TGT_INT_T		();
extern BOOL	xdr_WDB_STRING_T	();
extern BOOL	xdr_WDB_OPQ_DATA_T	();
extern BOOL	xdr_ARRAY		();
extern BOOL	xdr_CHECKSUM		();

extern BOOL	xdr_PARAM_WRAPPER	();
extern BOOL	xdr_REPLY_WRAPPER	();
extern BOOL	xdr_WDB_TGT_INFO 	();
extern BOOL	xdr_WDB_MEM_REGION	();
extern BOOL	xdr_WDB_MEM_XFER	();
extern BOOL	xdr_WDB_MEM_SCAN_DESC	();
extern BOOL	xdr_WDB_CTX		();
extern BOOL	xdr_WDB_CTX_CREATE_DESC	();
extern BOOL	xdr_WDB_CTX_STEP_DESC	();
extern BOOL	xdr_WDB_REG_READ_DESC	();
extern BOOL	xdr_WDB_REG_WRITE_DESC	();
extern BOOL	xdr_WDB_EVTPT_ADD_DESC	();
extern BOOL	xdr_WDB_EVTPT_DEL_DESC	();
extern BOOL	xdr_WDB_EVT_DATA	();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCwdbPh */

