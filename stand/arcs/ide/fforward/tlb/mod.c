/* Test tlb modified exctption.
 */

#ident "$Revision: 1.11 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <setjmp.h>
#include <fault.h>
#include <libsk.h>
#include "tlb.h"
#include "fforward/cache/cache.h"

/*  In 64 bit mode we need to explicitly extend small ints to pointer size
 * before pointer cast.
 */
#define const2ptr(addr)	((volatile u_char *)(__psunsigned_t)(addr))

/*ARGSUSED*/
int
tlbmod(__psunsigned_t *pt, u_int *not_used1, u_int *not_used2)
{
	int	error = 0;
	jmp_buf	faultbuf;
	int	i;
#if R4000 || R10000
	u_int	tlb_attrib = TLBLO_V | TLBLO_UNCACHED | TLBLO_G;
#else
	u_int	tlb_attrib = TLBLO_V | TLBLO_UNCACHED;
#endif

	msg_printf(VRB, "TLB mod bit test\n");

	for (i = 0; i < NTLBENTRIES; i++) {
		tlbwired(i, 0, 0,
			(uint)(pt[i * 2] >> 6) | tlb_attrib,
			(uint)(pt[i * 2 + 1] >> 6) | tlb_attrib);

		if (setjmp(faultbuf)) {
			if (_badvaddr_save != (__psunsigned_t)1) {
				msg_printf (ERR,
					"BadVaddr, Expected: 0x00000001, Actual: 0x%x\n",
					_badvaddr_save);
				error++;
			}

			DoEret();
		} else {
			nofault = faultbuf;
			*const2ptr(1) = i * 2;
			nofault = 0;
			msg_printf (ERR,
				"Did not receive expected exception\n");
			error++;
		}

		invaltlb(i);
	}

	for (i = 0; i < NTLBENTRIES; i++) {
		tlbwired(i, 0, 0,
			(uint)(pt[i * 2] >> 6) | tlb_attrib | TLBLO_D,
			(uint)(pt[i * 2 + 1] >> 6) | tlb_attrib | TLBLO_D);

		if (setjmp(faultbuf)) {
			show_fault();
			DoEret();
			return ++error;
		} else {
			nofault = faultbuf;
			*const2ptr(1) = i * 2;
			nofault = 0;
			if (*const2ptr(1) != *const2ptr(0)) {
				msg_printf(ERR,
					"Bad data, Expected: 0x%02x, Actual: 0x%02x\n",
					*const2ptr(0), *const2ptr(1));
				error++;
			}
		}

		invaltlb(i);
	}

	return error;
}
