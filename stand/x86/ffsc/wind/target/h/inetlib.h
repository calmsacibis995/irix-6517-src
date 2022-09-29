/* inetLib.h - header for Internet address manipulation routines */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01k,09aug93,elh  removed definitions of inet_addr, inet_ntoa, 
		 inet_makeaddr, and inet_network (SPR 2268). 
01j,22sep92,rrr  added support for c++
01i,04jul92,jcf  cleaned up.
01h,26may92,rrr  the tree shuffle
		  -changed includes to have absolute path from h/
01g,04oct91,rrr  passed through the ansification filter
		  -changed includes to have absolute path from h/
		  -changed VOID to void
		  -changed copyright notice
01f,19oct90,shl  added #include "in.h".
01e,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
                 added copyright notice.
01d,10aug90,dnw  added declaration of inet_ntoa_b().
01c,24mar88,ecs  added include of types.h.
01b,15dec87,gae  added INET_ADDR_LEN; used IMPORTs on function decl's.
01a,01nov87,llk  written
*/

#ifndef __INCinetLibh
#define __INCinetLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vwmodnum.h"
#include "sys/types.h"
#include "netinet/in.h"
#include "arpa/inet.h"

/* status codes */

#define S_inetLib_ILLEGAL_INTERNET_ADDRESS		(M_inetLib | 1)
#define S_inetLib_ILLEGAL_NETWORK_NUMBER		(M_inetLib | 2)

/* length of ASCII represention of inet addresses, eg. "90.0.0.0" */

#define	INET_ADDR_LEN	18

/* function declarations */


#if defined(__STDC__) || defined(__cplusplus)

extern int 	inet_lnaof (int inetAddress);
extern int 	inet_netof (struct in_addr inetAddress);
extern void 	inet_makeaddr_b (int netAddr, int hostAddr,
				 struct in_addr *pInetAddr);
extern void 	inet_netof_string (char *inetString, char *netString);
extern void 	inet_ntoa_b (struct in_addr inetAddress, char *pString);

#else	/* __STDC__ */

extern int 	inet_lnaof ();
extern int 	inet_netof ();
extern void 	inet_makeaddr_b ();
extern void 	inet_netof_string ();
extern void 	inet_ntoa_b ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCinetLibh */
