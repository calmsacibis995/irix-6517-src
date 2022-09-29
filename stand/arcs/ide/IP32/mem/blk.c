#include "mem.h"
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/types.h>
#include <fault.h>

#ident "$Revsion$"


/***************************************************************************/
/*  Driver module for the blk test.                                         */
/*  The memsetup call parses the incoming arguments and assigns a default  */
/*  address if no range is specified                                       */
/***************************************************************************/

blk_test(int argc, char *argv[])
{
	int		cflag = 0;
	u_int		count;
	u_int		cpuctrl0;
	int		error = 0;
	u_int		last_phys_addr;
	int             orion = ((get_prid()&0xFF00)>>8) == C0_IMP_R4600;
	int             r4700 = ((get_prid()&0xFF00)>>8) == C0_IMP_R4700;
	int             r5000 = ((get_prid()&0xFF00)>>8) == C0_IMP_R5000;
	extern u_int	memsize;
	extern int _scache_linesize;
	struct range	range;
	void 		map_mem(int);
	u_int		loop_count = 1;
	jmp_buf         faultbuf;
	char *memtestname;
	char *memrangestring;
	u_int  pattern = 0xaaaaaaaa;
	msg_printf(DBG, "memsize is %x\tMAX_LOW_MEM is %x\n",memsize,MAX_LOW_MEM);
	msg_printf(DBG,"%d\n", _scache_linesize);
	memtestname = argv[0];
	memrangestring = argv[1];

	if  (mem_setup(argc,memtestname,memrangestring, &range,&count,&last_phys_addr,&cflag) ) {
		error=-1;
	}
	else {
#ifndef IP32 
		u_int cpuctrl0_cpr = (orion | r5000 | r4700) ? 0 : CPUCTRL0_CPR;
#endif /* !IP32 */

		msg_printf(VRB, "BLK Test, Base: 0x%x, Size: 0x%x bytes\n",range.ra_base, count);

		if (setjmp(faultbuf)) {
			/*                busy(0);  */
			show_fault();
			return FALSE;
		}
		nofault = faultbuf;

#ifndef IP32 
		cpuctrl0 = *(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0);
		*(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0) = (cpuctrl0 |
		    CPUCTRL0_GPR | CPUCTRL0_MPR | cpuctrl0_cpr) &
		    ~CPUCTRL0_R4K_CHK_PAR_N;
#endif /* !IP32 */
		blkwrt(range.ra_base,range.ra_base+range.ra_count,_scache_linesize,pattern);
		busy(0);
		blkrd(range.ra_base,range.ra_base+range.ra_count,_scache_linesize);
		busy(1);
		blkwrt(range.ra_base,range.ra_base+range.ra_count,_scache_linesize,~pattern);
		busy(0);
		blkrd(range.ra_base,range.ra_base+range.ra_count,_scache_linesize);
		busy(1);
		tlbpurge();
#ifndef IP32 
		*(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0) = cpuctrl0;
#endif /* !IP32 */
	}  
	/* End of if-then for the memsetup condition */

	if (error) {
		sum_error("CPU local memory");
		nofault=0;
		return -1;
	} else {
		okydoky();
		nofault=0;
		return 0;
	}
}























