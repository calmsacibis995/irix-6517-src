/* etherLib.h - ethernet hook routines header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01j,22sep92,rrr  added support for c++
01i,04jul92,jcf  cleaned up.
01h,26may92,rrr  the tree shuffle
		  -changed includes to have absolute path from h/
01g,26feb92,elh  added prototypes for ether{Input,Output}HookDelete.
01f,04oct91,rrr  passed through the ansification filter
		  -changed includes to have absolute path from h/
		  -fixed #else and #endif
		  -changed copyright notice
01e,15jan91,shl  included "if.h" and "if_ether.h" so ifnet and ether_header
		 are defined before the prototypes.
01d,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
                 added copyright notice.
01c,07aug90,shl  added INCetherLibh to #endif.
01b,01nov87,dnw  added etherOutputHookRtn.
01a,28aug87,dnw  written
*/

#ifndef __INCetherLibh
#define __INCetherLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "net/if.h"
#include "netinet/if_ether.h"

IMPORT FUNCPTR etherInputHookRtn;
IMPORT FUNCPTR etherOutputHookRtn;

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	etherAddrResolve (struct ifnet *pIf, char *targetAddr,
				  char *eHdr, int numTries, int numTicks);
extern STATUS 	etherInputHookAdd (FUNCPTR inputHook);
extern STATUS 	etherOutput (struct ifnet *pIf, struct ether_header
		*pEtherHeader, char *pData, int dataLength);
extern STATUS 	etherOutputHookAdd (FUNCPTR outputHook);
extern void 	etherInputHookDelete (void);
extern void 	etherOutputHookDelete (void);


#else	/* __STDC__ */

extern STATUS 	etherAddrResolve ();
extern STATUS 	etherInputHookAdd ();
extern STATUS 	etherOutput ();
extern STATUS 	etherOutputHookAdd ();
extern void 	etherInputHookDelete ();
extern void 	etherOutputHookDelete ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCetherLibh */
