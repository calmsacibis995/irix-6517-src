/* proxyLib.h - include file for VxWorks Proxy Arp Client library */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01e,22sep92,rrr  added support for c++
01d,04jul92,jcf  cleaned up.
01c,23jun92,elh	 renamed to proxyLib, changed parameters.
01b,26may92,rrr  the tree shuffle
01a,20sep91,elh	 written.
*/

#ifndef __INCproxyLibh
#define __INCproxyLibh

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	proxyReg (char *ifName, char *proxyAddr);
extern STATUS 	proxyUnreg (char *ifName, char *proxyAddr);

#else   /* __STDC__ */

extern STATUS 	proxyReg ();
extern STATUS 	proxyUnreg ();

#endif  /* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCproxyLibh */
