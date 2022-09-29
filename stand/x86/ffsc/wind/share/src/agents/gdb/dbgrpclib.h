/* dbgRpcLib.h - header file for remote debugging via rpc */

/*
modification history
--------------------
01b,29aug92,maf  minor cleanups for ANSI compliance.
01a,09jan91,maf  created using v1c of dbgRpcLib.h from 4.0.2 VxWorks 68k.
                 added VX_SOURCE_STEP.
*/

#ifndef __INCdbxRpcLibh
#define __INCdbxRpcLibh	1

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
#define VX_BOOT_FILE_INQ	70
#define VX_SOURCE_STEP		71

#endif /* __INCdbxRpcLibh */
