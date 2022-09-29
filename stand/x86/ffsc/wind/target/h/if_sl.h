/* if_sl.h - Serial Line IP header  */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01h,22mar94,dzb  added CSLIP support.
01g,22sep92,rrr  added support for c++
01f,15sep92,jcf  added slip interface prototypes.
01e,04jul92,jcf  cleaned up.
01d,26may92,rrr  the tree shuffle
01c,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
01b,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01a,18jul89,hjb  written.
*/

#ifndef __INCif_slh
#define __INCif_slh

#ifdef __cplusplus
extern "C" {
#endif

#define S_if_sl_INVALID_UNIT_NUMBER			(M_if_sl | 1)
#define S_if_sl_UNIT_UNINITIALIZED			(M_if_sl | 2)
#define S_if_sl_UNIT_ALREADY_INITIALIZED		(M_if_sl | 3)

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS	slipInit (int unit, char *devName, char *myAddr,
		    char *peerAddr, int baud, BOOL compressEnable,
		    BOOL compressAllow);
extern STATUS	slipBaudSet (int unit, int baud);

#else	/* __STDC__ */

extern STATUS	slipInit ();
extern STATUS	slipBaudSet ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCif_slh */
