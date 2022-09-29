/* DBSYM.C -- DEBUGGER SYMBOL SUPPORT FUNCTIONS                 */

#include "asm29.h"

/*
 * Find a debugger symbol in the table.
 */
struct debug_sym *
finddbugsym(char *cp)
{
struct debug_sym *dbp;

	for (dbp = dbugtab; dbp; dbp = dbp->db_link)
		if (!strcmp (cp, dbp->db_name))
			return dbp;
	return NULL;
}

/*
 * Insert a debug symbol from "dbugsym" into the COFF linked-list.
 *   An auxiliary symbol, if present, is in the global "auxsym".
 */
idbugsym()
{
	struct debug_sym *dbp;
	char sclass, *sname;

	/* special handling of EFCN stg. class */
	if (dbugsym.db_cf.n_sclass == C_EFCN) {
		dbp = finddbugsym( dbugsym.db_name );
		if (dbp && dbp->db_auxp)
			dbp->db_auxp->x_sym.x_misc.x_fsize =
				dbugsym.db_cf.n_value - dbp->db_cf.n_value;
		return;
		}
	if (pass == 1)
		{
		lastdbp->db_link =
			(struct debug_sym *)e_alloc(1,sizeof(struct debug_sym));
		}
	lastdbp = lastdbp->db_link;

	/* necessary to keep certain values from pass 1 */
	dbugsym.db_auxp = lastdbp->db_auxp;
	dbugsym.db_link = lastdbp->db_link;
	*lastdbp = dbugsym;

	/* keep track of tag symbols for future references */
	if ((lastdbp->db_cf.n_sclass == C_STRTAG) ||
			(lastdbp->db_cf.n_sclass == C_UNTAG) ||
			(lastdbp->db_cf.n_sclass == C_ENTAG))
		{
		lsttag->db_nextag = lastdbp;
		lsttag = lastdbp;
		}
	lastdbp->db_index = ndbsyms;
	ndbsyms++;
	if (dbugsym.db_cf.n_numaux != 0) {
		if (pass == 1)
			lastdbp->db_auxp = (union auxent *)e_alloc(1,sizeof(union auxent));
		*(lastdbp->db_auxp) = auxsym;
		ndbsyms++;
	}

	/* make notes of any tagnames or beginnings of functions and blocks
	 * and fill in endndx field where necessary */
	sclass = dbugsym.db_cf.n_sclass;
	sname = dbugsym.db_name;
	if ((sclass == C_FCN && !strcmp(sname, ".bf"))
	    || (sclass == C_BLOCK && !strcmp(sname, ".bb"))
	    || (sclass == C_STRTAG || sclass==C_UNTAG || sclass==C_ENTAG)) {
		if (nblocks < MAXBLOCKS)
			block[nblocks++] = lastdbp;
	} else if ((sclass == C_FCN && !strcmp(sname, ".ef"))
		   || (sclass == C_BLOCK && !strcmp(sname, ".eb"))
		   || !strcmp (sname, ".eos")) {
		if (nblocks > 0) {
			dbp = block[--nblocks];
			if (dbp->db_auxp != NULL)
				dbp->db_auxp->xx_endndx = ndbsyms;
#ifdef SWAP_COFF
			if (target_order != host_order)
				putswap(dbp->db_auxp->xx_endndx,
					&dbp->db_auxp->xx_endndx,
					sizeof(long));
#endif
		} else if (pass == 3)
		printf("Warning: unmatched blocks in debug symbol table\n");
	}
}

/*
 * Find a previously-defined tag.
 */
unsigned long
dbfindtag(char *tagn)
{
	struct debug_sym *dbp;

	dbp = dbugtab;
	while ((dbp->db_nextag != NULL ) && (dbp != lsttag)) {
		dbp = dbp->db_nextag;
		if (!strcmp (dbp->db_name, tagn))
			return(dbp->db_index);
	}
	return 0;
}

/*
 * Write debugger symbols to the object file.
 */
wrdbugsyms(struct debug_sym *dbp)
{
	int i, slen;
	long save;

	while (dbp != NULL) {
		if ((slen = strlen (dbp->db_name)) > 8) {
			save = ftell (outf);
			fseek (outf, stringptr + cur_strgoff, 0);
			fwrite (dbp->db_name, 1, slen+1, outf);
			fseek (outf, save, 0);
			dbp->db_cf.n_zeroes = 0;
			dbp->db_cf.n_offset = cur_strgoff;
#ifdef SWAP_COFF
			if (target_order != host_order)
				putswap(cur_strgoff,
					&dbp->db_cf.n_offset,
					sizeof(long));
#endif
			cur_strgoff += slen+1;
		} else {
			for (i = 0; i < slen; i++)
				dbp->db_cf.n_name[i] = dbp->db_name[i];
			bzero(&dbp->db_cf.n_name[slen], S_LEN-slen);
		}
#ifdef SWAP_COFF
		if (target_order != host_order) {
			putswap(dbp->db_cf.n_value,
				 &dbp->db_cf.n_value,
				 sizeof(long));
			putswap((long)dbp->db_cf.n_scnum,
				&dbp->db_cf.n_scnum,
				sizeof(short));
			putswap((long)dbp->db_cf.n_type,
				&dbp->db_cf.n_type,
				sizeof(short));
		}
#endif
		fwrite((char *)dbp, 1, SYMESZ, outf);
		if (dbp->db_cf.n_numaux != 0)
			fwrite ((char *)dbp->db_auxp, 1, SYMESZ, outf);
		dbp = dbp->db_link;
	}
}
