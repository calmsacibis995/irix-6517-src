#include "sys/types.h"
#include "sys/sbd.h"
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

	uint	expected_pat;
	uint	rpat;
	__psunsigned_t	tlbhi_mask = TLBHI_VPNMASK | TLBHI_PIDMASK;
	__psunsigned_t	tlblo_mask = TLBLO_PFNMASK | TLBLO_CACHMASK | TLBLO_D;
	__psunsigned_t	wpat;
	__psunsigned_t	pmask;


	wpat = 0xa5a5a5a5;
	pmask = get_pgmask();
	pmask = ~((pmask >> 13) << 6);

	for (i = 0; i < NTLBENTRIES; i++) {
		for (j = 0; j < 2; j++) {

			tlbwired(i, 0, 0, (uint)(wpat & tlblo_mask), 
				 (uint)((~wpat) & tlblo_mask));

			/* gets value from EntryLo0 Register */
			rpat = (uint)get_tlblo0(i);

			expected_pat = (uint)(wpat & tlblo_mask & pmask);

			if (rpat != expected_pat) {
				msg_printf(ERR|PRCPU,
					"TLB entry %2d LO 0: Expected:0x%x, Actual:0x%x\n",
					i, expected_pat, rpat);
				error++;
			}

			/* gets value from EntryLo1 Register */
			rpat = (uint)get_tlblo1(i);

			expected_pat = (uint)((~wpat) & tlblo_mask & pmask);

			if (rpat != expected_pat) {
				msg_printf(ERR|PRCPU,
					"TLB entry %2d LO 1: Expected:0x%x, Actual:0x%x\n",
					i, expected_pat, rpat);
				error++;
			}

			wpat = ~wpat;
		}
		invaltlb(i);
	}

	for (i = 0; i < NTLBENTRIES; i++) {
		for (j = 0; j < 2; j++) {
			tlbwired(i, (uint)wpat & 0xff, (caddr_t) wpat, 0, 0);

			rpat = (uint)get_tlbhi(i);

			expected_pat = (uint)(wpat & tlbhi_mask);
			if (rpat != expected_pat) {
				msg_printf(ERR|PRCPU,
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




