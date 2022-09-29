#ident	"$Revision: 1.1 $"

#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/vnode.h>
#include "lofs_info.h"
#include "lofs_node.h"


/* Structure contains fields that are pointers to key kernel data 
 * structures. This forces the type information to be sucked into 
 * the kernel symbol table.
 */
typedef struct lofs_icrash_s {

	struct loinfo			*lofs_icrash0;
	struct lfsnode			*lofs_icrash1;
	lnode_t				*lofs_icrash2;

} lofs_icrash_t;

lofs_icrash_t *lofs_icrash_struct;

/* Dummy function called by icrash_init() to ensure that symbol
 * information from this module gets included in the kernel
 * symbol table.
 */
void
lofs_icrash(void)
{
}

