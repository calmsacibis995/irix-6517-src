#ident	"$Revision: 1.1 $"

#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/vnode.h>
#include "autofs.h"
#include "autofs_prot.h"


/* Structure contains fields that are pointers to key kernel data 
 * structures. This forces the type information to be sucked into 
 * the kernel symbol table.
 */
typedef struct autofs_icrash_s {

	struct autofs_args		*autofs_icrash0;
	autonode_t			*autofs_icrash1;
	struct autoinfo			*autofs_icrash2;
	mntrequest			*autofs_icrash3;
	mntres				*autofs_icrash4;
	umntrequest			*autofs_icrash5;
	umntres				*autofs_icrash6;

} autofs_icrash_t;

autofs_icrash_t *autofs_icrash_struct;

/* Dummy function called by icrash_init() to ensure that symbol
 * information from this module gets included in the kernel
 * symbol table.
 */
void
autofs_icrash(void)
{
}

