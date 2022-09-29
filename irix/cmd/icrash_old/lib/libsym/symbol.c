#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libsym/RCS/symbol.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"
#include <sys/types.h>
#include <sys/param.h>
#include <libelf.h>
#include <libdwarf.h>
#include <dwarf.h>
#include <nlist.h>
#include <stdio.h>
#include <errno.h>
#include "icrash.h"
#include "extern.h"
#include "eval.h"
#include "dwarflib.h"

#ifdef NOT
/*
 * free_sym()
 */
void 
free_sym(struct syment *sp) 
{
	if (!sp) {
		return;
	}

	if (sp->n_name) {
		free_block((k_ptr_t)sp->n_name);
	}
	free_block((k_ptr_t)sp);
}

/*
 * get_sym() -- Get symbol information for caller.
 */
struct syment *
get_sym(char *sym, int flag)
{
	int sp_alloced = 0;
	symdef_t *sdp;
	struct nlist64 *nl64;
	struct nlist *nl32;
	struct syment *sp;
	symaddr_t *taddr;

	if (DEBUG(K, DC_FUNCTRACE, 4)) {
		fprintf(KL_ERRORFP, "get_sym: sym=\"%s\", flag=0x%x\n", sym, flag);
	}

	kl_reset_error();

	if (!sym) {
		KL_SET_ERROR_CVAL(KLE_BAD_SYMNAME, sym);
		return ((struct syment *)NULL);
	}

	/* Depending on value of flag, allocate B_TEMP or B_PERM blocks
	 */
	if (flag == B_PERM) {
		sp = (struct syment *)alloc_block(sizeof(struct syment), B_PERM);
		sp->n_name = (char *)alloc_block((strlen(sym) + 1), B_PERM);
	}
	else {
		sp = (struct syment *)alloc_block(sizeof(struct syment), B_TEMP);
		sp->n_name = (char *)alloc_block((strlen(sym) + 1), B_TEMP);
	}

	if (PTRSZ64(K)) {
		nl64 = (struct nlist64 *)alloc_block(2*sizeof(struct nlist64), B_TEMP);
		nl64->n_name = sym;
	} 
	else {
		nl32 = (struct nlist *)alloc_block(2*sizeof(struct nlist), B_TEMP);
		nl32->n_name = sym;
	}

	if (taddr = sym_nmtoaddr(stp, sym)) {
		sprintf(sp->n_name, "%s", taddr->s_name);
		sp->n_value = taddr->s_lowpc;
		sp->n_type = 0;
		if (PTRSZ64(K)) {
			free_block((k_ptr_t)nl64);
		} 
		else {
			free_block((k_ptr_t)nl32);
		}
		if (DEBUG(K, DC_SYM, 3)) {
			fprintf(KL_ERRORFP, "get_sym: returning name=%s, value=%llx\n",
				sp->n_name, sp->n_value);
		}
		return (sp);
	} 
	else if ((PTRSZ64(K)) && (nlist64(namelist, nl64) > 0) &&
		(nl64->n_type != 0)) {
			sprintf(sp->n_name, "%s", nl64->n_name);
			sp->n_value = nl64->n_value;
			sp->n_type = nl64->n_type;
			free_block((k_ptr_t)nl64);
			if (DEBUG(K, DC_SYM, 1)) {
				fprintf(KL_ERRORFP, "get_sym: returning name=%s, value=%llx\n",
					sp->n_name, sp->n_value);
			}
			return (sp);
	} 
	else if ((!PTRSZ64(K)) && (nlist(namelist, nl32) > 0) &&
		(nl32->n_type != 0)) {
			sprintf(sp->n_name, "%s", nl32->n_name);
			sp->n_value = nl32->n_value;
			sp->n_type = nl32->n_type;
			free_block((k_ptr_t)nl32);
			if (DEBUG(K, DC_SYM, 1)) {
				fprintf(KL_ERRORFP, "get_sym: returning name=%s, value=%llx\n",
					sp->n_name, sp->n_value);
			}
			return (sp);
	}

	if (PTRSZ64(K)) {
		free_block((k_ptr_t)nl64);
	} 
	else {
		free_block((k_ptr_t)nl32);
	}
	free_sym(sp);
	return ((struct syment *)0);
}

/*
 * get_sym_addr()
 */
kaddr_t
get_sym_addr(char *sym)
{
	kaddr_t value;
	struct syment *s;

	/* It's not necessary to reset error because it's done in the
	 * get_sym() routine.
	 */
	s = get_sym(sym, B_TEMP);
	if (KL_ERROR) {
		KL_SET_ERROR_CVAL(KLE_BAD_SYMNAME, sym);
		return((kaddr_t)NULL);
	}
	else {
		value = (kaddr_t)s->n_value;
		free_sym(s);
		return(value);
	}
}

/*
 * get_sym_ptr()
 */
kaddr_t
get_sym_ptr(char *sym)
{
	kaddr_t value;
	struct syment *s;

	/* It's not necessary to reset error because it's done in the
	 * get_sym() routine.
	 */
	s = get_sym(sym, B_TEMP);
	if (KL_ERROR) {
		KL_SET_ERROR_CVAL(KLE_BAD_SYMNAME, sym);
		return((kaddr_t)NULL);
	}
	else {
		value = kl_kaddr_to_ptr(K, s->n_value);
		free_sym(s);
		return(value);
	}
}
#endif
