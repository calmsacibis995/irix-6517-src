/* l64853.h - L64853 SBus DMA Controller header */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01g,22sep92,rrr  added support for c++
01f,06aug92,ccc	 added function prototypes.
01e,26may92,rrr  the tree shuffle
01d,26may92,ajm  got rid of HOST_DEC def's (new compiler)
		  updated copyright
01c,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed ASMLANGUAGE to _ASMLANGUAGE
		  -changed copyright notice
01b,28sep91,ajm  ifdef'd HOST_DEC for compiler problem
01a,27jun91,jcc  written.
*/

#ifndef __INCl64853h
#define __INCl64853h

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _ASMLANGUAGE

#include "vxworks.h"
#include "dma.h"

typedef struct l64853dmaCtrl	/* L64853_DMA_CTRL - L64853 DMA ctrl info */
    {
    DMA_CTRL 		dmaCtrl;	/* generic DMA ctrl struct */
    volatile UINT32 *	pCsrReg;	/* ptr to Control / Status register */
    volatile UINT32 *	pAdrsReg;	/* ptr to Address Counter register */
    volatile UINT32 *	pBcntReg;	/* ptr to Byte Counter register */
    volatile UINT32 *	pIdReg;		/* ptr to ID register */
    } L64853_DMA_CTRL;

typedef L64853_DMA_CTRL L64853;

/* Control/Status Register Bit Fields */

#define L64853_CSR_INT_PEND	0x00000001	/* interrupt pending bit */
#define L64853_CSR_ERR_PEND	0x00000002	/* error pending bit */
#define L64853_CSR_PACK_CNT	0x0000000c	/* pack count bits */
#define L64853_CSR_INT_EN	0x00000010	/* interrupt enable bit */
#define L64853_CSR_FLUSH	0x00000020	/* flush buffer bit */
#define L64853_CSR_DRAIN	0x00000040	/* drain buffer bit */
#define L64853_CSR_RESET	0x00000080	/* reset DMA bit */
#define L64853_CSR_WRITE	0x00000100	/* read/write bit */
#define L64853_CSR_EN_DMA	0x00000200	/* enable DMA bit */
#define L64853_CSR_REQ_PEND	0x00000400	/* request pending bit */
#define L64853_CSR_BYTE_ADDR	0x00001800	/* byte address bits */
#define L64853_CSR_EN_CNT	0x00002000	/* enable counter bit */
#define L64853_CSR_TC		0x00004000	/* terminal count bit */
#define L64853_CSR_ILACC	0x00008000	/* ILACC 79C900 bit */
#define L64853_CSR_DEV_ID	0xf0000000	/* device ID bits */

#endif	/* _ASMLANGUAGE */

#ifndef	_ASMLANGUAGE
#if defined(__STDC__) || defined(__cplusplus)

IMPORT	void	l64853AdrsCountSet (L64853 *pL64853, char *pAdrsCount);
IMPORT	void	l64853ByteCountSet (L64853 *pL64853, int byteCount);
IMPORT	void	l64853Write (L64853 *pL64853);
IMPORT	void	l64853CountEnable (L64853 *pL64853);
IMPORT	void	l64853DmaEnable (L64853 *pL64853);
IMPORT	void	l64853CountDisable (L64853 *pL64853);
IMPORT	void	l64853DmaDisable (L64853 *pL64853);
IMPORT	void	l64853Read (L64853 *pL64853);
IMPORT	void	l64853IntEnable (L64853 *pL64853);
IMPORT	STATUS	l64853Show (DMA_CTRL *pDmaCtrl);
IMPORT	BOOL	l64853ErrPendTest (L64853 *pL64853);
IMPORT	void	l64853Flush (L64853 *pL64853);
IMPORT	BOOL	l64853TermCountTest (L64853 *pL64853);
IMPORT	void	l64853Drain (L64853 *pL64853);
IMPORT	DMA_CTRL *l64853CtrlCreate (UINT32 *baseAdrs, int regOffset,
				    UINT32 *pIdReg);

#else	/* __STDC__ */

IMPORT  void    l64853AdrsCountSet ();
IMPORT  void    l64853ByteCountSet ();
IMPORT  void    l64853Write ();
IMPORT  void    l64853CountEnable ();
IMPORT  void    l64853DmaEnable ();
IMPORT  void    l64853CountDisable ();
IMPORT  void    l64853DmaDisable ();
IMPORT  void    l64853Read ();
IMPORT  void    l64853IntEnable ();
IMPORT  STATUS  l64853Show ();
IMPORT  BOOL    l64853ErrPendTest ();
IMPORT  void    l64853Flush ();
IMPORT  BOOL    l64853TermCountTest ();
IMPORT  void    l64853Drain ();
IMPORT  DMA_CTRL *l64853CtrlCreate ();

#endif	/* __STDC__ */

#endif	/* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCl64853h */
