/* Test non cached tlb mapping
 */

#ident "$Revision: 1.2 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <setjmp.h>
#include <fault.h>
#include <libsk.h>
#include "tlb.h"
#include "fforward/cache/cache.h"

static __psunsigned_t va[sizeof(int)] = {
	K2BASE,
	K2BASE|0x20000000,
	KUBASE,
	KUBASE|0x20000000
};

int
tlbnocache(__psunsigned_t *pt, u_int *uncachedp, u_int *cachedp)
{
	u_int	cachepat = 0x87654321;
	int	error = 0;
	jmp_buf	faultbuf;
	int	i,set;
	u_int	mempat = 0x12345678;
	u_int	tlb_attrib = TLBLO_V | TLBLO_D;
	u_int	tlbpat;
	int	section1,section2,section3,section4;
	msg_printf(VRB, "TLB cached/uncached test\n");


	for (set = 0; set < NTLBSETS; set++) {

	msg_printf(DBG,"Set: %d\n",set);
	section1 = 0;
	section2 = 0;
	section3 = 0;
	section4 = 0;


	for (i = 0; i < NTLBENTRIES; i++) {
		__psunsigned_t addr = va[i & (sizeof(int) - 1)];
		msg_printf(DBG,"addr = %016x\n",addr);

		tfptlbfill(set, 0, addr, (pt[0] & TLBLO_PFNMASK) | TLBLO_NONCOHRNT | tlb_attrib);

		if (setjmp(faultbuf)) {
			section2++;
			show_fault();
			DoEret();
			error++;
			goto Done;
		}

		nofault = faultbuf;
		section1++;
		*(volatile u_int *)cachedp = cachepat;
		*(volatile u_int *)uncachedp = mempat;
		tlbpat = *(u_int *)addr;
		nofault = 0;
		if (tlbpat != cachepat) {
			msg_printf(ERR,
				"Cached TLB, Expected: 0x%08x, Actual: 0x%08x\n",
				cachepat, tlbpat);
			error++;
		}

/*		invaltlb(i);*/
		tlbpurge();
	}

	for (i = 0; i < NTLBENTRIES; i++) {

		tfptlbfill(i,0,0, (pt[0] & TLBLO_PFNMASK) | tlb_attrib | TLBLO_UNCACHED);

		if (setjmp(faultbuf)) {
			section4++;
			show_fault();
			DoEret();
			error++;
			goto Done;
		}

		nofault = faultbuf;
		section3++;
		*(volatile u_int *)cachedp = cachepat;
		*(volatile u_int *)uncachedp = mempat;
		tlbpat = *(u_int *)0;
		nofault = 0;
		if (tlbpat != mempat) {
			msg_printf(ERR,
				"Uncached TLB, Expected: 0x%08x, Actual: 0x%08x\n",
				mempat, tlbpat);
			error++;
		}

/*		invaltlb(i);*/
		tlbpurge();
	}

}


Done:

	msg_printf(DBG,"section1 = %d  section2 = %d\n",section1,section2);
	msg_printf(DBG,"section3 = %d  section4 = %d\n",section3,section4);

	return error;
}
