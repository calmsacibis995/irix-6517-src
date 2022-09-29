#include "sys/sbd.h"
#include "sys/types.h"
#include "tlb.h"
#include "libsk.h"

#define const2ptr(addr)	((char *)(__psunsigned_t)(addr))

/*
 * See if all the tlb slots respond to probes upon address match.
 */
/*ARGSUSED*/
int
tlbprobe(__psunsigned_t *pt, u_int *not_used1, u_int *not_used2)
{
	int	error = 0;
	int	i;
	int	index;
	u_int	tlblo_attrib = TLBLO_V | TLBLO_UNCACHED;

	msg_printf(VRB, "TLB probe test\n");

	for (i = 0; i < NTLBENTRIES; i++) {
		tlbwired(i, 0, const2ptr(i * PAGESIZE * 2),
			(uint)((pt[i * 2] >> 6) | tlblo_attrib),
			(uint)((pt[i * 2 + 1] >> 6) | tlblo_attrib));

		index = (int)probe_tlb(const2ptr(i * PAGESIZE * 2), 0);
		if (index != i) {
			msg_printf (ERR,
				"Index probed, Expected: 0x%08x, Actual: 0x%8x\n",
			   	i, index);
			error++;
		}

		index = (int)probe_tlb(const2ptr((i * 2 + 1) * PAGESIZE), 0);
		if (index != i) {
			msg_printf (ERR,
				"Index probed, Expected: 0x%08x, Actual: 0x%8x\n",
			   	i, index);
			error++;
		}

		invaltlb(i);
	}

	return error;
}
