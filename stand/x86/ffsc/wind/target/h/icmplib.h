/* icmpLib.h -- VxWorks ICMP Library header file */

/* Copyright 1990-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01g,22sep92,rrr  added support for c++
01f,04jul92,jcf  cleaned up.
01e,30jun92,jmm  moved checksum() declarations to vxLib.h
01d,11jun92,elh  changed parameters to ipHeaderCreate.
01c,26may92,rrr  the tree shuffle
01b,17apr92,elh  moved shared routine prototypes here (from bootpLib).
01a,11jun91,elh  created.
*/

#ifndef __INCicmpLibh
#define __INCicmpLibh

#ifdef __cplusplus
extern "C" {
#endif

/* includes */

#include "vwmodnum.h"
#include "net/if.h"

/* errors */

#define S_icmpLib_TIMEOUT		(M_icmpLib | 1)
#define S_icmpLib_NO_BROADCAST		(M_icmpLib | 2)
#define S_icmpLib_INVALID_INTERFACE	(M_icmpLib | 3)

/* function prototypes */

#if defined(__STDC__) || defined(__cplusplus)

STATUS    icmpMaskGet  (char *ifName, char *src, char *dst, int *pSubnet);
void 	  ipHeaderCreate (int proto, struct in_addr *pSrcAddr,
		          struct in_addr *pDstAddr, struct ip *pih,
			  int length);
u_char * ipHeaderVerify (struct ip *pih, int length, int proto);
STATUS   etherSend (struct ifnet *pIf, struct ip *pDatagram, int length);


#else	/* __STDC__ */

STATUS 		icmpMaskGet  ();
void 		ipHeaderCreate ();
u_char *	ipHeaderVerify ();
STATUS		etherSend ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCicmpLibh */
