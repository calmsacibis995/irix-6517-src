
#if _MIPS_SIM != _ABI64

#define R4000 1
#define _PAGESZ 4096
#define IP20 1

#include "ml.h"
#include <regdef.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/sbd.h>
#include <asm.h>

#define SCACHE_SIZE			0x100000
#define SCACHE_LINESIZE		(32*4)
#define SCACHE_LINEMASK		((32*4)-1)
#define MIN_CACH_POW2		12


LEAF(ip20promhack_cacheflush)
	.set    noreorder
	LI  	t0,K0_RAMBASE
	LI  	t3,SCACHE_SIZE
	addu    t3,t0   

	and 	t0,~SCACHE_LINEMASK
1:
	lw	zero,(t0)  			# read from line to flush

#define COMPANION_BIT 0x00400000
	li	t1,COMPANION_BIT   		# generate companion
	xor	t1,t0
	li	t2,K0_RAMBASE
	bgeu    t1,t2,2f
	nop
	or	t1,K0_RAMBASE
2:
	lw	zero,(t1)   			# cause writeback
	cache   CACH_SD|C_HINV,0(t1)		# invalidate cache line
	.set    reorder
	addu    t0,SCACHE_LINESIZE
	bltu    t0,t3,1b
	j	ra

	END(ip20promhack_cacheflush)

#endif

