#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/regdef.h>
#include <asm.h>
#include <sys/IP26.h>
#include <sys/IP22.h>


#define SCACHE_TESTADDR 0xa800000008840000


/************************************************************************/
/* 	Initialize dcache by invalidating it.  				*/
/*	dcache is 16KB, with 32Byte line,				*/
/* 	28 bits Tag + 1 bit Exclusive + 8 bits Valid + 32 Bytes Data	*/
/* 	Need two consecutive writes to half-line boundaries 		*/
/*	to clear Valid bits for each line				*/
/*	Hopefuly, this module leaves the Gcache intact			*/
/************************************************************************/
LEAF(dcache_only_inval)
        .set    noreorder
        move    t3, ra
        li      v1, 16                  # size of the half of Dcache line
        jal     get_dcachesize
        nop

        # v1: half line size, v0: cache size to be initialized

        dli     t1,SCACHE_TESTADDR	# Starting address for initializing
                                        #       Dcache
        daddu   t2, t1, v0              # t2: loop terminator

        # for each cache line:
        #       mark it invalid
1:
        DMTC0(t1,C0_BADVADDR)
        DMTC0(zero,C0_DCACHE)           # clear all (4) Valid bits
        dctw                            # write to the Dcache Tag
        ssnop; ssnop; ssnop             # pipeline hazard prevention
        .set    reorder

        daddu   t1, v1			# increment to next half-line
        bltu    t1, t2, 1b              # t2 is termination count
        j       t3
END(dcache_only_inval)


