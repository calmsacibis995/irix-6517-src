#ident "$Revision: 1.3 $"

#include <sys/types.h>
#include <sys/buf.h>
#include <sys/uuid.h>
#include <sys/xlate.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include "xlv_ioctx.h"
#include "xlv_procs.h"
#include "xlv_xlate.h"
#include "xlv_mem.h"

/* Structure that contains fields that are pointers to key kernel
 * structures). This forces the type information to be sucked into
 * kernel the symbol table.
 */
typedef struct xlv_icrash_s {
	xlv_io_context_t  			*xlv_icrash0;
	buf_queue_t 				*xlv_icrash2;
	xlv_res_mem_t 				*xlv_icrash3;
} xlv_icrash_t;

xlv_icrash_t *xlv_icrash_struct;

/* Dummy function called by icrash_init() to ensure that symbol
 * information from this module gets included in the kernel
 * symbol table.
 */
void xlv_icrash(void)
{
}
