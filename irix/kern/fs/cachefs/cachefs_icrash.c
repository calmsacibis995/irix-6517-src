#ident	"$Revision: 1.1 $"

#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/vnode.h>
#include "cachefs_fs.h"


/* Structure contains fields that are pointers to key kernel data 
 * structures. This forces the type information to be sucked into 
 * the kernel symbol table.
 */
typedef struct cachefs_icrash_s {

	fscache_t			*cachefs_icrash0;
	struct cache_label		*cachefs_icrash1;
	struct cachefscache		*cachefs_icrash2;
	allocmap_t			*cachefs_icrash3;
	cattr_t				*cachefs_icrash4;
	cachefsmeta_t			*cachefs_icrash5;
	fileheader_t			*cachefs_icrash6;
	cnode_t				*cachefs_icrash7;
	struct cachefs_stats		*cachefs_icrash8;
	struct filheader_cache_entry	*cachefs_icrash9;
	
} cachefs_icrash_t;

cachefs_icrash_t *cachefs_icrash_struct;

/* Dummy function called by icrash_init() to ensure that symbol
 * information from this module gets included in the kernel
 * symbol table.
 */
void
cachefs_icrash(void)
{
}

