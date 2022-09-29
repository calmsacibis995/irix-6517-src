/* fioLib.h - header for formatted i/o library */

/* Copyright 1984-1995 Wind River Systems, Inc. */

/*
modification history
--------------------
01o,14may95,p_m  added fioLibInit() prototype.
01n,20jul92,jmm  removed declaration of itob()
01m,13nov92,dnw  made include of stdarg.h conditional on __STDC__
01l,22sep92,rrr  added support for c++
01k,02aug92,jcf  added printExc().
01j,04jul92,jcf  cleaned up.
01i,26may92,rrr  the tree shuffle
01h,04dec91,rrr  some more ansi cleanup.
01g,04oct91,rrr  passed through the ansification filter
		  -changed includes to have absolute path from h/
		  -fixed #else and #endif
		  -changed copyright notice
01f,24mar91,del  added include of varargs.h.
01e,05oct90,shl  added ANSI function prototypes.
                 added copyright notice.
                 made #endif ANSI style.
01d,07aug90,shl  added INCfioLibh to #endif.
01c,10jun90,dnw  removed S_fioLib_UNEXPECTED_EOF (no longer returned by fioRead)
		 This file is now just a place holder!
01b,24dec86,gae  changed stsLib.h to vwModNum.h.
01a,07aug84,ecs  written
*/

#ifndef __INCfioLibh
#define __INCfioLibh

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__STDC__) || defined(__cplusplus)
#include "stdarg.h"
#endif /* __STDC__ */

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern void 	fioLibInit (void);
extern void 	fioFltInstall (FUNCPTR formatRtn, FUNCPTR scanRtn);
extern int 	fioFormatV (const char *fmt, va_list vaList,
			    FUNCPTR outRoutine, int outarg);
extern char *	nindex (char *buffer, int c, int n);
extern int 	fioScanV (const char *fmt, FUNCPTR getRtn, int getArg,
			  int *pUnget, va_list vaList);
extern int 	fioRead (int fd, char *buffer, int maxbytes);
extern int 	fioRdString (int fd, char string[], int maxbytes);
extern void 	printExc (char* fmt, int arg1, int arg2, int arg3,
			  int arg4, int arg5);

#else

extern void 	fioLibInit ();
extern void 	fioFltInstall ();
extern int 	fioFormatV ();
extern char *	nindex ();
extern int 	fioScanV ();
extern int 	fioRead ();
extern int 	fioRdString ();
extern void 	printExc ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCfioLibh */
