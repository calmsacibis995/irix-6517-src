#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <fault.h>
#include <libsc.h>
#include <libsk.h>
#include "mem.h"

#ident "$Revsion$"

#ifdef IP26
#define _scache_linesize	TCC_LINESIZE
#else
extern int _scache_linesize;
#endif

#define INT_EN 0x2000

/***************************************************************************/
/*  Driver module for the blk test.                                         */
/*  The memsetup call parses the incoming arguments and assigns a default  */
/*  address if no range is specified                                       */
/***************************************************************************/

int
blk_test(int argc, char *argv[])
{
	int		error = 0;
	struct range	range,range2;
	jmp_buf         faultbuf;
#if IP22
	int             orion = ((get_prid()&0xFF00)>>8) == C0_IMP_R4600;
	int             r4700 = ((get_prid()&0xFF00)>>8) == C0_IMP_R4700;
	int             r5000 = ((get_prid()&0xFF00)>>8) == C0_IMP_R5000;
#endif

	msg_printf(DBG, "memsize is %x\tMAX_LOW_MEM is %x\n",memsize,MAX_LOW_MEM);
	msg_printf(DBG,"%d\n", _scache_linesize);

#if IP26
	flush_cache();
#endif

	if  (mem_setup(argc,argv[0],argv[1],&range,&range2) != 0) {
		error=-1;
	}
	else {
#ifndef IP28
		u_int cpuctrl0;
		u_int cpuctrl0_cpr;
#if IP22
		cpuctrl0_cpr = (orion|r5000|r4700) ? 0 : CPUCTRL0_CPR;
#else
		cpuctrl0_cpr = CPUCTRL0_CPR;
#endif
#endif

		msg_printf(VRB, "BLK Test, Base: 0x%x, Size: 0x%x bytes\n",
			   range.ra_base, range.ra_size*range.ra_count);
#ifndef _USE_MAPMEM
		if (range2.ra_base)
			msg_printf(VRB,
				"BLK Test, Base: 0x%x, Size: 0x%x bytes\n",
				range2.ra_base,range2.ra_size*range2.ra_count);
#endif

		if (setjmp(faultbuf)) {
			/*                busy(0);  */
			show_fault();
			return FALSE;
		}
		nofault = faultbuf;

#ifndef IP28
		cpuctrl0 = *(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0);
		*(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0) = (cpuctrl0 |
		    CPUCTRL0_GPR | CPUCTRL0_MPR | cpuctrl0_cpr) &
		    ~CPUCTRL0_R4K_CHK_PAR_N;
#else
		IP28_MEMTEST_ENTER();
#endif	/* !IP28 */

		blkwrt((u_int *)range.ra_base,
		       (u_int *)range.ra_base+range.ra_count,
		       _scache_linesize,0xaaaaaaaa);

		busy(1);

#ifndef _USE_MAPMEM
		if (range2.ra_base) {
			blkwrt((u_int *)range2.ra_base,
				(u_int *)range2.ra_base+range2.ra_count,
				_scache_linesize,0xaaaaaaaa);

			busy(1);
		}
#endif

		blkrd((u_int *)range.ra_base,
		      (u_int *)range.ra_base+range.ra_count,
		      _scache_linesize);

		busy(1);

#ifndef _USE_MAPMEM
		if (range2.ra_base) {
			blkrd((u_int *)range2.ra_base,
				(u_int *)range2.ra_base+range2.ra_count,
				_scache_linesize);

			busy(1);
		}
#endif

		blkwrt((u_int *)range.ra_base,
                       (u_int *)range.ra_base+range.ra_count,
		       _scache_linesize,0x55555555);

		busy(1);

#ifndef _USE_MAPMEM
		if (range2.ra_base) {
			blkwrt((u_int *)range2.ra_base,
				(u_int *)range2.ra_base+range2.ra_count,
				_scache_linesize,0x55555555);

			busy(1);
		}
#endif

		blkrd((u_int *)range.ra_base,
		      (u_int *)range.ra_base+range.ra_count,
		      _scache_linesize);

		busy(1);

#ifndef _USE_MAPMEM
		if (range2.ra_base) {
			blkrd((u_int *)range2.ra_base,
				(u_int *)range2.ra_base+range2.ra_count,
				_scache_linesize);
		}
#endif

		busy(0);

		tlbpurge();

#ifndef IP28
		*(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0) = cpuctrl0;
#else
		IP28_MEMTEST_LEAVE();
#endif	/* !IP28 */
	}
	/* End of if-then for the memsetup condition */

	if (error) {
		sum_error("CPU local memory");
		nofault=0;
		return -1;
	}
	else {
		okydoky();
		nofault=0;
		return 0;
	}
}
