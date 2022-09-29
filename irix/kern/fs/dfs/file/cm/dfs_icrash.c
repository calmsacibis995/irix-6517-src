
#include <sys/types.h>
#include <sys/vnode.h>
#include <ksys/behavior.h>

#include <cm.h>
#include <cm_dcache.h>
#include <cm_scache.h>
#include <cm_server.h>
/* Structure that contains fields that are pointers to key kernel 
 * structures). This forces the type information to be sucked into 
 * kernel the symbol table.
 */
typedef struct dfs_icrash_s {
	struct cm_scache 				*dfs_icrash0;
	struct cm_dcache 		 		*dfs_icrash1;
	struct cm_server 		 		*dfs_icrash2;
	struct cm_conn					*dfs_icrash3;
	struct cm_fcache				*dfs_icrash4;
	afsFid						*dfs_icrash5;
	struct fshs_host				*dfs_icrash7;
	struct hs_host					*dfs_icrash8;
	struct fshs_principal				*dfs_icrash9;
	struct cm_volume				*dfs_icrash10;
	struct volreg_cache				*dfs_icrash11;
} dfs_icrash_t;

dfs_icrash_t *dfs_icrash_struct;

/* Dummy function called by icrash_init() to ensure that symbol
 * information from this module gets included in the kernel
 * symbol table.
 */
void 				  
dfs_icrash_init(void)
{
}
