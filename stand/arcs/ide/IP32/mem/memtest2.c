#include "mem.h"
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/types.h>
#include <fault.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ident "$Revision: 1.3 $"

#define MFG_USED 1

/*
 *  Driver module for the generic test.
 *
 *  The memsetup call parses the incoming arguments and assigns a default
 *  address if no range is specified Generic_l is the actual test code.
 */

int
memtest2_test(int argc, char *argv[])
{
	int cflag = 0;
	u_int count;
	u_int cpuctrl0;
	int error = 0;
	u_int last_phys_addr;
	struct range range;
	u_int loop_count = 1;
	jmp_buf	faultbuf;
	char *memtestname;
	char *memrangestring;
	int  memtestnum;
	int  volatile back_pat = 0x0;
	int  argcount;
	int orion = ((get_prid()&0xFF00)>>8) == C0_IMP_R4600;
	int r4700 = ((get_prid()&0xFF00)>>8) == C0_IMP_R4700;
	int r5000 = ((get_prid()&0xFF00)>>8) == C0_IMP_R5000;
	u_int                   cpuctrl0_cpr;
	extern u_int    memsize;

	if (memsize > MAX_LOW_MEM)
		return(0);

	run_cached();

	msg_printf(VRB, "--Addruniq=0,March=1,Mats=2,Kh=3,Altaccess=4,Khpar=5,Movi=6,Walkingbit=7\n");
	msg_printf(VRB, "--MarchX=8,MarchY=9,Threebit=10,Butterfly=11,Kh Dblwrd=12\n\n");
	memtestname = argv[1];
	memtestnum = atoi(argv[1]);

	if (memtestnum > 15){ 
		/* In test 16, 17, and 18 the background data pattern is argv[2] */ 
		back_pat = atoi(argv[2]);
                /* If 4 args, then range string was specified */
		if (argc == 4) { memrangestring = argv[3]; 
				 argcount = 3;
			} else {
				 argcount = 2;
			}	

		if (menumem_setup(argcount, memtestname, memrangestring, &range, &count,
			&last_phys_addr,&cflag) ) {
			return(-1);
			}
	} else { 
		memrangestring = argv[2]; /* else argv[2] is the range string */
		if (menumem_setup(argc, memtestname, memrangestring, &range, &count,
			&last_phys_addr,&cflag) ) {
			return(-1);
			}
	}

	if (setjmp(faultbuf)) {
		show_fault();
		return -1;
	}

	nofault = faultbuf;


	/* Note the -1 due to Mips code; the boundary condition is different */
	msg_printf(DBG,"count+range.ra_base=%x\tlast_phys_addr=%x\n",
			range.ra_base+count, last_phys_addr);

	if (argc == 1) {
		printf("Address Uniqueness Test, Base: 0x%x, "
			"Size: 0x%x bytes\n", range.ra_base, count); 
		if (!memaddruniq(&range, BIT_TRUE, RUN_UNTIL_DONE, memerr))
			error = -1;
		printf("Walking Bit Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count); 
		if (!memwalkingbit(&range, BIT_TRUE, RUN_UNTIL_DONE, memerr))
			error = -1;
		goto done;
	}
        /* disable ecc stuff)*/
	SetSR( GetSR() | SR_DE); 
	/* SetSR( (GetSR() | SR_IEC | 0x4000 ) & ~SR_DE); */


	switch (memtestnum) {
	case 0:
		printf("Address Uniqueness Test, Base: 0x%x, "
			"Size: 0x%x bytes\n", range.ra_base, count);
		if (!memaddruniq(&range, BIT_TRUE, RUN_UNTIL_DONE, memerr))
			error = -1;
		break;
	case 1:
		printf("March Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (!March_l(range.ra_base,(range.ra_base + count - 0x1)))
			error = -1;
		break;
	case 2:
		printf("Mats Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (!Mats_l(range.ra_base,(range.ra_base + count - 0x1)))
			error = -1;
		break;
	case 3:
		printf("Kh Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (!Kh_l(range.ra_base,(range.ra_base + count - 0x1)))
			error = -1;
		break;
	case 4:
		printf("Altaccess Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (!Altaccess_l(range.ra_base,(range.ra_base + count - 0x1)))
			error = -1;
		break;
	case 5:
		printf("Khpar Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (count < 0x8) {
			last_phys_addr = range.ra_base;
		}
		else {
			last_phys_addr = range.ra_base + count - 0x8 ;
		SetSR( (GetSR() | SR_IEC | 0x4000 ) & ~SR_DE);
		}
		if (khparity(range.ra_base, last_phys_addr))
			error = -1;
		break;
	case 6:
		printf("Movi Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (!Movi_l(range.ra_base, (range.ra_base + count - 0x1 )))
			error = -1;
		break;
	case 7:
		printf("Walking Bit Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (!memwalkingbit(&range, BIT_TRUE, RUN_UNTIL_DONE, memerr))
			error = -1;
		break;
	case 8:
		printf("March X Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (marchX(range.ra_base,(range.ra_base + count - 0x1)))
			error = -1;
		break;
	case 9:
		printf("March Y Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (marchY(range.ra_base,(range.ra_base + count - 0x1)))
			error = -1;
		break;
	case 10:
		printf("Three Bit Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (three_l(range.ra_base,(range.ra_base + count - 0x1)))
			error = -1;
		break;
	case 11:
		printf("Butterfly Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (Butterfly_l(range.ra_base,(range.ra_base + count - 0x1)))
			error = -1;
		break;
	case 12:
		printf("Knaizuk Hartmann Doublword Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (khdouble_drv(range.ra_base,(range.ra_base + count - 0x10)))
			error = -1;
		break;
	case 13:
		printf("Threebit new, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (three_ll(range.ra_base,(range.ra_base + count - 0x10)))
			error = -1;
		break;

/* #ifdef MFG_USED */
/*
	case 14:
		printf("Threebit correct, Base:: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (tb_test(range.ra_base,(range.ra_base + count - 0x10)))
			error = -1;
		break;
	case 15:
               printf("Checkerboard correct, Base:: 0x%x, Size: 0x%x bytes\n",
                       range.ra_base, count);
               if (checkerbd(range.ra_base,(range.ra_base + count - 0x10)))
			error = -1;
*/
	case 16:
		printf("Walking One Pattern Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (!memwalkingpat(&range, 0, RUN_UNTIL_DONE, memerr, back_pat))
			error = -1;
		break;
	case 17:
		printf("Walking Zero Pattern Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (!memwalkingpat(&range, 1, RUN_UNTIL_DONE, memerr, back_pat))
			error = -1;
		break;
	case 18:
		printf("Random Pattern Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (!randompat(&range, 1, RUN_UNTIL_DONE, memerr, back_pat))
			error = -1;
		break;
/* #endif */
			error = -1;
		break;
	default:
		printf("Incorrect selection of tests, try again\n");
		error = -1;
		break;
	}

done:
	tlbpurge();
	nofault=0;

	if (error) {
	sum_error("CPU local memory");
	return -1;
	}

	okydoky();
	return 0;
}
