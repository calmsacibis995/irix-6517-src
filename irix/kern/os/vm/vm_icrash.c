#ident	"$Revision: 1.2 $"


#include "anon_mgr.h"
#include "pcache.h"
#include "scache.h"
#include "scache_mgr.h"
#include "vnode_pcache.h"

/* Structure that contains fields that are pointers to key kernel
 * structures). This forces the type information to be sucked into
 * kernel the symbol table.
 */
typedef struct vm_icrash_s {
	anon_lock_t		*vm_icrash0;
	anon_t			*vm_icrash1;
	pcache_t		*vm_icrash2;
	scache_t		*vm_icrash3;
	struct hashblock	*vm_icrash4;
	scache_pool_t		*vm_icrash5;
	vnode_pcache_t		*vm_icrash6;
} vm_icrash_t;

vm_icrash_t *vm_icrash_struct;

/* Dummy function called by icrash_init() to ensure that symbol
 * information from this module gets included in the kernel
 * symbol table.
 */
void 				  
vm_icrash(void)
{
}
