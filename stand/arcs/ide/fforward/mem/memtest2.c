#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <fault.h>
#include "libsk.h"
#include "mem.h"

#include "libsc.h"

#ident "$Revision: 1.29 $"

extern khparity(__psunsigned_t, __psunsigned_t);

/*
 *  Driver module for the generic test.
 *
 *  The memsetup call parses the incoming arguments and assigns a default
 *  address if no range is specified Generic_l is the actual test code.
 */

int
memtest2_test(int argc, char *argv[])
{
	unsigned long count,count2;
	int error = 0;
	struct range range, range2;
	jmp_buf	faultbuf;
	int  memtestnum;
#ifndef IP28
	u_int cpuctrl0;
	u_int cpuctrl0_cpr;
#endif
#if !IP26 && !IP28
	__psunsigned_t end;
#endif

#ifndef IP28
#ifdef IP22
	int orion = ((get_prid()&0xFF00)>>8) == C0_IMP_R4600;
	int r4700 = ((get_prid()&0xFF00)>>8) == C0_IMP_R4700;
	int r5000 = ((get_prid()&0xFF00)>>8) == C0_IMP_R5000;
	cpuctrl0_cpr = (orion|r4700|r5000) ? 0 : CPUCTRL0_CPR;
#else
	cpuctrl0_cpr = CPUCTRL0_CPR;
#endif
#endif

	msg_printf(VRB, "--Addruniq=0,March=1,Mats=2,Kh=3,Altaccess=4,Khpar=5\n");
	msg_printf(VRB, "--Movi=6,Walkingbit=7,MarchX=8,MarchY=9,");

#if IP22
	msg_printf(VRB, "3bit-test=10\n");
	msg_printf(VRB, "Butterfly=11,Kh DWord=12,3bit-new=13\n");
#else
	msg_printf(VRB, "Butterfly=11\n");
#endif

	memtestnum = atoi(argv[1]);

#if IP26
	flush_cache();
#endif

	if (mem_setup(argc-1, argv[0], argc == 3 ? argv[2] : "", &range,
		      &range2) != 0) {
		return(-1);
	}
	count = range.ra_count*range.ra_size;
	count2 = range2.ra_count*range2.ra_size;

	if (setjmp(faultbuf)) {
		show_fault();
		return -1;
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

	/* Note the -1 due to Mips code; the boundary condition is different */
	msg_printf(DBG,"count+range.ra_base=%x\n", range.ra_base+count);
#ifndef _USE_MAPMEM
	if (range2.ra_base)
		msg_printf(DBG,"count2+range2.ra_base=%x\n",
			   range2.ra_base+count2);
#endif

	if (argc == 1) {
		msg_printf(VRB,"Address Uniqueness Test, Base: 0x%x, "
			"Size: 0x%x bytes\n", range.ra_base, count); 
		if (!memaddruniq(&range, BIT_TRUE, RUN_UNTIL_DONE, memerr))
			error = -1;
#ifndef _USE_MAPMEM
		if (range2.ra_base) {
			msg_printf(VRB,"Address Uniqueness Test, Base: 0x%x, "
				"Size: 0x%x bytes\n", range2.ra_base, count2); 
			if (!memaddruniq(&range2, BIT_TRUE, RUN_UNTIL_DONE,
					 memerr))
				error = -1;
		}
#endif
		msg_printf(VRB, "Walking Bit Test, Base: 0x%x, "
			"Size: 0x%x bytes\n", range.ra_base, count);
		if (!memwalkingbit(&range, BIT_TRUE, RUN_UNTIL_DONE, memerr))
			error = -1;
#ifndef _USE_MAPMEM
		if (range2.ra_base) {
			msg_printf(VRB, "Walking Bit Test, Base: 0x%x, "
				"Size: 0x%x bytes\n", range2.ra_base, count2);
			if (!memwalkingbit(&range2, BIT_TRUE, RUN_UNTIL_DONE,
					   memerr))
				error = -1;
		}
#endif
		goto done;
	}

#ifdef R4000
        /* disable ecc stuff */
	SetSR( (GetSR() | SR_IEC | 0x4000 ) & ~SR_DE); 
#endif

	switch (memtestnum) {
	case 0:
		msg_printf(VRB,"Address Uniqueness Test, Base: 0x%x, "
			"Size: 0x%x bytes\n", range.ra_base, count);
		if (!memaddruniq(&range, BIT_TRUE, RUN_UNTIL_DONE, memerr))
			error = -1;
#ifndef _USE_MAPMEM
		if (range2.ra_base) {
			msg_printf(VRB,"Address Uniqueness Test, Base: 0x%x, "
				"Size: 0x%x bytes\n", range2.ra_base, count2);
			if (!memaddruniq(&range2, BIT_TRUE, RUN_UNTIL_DONE,
					 memerr))
				error = -1;
		}
#endif
		break;
	case 1:
		msg_printf(VRB,"March Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (!March_l(range.ra_base,(range.ra_base + count - 0x1)))
			error = -1;
#ifndef _USE_MAPMEM
		if (range2.ra_base) {
			msg_printf(VRB,"March Test, Base: 0x%x, Size: 0x%x bytes\n",
				range2.ra_base, count2);
			if (!March_l(range2.ra_base,range2.ra_base+count2-0x1))
				error = -1;
		}
#endif
		break;
	case 2:
		msg_printf(VRB,"Mats Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (!Mats_l(range.ra_base,(range.ra_base + count - 0x1)))
			error = -1;
#ifndef _USE_MAPMEM
		if (range2.ra_base) {
			msg_printf(VRB,"Mats Test, Base: 0x%x, Size: 0x%x bytes\n",
				range2.ra_base, count2);
			if (!Mats_l(range2.ra_base,range2.ra_base+count2-0x1))
				error = -1;
		}
#endif
		break;
	case 3:
		msg_printf(VRB,"Kh Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (!Kh_l(range.ra_base,(range.ra_base + count - 0x1)))
			error = -1;
#ifndef _USE_MAPMEM
		if (range2.ra_base) {
			msg_printf(VRB,"Kh Test, Base: 0x%x, Size: 0x%x bytes\n",
				range2.ra_base, count2);
			if (!Kh_l(range2.ra_base,range2.ra_base+count2-0x1))
				error = -1;
		}
#endif
		break;
	case 4:
		msg_printf(VRB,"Altaccess Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (!Altaccess_l(range.ra_base,(range.ra_base + count - 0x1)))
			error = -1;
#ifndef _USE_MAPMEM
		if (range2.ra_base) {
			msg_printf(VRB,"Altaccess Test, Base: 0x%x, Size: 0x%x bytes\n",
				range2.ra_base, count2);
			if (!Altaccess_l(range2.ra_base,
					 range2.ra_base+count2-1))
				error = -1;
		}
#endif
		break;
	case 5:
#if !IP26 && !IP28	/* ECC memory */
		msg_printf(VRB,"Khpar Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (count < 0x8) {
			end = range.ra_base;
		}
		else {
			end = range.ra_base + count - 0x8 ;
		}
		if (khparity(range.ra_base, end))
			error = -1;
#ifdef R4000
		SetSR( (GetSR() | SR_IEC | 0x4000 ) & ~SR_DE);
#endif
#endif	/* ndef IP26/8 */
		break;
	case 6:
		msg_printf(VRB,"Movi Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (!Movi_l(range.ra_base, (range.ra_base + count - 0x1 )))
			error = -1;
#ifndef _USE_MAPMEM
		if (range2.ra_base) {
			msg_printf(VRB,"Movi Test, Base: 0x%x, Size: 0x%x bytes\n",
				range2.ra_base, count2);
			if (!Movi_l(range2.ra_base,range2.ra_base+count2-0x1))
				error = -1;
		}
#endif
		break;
	case 7:
		msg_printf(VRB,"Walking Bit Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (!memwalkingbit(&range, BIT_TRUE, RUN_UNTIL_DONE, memerr))
			error = -1;
#ifndef _USE_MAPMEM
		if (range2.ra_base) {
			msg_printf(VRB,"Walking Bit Test, Base: 0x%x, Size: "
				"0x%x bytes\n", range2.ra_base, count2);
			if (!memwalkingbit(&range2, BIT_TRUE, RUN_UNTIL_DONE,
					   memerr))
				error = -1;
		}
#endif
		break;
	case 8:
		msg_printf(VRB,"March X Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (marchX(range.ra_base,(range.ra_base + count - 0x1)))
			error = -1;
#ifndef _USE_MAPMEM
		if (range2.ra_base) {
			msg_printf(VRB,"March X Test, Base: 0x%x, Size: 0x%x bytes\n",
				range2.ra_base, count2);
			if (marchX(range2.ra_base,range2.ra_base+count2-0x1))
				error = -1;
		}
#endif
		break;
	case 9:
		msg_printf(VRB,"March Y Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (marchY(range.ra_base,(range.ra_base + count - 0x1)))
			error = -1;
#ifndef _USE_MAPMEM
		if (range2.ra_base) {
			msg_printf(VRB,"March Y Test, Base: 0x%x, Size: 0x%x bytes\n",
				range2.ra_base, count2);
			if (marchY(range2.ra_base,range2.ra_base+count2-0x1))
				error = -1;
		}
#endif
		break;
#ifdef IP22
	case 10:
		msg_printf(VRB,"Three Bit Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (three_l(range.ra_base,(range.ra_base + count - 0x1),0))
			error = -1;
#ifndef _USE_MAPMEM
		if (range2.ra_base) {
			msg_printf(VRB,"Three Bit Test, Base: 0x%x, Size: 0x%x bytes\n",
				range.ra_base2, count2);
			if (three_l(range2.ra_base,range2.ra_base+count2-0x1,0))
				error = -1;
		}
#endif
		break;
#endif
	case 11:
		msg_printf(VRB,"Butterfly Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (Butterfly_l(range.ra_base,(range.ra_base + count - 0x1)))
			error = -1;
#ifndef _USE_MAPMEM
		if (range2.ra_base) {
			msg_printf(VRB,"Butterfly Test, Base: 0x%x, Size: 0x%x bytes\n",
				range.ra_base, count);
			if (Butterfly_l(range2.ra_base,range2.ra_base+count2-1))
				error = -1;
		}
#endif
		break;
#ifdef IP22
	case 12:
		msg_printf(VRB,"Knaizuk Hartmann Doublword Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (khdouble_drv(range.ra_base,(range.ra_base + count - 0x10)))
			error = -1;
#ifndef _USE_MAPMEM
		if (range2.ra_base) {
			msg_printf(VRB,"Knaizuk Hartmann Doublword Test, Base: 0x%x, "
				"Size: 0x%x bytes\n", range2.ra_base, count2);
			if (khdouble_drv(range2.ra_base,
					 range2.ra_base + count2 - 0x10))
				error = -1;
		}
#endif
		break;
	case 13:
		msg_printf(VRB,"Threebit new, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (three_ll(range.ra_base,(range.ra_base + count - 0x10),0))
			error = -1;
#ifndef _USE_MAPMEM
		if (range2.ra_base) {
			msg_printf(VRB,"Threebit new, Base: 0x%x, Size: 0x%x bytes\n",
				range2.ra_base, count2);
			if (three_ll(range2.ra_base,
				     range2.ra_base + count2 - 0x10,0))
				error = -1;
		}
#endif
		break;
#endif
#ifdef MFG_USED
	case 14:
		msg_printf(VRB,"Threebit correct, Base: 0x%x Size: 0x%x bytes\n",
			range.ra_base, count);
		if (tb_test(range.ra_base,(range.ra_base + count - 0x10)))
			error = -1;
#ifndef _USE_MAPMEM
		if (range2.ra_base) {
			msg_printf(VRB,"Threebit correct, Base: 0x%x "
				"Size: 0x%x bytes\n", range2.ra_base, count2);
			if (tb_test(range2.ra_base,
				    range2.ra_base + count2 - 0x10))
				error = -1;
		}
#endif
		break;
	case 15:
		msg_printf(VRB,"Checkerboard correct, Base:: 0x%x, Size: 0x%x bytes\n",
                       range.ra_base, count);
		if (checkerbd(range.ra_base,(range.ra_base + count - 0x10)))
			error = -1;
#ifndef _USE_MAPMEM
		if (range2.ra_base) {
               		msg_printf(VRB,"Checkerboard correct, Base:: 0x%x, Size: "
				"0x%x bytes\n", range2.ra_base, count2);
			if (checkerbd(range2.ra_base,
				      range2.ra_base + count2 - 0x10))
				error = -1;
		}
#endif
		break;
#endif
	default:
		msg_printf(VRB,"Incorrect selection of tests, try again\n");
		error = -1;
		break;
	}

done:
	tlbpurge();

#ifndef IP28
	*(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0) = cpuctrl0;
#else
	IP28_MEMTEST_LEAVE();
#endif	/* !IP28 */

	nofault=0;

	if (error) {
		sum_error("CPU main memory");
		return -1;
	}

	okydoky();
	return 0;
}
