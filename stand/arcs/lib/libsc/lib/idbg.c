#ident	"lib/libsc/lib/idbg.c:  $Revision: 1.18 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/loaddrs.h>
#include <arcs/debug_block.h>
#include <arcs/spb.h>
#include <sys/idbg.h>
#include <libsc.h>

int symmon;

/*
 * symbol table for symmon, filled in by setsym
 * Must be .data
 *
 */
#define SYMMAX	4800
#define NAMESIZ SYMMAX*12

struct dbstbl dbstab[SYMMAX] = {0};

char nametab[NAMESIZ] = {0};
int symmax = SYMMAX;
int namesize = NAMESIZ;

void
idbg_init(void)
{
	struct debug_block *dbp;
	int i;

	if (SPB->DebugBlock == 0)
		return;

	dbp = (struct debug_block *)SPB->DebugBlock;

	dbp->db_nametab = (__scunsigned_t)&nametab[0];
	dbp->db_symtab = (__scunsigned_t)&dbstab[0];
	dbp->db_symmax = SYMMAX;

	/*
	 * Relocate the symbol table if necessary
	 */
	for ( i = 0; i < SYMMAX && dbstab[i].addr; i++ )
		if ( ! strncmp(nametab+dbstab[i].noffst, "idbg_init", 9) ) {
			__scunsigned_t offset = (__scunsigned_t)&idbg_init-dbstab[i].addr;
			if ( offset )
				for ( i = 0; i < SYMMAX && dbstab[i].addr; i++ )
					dbstab[i].addr += offset;
			break;
		}
}
