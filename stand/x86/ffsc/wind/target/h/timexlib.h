/* timexLib.h - header for execution timer facilities */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02b,22sep92,rrr  added support for c++
02a,04jul92,jcf  cleaned up.
01e,26may92,rrr  the tree shuffle
01d,04oct91,rrr  passed through the ansification filter
		  -changed VOID to void
		  -changed copyright notice
01c,19oct90,shl  changed timexFunc(), etc, to use variable length argument list.
01b,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
                 added copyright notice.
01a,10aug90,dnw  written
*/

#ifndef __INCtimexLibh
#define __INCtimexLibh

#ifdef __cplusplus
extern "C" {
#endif

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern void 	timex (FUNCPTR func, int arg1, int arg2, int arg3, int arg4,
		       int arg5, int arg6, int arg7, int arg8);
extern void 	timexClear (void);
extern void 	timexFunc (int i, FUNCPTR func, int arg1, int arg2, int arg3,
			   int arg4, int arg5, int arg6, int arg7, int arg8);
extern void 	timexHelp (void);
extern void 	timexInit (void);
extern void 	timexN (FUNCPTR func, int arg1, int arg2, int arg3, int arg4,
			int arg5, int arg6, int arg7, int arg8);
extern void 	timexPost (int i, FUNCPTR func, int arg1, int arg2, int arg3,
			   int arg4, int arg5, int arg6, int arg7, int arg8);
extern void 	timexPre (int i, FUNCPTR func, int arg1, int arg2, int arg3,
			  int arg4, int arg5, int arg6, int arg7, int arg8);
extern void 	timexShow (void);

#else	/* __STDC__ */

extern void 	timex ();
extern void 	timexClear ();
extern void 	timexFunc ();
extern void 	timexHelp ();
extern void 	timexInit ();
extern void 	timexN ();
extern void 	timexPost ();
extern void 	timexPre ();
extern void 	timexShow ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCtimexLibh */
