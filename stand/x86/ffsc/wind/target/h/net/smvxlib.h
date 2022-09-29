/* smVxLib.h	- VxWorks specific backplane driver header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01f,23jul93,jmm  changed #include "socket.h" to "sys/socket.h" (spr 2033)
01e,22sep92,rrr  added support for c++
01d,26may92,rrr  the tree shuffle
01c,02may92,elh  added mask to smVxInit
01b,10apr92,elh  added startAddr for sequential addressing.
01a,17nov90,elh  written.
*/

#ifndef __INCsmVxLibh
#define __INCsmVxLibh

#ifdef __cplusplus
extern "C" {
#endif

/* includes */

#include "vxworks.h"
#include "smlib.h"
#include "net/mbuf.h"
#include "net/if.h"
#include "netinet/if_ether.h"
#include "drv/netif/if_sm.h"
#include "errno.h"
#include "sys/ioctl.h"
#include "etherlib.h"
#include "net/unixlib.h"
#include "net/if_subr.h"
#include "inetlib.h"

/* defines */

#define	SM_LOAN			FALSE
#define	SM_NUM_LOAN		10
#define SM_MEM_SIZE		0x10000
#define SM_INPUT_PKTS 		200
#define	NSM	 		1

/*
 * vxWorks convention is to malloc the driver softc structure, whereas
 * UNIX wants this to be a static structure.  We provide macros to map
 * these differences.
 */

IMPORT SM_SOFTC *		sm_softc[];
#define UNIT_TO_SOFTC(unit)	(sm_softc [unit])
					/* is a struct in sunOS */
#define etherAddrPtr(eaddr)	(eaddr)

/* different parameters from sunOs */

#define copyFromMbufs(pData, pMbuf, len)			\
		copy_from_mbufs ((pData), (pMbuf), (len))

#define copyToMbufs(pData, len, off, pIf) 			\
		copy_to_mbufs ((pData), (len), (off), (pIf))

#define do_protocol(eh, pMbuf, acp, len) 			\
		do_protocol_with_type ((eh)->ether_type, (pMbuf), (acp), (len))


#define outputHook(pIf, pData, bytes)				\
	((etherOutputHookRtn != NULL) && 			\
	((*etherOutputHookRtn) ((pIf), (pData), (bytes)))) ?	\
	(TRUE) : (FALSE)

#define inputHook(pIf, pData, bytes)				\
	((etherInputHookRtn != NULL) &&				\
	((*etherInputHookRtn)((pIf), (pData), (bytes)))) ?	\
	(TRUE) : (FALSE)

#define deviceValid(unit)	((sm_softc [unit] != NULL) ? TRUE : FALSE)

/* Function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS smVxInit (SM_ANCHOR * pAnchor, char * pMem, int memSize,
			BOOL tasOK,
#ifndef USE_OFFSET
		 	int busToLocalOffset,
#endif
		 	int cpuMax, int maxPktBytes, u_long startAddr, u_long mask);

extern STATUS smVxAttach (int unit, SM_ANCHOR * pAnchor,
#ifndef USE_OFFSET
		          int busToLocalOffset,
#endif
			  int maxInputPkts, int intType, int intArg1, int intArg2,
			  int intArg3);

extern STATUS smVxInetGet (char * smName, struct in_addr * pSmInet, int cpuNum);
struct	mbuf * smLoanBuild (SM_SOFTC * xs, SM_PKT * pPkt, u_char * pData,
			    int len);
#else

extern STATUS smVxInit ();
extern STATUS smVxAttach ();
extern STATUS smVxInetGet ();
struct mbuf * smLoanBuild ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCsmVxLibh */
