/* sun.h - miscellaneous SUN constants */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
------------------
02b,22sep92,rrr  added support for c++
02a,04jul92,jcf  cleaned up.
01d,26may92,rrr  the tree shuffle
01c,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
01b,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01a,28apr89,gae  created.
*/

#ifndef __INCsunh
#define __INCsunh

#ifdef __cplusplus
extern "C" {
#endif

#define NBPG		0x2000

#define SYSBASE         0x0F000000

#define MMU_PAGESIZE    0x2000          /* 8192 bytes */
#define MMU_PAGESHIFT   13              /* log2(MMU_PAGESIZE) */
#define MMU_PAGEOFFSET  (MMU_PAGESIZE-1)/* Mask of address bits in page */
#define MMU_PAGEMASK    (~MMU_PAGEOFFSET)

#define PAGESIZE        0x2000          /* All of the above, for logical */
#define PAGESHIFT       13
#define PAGEOFFSET      (PAGESIZE - 1)
#define PAGEMASK        (~PAGEOFFSET)

#define ptob(x)         ((x) << PAGESHIFT)
#define btop(x)         (((unsigned)(x)) >> PAGESHIFT)
#define btopr(x)        ((((unsigned)(x) + PAGEOFFSET) >> PAGESHIFT))

/* memory space types */

#define SUN_MEM         0x0                     /* Main Memory/ video memory */
#define SUN_IO          0x1                     /* SUN I/O space */
#define SUN_VME16       0x2                     /* VME 16 bit data space */
#define SUN_VME32       0x3                     /* VME 32 bit data space */

#ifdef __cplusplus
}
#endif

#endif /* __INCsunh */
