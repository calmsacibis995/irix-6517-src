

#include <sys/types.h>
#include <sys/vnode.h>
#include <ksys/behavior.h>

#include <dg.h>
#include <comprot.h>
#include <comnetp.h>
/* Structure that contains fields that are pointers to key kernel 
 * structures). This forces the type information to be sucked into 
 * kernel the symbol table.
 */
typedef struct rpc_icrash_s {
	rpc_listener_state_t 			*rpc_icrash0;
	rpc_dg_recvq_elt_t	 		*rpc_icrash1;
	rpc_dg_call_t	 		 	*rpc_icrash2;
	rpc_dg_ccall_t				*rpc_icrash3;
} rpc_icrash_t;

rpc_icrash_t *rpc_icrash_struct;

/* Dummy function called by icrash_init() to ensure that symbol
 * information from this module gets included in the kernel
 * symbol table.
 */
void 				  
rpc_icrash_init(void)
{
}
