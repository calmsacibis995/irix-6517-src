/* wdb.h - WDB protocol definition header */

/* Copyright 1984-1995 Wind River Systems, Inc. */

/*
modification history
--------------------
01e,10jun95,pad removed #include <rpc/xdr.h> and moved #include <rpc/types.h>
		into #else statement.
01d,07jun95,ms  added WDB_ERR_NOT_FOUND 
01c,01jun95,c_s added some gopher constants
01b,17may95,tpr added #include <rpc/xdr.h>.
01a,04apr95,ms	derived from work by tpr (and pme and ms).
		  merged wdb.h, wdbtypes.h, comtypes.h, and  xdrwdb.h
		  removed #ifdef UNIX
		  made all data types of the form wdbXxx_t.
		  added types WDB_STRING_T, WDB_OPQ_DATA_T, TGT_ADDR_T
		    TGT_INT_T, and UINT32
		  made most event data an array of ints.
		  removed obsolete data types.
*/

#ifndef __INCwdbh
#define __INCwdbh

#ifdef __cplusplus
extern "C" {
#endif

/*
DESCRIPTION

The WDB protocol provides and extensible interface for remote debugging.

REQUIREMENTS

Both the host and target must support 32 bit integers.
All tasks on the target must share a common address space.

INTERFACE

errCode = wtxBeCall (HBackend, procNum, &params, &reply);

DATA TYPES

	UINT32		/@ 32 bit unsigned integer (defined in host.h)  @/
	BOOL		/@ TRUE or FALSE @/
	STATUS		/@ OK or ERROR @/
	TGT_ADDR_T	/@ target address  (defined in host.h) @/
	TGT_INT_T	/@ target integer  (defined in host.h) @/
	WDB_STRING_T	/@ pointer to a string (a local address) @/
	WDB_OPQ_DATA_T	/@ pointer to a block of memory (a local address) @/

Each side must also provide the following XDR filters:

	xdr_UINT32	/@ encode/decode a 32 bit integer @/
*/

/* includes */

#ifdef HOST
#include "host.h"
#else
#include "types/vxansi.h"
#include <rpc/types.h>
#endif

/* definitions */

/*
 * WDB function numbers.
 * Each remote service is identified by an integer.
 */

	/* Session Management */

#define WDB_TARGET_PING			 0	/* check if agent is alive */
#define	WDB_TARGET_CONNECT		 1	/* connect to the agent */
#define WDB_TARGET_DISCONNECT		 2	/* terminate the connection */
#define WDB_TARGET_MODE_SET		 3	/* change the agents mode */

	/* Memory Operations */

#define	WDB_MEM_READ			10	/* read a memory block */
#define	WDB_MEM_WRITE			11	/* write a memory block */
#define	WDB_MEM_FILL			12	/* fill memory with pattern */
#define WDB_MEM_MOVE			13	/* move memory on target */
#define	WDB_MEM_CHECKSUM		14	/* checksum a memory block */
#define WDB_MEM_PROTECT			15	/* write (un)protecting */
#define WDB_MEM_CACHE_TEXT_UPDATE	16	/* called after loading text */
#define WDB_MEM_SCAN			17	/* scan memory for a pattern */
#define WDB_MEM_WRITE_MANY		18	/* scatter write */
#define WDB_MEM_WRITE_MANY_INT		19	/* scatter write of ints */

	/* Context Control */

#define	WDB_CONTEXT_CREATE		30	/* create a new context */
#define	WDB_CONTEXT_KILL		31	/* remove a context */
#define	WDB_CONTEXT_SUSPEND		32	/* suspend a context */
#define WDB_CONTEXT_RESUME		33	/* resume a context */

	/* Register Manipulation */

#define	WDB_REGS_GET			40	/* get register(s) */
#define	WDB_REGS_SET			41	/* set register(s) */

	/* Virtual I/O */

#define	WDB_VIO_WRITE			51	/* write a virtual I/O buffer */

	/* Eventpoints */

#define	WDB_EVENTPOINT_ADD		60	/* add an eventpoint */
#define	WDB_EVENTPOINT_DELETE		61	/* delete an eventpoint */

	/* Events */

#define	WDB_EVENT_GET			70	/* get info about an event */

	/* debugging */

#define WDB_CONTEXT_CONT		80	/* XXX - same as resume? */
#define	WDB_CONTEXT_STEP		81	/* continue a context */

	/* Miscelaneous */

#define	WDB_FUNC_CALL			90	/* spawn a function */
#define WDB_EVALUATE_GOPHER		91	/* evaluate a gopher tape */
#define WDB_DIRECT_CALL			92	/* call a function directly */

/*
 * WDB error codes.
 * Each WDB function returns an error code.
 * If the error code is zero (OK), then the procedure succeded and
 * the reply data is valid.
 * If the error code is nonzero, then the procedure failed. In this case
 * the error code indicates the reason for failure and the reply data
 * is invalid.
 */

#define WDB_OK				OK	/* success */
#define WDB_ERR_INVALID_PARAMS		0x501	/* params invalid */
#define WDB_ERR_MEM_ACCES	 	0x502	/* memory fault */
#define WDB_ERR_AGENT_MODE		0x503	/* wrong agent mode */
#define WDB_ERR_RT_ERROR 		0x504	/* run-time callout failed */
#define WDB_ERR_INVALID_CONTEXT 	0x505	/* bad task ID */
#define WDB_ERR_INVALID_VIO_CHANNEL	0x506	/* bad virtual I/O channel */
#define WDB_ERR_INVALID_EVENT 		0x507	/* no such event type */
#define WDB_ERR_INVALID_EVENTPOINT 	0x508	/* no such eventpoint */
#define WDB_ERR_GOPHER_FAULT 		0x509	/* gopher fault */
#define WDB_ERR_GOPHER_TRUNCATED 	0x50a	/* gopher tape too large */
#define WDB_ERR_EVENTPOINT_TABLE_FULL	0x50b	/* out of room */
#define WDB_ERR_NO_AGENT_PROC		0x50c	/* agent proc not installed */
#define WDB_ERR_NO_RT_PROC		0x50d	/* run-time callout unavail */
#define WDB_ERR_GOPHER_SYNTAX		0x50e   /* gopher syntax error */
#define WDB_ERR_NOT_FOUND		0x50f	/* object not found */
#define WDB_ERR_PROC_FAILED		0x5ff	/* generic proc failure */

#define WDB_ERR_NO_CONNECTION		0x600	/* not connected */
#define WDB_ERR_CONNECTION_BUSY		0x601	/* someone else connected */
#define WDB_ERR_COMMUNICATION		0x6ff	/* generic comm error */

/*
 * WDB miscelaneous definitions.
 */

	/* agent modes */

#define WDB_MODE_TASK	1			/* task mode agent */
#define WDB_MODE_EXTERN	2			/* system mode agent */
#define WDB_MODE_DUAL	(WDB_MODE_TASK | WDB_MODE_EXTERN) /* dual mode */
#define WDB_MODE_BI	WDB_MODE_DUAL

	/* maximum number of words of event data */

#define WDB_MAX_EVT_DATA	20

	/* gopher stream format type codes */

#define GOPHER_UINT32		0
#define GOPHER_STRING		1
#define GOPHER_UINT16		2
#define GOPHER_UINT8            3
#define GOPHER_FLOAT32          4
#define GOPHER_FLOAT64          5
#define GOPHER_FLOAT80          6

	/* option bits for task creation */

#define WDB_UNBREAKABLE	0x0002		/* ignore breakpoints */
#define WDB_FP_TASK	0x0008		/* task uses floating point */
#define WDB_FP_RETURN	0x8000		/* return value is a double */

/* data types */

/*
 * WDB primitive data types.
 */

#ifndef	HOST
typedef char *	TGT_ADDR_T;
typedef int	TGT_INT_T;
#define BOOL	bool_t
#endif

typedef char *	WDB_STRING_T;
typedef char *	WDB_OPQ_DATA_T;

/*
 * WDB compound data types.
 */

typedef struct wdb_mem_region		/* a region of target memory */
    {
    TGT_ADDR_T		baseAddr;	/* memory region base address */
    TGT_INT_T		numBytes;	/* memory region size */
    UINT32		param;		/* proc dependent parameter */
    } WDB_MEM_REGION;

typedef struct wdb_mem_xfer		/* transfer a block of memory */
    {
    WDB_OPQ_DATA_T	source;		/* data to transfer */
    TGT_ADDR_T		destination;	/* requested destination */
    TGT_INT_T		numBytes;	/* number of bytes transfered */
    } WDB_MEM_XFER;

typedef struct wdb_mem_scan_desc
    {
    WDB_MEM_REGION	memRegion;	/* region of memory to scan */
    WDB_MEM_XFER	memXfer;	/* pattern to scan for */
    } WDB_MEM_SCAN_DESC;

typedef enum wdb_ctx_type		/* type of context on the target */
    {
    WDB_CTX_SYSTEM	= 0,		/* system mode */
    WDB_CTX_GROUP	= 1,		/* process group */
    WDB_CTX_ANY		= 2,		/* any context */
    WDB_CTX_TASK	= 3,		/* specific task or processes */
    WDB_CTX_ANY_TASK	= 4,		/* any task */
    WDB_CTX_ISR		= 5,		/* specific ISR */
    WDB_CTX_ANY_ISR	= 6		/* any ISR */
    } WDB_CTX_TYPE;

typedef struct wdb_ctx			/* a particular context */
    {
    WDB_CTX_TYPE	contextType;	/* type of context */
    UINT32		contextId;	/* context ID */
    } WDB_CTX;

typedef struct wdb_ctx_step_desc	/* how to sigle step a context */
    {
    WDB_CTX		context;	/* context to step */
    TGT_ADDR_T		startAddr;	/* lower bound of step range */
    TGT_ADDR_T 		endAddr;	/* upper bound of step range */
    } WDB_CTX_STEP_DESC;

typedef struct wdb_ctx_create_desc	/* how to create a context */
    {
    WDB_CTX_TYPE	contextType;	/* task or system context */

    /* the following are used for task and system contexts */

    TGT_ADDR_T		stackBase;	/* bottom of stack (NULL = malloc) */
    UINT32		stackSize;	/* stack size */
    TGT_ADDR_T 		entry;		/* context entry point */
    TGT_INT_T		args[10];	/* arguments */

    /* the following are only used for task contexts */

    WDB_STRING_T	name;		/* name */
    TGT_INT_T		priority;	/* priority */
    TGT_INT_T		options;	/* options */
    TGT_INT_T		redirIn;	/* redirect input file (or 0) */
    TGT_INT_T		redirOut;	/* redirect output file (or 0) */
    TGT_INT_T		redirErr;	/* redirect error output file (or 0) */
    } WDB_CTX_CREATE_DESC;

typedef enum wdb_reg_set_type		/* a type of register set */
    {
    WDB_REG_SET_IU	= 0,            /* integer unit register set */
    WDB_REG_SET_FPU	= 1,            /* floating point unit register set */
    WDB_REG_SET_MMU	= 2,            /* memory management unit reg set */
    WDB_REG_SET_CU	= 3,            /* cache unit register set */
    WDB_REG_SET_TPU	= 4,            /* timer processor unit register set */
    WDB_REG_SET_SYS	= 5		/* system registers */
    } WDB_REG_SET_TYPE;

typedef struct wdb_reg_read_desc	/* register data to read */
    {
    WDB_REG_SET_TYPE regSetType;	/* type of register set to read */
    WDB_CTX	     context;		/* context associated with registers */
    WDB_MEM_REGION   memRegion;		/* subregion of the register block */
    } WDB_REG_READ_DESC;

typedef struct wdb_reg_write_desc	/* register data to write */
    {
    WDB_REG_SET_TYPE regSetType;	/* type of register set to write */
    WDB_CTX	     context;		/* context associated with registers */
    WDB_MEM_XFER     memXfer;		/* new value of the register set */
    } WDB_REG_WRITE_DESC;

typedef enum wdb_rt_type		/* type of run-time system */
    {
    WDB_RT_NULL		= 0,		/* standalone WDB agent */
    WDB_RT_VXWORKS	= 1		/* WDB agent integrated in VxWorks */
    } WDB_RT_TYPE;

typedef struct wdb_rt_info		/* info on the run-time system */
    {
    WDB_RT_TYPE		rtType;		/* runtime type */
    WDB_STRING_T	rtVersion;	/* run time version */
    TGT_INT_T		cpuType;	/* target processor type */
    BOOL		hasFpp;		/* target has a floating point unit */
    BOOL		hasWriteProtect; /* target can write protect memory */
    TGT_INT_T		pageSize;	/* size of a page */
    TGT_INT_T		endian;		/* endianness (0x4321 or 0x1234) */
    WDB_STRING_T	bspName;	/* board support package name */
    WDB_STRING_T	bootline;	/* boot file path or NULL if embedded */
    TGT_ADDR_T		memBase;	/* target main memory base address */
    UINT32		memSize;	/* target main memory size */
    TGT_INT_T		numRegions;	/* number of memory regions */
    WDB_MEM_REGION *	memRegion;	/* memory region descriptor(s) */
    TGT_ADDR_T		hostPoolBase;	/* host-controlled tgt memory pool */
    UINT32		hostPoolSize;	/* host-controlled memory pool size */
    } WDB_RT_INFO;

typedef struct wdb_agent_info		/* info on the debug agent */
    {
    WDB_STRING_T	agentVersion;	/* version of the WDB agent */
    TGT_INT_T		mtu;		/* maximum tranfert size in bytes */
    TGT_INT_T		mode;		/* available agent modes */
    } WDB_AGENT_INFO;

typedef struct wdb_tgt_info		/* info on the target */
    {
    WDB_AGENT_INFO  	agentInfo;	/* info on the agent */
    WDB_RT_INFO	    	rtInfo;		/* info on the run time system */
    } WDB_TGT_INFO;

typedef enum wdb_evt_type		/* type of event on the target */
    {
    WDB_EVT_NONE	= 0,		/* no event */
    WDB_EVT_CTX_START	= 1,		/* context creation */
    WDB_EVT_CTX_EXIT	= 2,		/* context exit */
    WDB_EVT_BP		= 3,		/* breakpoint */
    WDB_EVT_HW_BP	= 4,		/* hardware breakpoint */
    WDB_EVT_WP		= 5,		/* watchpoint */
    WDB_EVT_EXC		= 6,		/* exception */
    WDB_EVT_VIO_WRITE	= 7,		/* virtual I/O write */
    WDB_EVT_CALL_RET	= 8		/* function call finished */
    } WDB_EVT_TYPE;

typedef enum wdb_action_type		/* what to do when an event occurs */
    {
    WDB_ACTION_CALL	= 1,		/* condition the evtpt via a proc */
    WDB_ACTION_NOTIFY	= 2,		/* notify the host */
    WDB_ACTION_STOP	= 4		/* stop the context */
    } WDB_ACTION_TYPE;

typedef struct wdb_action		/* a specific action */
    {
    WDB_ACTION_TYPE	actionType;
    UINT32		actionArg;
    TGT_ADDR_T		callRtn;
    TGT_INT_T		callArg;
    } WDB_ACTION;

typedef struct wdb_evtpt_add_desc	/* how to add an eventpt */
    {
    WDB_EVT_TYPE	evtType;	/* type of event to detect */
    TGT_INT_T		numArgs;	/* eventType dependent arguments */
    UINT32  *  		args;		/* arg list */
    WDB_CTX		context;	/* context in which event must occur */
    WDB_ACTION 		action;		/* action to perform */
    } WDB_EVTPT_ADD_DESC;

typedef struct wdb_evtpt_del_desc	/* how to delete an eventpoint */
    {
    WDB_EVT_TYPE 	evtType;	/* type of event */
    TGT_ADDR_T	 	evtptId;	/* eventpoint ID */
    } WDB_EVTPT_DEL_DESC;

typedef struct wdb_evt_info		/* event info for anything but VIO */
    {
    TGT_INT_T		numInts;	 /* number of ints of event data */
    UINT32 		info [WDB_MAX_EVT_DATA]; /* event data */
    } WDB_EVT_INFO;

typedef enum wdb_call_ret_type		/* type of return value */
    {
    WDB_CALL_RET_INT	= 0,
    WDB_CALL_RET_DBL	= 1
    } WDB_CALL_RET_TYPE;

typedef struct wdb_call_return_info
    {
    UINT32		callId;		/* returned from WDB_FUNC_CALL */
    WDB_CALL_RET_TYPE	returnType;	/* return type */
    union
	{
	double		returnValDbl;	/* double return value */
	TGT_INT_T	returnValInt;	/* integer return value */
	} returnVal;
    TGT_INT_T		errnoVal;	/* error status */
    } WDB_CALL_RET_INFO;

typedef struct wdb_evt_data		/* reply to a WDB_EVENT_GET */
    {
    WDB_EVT_TYPE 	evtType;	/* event type detected */
    union 				/* eventType specific info */
	{
	WDB_MEM_XFER		vioWriteInfo;	/* vio write event data */
	WDB_CALL_RET_INFO	callRetInfo;	/* call return event data */
	WDB_EVT_INFO		evtInfo;	/* any other event info */
	} eventInfo;
    } WDB_EVT_DATA;

typedef struct wdb_exc_info		/* WDB_EVT_INFO for exceptions */
    {
    TGT_INT_T		numInts;	/* 4 */
    WDB_CTX		context;	/* context that incurred the exception */
    TGT_INT_T		vec;		/* hardware trap number */
    TGT_ADDR_T		pEsf;		/* address of the exception stack frame */
    } WDB_EXC_INFO;

typedef struct wdb_bp_info		/* WDB_EVT_INFO for breakpoints */
    {
    TGT_INT_T		numInts;	/* 5 */
    WDB_CTX		context;	/* context when the breakpoint was hitten */
    TGT_ADDR_T		pc;		/* program counter */
    TGT_ADDR_T		fp;		/* frame pointer */
    TGT_ADDR_T		sp;		/* stack pointer */
    } WDB_BP_INFO;

typedef struct wdb_ctx_exit_info	/* WDB_EVT_INFO for context exit */
    {
    TGT_INT_T		numInts;	/* 4 */
    WDB_CTX		context;	/* context that exited */
    TGT_INT_T		returnVal;	/* context's return value/exit code */
    TGT_INT_T		errnoVal;	/* context's error status */
    } WDB_CTX_EXIT_INFO;

#ifdef __cplusplus
}
#endif

#endif /* __INCwdbh */

