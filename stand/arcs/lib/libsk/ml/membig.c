#ident	"lib/libsc/lib/membig.c:  $Revision: 1.8 $"
#ifdef _K64PROM32
#define __USE_SPB32 1
#endif

#include <sys/types.h>
#include <sys/sbd.h>
#include <arcs/hinv.h>
#include <arcs/spb.h>
#include <libsc.h>
#include <libsk.h>

/* CallArcsProm, if used only for GetMemoryDesc, returns value
 * in low-order 32-bits per ARCS spec
 */
MEMORYDESCRIPTOR *
arcsGetMemDesc(MEMORYDESCRIPTOR *d)
{
#ifdef _K64PROM32
	/* the following really must be signed */
	__int32_t CallArcsProm(void *, __psint_t), i;
#define PTR	\
	((FirmwareVector32 *)(__psint_t)(SPB->TransferVector))->GetMemoryDesc

	/* this test is needed for 64-bit DPROMs on 32-bit stock PROM machines*/
	if(_isK64PROM32()) {
		i = CallArcsProm((void *)d, PTR);
		if(i) {
			return (MEMORYDESCRIPTOR *)(PHYS_TO_K1(i));
		}
		return 0;
	}
	else
#endif
		return (__TV->GetMemoryDesc)(d);
}
