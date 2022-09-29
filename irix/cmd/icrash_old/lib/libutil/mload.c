#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libutil/RCS/mload.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/stream.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <libelf.h>
#include <stdlib.h>
#include <string.h>
#include <filehdr.h>
#include "icrash.h"
#include "extern.h"

#define M_ADDR(symp) (PTRSZ64(K) ? (*(uint64*)symp) : (*(uint*)symp))

/* This is a generic name for symbols that are not found in a loadable
 * module's symbol table. Since the functions that reference it (trace, 
 * dis, etc.) do not free it, we can use it freely whenever we are not 
 * able to locate an actual symbol name.
 */
char *mlinfoname = "<LOADABLE_MODULE>";

/*
 * addr_to_mlinfo()
 *
 * Return a pointer to a block of memory containing the ml_info struct
 * for a given virtual address (addr).
 *
 */
k_ptr_t 
addr_to_mlinfo(kaddr_t addr)
{
	k_ptr_t mlp;
	kaddr_t mlinfo, end, text;

	mlp = alloc_block(ML_INFO_SIZE(K), B_TEMP);
	mlinfo = K_MLINFOLIST(K);
	while (mlinfo) {

		kl_get_struct(K, mlinfo, ML_INFO_SIZE(K), mlp, "ml_info");

		text = kl_kaddr(K, mlp, "ml_info", "ml_text");
		end = kl_kaddr(K, mlp, "ml_info", "ml_end");

		/* Check to see if addr falls inside this module's address 
		 * space.
		 */
		if (text && ((addr >= text) && (addr <= end))) {
			return(mlp);
		}
		else {
			mlinfo = kl_kaddr(K, mlp, "ml_info", "ml_next");
		}
	}
	free_block(mlp);
	return((k_ptr_t)NULL);
}

/*
 * ml_symname() 
 *
 * Return the name of the symbol in symp. If strtab is NULL, use mlp,
 * alloc space for the stringtab, and find the symbol name. Note that
 * the symbol name returned is a dup of the one found in strtab. We have 
 * to do this because we don't know how long strtab will be staying around.
 */
char *
ml_symname(char *strtab, k_ptr_t symp, k_ptr_t mlp)
{
	char *s, *name;
	int strtab_alloced = 0;

	if (!strtab) {
		int strtabsz;
		kaddr_t stringtab;

		if (!mlp) {
			/* Can't do anything here (XXX - need error code?)
			 */
			return((char *)NULL);
		}

		/* Get the stringtab pointer. We have to test for a vlaid
		 * stringtab pointer because there was a case when ml_nsyms
		 * was > 0 and stringtab was NULL.
		 */
		if (!(stringtab = kl_kaddr(K, mlp, "ml_info", "ml_stringtab"))) {
			return((char *)NULL);
		}
		strtabsz = KL_INT(K, mlp, "ml_info", "ml_strtabsz");
		strtab = (char*)alloc_block(strtabsz, B_TEMP);
		kl_get_block(K, stringtab, strtabsz, strtab, "ml_sym");
		strtab_alloced = 1;
	}

	if (PTRSZ64(K)) {
		s = strtab + *(uint*)((uint)symp + 8);
	}
	else {
		s = strtab + *(uint*)((uint)symp + 4); 
	} 

	/* We have to copy over the name to a temp block because the strtab 
	 * that contains the name string is likely to be freed before the
	 * name can be used (it will be if it was alloced in this function).
	 */
	name = (char*)alloc_block(strlen(s) + 1, B_TEMP);
	strcpy(name, s);
	if (strtab_alloced) {
		free_block((k_ptr_t)strtab);
	}
	return(name);
}

/* 
 * ml_findsym()
 * 
 * Walk through the symbol table for a given loadable module and check
 * to see if there is a match for a particular name or address. Note that
 * with address, we will return a pointer to the closest record (equal to
 * or less than). Also, we can't check for both addr and name, so name 
 * takes precedence (because it's possible for there to be an exact match).
 */
k_ptr_t 
ml_findsym(kaddr_t addr, char *name, k_ptr_t mlp)
{
	int i, nsyms, strtabsz;
	char *s, *strtab;
	kaddr_t stringtab, sym, lastsym = 0;
	k_ptr_t symp;

	/* Just in case...
	 */
	if (!mlp) {
		return((k_ptr_t)NULL);
	}

	nsyms = KL_INT(K, mlp, "ml_info", "ml_nsyms");
	if (nsyms == 0) {
		return((k_ptr_t)NULL);
	}

	/* If we are doing a name search, we have to allocate space for
	 * the module string table.
	 */
	if (name) {
		/* Get the stringtab pointer. We have to test for a vlaid
		 * stringtab pointer because there was a case when ml_nsyms
		 * was > 0 and stringtab was NULL.
		 */
		stringtab = kl_kaddr(K, mlp, "ml_info", "ml_stringtab");
		if (!stringtab) {
			return((k_ptr_t)NULL);
		}
		strtabsz = KL_INT(K, mlp, "ml_info", "ml_strtabsz");
		if (!strtabsz) {
			return((k_ptr_t)NULL);
		}
		strtab = (char*)alloc_block(strtabsz, B_TEMP);
		kl_get_block(K, stringtab, strtabsz, strtab, "ml_sym");
	}

	sym = kl_kaddr(K, mlp, "ml_info", "ml_symtab");
	symp = alloc_block(ML_SYM_SIZE(K), B_TEMP);
	for (i = 0; i < nsyms; i++) {
		kl_get_block(K, sym, ML_SYM_SIZE(K), symp, "ml_sym");
		if (name) {
			s = ml_symname(strtab, symp, (k_ptr_t)NULL);
			if (!strcmp(name, s)) {
				free_block((k_ptr_t)strtab);
				free_block((k_ptr_t)s);
				return(symp);
			}
			free_block((k_ptr_t)s);
		}
		else {
			if (addr == M_ADDR(symp)) {
				return(symp);
			}
			if (addr < M_ADDR(symp)) {

				/* lastsym is really the entry we want. We have to load it
				 * back in and return it...unless addr falls before the
				 * first sym, in which case we return a NULL pointer.
				 */
				if (i) {
					kl_get_block(K, lastsym, ML_SYM_SIZE(K), symp, "ml_sym");
					return (symp);
				}
				else {
					free_block(symp);
					return((k_ptr_t)NULL);
				}
			}
			lastsym = sym;
		}
		sym += ML_SYM_SIZE(K);
	}
	if (name) {
		free_block((k_ptr_t)strtab);
	}
	free_block(symp);
	return((k_ptr_t)NULL);
}

/*
 * ml_addrtosym()
 *
 * Finds the symbol with the closest address (equal to or less than).
 * Note that an exact match is not necessary. As such, there is no
 * gaurantee that the symbol entry found is the correct one (it's up
 * to the caller to verify this).
 */
k_ptr_t
ml_addrtosym(kaddr_t addr, k_ptr_t mlp)
{
	int mlp_alloced = 0;
	k_ptr_t symp;

	/* If mlp is NULL, find out which module addr is from...
	 */
	if (!mlp) {
		if (!(mlp = addr_to_mlinfo(addr))) {
			return((k_ptr_t)NULL);
		}
		mlp_alloced = 1;
	}
	symp = ml_findsym(addr, (char *)NULL, mlp);
	if (mlp_alloced) {
		free_block(mlp);
	}
	return (symp);
}

/*
 * ml_nmtosym() -- Finds a loadable module symbol that matches for name. 
 */
k_ptr_t
ml_nmtosym(char *name, k_ptr_t mlp)
{
	k_ptr_t symp;

	symp = ml_findsym(0, name, mlp);
	return (symp);
}

/*
 * ml_findaddr()
 *
 * Return a pointer to a symaddr_t struct containing information for 
 * the loadable module that contains addr. With functions, addre
 * must be >= lowpc and <= highpc (non-text symbols must be an exact
 * match). If addr is greater than the first addr in the list AND
 * no match is found, last will be loaded with the pointer to the
 * struct where the new record (for addr) should be inserted (if there
 * is a last pointer).
 */
symaddr_t *
ml_findaddr(kaddr_t addr, symaddr_t **last, int flag)
{
	symaddr_t *lastsp = 0;
	symaddr_t *sp;

	/* Zero out the last pointer
	 */
	if (last) {
		*last = (symaddr_t *)NULL;
	}

	sp = stp->st_ml_addr_ptr;
	while (sp) {
		if (addr < sp->s_lowpc) {
			/* We've gone far enough, it's not here...
			 */
			break;
		}

		/* If the address is from a function, then sp will contain both 
		 * lowpc and highpc values. Otherwise, only the lowpc vlaue will 
		 * be set. With a non-function address, only a match of addr to 
		 * lowpc will result in success.
		 */
		if ((addr <= sp->s_highpc) || (addr == sp->s_lowpc)) {
			return(sp);
		}
		lastsp = sp;
		sp = sp->s_next;
	}
	if (lastsp) {
		if (last) {
			*last = lastsp;
		}
		if (flag) {
			return(lastsp);
		}
	}
	return((symaddr_t *)NULL);
}

/*
 * ml_findname()
 *
 * Return a pointer to a symaddr_t struct containing information for 
 * the loadable module symbol that matches name. If there is no match 
 * match). 
 */
symaddr_t *
ml_findname(char *name)
{
	symaddr_t *sp;

	sp = stp->st_ml_addr_ptr;
	while (sp) {
		if (!strcmp(name, sp->s_name)) {
			return(sp);
		}
		sp = sp->s_next;
	}
	return((symaddr_t *)NULL);
}

/*
 * ml_insertaddr()
 *
 * Insert a record in the symbol table for a loadable module symbol/addr.
 * If a last pointer is passed in, then we insert the new record at that
 * point. Otherwise we have to search the list to find the insert point.
 */
symaddr_t *
ml_insertaddr(kaddr_t lowpc, kaddr_t highpc, char *name, symaddr_t *last)
{
	char *s = 0;
    symaddr_t *newsp;

    /* Before we do anything, we have to see if we have been passed
     * a pointer (last) to an insert point in the list. If we weren't
     * then we have to walk the list and find it. Just in case, we
     * will check to see if we found a record matching lowpc. If we
     * do then we will return a pointer to the record instead.
     */
    if (!last) {
        if (newsp = ml_findaddr(lowpc, &last, 0)) {
            return(newsp);
        }
    }

    /* Allocate permanent space for the new entry
     */
    newsp = (symaddr_t *)alloc_block(sizeof(symaddr_t), B_PERM);
    newsp->s_lowpc = lowpc;
    newsp->s_highpc = highpc;

	/* Check to see if there is a name string. If there isn't, we need
	 * to get it from the symbol table in the ml_info struct. If there is,
	 * we need to check to see if it is the generic mlinfoname. If
	 * it is, then we don't have to dup it...
	 */
	if (!name) {
		k_ptr_t mlp, symp;

		/* Just in case, check to make sure lowpc is from a module.
		 */
		if (!(mlp = addr_to_mlinfo(lowpc))) {
			return((symaddr_t *)NULL);
		}

		if (symp = ml_addrtosym(lowpc, mlp)) {

			/* Make sure the symbol address and the lowpc match
			 */
			if (lowpc == M_ADDR(symp)) {
				s = ml_symname((char*)NULL, symp, mlp);
			}
			if (s) {
				newsp->s_name = strdup(s);
				free_block((k_ptr_t)s);
			}
			else {
				newsp->s_name = mlinfoname;
			}
			free_block(symp);
		}
		else {
			newsp->s_name = mlinfoname;
		}
		free_block(mlp);
	}
	else if (name == mlinfoname) {
		newsp->s_name = mlinfoname;
	}
	else {
		newsp->s_name = strdup(name);
	}

    /* Insert the new record after sp if the new record goes anyware
	 * but the front of the list.
     */
    if (last) {
        newsp->s_next = last->s_next;
        last->s_next = newsp;
    }
    else if (stp->st_ml_addr_ptr) {
        newsp->s_next = stp->st_ml_addr_ptr;
        stp->st_ml_addr_ptr = newsp;
    }
    else {
        stp->st_ml_addr_ptr = newsp;
    }
    stp->st_ml_addr_cnt++;
    return(newsp);
}

/*
 * ml_addrtonm()
 *
 */
char *
ml_addrtonm(kaddr_t addr)
{
	int i, strtabsz;
	char *s, *strtab;
	kaddr_t stringtab;
	k_ptr_t symp, mlp;
	kaddr_t start, end;
	symaddr_t *sp, *lastsp, *newsp;

	/* If addr is already on the list, we don't have to go any
	 * furthur, just return the name.
	 */
	if (sp = ml_findaddr(addr, &lastsp, 0)) {
		return(sp->s_name);
	}

	/* Find out which module addr is from...
	 */
	if (!(mlp = addr_to_mlinfo(addr))) {
		return((char*)NULL);
	}

	/* Treat addr as an instruction and try and get the start address 
	 * of the function. If there isn't a start address, try and find
	 * a sym address...
	 */
	start = ml_funcaddr(addr);
	if (!start) {

		free_block(mlp);

		/* Check and see if there is a (non-text) symbol that 
		 * matches this addr. Since we don't have any way to
		 * determine if the next symbol is REALLY the next symbol,
		 * we only determine success if there is an exact match.
		 */
		if (start = ml_symaddr(addr)) {

			/* This is the closest symbol. Add it to the list. If
			 * (addr == start) then return the symbol's name.
			 */
			sp = ml_insertaddr(start, 0, (char *)NULL, lastsp);
			if (addr == start) {
				return(sp->s_name);
			}
		}
		return((char *)NULL);
	}

	/* Now try for an end address. If we get an end address, make 
	 * sure addr falls between it and the start address. Because we 
	 * are doing this process via brute force, we may walk back to 
	 * the start of the last function and then get ITS function end. 
	 * If the address falls beyond that addresss, then addr is not 
	 * from a function.
	 */
	end = ml_funcend(start);
	if (addr > end) {
		end = 0;
	}
	if (!end) {

		free_block(mlp);

		/* This isn't a function. All we need to do is check to see
		 * if this is a symbol address, log the record, and return 
		 * the name.
		 */
		if (start = ml_symaddr(addr)) {
			if (addr == start) {

				/* Add this symbol to the list and return its name...
				 */
				sp = ml_insertaddr(start, 0, (char *)NULL, lastsp);
				return(sp->s_name);
			}
		}
		return((char *)NULL);
	}

	/* We now know that addr is from a function and that the function
	 * is NOT on the list. We need to determine the name of the function,
	 * add the function to the list, and return the function name.
	 */
	if (KL_INT(K, mlp, "ml_info", "ml_nsyms") == 0) {
		ml_insertaddr(start, end, mlinfoname, lastsp);
		free_block(mlp);
		return((char *)NULL);
	}

	/* Get the stringtab pointer. We have to test for a vlaid
	 * stringtab pointer because there was a case when ml_nsyms
	 * was > 0 and stringtab was NULL.
	 */
	stringtab = kl_kaddr(K, mlp, "ml_info", "ml_stringtab");
	strtabsz = KL_INT(K, mlp, "ml_info", "ml_strtabsz");
	if (!stringtab || (strtabsz == 0)) {
		free_block(mlp);
		return((char *)NULL);
	}

	strtab = (char*)alloc_block(strtabsz, B_TEMP);
	kl_get_block(K, stringtab, strtabsz, strtab, "ml_sym");

	if (symp = ml_addrtosym(addr, mlp)) {
		if (start == M_ADDR(symp)) {
			s = ml_symname(strtab, symp, (k_ptr_t)NULL);

			/* Add the entry to the list and then return the name...
			 */
			newsp = ml_insertaddr(start, end, s, lastsp);
			free_block((k_ptr_t)s);
			free_block((k_ptr_t)strtab);
			free_block(mlp);
			free_block(symp);
			return(newsp->s_name);
		}
		free_block(symp);
	}
	ml_insertaddr(start, end, mlinfoname, lastsp);
	free_block(mlp);
	free_block((k_ptr_t)strtab);
	return(mlinfoname);
}

/*
 * ml_nmtoaddr() -- Return the address of the loadable module symbol 'name'
 */
kaddr_t
ml_nmtoaddr(char *name)
{
	k_ptr_t mlp, symp;
	kaddr_t addr, mlinfo, start, end;
	symaddr_t *sp;

	/* Check to see if we already have an entry that matches for name
	 */
  	if (sp = ml_findname(name)) {
		return(sp->s_lowpc);
	}

	mlp = alloc_block(ML_INFO_SIZE(K), B_TEMP);
	mlinfo = K_MLINFOLIST(K);
	while (mlinfo) {
		kl_get_struct(K, mlinfo, ML_INFO_SIZE(K), mlp, "ml_info");

		/* Make sure there is address space associated with this
		 * module. There is a case with 32-bit systems where base,
		 * text and end are zero, but there are a TON of symbols. The
		 * symbols are the KERNELS symbols. Since we already have these,
		 * there is no need to look through this list...
		 */
		if (!kl_kaddr(K, mlp, "ml_info", "ml_text")) {
			mlinfo = kl_kaddr(K, mlp, "ml_info", "ml_next");
			continue;
		}

		if (symp = ml_nmtosym(name, mlp)) {
			addr = M_ADDR(symp);
			if (!ml_funcaddr(addr)) {
				ml_insertaddr(addr, 0, name, (symaddr_t*)NULL);
			}
			free_block(mlp);
			free_block(symp);
			return(addr);
		}
		mlinfo = kl_kaddr(K, mlp, "ml_info", "ml_next");
	}
	free_block(mlp);
	return((kaddr_t)NULL);
}

/*
 * is_mload() -- is addr from a loadable module?
 */
int 
is_mload(kaddr_t addr)
{
	k_ptr_t mlp;

	if (mlp = addr_to_mlinfo(addr)) {

		/* If there was success, we have to free the block here
		 * or it will get lost.
		 */
		free_block(mlp);
		return(1);
	}
	return(0);
}

/*
 * ml_symaddr()
 *
 * Check to see if addr is from a loadable module address space. If it
 * is, then see if it matches with an entry in the module's symbol table.
 * Note that this operation is VERY crude. Only the start address of a
 * symbol and an offset into the name space is contained in a symbol table
 * entry. It isn't even clear if the symbol is for a function, variable, or
 * whatever.
 */
kaddr_t
ml_symaddr(kaddr_t addr)
{
	int i, nsyms;
	kaddr_t sym, cur_addr, last_addr = 0;
	k_ptr_t mlp, symp;

	/* Check to see if addr is from a loadable module.
	 */
	if (!(mlp = addr_to_mlinfo(addr))) {
		return((kaddr_t)NULL);
	}

	symp = alloc_block(ML_SYM_SIZE(K), B_TEMP);
	sym = kl_kaddr(K, mlp, "ml_info", "ml_symtab");
	nsyms = KL_INT(K, mlp, "ml_info", "ml_nsyms");

	for (i = 0; i < nsyms; i++) {
		kl_get_block(K, sym, ML_SYM_SIZE(K), symp, "ml_sym");
		cur_addr = M_ADDR(symp);
		if (addr < cur_addr) {
			break;
		}
		last_addr = cur_addr;
		sym += ML_SYM_SIZE(K);
	}
	free_block(mlp);
	free_block(symp);
	return (last_addr);
}

/*
 * ml_funcname()
 *
 */
char *
ml_funcname(kaddr_t addr) 
{
	char *name;
	symaddr_t *sp;

    /* Check to see if addr is from a function...
     */
    if (ml_funcaddr(addr)) {
		if (name = ml_addrtonm(addr)) {
			return(name);
		}
    }
	return((char *)NULL);
}

/*
 * ml_funcaddr() 
 *
 * walk back through the code to find the first instruction of a function. 
 * This is really a hack! It's something we have to do because little if 
 * any information is stored in the loadable module symbol table. We 
 * consider the instruction where the stack pointer gets decremented to 
 * be the first instruction of the function (even though, in some cases, 
 * it isn't). That is if there is no symbol table entry. If there is
 * a symbol table entry, we use the address of the symbol as the start
 * address.
 */
kaddr_t 
ml_funcaddr(kaddr_t addr) 
{
	int instr, instr_cnt;
	int i, off, size;
	char *name, *s;
	k_ptr_t symp, mlp, blk, cur;
	kaddr_t start_addr, blk_addr, cur_addr, sym_addr, sym_end, end;
	kaddr_t ml_text;
	symaddr_t *sp, *lastsp;

	/* Check to see if we already have an address record that matches 
	 * for addr.
	 */
  	if (sp = ml_findaddr(addr, &lastsp, 0)) {

		/* Make sure this is a function
		 */
		if (sp->s_highpc) {
			return(sp->s_lowpc);
		}
		else {
			return((kaddr_t)NULL);
		}
	}

	/* We didn't find it, so get the ml_info struct this addr is from.
	 */
	if (!(mlp = addr_to_mlinfo(addr))) {
		return((kaddr_t)NULL);
	}

	/* Get the start of the module's text segment...
	 */
	ml_text = kl_kaddr(K, mlp, "ml_info", "ml_text");
	free_block(mlp);

	/* Make sure addr is aligned properly (should we just return?)
	 */
	if (addr % 4) {
		start_addr = addr & (~3);
	}
	else {
		start_addr = addr;
	}

	blk_addr = start_addr - 4096 + 4;
	if (blk_addr < ml_text) {
		blk_addr = ml_text;
	}
	size = start_addr - blk_addr + 4;
	instr_cnt = size / 4;

	blk = alloc_block(4096, B_TEMP);

	while (start_addr >= ml_text) {
		cur = (k_ptr_t)((unsigned)blk + (size - 4));
		cur_addr = start_addr;
		kl_get_block(K, blk_addr, size, blk, "instr");
		for (i = 0; i < instr_cnt; i++, cur_addr -= 4) {
			off = 0;
			instr = *((int*)cur);
			if (PTRSZ64(K)) {
				int op;

				if (op = (instr & 0xfc000000) >> 26) {
					if ((op == 25) && (((instr & 0x3e00000) >> 21) == 29) &&
								(((instr & 0x1f0000) >> 16) == 29)) {
						off = (short)(instr & 0xffff);
					}
				}
			}
			else {
				if ((instr & 0xffff0000) == 0x27bd0000) {
					off = (short)(instr & 0xffff);
				}
			}

			/* Check to see if the offset is less than zero. If
			 * it is then it means we are at the start of a new 
			 * function (or block of text?).
			 */
			if (off < 0) {

				free_block(blk);

				/* Get the start address of the closest symbol. We will 
				 * compare it with the start address we arrived at to see
				 * if there is a match...
				 */
				sym_addr = ml_symaddr(addr);
				if (!sym_addr) {

					/* If there is no matching symbol, we check to make
					 * sure the address falls within the function starting
					 * at cur_addr.
					 */
					end = ml_funcend(cur_addr);
					if (addr > end) {
						return((kaddr_t)NULL);
					}
					else {
						ml_insertaddr(cur_addr, end, mlinfoname, lastsp);
						return(cur_addr);
					}
				}
				sym_end = ml_funcend(sym_addr);

				/* If there is a gap between the symbol address and the
				 * instruction that decrements the SP, AND addr falls
				 * between the two, cur_add will be from the previous
				 * function. If the symbol found is not a function,
				 * return NULL.
				 */
				if (cur_addr < sym_addr) {
					if (sym_end) {
						ml_insertaddr(sym_addr, sym_end, (char *)NULL, lastsp);
						return(sym_addr);
					}
					return((kaddr_t)NULL);
				}

				/* Now, we have to see if addr falls within the function 
				 * found by ml_symaddr(). It's possible that there is a 
				 * gap in the symbol table (we can count on it!). It's also 
				 * possible that the addr falls beyond the end of the last 
				 * function. In that case, just return NULL.
				 */
				if (addr > sym_end) {
					/* Check and see if cur_addr is a start of a function
					 */
					end = ml_funcend(cur_addr);
					if (addr <= end) {
						ml_insertaddr(cur_addr, end, (char *)NULL, lastsp);
						return(cur_addr);
					}
					return((kaddr_t)NULL);
				}
				ml_insertaddr(sym_addr, sym_end, (char *)NULL, lastsp);
				return(sym_addr);
			}
			cur = (k_ptr_t)((unsigned)cur - 4);
		}

		/* There's a special case that exists when the first symbol
		 * has the same address as ml_text and the first instruction is
		 * not the one where SP is decremented. If we don't handle it
		 * here, we will just loop forever...
		 */
		if (cur_addr < ml_text) {
			kaddr_t taddr;

			cur_addr = addr;
			sym_addr = ml_symaddr(cur_addr);	
			sym_end = ml_funcend(sym_addr);	

			/* We don't know if sym_end is really the end of the function
			 * that starts at sym_addr. We need to walk back, symbol
			 * by symbol, until we have the real symbol end (at least
			 * based on what's in the symbol table).
			 */
			taddr = ml_symaddr(sym_end);
			while (taddr > sym_addr) {
				sym_end = taddr - 4;
				taddr = ml_symaddr(sym_end);
			}

			if ((addr >= sym_addr) && (addr <= sym_end)) {
				free_block(blk);
				ml_insertaddr(sym_addr, sym_end, (char *)NULL, lastsp);
				return(sym_addr);
			}
			else {
				break;
			}
		}

		start_addr = blk_addr;
		blk_addr = start_addr - 4096 + 4;
		if (blk_addr < ml_text) {
			blk_addr = ml_text;
		}
		size = start_addr - blk_addr + 4;
		instr_cnt = size / 4;
	}
	free_block(blk);
	return((kaddr_t)NULL);
}

/*
 * ml_funcend()
 *
 * Determine the end address for a function starting at addr (we have 
 * to have faith that addr is in deed a function start address).
 *
 * This is an mlinfo internal function.
 */
kaddr_t
ml_funcend(kaddr_t addr)
{
	int cnt = 0, instr, instr_cnt;
	int i, off, size, func_size;
	k_ptr_t mlp;
	k_ptr_t blk, cur;
	kaddr_t end_addr, cur_addr, blk_addr, tmp_addr = 0;
	kaddr_t ml_end;
	symaddr_t *sp, *lastsp;

	/* Check to see if we already have an address record that matches 
	 * for addr.
	 */
  	if (sp = ml_findaddr(addr, &lastsp, 0)) {
		return(sp->s_highpc);
	}

	/* Make sure addr is from a loadable module...
	 */
	if (!(mlp = addr_to_mlinfo(addr))) {
		return((kaddr_t)NULL);
	}

	/* Get the end of the text segment. We have to make sure
	 * that it is aligned properly (it isn't always).
	 */
	ml_end = kl_kaddr(K, mlp, "ml_info", "ml_end");
	free_block(mlp);
	if (ml_end % 3) {
		 ml_end = ml_end & (~3);	
	}
	
	/* We have to walk past the instruction that decrements the SP (Note
	 * that this may not be the first instruction of the function).
	 */
	off = 0;
	blk_addr = addr;
	do {
		kl_get_block(K, blk_addr, 4, (char*)&instr, "instr");

		if (PTRSZ64(K)) {
			int op;

			if (op = (instr & 0xfc000000) >> 26) {
				if ((op == 25) && (((instr & 0x3e00000) >> 21) == 29) &&
							(((instr & 0x1f0000) >> 16) == 29)) {
					off = (short)(instr & 0xffff);
				}
			}
		}
		else {
			if ((instr & 0xffff0000) == 0x27bd0000) {
				off = (short)(instr & 0xffff);
			}
		}

		/* We can expect to find the instruction that decrements the SP
		 * fairly soon in the function. If we don't find it in a reasonible
		 * number of searches, we just bail...
		 */
		cnt++;
		if (cnt == 25) {
			return((kaddr_t)NULL);
		}

		/* Step over the current instr in any event
		 */
		blk_addr += 4;
		if (off < 0) {
			break;
		}

	} while (blk_addr < ml_end);

	if ((blk_addr + 4096) > ml_end) {
		size = (ml_end - blk_addr);
	}
	else {
		size = 4096;
	}
	instr_cnt = size / 4;

	blk = alloc_block(4096, B_TEMP);

	while (blk_addr <=  ml_end) {
		kl_get_block(K, blk_addr, size, blk, "instr");
		cur_addr = blk_addr;
		cur = blk;
		for (i = 0; i < instr_cnt; i++, cur_addr += 4) {
			off = 0;
			instr = *((int*)cur);
			if (PTRSZ64(K)) {
				int op;

				if (op = (instr & 0xfc000000) >> 26) {
					if ((op == 25) && (((instr & 0x3e00000) >> 21) == 29) &&
								(((instr & 0x1f0000) >> 16) == 29)) {
						off = (short)(instr & 0xffff);
					}
				}
			}
			else {
				if ((instr & 0xffff0000) == 0x27bd0000) {
					off = (short)(instr & 0xffff);
				}
			}
			/* Check to see if the offset is less than zero. If
			 * it is then we have reached the start of the next
			 * function. We just back up one and figure that as
			 * the end of the function.
			 *
			 * There is a problem when the start address of a 
			 * function comes before the instr where the SP is 
			 * incremented. In effect, this puts our end address 
			 * within the next function!
			 */
			if (off < 0) {
				kaddr_t sym_addr;

				end_addr = cur_addr - 4;
				sym_addr = ml_symaddr(end_addr);
				if (sym_addr > addr) {
					end_addr = sym_addr - 4;
				}
				free_block(blk);
				return(end_addr);
			}
			else if (off > 0) {
				/* We have to capture this in the event that this is the
				 * last function in the module address space. In that
				 * instance, there won't be a next function's decrementing
				 * of the SP.
				 */
				tmp_addr = cur_addr;
			}
			cur = (k_ptr_t)((unsigned)cur + 4);
		}
		if (size == 4096) {
			blk_addr += 4096;
		}
		else {
			/* We've finished walking the code for this loadable module
			 */
			break;
		}

		if ((blk_addr + 4096) > ml_end) {
			size = (ml_end - blk_addr);
		}
		else {
			size = 4096;
		}
		instr_cnt = size / 4;
	}
	free_block(blk);
	return(tmp_addr);
}

/*
 * ml_funcsize()
 */
int
ml_funcsize(kaddr_t addr)
{
	int func_size;
	kaddr_t func_addr, func_end;
	kaddr_t ml_end;

	/* Get the start address of a function..
	 */
	func_addr = ml_funcaddr(addr);
	if (func_addr == (kaddr_t)NULL) {
		return((kaddr_t)NULL);
	}
	func_end = ml_funcend(func_addr);
	if (func_end == (kaddr_t)NULL) {
		return((kaddr_t)NULL);
	}
	func_size = (func_end - func_addr) + 4;
	return(func_size);
}

#ifdef NOT

XXX : This is old stuff that may still be useful. We're going to keep
      it around for a while...

#define CHECK_MLINFOLIST() \
	if (ACTIVE(K)) {\
	} \
	else { \
		if (!K_MLINFOLIST(K)) { \
			if (DEBUG(DC_GLOBAL, 1)) { \
				fprintf(stderr, "No modules loaded.\n"); \
			} \
			return((k_ptr_t)NULL); \
		} \
	}


/* 
 * print_minfo()
 */
print_minfo(i_ml_info_t *ml, int flags, FILE *ofp)
{
	int i;

	if (DEBUG(DC_GLOBAL, 3)) {
		fprintf(stderr, "ml=0x%x\n", ml);
	}
	
	fprintf(ofp, "%4d  %15s  %8x  %5x  %6d\n",
		ml->desc.m_id, ml->desc.m_prefix, ml->mp.ml_text, 
		ml->desc.m_flags, ml->desc.m_refcnt);

	if (flags & DO_FULL) {

		cfg_driver_t *drv = (cfg_driver_t *)NULL;

		if (ml->desc.m_type == M_DRIVER) {
			drv = (cfg_driver_t *)alloc_block(sizeof(cfg_driver_t), B_TEMP);
			if (!get_block(sizeof(cfg_driver_t), ml->desc.m_data, 
						drv, "cfg_driver_t")) {
				free_block(drv);
				return;
			}
			fprintf(ofp, "\n");
			fprintf(ofp, "  DRIVER INFORMATION\n");
			fprintf(ofp, "  ------------------\n");
			for (i = 0; i< drv->d.d_nmajors; i++) {
				fprintf(ofp, "  MAJOR: %3d -- ", drv->d.d_majors[i]);
				if (drv->d_typ == (MDRV_CHAR|MDRV_BLOCK)) {
					fprintf (ofp, "b/cdevsw[%d]\n", drv->d_dev);
				}
				else if (drv->d_typ == MDRV_CHAR) {
					fprintf (ofp, "cdevsw[%d]\n", drv->d_dev);
				}
				else if (drv->d_typ == MDRV_BLOCK) {
					fprintf (ofp, "bdevsw[%d]\n", drv->d_dev);
				}
				else if (drv->d_typ == MDRV_STREAM) {
					fprintf (ofp, "cdevsw[%d])\n", drv->d_dev);
					fprintf (ofp, "    open 0x%x, unload 0x%x\n",
							drv->d_ropen, drv->d_unload);
				} 
				else if (ml->desc.m_type == M_FILESYS) {
					fprintf (ofp, "  Illegal module type: filesys\n");
				} 
				else if (ml->desc.m_type == M_STREAMS) {

					cfg_streams_t *str;

					str = (cfg_streams_t *)
							alloc_block(sizeof(cfg_streams_t), B_TEMP);
					get_block(ml->desc.m_data, sizeof(cfg_streams_t), 
							str, "cfg_streams_t");;
					fprintf(ofp, "\n");
					fprintf(ofp, "  STREAMS MODULE INFO\n");
					fprintf(ofp, "  -------------------\n");
					fprintf(ofp, "    fmodsw entry %d\n", str->s_fmod);
					fprintf(ofp, "    open 0x%x, unload 0x%x, close 0x%x, "
							"info 0x%x\n", str->s_ropen, str->s_unload, 
							str->s_rclose, str->s_info);
				} 
				else if (ml->desc.m_type == M_OTHER) {
					fprintf (ofp, "  Illegal module type: other\n");
				}
			}
		}
		if (drv) {
			free_block(drv);
		}
		fprintf(ofp, "\n");
	}
	return;
}

/* 
 * domlist()
 */
domlist(command_t cmd)
{
	int i, mode, firsttime = 1, module_cnt = 0;
	unsigned value;
	i_ml_info_t *ml;

	CHECK_MLINFOLIST();

	fprintf(cmd.ofp, "  ID           PREFIX      TEXT  FLAGS  REFCNT\n");
	fprintf(cmd.ofp, "============================================================================\n");

	ml = mlinfo;
	for (i = 0; i < cmd.nargs; i++) {
		value = get_value(cmd.args[i], &mode, 0);
		if (mode == 0) {
			mode = 1;
		}

		while (ml) {
			if ((mode == 2) && ((unsigned)ml->m != value)) {
				ml = ml->next;
				continue;
			}
			if ((mode == 2) || (value == ml->desc.m_id)) {
				if (DEBUG(DC_GLOBAL, 1) || (cmd.flags & DO_FULL)) {
					if (!firsttime) {
						fprintf(cmd.ofp, "  ID           PREFIX      TEXT  FLAGS  REFCNT\n");
						fprintf(cmd.ofp, "============================================================================\n");
					}
					else {
						firsttime = 0;
					}
				}
				module_cnt++;
				print_minfo(ml, cmd.flags, cmd.ofp);
				break;
			}
			ml = ml->next;
		}
	}

	if (!cmd.nargs) {
		while (ml) {
			if (DEBUG(DC_GLOBAL, 1) || (cmd.flags & DO_FULL)) {
				if (!firsttime) {
					fprintf(cmd.ofp, "  ID           PREFIX      TEXT  FLAGS  REFCNT\n");
					fprintf(cmd.ofp, "============================================================================\n");
				}
				else {
					firsttime = 0;
				}
			}
			module_cnt++;
			print_minfo(ml, cmd.flags, cmd.ofp);
			ml = ml->next;
		}
	}
	fprintf(cmd.ofp, "============================================================================\n");
	fprintf(cmd.ofp, "%d module%s found\n",
			module_cnt, (module_cnt != 1) ? "s" : "");

}

/* 
 * dominfo()
 */
dominfo(command_t cmd)
{
	int i, mode, module_cnt = 0;
	unsigned value;
	i_ml_info_t *ml;

	CHECK_MLINFOLIST();

	fprintf(cmd.ofp, " ML_INFO       OBJ    SYMTAB  NSYMS  STRINGTAB      DESC      TEXT       END\n");
	fprintf(cmd.ofp, "============================================================================\n");
	for (i = 0; i < cmd.nargs; i++) {
		value = get_value(cmd.args[i], &mode, 0);
		if (mode == 0) {
			mode = 1;
		}
		if (!(ml = get_ml_info(value, mode))) {
			continue;
		}
		module_cnt++;
		print_ml_info(ml, cmd.flags, cmd.ofp);
	}
	if (!cmd.nargs) {
		ml = mlinfo;
		while (ml) {
			module_cnt++;
			print_ml_info(ml, cmd.flags, cmd.ofp);
			ml = ml->next;
		}
	}
	fprintf(cmd.ofp, "============================================================================\n");
	fprintf(cmd.ofp, "%d module%s found\n",
		module_cnt, (module_cnt != 1) ? "s" : "");

}

#define STRSZ 128
/* 
 * domlkup()
 */
domlkup(command_t cmd)
{
	int i, j;
	SYMR *sym;
	i_ml_info_t *ml;
	char *str;

	CHECK_MLINFOLIST();

	fprintf(cmd.ofp, "                NAME     VALUE  TYPE  CLASS\n");
	fprintf(cmd.ofp, "===========================================\n");
	for (i = 0; i < cmd.nargs; i++) {
		ml = mlinfo;
		while (ml) {
			if (sym = ml_find_by_name(cmd.args[i], ml)) {
				str = ml->stringtab + sym->iss;
				print_ml_sym(sym, str, cmd.ofp);
				break;
			}
			ml = ml->next;
		}
	}
	fprintf(cmd.ofp, "===========================================\n");
}

/*
 * domaddr() -- Print information for symbol containing addr
 */
int
domaddr(command_t cmd)
{
	int i, j;
	unsigned addr;
	SYMR *sym;
	i_ml_info_t *ml;
	char *str;

	CHECK_MLINFOLIST();

	fprintf(cmd.ofp, "                NAME     VALUE  TYPE  CLASS\n");
	fprintf(cmd.ofp, "===========================================\n");
	for (i = 0; i < cmd.nargs; i++) {
		addr = GET_VALUE(cmd.args[i]);
		if (!(ml = get_ml_info(addr, 3))) {
			continue;
		}

		if (sym = ml_find_by_addr(addr, ml)) {
			str = ml->stringtab + sym->iss;
			print_ml_sym(sym, str, cmd.ofp);
		}
	}
	fprintf(cmd.ofp, "===========================================\n");
}

/* 
 * domsym() -- Print symbol list for loadable module
 */
int
domsym(command_t cmd)
{
	int i, j, mode;
	unsigned value;
	SYMR *sym;
	i_ml_info_t *ml;
	char *str;

	CHECK_MLINFOLIST();

	for (i = 0; i < cmd.nargs; i++) {

		value = get_value(cmd.args[i], &mode, 0);
		if (mode == 0) {
			mode = 1;
		}
		if (mode == 1) {
			fprintf(cmd.ofp, "SYMBOLS FOR MODULE ID: %d\n\n", value);
		}
		else {
			fprintf(cmd.ofp, "SYMBOLS FOR MODULE : 0x%x\n\n", value);
		}
		fprintf(cmd.ofp, "                NAME     VALUE  TYPE  CLASS\n");
		fprintf(cmd.ofp, "===========================================\n");

		if (!(ml = get_ml_info(value, mode))) {
			continue;
		}
		sym = ml->symtab;

		for (j = 0; j < ml->mp.ml_nsyms; j++, sym++) {
			str = ml->stringtab + sym->iss;
			print_ml_sym(sym, str, cmd.ofp);
		}
		fprintf(cmd.ofp, "===========================================\n");
		fprintf(cmd.ofp, "\n");
	}
}
#endif

