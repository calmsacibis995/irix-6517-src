#ident	"$Revision: 1.4 $"

#include "as_private.h"

/*
#include "stddef.h"
#include "sys/types.h"
#include "ksys/as.h"
*/
/*
#include "sys/atomic_ops.h"
#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/getpages.h"
#include "sys/kmem.h"
#include "limits.h"
#include "sys/numa.h"
#include "sys/page.h"
#include "pmap.h"
#include "sys/proc.h"
#include "os/proc/pproc_private.h"
#include "sys/vnode.h"
#include "sys/sysmacros.h"
#include "sys/systm.h"
#include "ksys/vpag.h"
#include "os/numa/pm.h"
#include "os/numa/pmo_ns.h"
#include "os/numa/aspm.h"
*/

/* Structure that contains fields that are pointers to key kernel
 * structures). This forces the type information to be sucked into
 * kernel the symbol table.
 */
typedef struct as_icrash_s {
	pas_t 			 	*as_icrash0;
	ppas_t 		 		*as_icrash1;
	vnmap_t				*as_icrash2;
#if CELL
	dcas_t 		 		*as_icrash3;
	dcpas_t 		 	*as_icrash4;
	dsas_t 		 		*as_icrash5;
	dspas_t 		 	*as_icrash6;
#if NEVER
	pas_blist_t 			*as_icrash7;
#endif
#endif /* CELL */
} as_icrash_t;

as_icrash_t *as_icrash_struct;

/* Dummy function called by icrash_init() to ensure that symbol
 * information from this module gets included in the kernel
 * symbol table.
 */
void 				  
as_icrash(void)
{
}
