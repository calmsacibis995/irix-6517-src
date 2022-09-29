#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_findpde.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>
#include "icrash.h"
#include "extern.h"

#ifdef NOTYET
/*
 * dofindpde() -- Find specific pde_t structs.
 */
int
dofindpde(command_t cmd)
{
	int pgno, value = 0, pte_cnt = 0, i, firsttime = 1;
	unsigned map, addr, pde;
	struct pmap *pmp, pmbuf;
	pde_t *pp, ppbuf;
	struct pregion *prp, prbuf, *npr;

	map = GET_VALUE(cmd.args[0]);
	addr = GET_VALUE(cmd.args[1]);

	if (!(pmp = GET_PMAP(map, &pmbuf))) {
		fprintf(cmd.ofp, "could not get pmap 0x%x\n", map);
		return;
	}

	if (!GET_POINTER(pde_t, pmp->pmap_preg, &npr, "pde_t")) {
		if (debug) {
			fprintf(cmd.ofp, "could not read pregion at 0x%x\n",
				pmp->pmap_preg);
		}
		return;
	}

	for (i = 0; npr; i++) {
		if ((prp = GET_PREGION(npr, &prbuf)) == 0) {
			return;
		}
		if ((addr >= (unsigned)prp->p_attrs.attr_start) && 
			(addr < (unsigned)prp->p_attrs.attr_end)) {
				pgno = ((addr - (unsigned)prp->p_attrs.attr_start) / NBPC);
				break;
		}
		npr = prp->p_forw;
		if (!npr) {
			fprintf(cmd.ofp,
				"fpde: pmap 0x%x does not contain mapping "
				"for virtual address 0x%x\n", map, addr);
			return;
		}
	}

	if ((pp = pmap_to_pde(pmp, addr, &ppbuf, &pde)) == 0) {
		fprintf(cmd.ofp, "Could not locate pte 0x%x/%d\n",
			map, addr);
		return;
	}
	fprintf (cmd.ofp, "     PDE       PTE      PFN   N  M  VR  G  CC  D  SV  NR  EOP\n");
	fprintf (cmd.ofp, "=============================================================\n");
	print_pde(pde, pp, cmd.ofp);
}
#endif /* NOTYET */

