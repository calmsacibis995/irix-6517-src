/* aioSysDrv.h - AIO system driver header file */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,12jan94,kdl  changed name from aioSysPxLib to aioSysDrv; general cleanup.
01b,06dec93,dvs  removed S_aioSysPxLib_NO_PIPES
01a,04apr93,elh  written.
*/

#ifndef __INCaioSysDrvh
#define __INCaioSysDrvh

/* includes */

#ifdef __cplusplus
extern "C" {
#endif

#include "aio.h"

/* defines */

/* default AIO task parameters */

#define AIO_IO_TASKS_DFLT       2               /* number of I/O tasks */
#define AIO_IO_STACK_DFLT       0x7000          /* I/O task stack size */
#define AIO_IO_PRIO_DFLT        50              /* I/O task priority */
#define AIO_TASK_OPT            0               /* task options */
#define AIO_WAIT_STACK          0x7000          /* wait task stack size */

/* forward declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS aioSysInit (int numTasks, int taskPrio, int taskStackSize);

#else	/* __STDC__ */

extern STATUS aioSysInit ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCaioSysDrvh */
