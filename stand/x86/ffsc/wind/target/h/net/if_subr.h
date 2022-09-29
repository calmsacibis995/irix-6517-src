/* if_subr.h - header for common routines for network interface drivers */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01l,06sep93,jcf  added prototype for netTypeInit.
01k,22sep92,rrr  added support for c++
01j,23jun92,ajm  added prototype for do_protocol_with_type
01i,06jun92,elh  added includes.
01h,04jun92,ajm  Fixed erroneous ansi prototype.
01g,26may92,rrr  the tree shuffle
01f,01apr92,elh  Added ansi prototypes.  Added functions.
01e,04oct91,rrr  passed through the ansification filter
		  -changed VOID to void
		  -changed copyright notice
01d,19jul91,hdn  moved SIZEOF_ETHERHEADER to if_ether.h.
01c,29apr91,hdn  added defines and macros for TRON architecture.
01b,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01a,10aug90,dnw  written
*/

#ifndef __INCif_subrh
#define __INCif_subrh

#ifdef __cplusplus
extern "C" {
#endif

/* includes */

#include "net/if.h"
#include "netinet/if_ether.h"
#include "net/mbuf.h"

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern void check_trailer (struct ether_header *eh, unsigned char *pData,
    		    	   int *pLen, int *pOff, struct ifnet *ifp);

extern void do_protocol (struct ether_header *eh, struct mbuf *m,
			 struct arpcom *pArpcom, int len);

extern void do_protocol_with_type (u_short type, struct mbuf *m,
                                   struct arpcom *pArpcom, int len);

extern int ether_output (struct ifnet *ifp, struct mbuf *m0,
			 struct sockaddr *dst, FUNCPTR startRtn,
			 struct arpcom *pArpcom);

extern int set_if_addr (struct ifnet *ifp, char *data, u_char *enaddr);

extern void ether_attach (struct ifnet *ifp, int unit, char *name,
		          FUNCPTR initRtn, FUNCPTR ioctlRtn, FUNCPTR outputRtn,
			  FUNCPTR resetRtn);

extern STATUS netTypeAdd (int etherType, FUNCPTR inputRtn);
extern STATUS netTypeDelete (int etherType);
extern void netTypeInit ();

#else

extern void check_trailer ();
extern void do_protocol ();
extern void do_protocol_with_type ();
extern int ether_output ();
extern int set_if_addr ();
extern void ether_attach ();
extern STATUS netTypeAdd ();
extern STATUS netTypeDelete ();
extern void netTypeInit ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCif_subrh */
