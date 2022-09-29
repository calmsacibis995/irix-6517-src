/* netShow.h - header file for netShow.c */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01h,12sep94,dzb  added tcpDebugShow() declaration (SPR #1552).
01g,22sep92,rrr  added support for c++
01f,27jul92,elh  Moved hostShow and routeShow here.
01e,04jul92,jcf  cleaned up.
01d,11jun92,elh  moved arpShow here from arpLib.
01c,26may92,rrr  the tree shuffle
01b,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed VOID to void
		  -changed copyright notice
01a,05oct90,shl created.
*/

#ifndef __INCnetShowh
#define __INCnetShowh

#ifdef __cplusplus
extern "C" {
#endif

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern void 	arptabShow (void);
extern void 	arpShow (void);
extern void 	icmpstatShow (void);
extern void 	ifShow (char *ifName);
extern void 	inetstatShow (void);
extern void 	ipstatShow (BOOL zero);
extern void 	mbufShow (void);
extern void 	netShowInit (void);
extern void 	tcpDebugShow (int numPrint, int verbose);
extern void 	tcpstatShow (void);
extern void 	udpstatShow (void);
extern void 	routeShow (void);
extern void 	hostShow (void);

#else	/* __STDC__ */

extern void 	arptabShow ();
extern void 	arpShow ();
extern void 	icmpstatShow ();
extern void 	ifShow ();
extern void 	inetstatShow ();
extern void 	ipstatShow ();
extern void 	mbufShow ();
extern void 	netShowInit ();
extern void 	tcpDebugShow ();
extern void 	tcpstatShow ();
extern void 	udpstatShow ();
extern void 	routeShow ();
extern void 	hostShow ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCnetShowh */
