/* taskHookLib.h - header file for taskHookLib.c */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02b,22sep92,rrr  added support for c++
02a,04jul92,jcf  cleaned up.
01c,26may92,rrr  the tree shuffle
01b,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed VOID to void
		  -changed copyright notice
01a,05oct90,shl created.
*/

#ifndef __INCtaskHookLibh
#define __INCtaskHookLibh

#ifdef __cplusplus
extern "C" {
#endif

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern void	taskHookInit (void);
extern STATUS 	taskCreateHookAdd (FUNCPTR createHook);
extern STATUS 	taskCreateHookDelete (FUNCPTR createHook);
extern STATUS 	taskDeleteHookAdd (FUNCPTR deleteHook);
extern STATUS 	taskDeleteHookDelete (FUNCPTR deleteHook);
extern STATUS 	taskSwapHookAdd (FUNCPTR swapHook);
extern STATUS 	taskSwapHookAttach (FUNCPTR swapHook,int tid,BOOL in,BOOL out);
extern STATUS 	taskSwapHookDelete (FUNCPTR swapHook);
extern STATUS 	taskSwapHookDetach (FUNCPTR swapHook,int tid,BOOL in,BOOL out);
extern STATUS 	taskSwitchHookAdd (FUNCPTR switchHook);
extern STATUS 	taskSwitchHookDelete (FUNCPTR switchHook);
extern void	taskHookShowInit (void);
extern void 	taskCreateHookShow (void);
extern void 	taskDeleteHookShow (void);
extern void 	taskSwapHookShow (void);
extern void 	taskSwitchHookShow (void);

#else	/* __STDC__ */

extern void	taskHookInit ();
extern STATUS 	taskCreateHookAdd ();
extern STATUS 	taskCreateHookDelete ();
extern STATUS 	taskDeleteHookAdd ();
extern STATUS 	taskDeleteHookDelete ();
extern STATUS 	taskSwapHookAdd ();
extern STATUS 	taskSwapHookAttach ();
extern STATUS 	taskSwapHookDelete ();
extern STATUS 	taskSwapHookDetach ();
extern STATUS 	taskSwitchHookAdd ();
extern STATUS 	taskSwitchHookDelete ();
extern void	taskHookShowInit ();
extern void 	taskCreateHookShow ();
extern void 	taskDeleteHookShow ();
extern void 	taskSwapHookShow ();
extern void 	taskSwitchHookShow ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCtaskHookLibh */
