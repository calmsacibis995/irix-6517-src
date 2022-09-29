#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libsym/RCS/symlib.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <libelf.h>
#include <libdwarf.h>
#include <dwarf.h>
#include <nlist.h>
#include <stdlib.h>
#include <string.h>
#include <filehdr.h>
#include <errno.h>
#include <assert.h>
#include "icrash.h"
#include "extern.h"
#include "eval.h"
#include "dwarflib.h"

/* Local variables
 */
int func_cnt, addr_cnt;
char namebuf[BUFSIZ];
symaddr_t *slist, *slist2;
symaddr_t *hash_addrs[SYM_SLOTS];

/* The nmlist[] array contains information about various namelists
 * open for access. The namelist referenced by nmlist[0] is considered
 * the primary namelist. It is used in the setup of the global symtab_s
 * struct. All symbol, function, address, and type, information gets
 * loaded from the primary namelist. The rest of the namelists can be
 * used for accessing type information only. At that, only those types
 * which are not defined in the primary namelist get added to the type
 * tree. All of the Dwarf access routines operate on the current
 * namelist (determined by the contents of the variable curnmlist).
 */
nmlist_t nmlist[MAXNMLIST];
int numnmlist = 0;
int curnmlist;

void
set_curnmlist(int index)
{
	/* Make sure that there is at least one nmlist record. If there
	 * isn't, then BIG errors will occur if we continue...so we don't.
	 */
	assert(numnmlist);
	if ((index >= 0) && (index < numnmlist)) {
		curnmlist = index;
	}
}

/*
 * init nmlist()
 */
int
init_nmlist(char *namelist)
{
	struct filehdr hdr;
	Elf32_Ehdr ehdr;
	Elf *elfp, *elf_open();
	Dwarf_Debug dbg;
	Dwarf_Error derr;

	/* Make sure there is room for this namelist in the table
	 */
	if (numnmlist == (MAXNMLIST - 1)) {
		return(1);
	}

	/* Open up namelist for elf access
	 */
	elfp = elf_open(namelist, &hdr, &ehdr);
	if (elfp) {

		/* Setup for Dwarf access
		 */
		dwarf_elf_init(elfp, DW_DLC_READ, NULL, NULL, &dbg, &derr);
		if (derr) {
			return(1);
		}

		/* Set the current nmlist
		 */
		if (numnmlist) {
			numnmlist++;
		}
		else {
			numnmlist = 1;
		}
		curnmlist = (numnmlist - 1);

		/* Setup the nmlist entry
		 */
		nmlist[curnmlist].index = curnmlist;
		nmlist[curnmlist].ptr = dbg;
		nmlist[curnmlist].elfp = elfp;
		nmlist[curnmlist].namelist = strdup(namelist);
		nmlist[curnmlist].hdr = hdr;
		nmlist[curnmlist].ehdr = ehdr;
		return(0);
	}
	return(1);
}

/*
 * init_symtab() - Initialize the global symtab struct
 */
symtab_t *
init_symtab(nmlist_t *nmlp)
{
	int i;

	if (!nmlp) {
		return((symtab_t *)NULL);
	}

	/* Allocate space for the symbab_s struct
	 */
	if ((stp = malloc(sizeof(*stp))) == NULL) {
		return((symtab_t*)NULL);
	}

	/* Set up the symtab_s struct
	 */
	if (nmlp->ehdr.e_ident[EI_CLASS] == ELFCLASS64) {
		stp->st_flags = ST_64BIT;
	}
	else {
		stp->st_flags = ST_32BIT;
	}

	stp->st_addr_ptr = (symaddr_t *)NULL;
	stp->st_func_ptr = (symaddr_t *)NULL;

	/* Initialize the top of the symaddr list.
	 */
	func_cnt = 0;
	addr_cnt = 0;
	slist = (symaddr_t *)malloc(sizeof(symaddr_t));
	slist->s_highpc = slist->s_lowpc = -1;
	slist->s_name = (char *)NULL;
	slist->s_next = (symaddr_t *)NULL;
	slist2 = slist;

	/* Initialize the hash addresses.
	 */
	for (i = 0; i < SYM_SLOTS; i++) {
		hash_addrs[i] = (symaddr_t *)0;
	}

	/* Get the type, global, and variable information from the
	 * primary namelist
	 */
	init_types(stp);
	init_globals(stp);
	init_vars(stp);

	/* Get addresses and functions. The functions and addresses
	 * end up, sorted, on the same list.
	 */
	sym_addrs(nmlp->elfp, stp);
	sym_funcs(stp);
	sym_sort_list(stp);
	return(stp);
}

/*
 * init_symlib()
 */
symtab_t *
init_symlib(char *namelist)
{
	symtab_t *stp;

	/*
	 * Initialize the primary namelist and then setup the symtab struct
	 */
	if (init_nmlist(namelist)) {
		if (DEBUG(DC_SYM, 1)) {
			fprintf(KL_ERRORFP, "\ninit_symbols: Could not initialize "
				"namelist \"%s\"!\n", namelist);
		}
		return((symtab_t *)NULL);
	}
	if ((stp = init_symtab(&nmlist[curnmlist])) == (symtab_t *)NULL) {
		if (DEBUG(DC_SYM, 1)) {
			fprintf(KL_ERRORFP, "\ninit_symlib: Could not initialize "
				"symtab!\n");
		}
		return((symtab_t *)NULL);
	}
	return(stp);
}

/*
 * init_types()
 */
void
init_types(symtab_t *stp)
{
#ifdef SYM_DEBUG
    fprintf(KL_ERRORFP, "init_types()   : ");
    Start_Timer();
    dw_init_types();
    Stop_Timer(); Print_Timer(KL_ERRORFP);
#else
    dw_init_types();
#endif
}

/*
 * init_globals()
 */
void
init_globals(symtab_t *stp)
{
#ifdef SYM_DEBUG
    fprintf(KL_ERRORFP, "init_globals() : ");
    Start_Timer();
    dw_init_globals();
    Stop_Timer(); Print_Timer(KL_ERRORFP);
#else
    dw_init_globals();
#endif
}

/*
 * init_vars()
 */
void
init_vars(symtab_t *stp)
{
#ifdef SYM_DEBUG
    fprintf(KL_ERRORFP, "init_vars()    : ");
    Start_Timer();
    dw_init_vars();
    Stop_Timer(); Print_Timer(KL_ERRORFP);
#else
    dw_init_vars();
#endif
}

/*
 * init_funcnames() -- Grab functions and put them on the symaddr_t list
 */
void
init_funcnames(Dwarf_Die newset)
{
	char *str;
	Dwarf_Error err;
	Dwarf_Die tmpset;
	Dwarf_Attribute *atlist;
	kaddr_t highpc, lowpc;
	Dwarf_Signed i, atcnt, ares, tres, status, bres;
	Dwarf_Signed sibres = DW_DLV_OK, cldres = DW_DLV_OK;
	Dwarf_Half tag, attr;
	int ext;

	while (sibres == DW_DLV_OK) {
		lowpc = 0;
		highpc = 0;
		tres = dwarf_tag(newset, &tag, &err);
		if (tag == DW_TAG_subprogram) {
			ext = FALSE;
			if (dwarf_attrlist(newset, &atlist, &atcnt, &err) == DW_DLV_OK) {
				for (i = 0; i < atcnt; i++) {
					if (dwarf_whatattr(atlist[i], &attr, &err) == DW_DLV_OK) {
						switch (attr) {
							case DW_AT_external:
								ext = TRUE;
								break;
							case DW_AT_low_pc:
								if (dwarf_formaddr(atlist[i], &lowpc, &err) != 
										DW_DLV_OK) {
									lowpc = 0;
								}
								break;
							case DW_AT_high_pc:
								if (dwarf_formaddr(atlist[i], &highpc, &err) != 
										DW_DLV_OK) {
									highpc = 0;
								}
								break;
							case DW_AT_name:
								if (dwarf_formstring(atlist[i], &str, &err) == 
										DW_DLV_OK) {
									sprintf(namebuf, "%s", str);
									dwarf_dealloc(DBG, str, DW_DLA_STRING);
								}
								break;
						}
					}
					dwarf_dealloc(DBG, atlist[i], DW_DLA_ATTR);
				}
			}
			dwarf_dealloc(DBG, atlist, DW_DLA_LIST);

			if ((lowpc) || (highpc)) {
				slist2->s_next = (symaddr_t *)malloc(sizeof(symaddr_t));
				slist2->s_next->s_lowpc = lowpc;
				if (highpc != 0) {
					slist2->s_next->s_highpc = highpc - sizeof(Elf32_Addr);
				}
				slist2->s_next->s_name = (char *)malloc(strlen(namebuf) + 1);
				sprintf(slist2->s_next->s_name, "%s", namebuf);
				slist2->s_next->s_next = (symaddr_t *)NULL;
				slist2 = slist2->s_next;
				func_cnt++;
			}
		}

		cldres = dwarf_child(newset, &tmpset, &err);
		if (cldres == DW_DLV_OK) {
			init_funcnames(tmpset);
		}

		sibres = dwarf_siblingof(DBG, newset, &tmpset, &err);
		newset = tmpset;
	}
}

/*
 * sym_funcs() -- 
 * 
 *   Dump into the symaddr_ptr for a symtab the set of non-external
 *   static functions in the DWRF information.
 */
void
sym_funcs(symtab_t *stp)
{
	Dwarf_Error err;
	Dwarf_Die head_die;
	Dwarf_Unsigned cu_header_length = 0, abbrev_offset = 0, next_cu_header = 1;
	Dwarf_Half version_stamp = 0, address_size = 0;
	Dwarf_Signed status = 0, sres = 0;

	/* Loop through all of the compilation units until we find all
	 * of the function (subprogram) headers.
	 */
	for (status = dwarf_next_cu_header(DBG, &cu_header_length,&version_stamp,
			&abbrev_offset, &address_size, &next_cu_header, &err);
		status == DW_DLV_OK;
		status = dwarf_next_cu_header(DBG, &cu_header_length, &version_stamp,
			&abbrev_offset, &address_size, &next_cu_header, &err)) {
				sres = dwarf_siblingof(DBG, 0, &head_die, &err);
				if (sres == DW_DLV_OK) {
					init_funcnames(head_die);
				}
	}
}

/*
 * sym_addrs() -- Set locations of symbol addresses for all kernel types.
 */
void
sym_addrs(Elf *elfp, symtab_t *stp)
{
	Elf_Scn *esp;
	Elf32_Ehdr *ehdrp32;
	Elf32_Shdr *eshdrp32;
	Elf64_Ehdr *ehdrp64;
	Elf64_Shdr *eshdrp64;
	Elf_Data *edp;
	int stabno, dstabno;
	Elf64_Sym *sp64;
	Elf32_Sym *sp32;
	int i, cnt, ndx; 

	
	stabno = dstabno = 0;

	/* now pull out the symbol table information 
	 */
	if (stp->st_flags == ST_64BIT) {
		if ((ehdrp64 = elf64_getehdr(elfp)) == NULL) {
			return;
		}
		for (i = 0 ; i < ehdrp64->e_shnum ; i++) {

			/* get a section 
			 */
			if ((esp = elf_getscn(elfp,i)) == NULL) {
				printf("unable to get section %d\n",i);
				continue;
			}

			if ((eshdrp64 = elf64_getshdr(esp)) == NULL) {
				printf("unable to get section header %d\n",i);
				continue;
			}

			if (eshdrp64->sh_type == SHT_SYMTAB) {
				stabno = i;
			}

			if (eshdrp64->sh_type == SHT_DYNSYM) {
				dstabno = i;
			}
		}
	}
	else {
		if ((ehdrp32 = elf32_getehdr(elfp)) == NULL) {
			return;
		}
		for (i = 0 ; i < ehdrp32->e_shnum ; i++) {

			/* get a section 
			 */
			if ((esp = elf_getscn(elfp,i)) == NULL) {
				printf("unable to get section %d\n",i);
				continue;
			}

			if ((eshdrp32 = elf32_getshdr(esp)) == NULL) {
				printf("unable to get section header %d\n",i);
				continue;
			}

			if (eshdrp32->sh_type == SHT_SYMTAB) {
				stabno = i;
			}

			if (eshdrp32->sh_type == SHT_DYNSYM) {
				dstabno = i;
			}
		}
	}

	if (!stabno) {
		stabno = dstabno;
	}

	if (!stabno) {
		return;
	}

	/* pull up the symbol table 
	 */
	if ((esp = elf_getscn(elfp, stabno)) == NULL) {
		return;
	}

	if (stp->st_flags == ST_64BIT) {
		if ((eshdrp64 = elf64_getshdr(esp)) == NULL) {
			return;
		}
	}
	else {
		if ((eshdrp32 = elf32_getshdr(esp)) == NULL) {
			return;
		}
	}

	if ((edp = elf_getdata(esp, NULL)) == NULL) {
		return;
	}

	if (stp->st_flags == ST_64BIT) {
		ndx = eshdrp64->sh_link;
		cnt = edp->d_size / sizeof(*sp64);
		sp64 = (Elf64_Sym *)edp->d_buf;
		sp64++;
	}
	else {
		ndx = eshdrp32->sh_link;
		cnt = edp->d_size / sizeof(*sp32);
		sp32 = (Elf32_Sym *)edp->d_buf;
		sp32++;
	}

	cnt--;

	/* Finally, put them into the main list.
	 */
	if (stp->st_flags == ST_64BIT) {
		for (i = 0; i < cnt; i++, sp64++) {
			if (ELF32_ST_TYPE(sp64->st_info) != STT_FUNC) {
				slist2->s_next = (symaddr_t *)malloc(sizeof(symaddr_t));
				slist2->s_next->s_lowpc = sp64->st_value;
				slist2->s_next->s_highpc = 0;
				slist2->s_next->s_name =
					strdup(elf_strptr(elfp, ndx, sp64->st_name));
				slist2->s_next->s_next = (symaddr_t *)NULL;
				slist2 = slist2->s_next;
				addr_cnt++;
			}
		}
	}
	else {
		for (i = 0; i < cnt; i++, sp32++) {
			if (ELF32_ST_TYPE(sp32->st_info) != STT_FUNC) {
				slist2->s_next = (symaddr_t *)malloc(sizeof(symaddr_t));
				slist2->s_next->s_lowpc = sp32->st_value;
				slist2->s_next->s_highpc = 0;
				slist2->s_next->s_name =
					strdup(elf_strptr(elfp, ndx, sp32->st_name));
				slist2->s_next->s_next = (symaddr_t *)NULL;
				slist2 = slist2->s_next;
				addr_cnt++;
			}
		}
	}
}

/*
 * sym_populate_type()
 */
int 
sym_populate_type(symdef_t *sdp)
{
	return(dw_populate_type((void*)sdp));
}

/*
 * sym_cmp32() -- Compare symbol pointers for a 32-bit system.
 */
signed int
sym_cmp32(symaddr_t *a1, symaddr_t *a2)
{
	if (a1->s_lowpc >= a2->s_lowpc) {
		return(1);
	}
	return(-1);
}

/*
 * sym_cmp64() -- Compare symbol pointers for a 64-bit system.
 */
signed int
sym_cmp64(symaddr_t *a1, symaddr_t *a2)
{
	return((signed int)(a1->s_lowpc - a2->s_lowpc));
}

/*
 * sym_hash() -- Hash on the symbol's name.
 */
int
sym_hash(char *name)
{
	if (!name) {
		return(-1);
	}
	switch (strlen(name)) {
		case 1:
			return(name[0] % SYM_SLOTS);
		case 2:
			return((name[0] * name[1]) % SYM_SLOTS);
		default:
			return((name[0] + name[1] * name[2]) % SYM_SLOTS);
	}
}

/*
 * sym_insert_hash()
 */
void
sym_insert_hash(symaddr_t *ptr)
{
	int i;

	if ((i = sym_hash(ptr->s_name)) >= 0) {
		if (hash_addrs[i] != (symaddr_t *)NULL) {
			ptr->s_next = hash_addrs[i];
		}
		hash_addrs[i] = ptr;
	}
}

/*
 * sym_sort_list() -- Sort the list of symaddr_t items.
 */
void
sym_sort_list(symtab_t *stp)
{
	int sptr_cnt = 0, sptr2_cnt = 0, j, symsize = sizeof(symaddr_t);
	symaddr_t *sp, *sptr, *sptr2;

	slist2 = slist->s_next;
	sptr  = (symaddr_t *)calloc(addr_cnt, symsize);
	sptr2 = (symaddr_t *)calloc(func_cnt, symsize);
	while (slist2->s_next != (symaddr_t *)NULL) {
		if (!slist2->s_highpc) {
			sptr[sptr_cnt].s_lowpc = slist2->s_lowpc;
			sptr[sptr_cnt].s_highpc = slist2->s_highpc;
			sptr[sptr_cnt].s_name = strdup(slist2->s_name);
			sptr[sptr_cnt].s_next = (symaddr_t *)NULL;
			sptr_cnt++;
		}
		else {
			sptr2[sptr2_cnt].s_lowpc = slist2->s_lowpc;
			sptr2[sptr2_cnt].s_highpc = slist2->s_highpc;
			sptr2[sptr2_cnt].s_name = strdup(slist2->s_name);
			sptr2[sptr2_cnt].s_next = (symaddr_t *)NULL;
			sptr2_cnt++;
		}
		free(slist2->s_name);
		sp = slist2;
		slist2 = slist2->s_next;
		free(sp);
	}

	free(slist);

	/* Sort the address and function lists.
	 */
	if (nmlist[curnmlist].ehdr.e_ident[EI_CLASS] == ELFCLASS64) {
		qsort(sptr,  sptr_cnt,  symsize, (int (*)())sym_cmp64);
		qsort(sptr2, sptr2_cnt, symsize, (int (*)())sym_cmp64);
	}
	else {
		qsort(sptr,  sptr_cnt,  symsize, (int (*)())sym_cmp32);
		qsort(sptr2, sptr2_cnt, symsize, (int (*)())sym_cmp32);
	}

	for (j = 0; j < sptr_cnt; j++) {
		sym_insert_hash(&(sptr[j]));
	}
	for (j = 0; j < sptr2_cnt; j++) {
		if (j + 1 < sptr2_cnt) {
			sptr2[j].s_highpc = sptr2[j+1].s_lowpc - sizeof(Elf32_Addr);
		}
		sym_insert_hash(&(sptr2[j]));
	}
	stp->st_addr_ptr = sptr;
	stp->st_addr_cnt = sptr_cnt;
	stp->st_func_ptr = sptr2;
	stp->st_func_cnt = sptr2_cnt;
}

/*
 * sym_debug_print() -- Print out all symbols
 */
void
sym_debug_print()
{
	int j;
	FILE *testfp;

	testfp = fopen("/usr/tmp/sym.debug.test", "w");
	if (stp->st_addr_cnt) {
		fprintf(testfp, "ADDRESSES:\n");
		for (j = 0; j < stp->st_addr_cnt; j++) {
			fprintf(testfp,
				"\tLOWPC: 0x%llx, HIGHPC: 0x%llx, NAME: %s\n",
				stp->st_addr_ptr[j].s_lowpc, stp->st_addr_ptr[j].s_highpc,
				stp->st_addr_ptr[j].s_name);
		}
	}
	if (stp->st_func_cnt) {
		fprintf(testfp, "FUNCTIONS:\n");
		for (j = 0; j < stp->st_func_cnt; j++) {
			fprintf(testfp,
				"\tLOWPC: 0x%llx, HIGHPC: 0x%llx, NAME: %s\n",
				stp->st_func_ptr[j].s_lowpc, stp->st_func_ptr[j].s_highpc,
				stp->st_func_ptr[j].s_name);
		}
	}
	fclose(testfp);
}

/*
 * sym_lkup()
 */
symdef_t *
sym_lkup(symtab_t *stp, char *name)
{
	int max_depth = 0;
	symdef_t *nsdp;

	kl_reset_error();

	/* Search for name in the types list
	 */
	if (DEBUG(DC_SYM, 3)) {
		nsdp = (symdef_t *)find_btnode((btnode_t *)stp->st_type_ptr, 
						name, &max_depth);
		fprintf(KL_ERRORFP, "sym_lkup: typelist: name=%s (%s), max_count=%d\n", 
			name, (nsdp ? "FOUND" : "NOT FOUND"), max_depth);
	}
	else {
		nsdp = (symdef_t *)find_btnode((btnode_t *)stp->st_type_ptr, 
						name, (int *)NULL);
	}

	/* If we didn't find it in the type list, search for name 
	 * in the variables list
	 */
	if (!nsdp) {
		if (DEBUG(DC_SYM, 3)) {
			max_depth = 0;
			nsdp = (symdef_t *)find_btnode((btnode_t *)stp->st_var_ptr, 
						name, &max_depth);
			fprintf(KL_ERRORFP, "sym_lkup: varlist: name=%s (%s), "
				"max_count=%d\n", name, (nsdp ? "FOUND" : "NOT FOUND"), 
				max_depth);
		}
		else {
			nsdp = (symdef_t *)find_btnode((btnode_t *)stp->st_var_ptr, 
						name, (int *)NULL);
		}
	}
	if (!nsdp) {
		KL_SET_ERROR_CVAL(KLE_BAD_SYMNAME, name);
		return((symdef_t *)NULL);
	}

	/* If this is a structure or union type, then we have to make
	 * sure that the member information gets filled in.
	 */
	if (((nsdp->sd_type == SYM_STRUCT_UNION) 
				|| (nsdp->sd_type == SYM_ENUMERATION)) && !nsdp->sd_member) {
		sym_populate_type(nsdp);
	}
	return(nsdp);
}

/*
 * sym_loopfunc()
 */
static symaddr_t *
sym_loopfunc(int i, int c, kaddr_t addr)
{
	symaddr_t *sp = stp->st_func_ptr, *sp2;

	sp2 = &sp[i - 1];
	if ((addr >= sp2->s_lowpc) && (addr <= sp2->s_highpc)) {
		return(sp2);
	}
	else if (c == 1) {
		return((symaddr_t *)NULL);
	}
	else if (addr < sp2->s_lowpc) {
		return(sym_loopfunc(i - c / 2, (c / 2) + (c % 2), addr));
	}
	else {
		return(sym_loopfunc(i + c / 2, (c / 2) + (c % 2), addr));
	}
}

/*
 * sym_loopaddr()
 */
static symaddr_t *
sym_loopaddr(int i, int c, kaddr_t addr)
{
	symaddr_t *sp = stp->st_addr_ptr, *sp2;

	sp2 = &sp[i - 1];
	if (addr == sp2->s_lowpc) {
			return(sp2);
	}
	else if (c == 1) {
		return((symaddr_t *)NULL);
	}
	else if (addr > sp2->s_lowpc) {
		return(sym_loopaddr(i + c / 2, (c / 2) + (c % 2), addr));
	}
	else {
		return(sym_loopaddr(i - c / 2, (c / 2) + (c % 2), addr));
	}
}

/*
 * sym_nmtoaddr()
 */
symaddr_t *
sym_nmtoaddr(symtab_t *stp, char *name)
{
	int i;
	symaddr_t *sp;

	kl_reset_error();

	if ((i = sym_hash(name)) >= 0) {
		sp = hash_addrs[i];
		while (sp) {
			if ((sp->s_name) && (!strcmp(sp->s_name, name))) {
				return(sp);
			}
			sp = sp->s_next;
		}
	}

	/* If we get here it means that the address was not found in the
	 * address hash table. So, we need to check the symbols for loadable
	 * modules to see if there is a match there. 
	 */
	sp = ml_findname(name);
	if (!sp) {
		/* If we STILL haven't found the name, it may be that it hasn't
		 * been added to the mload addr list yet. Try that as a last 
		 * resort (this MAY take a while).
		 */
		if (ml_nmtoaddr(name)) {
			sp = ml_findname(name);
		}
	}
	if (!sp) {
		KL_SET_ERROR_CVAL(KLE_BAD_SYMNAME, name);
	}
	return(sp);
}

/*
 * sym_addrtonm()
 */
symaddr_t *
sym_addrtonm(symtab_t *stp, kaddr_t addr)
{
	symaddr_t *sp;

	kl_reset_error();

	if (addr == (kaddr_t)0) {
		KL_SET_ERROR_NVAL(KLE_BAD_SYMADDR, 0, 2);
		return((symaddr_t *)NULL);
	}

	/*
	 * Loop around and look for a function.
	 */
	sp = sym_loopfunc((stp->st_func_cnt / 2) + (stp->st_func_cnt % 2),
				(stp->st_func_cnt / 2) + (stp->st_func_cnt % 2), addr);
	if (!sp) {
		sp = sym_loopaddr((stp->st_addr_cnt / 2) + (stp->st_addr_cnt % 2),
				(stp->st_addr_cnt / 2) + (stp->st_addr_cnt % 2), addr);
	}
	if (!sp) {
		sp = ml_findaddr(addr, (symaddr_t**)NULL, 0);
		if (!sp) {

			/* If we STILL haven't found the symbol, it may be that it 
			 * hasn't been added to the mload addr list yet. Try this as 
			 * a last resort (it MAY take a while).
			 */
			(void)ml_addrtonm(addr);
			sp = ml_findaddr(addr, (symaddr_t**)NULL, 1);
		}
	}
	if (!sp) {
		KL_SET_ERROR_NVAL(KLE_BAD_SYMADDR, addr, 2);
	}
	return(sp);
}

/*
 * STRUCT()
 */
int
STRUCT(char *a)
{
	return(dw_struct(a));
}

/*
 * FIELD()
 */
int
FIELD(char *a, char *b)
{
	return(dw_field(a, b));
}

/*
 * IS_FIELD() -- Return 1 if b is field of a, else 0.
 */
int
IS_FIELD(char *a, char *b)
{
	return(dw_is_field(a, b));
}

/*
 * FIELD_SZ()
 */
int
FIELD_SZ(char *a, char *b)
{
	return(dw_field_sz(a, b));
}

/*
 * get_field()
 */
symdef_t *
get_field(char *a, char *b)
{
	symdef_t *sp;

	sp = (symdef_t*)dw_get_field(a, b);
	return(sp);
}

/*
 * base_value()
 */
int 
base_value(k_ptr_t ptr, char *a, char *b, k_uint_t *value)
{
	int ret;
	k_uint_t v;
	symdef_t *sp;
	k_ptr_t p;

	kl_reset_error();

	if (DEBUG(DC_FUNCTRACE, 5)) {
		fprintf(KL_ERRORFP, "base_value: ptr=0x%x, a=0x%x, b=0x%x, "
			"value=0x%x\n", ptr, a, b, value);
	}

	if (!(sp = get_field(a, b))) {
		KL_SET_ERROR_CVAL(KLE_BAD_FIELD, b);
		return(-1);
	}

	/* Adjust the pointer to the specified field in the struct.
	 */
	p = (k_ptr_t)((unsigned)ptr + FIELD(a, b));
	if ((ret = dw_base_value(p, (dw_type_t *)sp, &v)) < 0) {
		KL_ERROR |= KLE_BAD_BASE_VALUE;
	}
	else if (value) {
		*value = v;
	}
	return(ret);
}

/*
 * get_funcname() -- Get the function name from a specific address.
 *               
 *  Note that the function name returned does NOT need to be freed
 *  up by the caller.
 */
char *
get_funcname(kaddr_t pc)
{
	char *name;
	symaddr_t *sp;
	char *ml_funcname();

	kl_reset_error();

	if (sp = sym_addrtonm(stp, pc)) {

		/* Just in case, make sure there is a highpc (if highpc is
		 * equal to zero then this symbol is not a function).
		 */
		if (sp->s_highpc == 0) {
			return((char *)NULL);
		}
	}
	else if (name = ml_funcname(pc)) {
		return(name);
	}
	else {
		KL_SET_ERROR_NVAL(KLE_BAD_FUNCADDR, pc, 2);
		return((char *)NULL);
	}
	return(sp->s_name);
}

/*
 * get_funcaddr() -- Get the start address for function address is from.
 */
kaddr_t
get_funcaddr(kaddr_t pc)
{
	symaddr_t *sp;
	kaddr_t addr;

	kl_reset_error();

	if (sp = sym_addrtonm(stp, pc)) {

		/* Just in case, make sure there is a highpc (if highpc is
		 * equal to zero then this symbol is not a function).
		 */
		if (sp->s_highpc) {
			return(sp->s_lowpc);
		}
	}
	else if (addr = ml_funcaddr(pc)) {
		return (addr);
	}
	KL_SET_ERROR_NVAL(KLE_BAD_FUNCADDR, pc, 2);
	return((kaddr_t)NULL);
}

/*
 * get_funcsize() -- Get the size of a function.
 */
int
get_funcsize(kaddr_t pc)
{
	int size;
	symaddr_t *sp;

	kl_reset_error();

	if (sp = sym_addrtonm(stp, pc)) {
		/* Just in case, make sure there is a highpc (if highpc is
		 * equal to zero then this symbol is not a function).
		 */
		if (sp->s_highpc == 0) {
			return(0);
		}
	}
	else if (size = ml_funcsize(pc)) {
		return(size);
	}
	else {
		KL_SET_ERROR_NVAL(KLE_BAD_FUNCADDR, pc, 2);
		return(-1);
	}
	return(sp->s_highpc - sp->s_lowpc + 4);
}

/*
 * get_srcfile()
 */
char *
get_srcfile(kaddr_t pc)
{
	return((char *)dw_get_srcfile(pc));
}

/*
 * get_lineno()
 */
int
get_lineno(kaddr_t pc)
{
	return((int)dw_get_lineno(pc));
}

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
		klib_free_block(K, (k_ptr_t)sp->n_name);
	}
	klib_free_block(K, (k_ptr_t)sp);
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

	if (DEBUG(DC_FUNCTRACE, 4)) {
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
		sp = (struct syment *)
			klib_alloc_block(K, sizeof(struct syment), B_PERM);
		sp->n_name = (char *)klib_alloc_block(K, (strlen(sym) + 1), B_PERM);
	}
	else {
		sp = (struct syment *)
			klib_alloc_block(K, sizeof(struct syment), B_TEMP);
		sp->n_name = (char *)klib_alloc_block(K, (strlen(sym) + 1), B_TEMP);
	}

	if (PTRSZ64(K)) {
		nl64 = (struct nlist64 *)
			klib_alloc_block(K, 2*sizeof(struct nlist64), B_TEMP);
		nl64->n_name = sym;
	} 
	else {
		nl32 = (struct nlist *)klib_alloc_block(K, 2 * 
				sizeof(struct nlist), B_TEMP);
		nl32->n_name = sym;
	}

	if (taddr = sym_nmtoaddr(stp, sym)) {
		sprintf(sp->n_name, "%s", taddr->s_name);
		sp->n_value = taddr->s_lowpc;
		sp->n_type = 0;
		if (PTRSZ64(K)) {
			klib_free_block(K, (k_ptr_t)nl64);
		} 
		else {
			klib_free_block(K, (k_ptr_t)nl32);
		}
		if (DEBUG(DC_SYM, 3)) {
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
			klib_free_block(K, (k_ptr_t)nl64);
			if (DEBUG(DC_SYM, 1)) {
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
			klib_free_block(K, (k_ptr_t)nl32);
			if (DEBUG(DC_SYM, 1)) {
				fprintf(KL_ERRORFP, "get_sym: returning name=%s, value=%llx\n",
					sp->n_name, sp->n_value);
			}
			return (sp);
	}

	if (PTRSZ64(K)) {
		klib_free_block(K, (k_ptr_t)nl64);
	} 
	else {
		klib_free_block(K, (k_ptr_t)nl32);
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
