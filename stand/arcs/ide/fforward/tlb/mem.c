#include "sys/types.h"
#include "sys/sbd.h"
#include "sys/mc.h"
#include "tlb.h"
#include "libsk.h"

/*
 * Test the tlb as a small memory array.  Just see if all the read/write
 * bits can be toggled and that all undefined bit read back zero.
 */
/*ARGSUSED*/
int
tlbmem(__psunsigned_t *not_used1, u_int *not_used2, u_int *not_used3)
{
	int	error = 0;
	int	i;
	int	j;
	u_int	expected_pat;
	u_int	rpat;
	__psunsigned_t	tlbhi_mask = TLBHI_VPNMASK | TLBHI_PIDMASK;
	__psunsigned_t	tlblo_mask = TLBLO_PFNMASK | TLBLO_CACHMASK | TLBLO_D;
	__psunsigned_t	wpat;
	__psunsigned_t  loword_mask = 0xffffffff;
#ifdef R10000
	__psunsigned_t	pmask;
#endif

	msg_printf(VRB, "TLB data test\n");

	/*
	 * dwong: without the following read, this test will fail within IDE
	 * if report level is set to verbose and the machine has GR2 graphics
	 */
	*(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0);

	wpat = 0xa5a5a5a5;
#ifdef R10000
	pmask = get_pgmask();
	pmask = ~((pmask >> 13) << 6);
#endif

	for (i = 0; i < NTLBENTRIES; i++) {
		for (j = 0; j < 2; j++) {

			tlbwired(i, 0, 0, (uint)(wpat & tlblo_mask), 
				 (uint)((~wpat) & tlblo_mask));

			rpat = (uint)get_tlblo0(i);
#ifdef R10000
			expected_pat = (uint)(wpat & tlblo_mask & pmask) & loword_mask;
			rpat &= loword_mask;	
#else
			expected_pat = wpat & tlblo_mask;
#endif
			if (rpat != expected_pat) {
				msg_printf(ERR,
					"TLB entry %2d LO 0: Expected:0x%x, Actual:0x%x, diff: 0x%x\n",
					i, expected_pat, rpat, (expected_pat^rpat));
				error++;
			}

			wpat = ~wpat;

			rpat = (uint)get_tlblo1(i);
#ifdef R4000
			expected_pat = wpat & tlblo_mask;
#else
			expected_pat = (uint)(wpat & tlblo_mask & pmask);
#endif
			if (rpat != expected_pat) {
				msg_printf(ERR,
					"TLB entry %2d LO 1: Expected:0x%x, Actual:0x%x\n",
					i, expected_pat, rpat);
				error++;
			}
		}
		invaltlb(i);
	}

	for (i = 0; i < NTLBENTRIES; i++) {
		for (j = 0; j < 2; j++) {
			tlbwired(i, (uint)wpat & 0xff, (caddr_t) wpat, 0, 0);

			rpat = (uint)get_tlbhi(i);
			expected_pat = (uint)(wpat & tlbhi_mask);
			if (rpat != expected_pat) {
				msg_printf(ERR,
					"TLB entry %2d HI: Expected:0x%x, Actual:0x%x\n",
					i, expected_pat, rpat);
				error++;
			}

			wpat = ~wpat;
		}
		invaltlb(i);
	}

	return error;
}
