#ident	"lib/libsc/lib/setpage.c:  $Revision: 1.3 $"

#include <sys/types.h>
#include <libsc.h>
#include <libsk.h>

#if TFP
#include <sys/tfp.h>
#define PAGE_MASK_BITS (~(SR_UPSMASK|SR_KPSMASK))
static
__psunsigned_t pagebits[] = {
	SR_UPS_4K|SR_KPS_4K,	/* 4K  pages */
	SR_UPS_8K|SR_KPS_8K,	/* 8K  pages */
	SR_UPS_16K|SR_KPS_16K,	/* 16K pages */
	SR_UPS_64K|SR_KPS_64K,	/* 64K pages */
	SR_UPS_64K|SR_KPS_64K,	/* 256Kpages: ERROR on TFP -- do 64K pages */
	SR_UPS_1M|SR_KPS_1M,	/* 1M  pages */
	SR_UPS_4M|SR_KPS_4M,	/* 4M  pages */
	SR_UPS_16M|SR_KPS_16M	/* 16M pages */
};
#elif R4000 || R10000
static
unsigned pagebits[] = {
	(0 << 13),	/* 4K  pages */
	(0 << 13),	/* 8K  pages: ERROR, do 4K pages */
	(3 << 13),	/* 16K pages */
	(15 << 13),	/* 64K pages */
	(31 << 13),	/* 256K  pages */
	(63 << 13),	/* 1M  pages */
	(127 << 13),	/* 4M  pages */
	(255 << 13)	/* 16M pages */
};
#else
#error Unsupported CPU (!TFP and !R4000).
#endif

/* machine-independent way to set the page size
 * of a processor; the only caveat is that
 * you must be careful to not specify 8K pages unless
 * you run only on TFP, and don't specify 256K pages
 * on a TFP
 *
 * calling protocol is:
 *	k_machreg_t old = set_pagesize(_pageXX);
 *	......
 *	restore_pagesize(old);
 */

k_machreg_t
set_pagesize(enum pagesize p)
{
#if TFP
	k_machreg_t o = GetSR();
	SetSR((o & PAGE_MASK_BITS) | pagebits[(int)p]);
#else
	long o = get_pgmask();
	set_pgmask(pagebits[(int)p]);
#endif
	return (k_machreg_t)o;
}

void
restore_pagesize(k_machreg_t r)
{
#if TFP
	SetSR(r);
#else
	set_pgmask((unsigned)r);
#endif
}
