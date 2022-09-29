/* dbgRpcLib.h - header file for remote debugging via rpc */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01f,22sep92,rrr  added support for c++
01e,04jul92,jcf  cleaned up.
01d,26may92,rrr  the tree shuffle
01c,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
01b,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01a,05jun90,llk  extracted from xdr_dbx.h.
*/

#ifndef __INCdbgRpcLibh
#define __INCdbgRpcLibh

#ifdef __cplusplus
extern "C" {
#endif

#define PROCESS_START		50
#define PROCESS_WAIT		51
#define VX_STATE_INQ		60
#define VX_LOAD			61
#define VX_SYMBOL_INQ		62
#define VX_BREAK_ADD		63
#define VX_BREAK_DELETE		64
#define VX_FP_INQUIRE		65
#define VX_TASK_SUSPEND		66
#define VX_CALL_FUNC		67
#define VX_CONV_FROM_68881	68
#define VX_CONV_TO_68881	69

#ifdef __cplusplus
}
#endif

#endif /* __INCdbgRpcLibh */
