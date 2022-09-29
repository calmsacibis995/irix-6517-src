/* tlb translation test.
 */

#ident "$Revision: 1.10 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <setjmp.h>
#include <fault.h>
#include <libsk.h>
#include "tlb.h"
#include "fforward/cache/cache.h"

/*ARGSUSED2*/
int
tlbtranslate(__psunsigned_t *pt, u_int *uncachedp, uint *not_used)
{
	int	error = 0;
	jmp_buf	faultbuf;
	int	i;
#ifdef R10000
	ulong status;
#endif

	u_int	tlblo_attrib = TLBLO_V | TLBLO_UNCACHED | TLBLO_G;

	msg_printf(VRB, "TLB translation test\n");

#ifdef R10000
	status = get_SR();

	/* turn on xtended-addr TLB vector */
	set_SR(status | SR_UX);
#endif

	for (i = 0; i < NTLBENTRIES; i++) {
		__psunsigned_t addr;

		for (addr = KUBASE; addr < KUSIZE; addr = (addr << 1) + PAGESIZE) {
			__psunsigned_t	_addr = addr & ~PAGESIZE;
			u_char	bucket;
			u_char	*ptr = (u_char *)uncachedp;

			tlbwired(i, 0, (caddr_t)_addr,
				(uint)((pt[i * 2] >> 6) | tlblo_attrib),
				(uint)((pt[i * 2 + 1] >> 6) | tlblo_attrib));

			if (setjmp(faultbuf)) {
				show_fault();
				DoEret();
				return ++error;
			}

			nofault = faultbuf;
			bucket = *(u_char *)_addr;
			nofault = 0;

			if (bucket != *ptr) {
				msg_printf (ERR,
					"looking at *0x%x(uncached) vs *0x%x(cached)\n",
						ptr, _addr);

				msg_printf (ERR,
					"Data, Expected: 0x%x, Actual: 0x%x\n",
					*ptr, bucket);
				error++;
			}

			_addr |= PAGESIZE;
			nofault = faultbuf;
			bucket = *(u_char *)_addr;
			nofault = 0;

			ptr += PAGESIZE;
			if (bucket != *ptr) {
				msg_printf (ERR,
					"looking at *0x%x(uncached) vs *0x%x(cached)\n",
						ptr, _addr);
				msg_printf (ERR,
					"Data, Expected: 0x%x, Actual: 0x%x\n",
					*ptr, bucket);
				error++;
			}

			invaltlb(i);
		}

		uncachedp += PAGESIZE * 2 / sizeof(*uncachedp);
	}

#ifdef R10000
	set_SR(status);		/* reset status register to original value */
#endif

	return error;
}
