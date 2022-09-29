/* Test non cached tlb mapping
 */

#ident "$Revision: 1.10 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <setjmp.h>
#include <fault.h>
#include <libsk.h>
#include "tlb.h"
#include "IP30/cache/cache.h"

static __psunsigned_t va[sizeof(int)] = {
	KUBASE,
	KUBASE|0x20000000,
	K2BASE,
	K2BASE|0x20000000
};

int
tlbnocache(__psunsigned_t *pt, u_int *uncachedp, u_int *cachedp)
{
	u_int	cachepat = 0x87654321;
	int	error = 0;
	jmp_buf	faultbuf;
	int	i;
	u_int	mempat = 0x12345678;
	u_int	tlb_attrib = TLBLO_V | TLBLO_D;
	u_int	tlbpat;

	for (i = 0; i < NTLBENTRIES; i++) {
		__psunsigned_t addr = va[i & (sizeof(int) - 1)];

		tlbwired(i, 0, (caddr_t)addr,
			(uint)((pt[0] >> 6) | tlb_attrib | TLBLO_NONCOHRNT),
			(uint)((pt[1] >> 6) | tlb_attrib | TLBLO_NONCOHRNT));
		if (setjmp(faultbuf)) {
			show_fault();
			DoEret();
			return ++error;
		}

		nofault = faultbuf;
		*(volatile u_int *)cachedp = cachepat;
		*(volatile u_int *)uncachedp = mempat;
		tlbpat = *(u_int *)addr;
		nofault = 0;
		if (tlbpat != cachepat) {
			msg_printf(ERR|PRCPU,
				"Cached TLB, Expected: 0x%08x, Actual: 0x%08x\n",
				cachepat, tlbpat);
			error++;
		}

		invaltlb(i);
	}

	for (i = 0; i < NTLBENTRIES; i++) {
		tlbwired(i, 0, 0,
			(uint)((pt[0] >> 6) | tlb_attrib | TLBLO_UNCACHED),
			(uint)((pt[1] >> 6) | tlb_attrib | TLBLO_UNCACHED));

		if (setjmp(faultbuf)) {
			show_fault();
			DoEret();
			return ++error;
		}

		nofault = faultbuf;
		*(volatile u_int *)cachedp = cachepat;
		*(volatile u_int *)uncachedp = mempat;
		/*
		 * the test may not work without this second write due to a
		 * possible R4000 bug
		 */
		*(volatile u_int *)uncachedp = mempat;
		tlbpat = *(u_int *)0;
		nofault = 0;
		if (tlbpat != mempat) {
			msg_printf(ERR|PRCPU,
				"Uncached TLB, Expected: 0x%08x, Actual: 0x%08x\n",
				mempat, tlbpat);
			error++;
		}

		invaltlb(i);
	}

	return error;
}
