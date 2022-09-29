#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libsym/RCS/elf64.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <libelf.h>
#include <libdwarf.h>
#include <dwarf.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "icrash.h"
#include "extern.h"
#include "eval.h"
#include "dwarflib.h"

/* Local variables
 */
Dwarf_Error err;
Dwarf_Die module_die;
Dwarf_Line *linebuf;
Dwarf_Signed linecount;
Dwarf_Arange *aranges_get_info_for_loc;
Dwarf_Signed current_module_arange_count;
Dwarf_Arange current_module_arange_info;
Dwarf_Debug dbg;
dwarftab_t *dtp;

/*
 * get_size64()
 */
static int
get_size64(Dwarf_Die die, Dwarf_Off off)
{
	Dwarf_Half attr;
	Dwarf_Attribute *atlist;
	Dwarf_Signed atcnt;
	Dwarf_Error derr;
	Dwarf_Off cu_off, die_cu_off, die_off;
	int	size = -1, dres;
	int	i, dtype = -1;

	/* Get the Compilation Unit offset of this die 
	 */
	dres = dwarf_die_CU_offset(die,&die_cu_off,&derr);
	if( dres != DW_DLV_OK )
		return -1;

	/* Get the die offset from the beginning of debug section 
	 */
	dres = dwarf_dieoffset(die,&die_off,&derr);
	if( dres != DW_DLV_OK )
		return -1;

	/* This offset is most needed 
	 */
	cu_off = die_off - die_cu_off;

	dres = dwarf_offdie(dbg, cu_off + off, &die, &derr);
	if( dres != DW_DLV_OK )
		return -1;

	/* pull out the items attributes 
	 */
	dres = dwarf_attrlist(die, &atlist, &atcnt, &derr);
	if( dres != DW_DLV_OK )
		return -1;
	
	/* walk down the attribute list 
	 */
	for( i = 0 ; i < atcnt; i++ ) {
		Dwarf_Unsigned bsize;


		/* pull out the attribute 
		 */
		dres = dwarf_whatattr(atlist[i], &attr, &derr);
		if( dres != DW_DLV_OK ) {
			printf("dwarf_whatattr: %d\n",dres);
			return 0;
		}

		if( attr == DW_AT_type ) {
			dtype = i;
			continue;
		}

		if( attr != DW_AT_byte_size )
			continue;


		dres = dwarf_formudata(atlist[i],&bsize,&derr);
		if( dres != DW_DLV_OK )
			bsize = 0;

		size = bsize;
		break;
	}

	if( (i == atcnt) && (dtype != -1) ) {
		Dwarf_Off off;


		dres = dwarf_formref(atlist[dtype],&off,&derr);
		if( dres == DW_DLV_OK )
			size = get_size64(die, off);
	}

	for( i = 0 ; i < atcnt; i++ ) {
		dwarf_dealloc(dbg,atlist[i], DW_DLA_ATTR);
	}
	dwarf_dealloc(dbg,atlist,DW_DLA_LIST);

	return size;
}

/* 
 * dump_attr64()
 */
static void
dump_attr64(Dwarf_Debug dbg, Dwarf_Die die, symdef_t *sdp,
	Dwarf_Attribute dattr)
{
	Dwarf_Half attr;
	Dwarf_Error derr;
	int dres;


	/* pull out the attribute */
	dres = dwarf_whatattr(dattr, &attr, &derr);
	if( dres != DW_DLV_OK ) {
		printf("dwarf_whatattr: %d\n",dres);
		return;
	}

	switch( attr ) {
	case DW_AT_name: {
		char *name;
		
		dres = dwarf_formstring(dattr,&name,&derr);
		if( dres != DW_DLV_OK )
			break;

		sdp->s_name = strdup(name);

		if( dres == DW_DLV_OK )
			dwarf_dealloc(dbg,name,DW_DLA_STRING);
		break;
	}

	case DW_AT_data_member_location:
		sdp->s_offset = dw_loc_list(dattr);
		break;

	case DW_AT_byte_size: {
		Dwarf_Unsigned bsize;

		dres = dwarf_formudata(dattr,&bsize,&derr);
		if( dres != DW_DLV_OK )
			bsize = 0;

		sdp->s_size = bsize;
		break;
	}

	case DW_AT_type: {
		Dwarf_Off off;
		Dwarf_Off type_die_off;

		if( sdp->s_size >= 0 )
			break;

		dres = dwarf_formref(dattr,&off,&derr);
		if( dres != DW_DLV_OK )
			break;

		/* Get the offset to the type die
		 */
		sdp->s_size = get_size64(die, off);
		if (dw_type_off(die, off, &type_die_off) == -1) {
			break;
		}
	}

	} /* switch */
}

/* 
 * dump_die64()
 */
static symdef_t *
dump_die64(Dwarf_Debug dbg, Dwarf_Die die)
{
	Dwarf_Die chld_die;
	Dwarf_Attribute *atlist;
	Dwarf_Signed atcnt;
	Dwarf_Error derr;
	Dwarf_Half dtag;
	Dwarf_Off die_off;
	symdef_t *sdp, *ndp = NULL;
	int dres, i;
	

	/* pull out the tag - see what it is. 
	 */
	dres = dwarf_tag(die, &dtag, &derr);
	if( dres != DW_DLV_OK ) {
		printf("dwarf_tag failed %d\n",dres);
		return NULL;
	}

	switch( dtag ) {
	case DW_TAG_structure_type :
	case DW_TAG_member :
	case DW_TAG_union_type :
		break;

	default:
		return NULL;
	}

	if( (sdp = malloc(sizeof(*sdp))) == NULL )
		return NULL;

	sdp->s_member = sdp->s_next = NULL;
	sdp->s_name = NULL;
	sdp->s_offset = sdp->s_size = -1;
	
	/* Get some extra info on dwarf stuff 
	 */
	dres = dwarf_dieoffset(die, &die_off, &derr);
	sdp->s_die_off = die_off;
	sdp->s_tag = dtag;

	/* pull out the items attributes 
	 */
	dres = dwarf_attrlist(die, &atlist, &atcnt, &derr);
	if( dres != DW_DLV_OK ) {
		printf("dwarf_attrlist: %d\n",dres);
		return NULL;
	}
	
	/* walk down the attribute list 
	 */
	for( i = 0 ; i < atcnt; i++ ) {
		dump_attr64(dbg, die, sdp, atlist[i]);
		dwarf_dealloc(dbg,atlist[i], DW_DLA_ATTR);
	}

	dwarf_dealloc(dbg,atlist,DW_DLA_LIST);

	/* get the children or siblings 
	 */
	if( (dtag == DW_TAG_structure_type) || (dtag == DW_TAG_union_type) )
		dres = dwarf_child(die, &chld_die, &derr);
	else
		dres = dwarf_siblingof(dbg, die, &chld_die, &derr);

	if( (dres == DW_DLV_OK) && (ndp = dump_die64(dbg, chld_die)) )
		sdp->s_member = ndp;

	return sdp;
}

/*
 * sym_defs64()
 */
static void
sym_defs64(Elf *elfp, symtab_t *stp)
{
	int dres, i;
	Dwarf_Error derr;
	Dwarf_Type *dtypebuf;
	Dwarf_Signed dcount;

	dres = dwarf_elf_init(elfp,DW_DLC_READ,NULL,NULL,&dbg,&derr);

	if( dres == DW_DLV_NO_ENTRY ) {
		return;
	}
	else if( dres != DW_DLV_OK ) {
		return;
	}

	/* get the list of the user defined types 
	 */
	dres = dwarf_get_types(dbg, &dtypebuf, &dcount, &derr);
	if( dres == DW_DLV_ERROR ) {
		return;
	}
	else if( dres == DW_DLV_NO_ENTRY ) {
		return;
	}

	for( i = 0 ; i < dcount ; i++ ) {
		Dwarf_Off die_off;
		Dwarf_Die die;
		char *name;
		symdef_t *sdp;

		/* get at the die for this type first get the offset
		 */
		dres = dwarf_type_die_offset(dtypebuf[i], &die_off, &derr);
		if( dres != DW_DLV_OK ) {
			printf("d_type_nm_offs: %d\n",dres);
			continue;
		}

		/* convert the offset to a die 
		 */
		dres = dwarf_offdie(dbg, die_off, &die, &derr);
		if( dres != DW_DLV_OK ) {
			printf("dwarf_die: %d\n",dres);
			continue;
		}

		dres = dwarf_typename(dtypebuf[i], &name, &derr);
		if( dres != DW_DLV_OK ) {
			printf("dwarf_typename failed: %d\n",dres);
			continue;
		}

		dwarf_dealloc(dbg,name,DW_DLA_STRING);

		/* traverse the tree for this die 
		 */
		sdp = dump_die64(dbg, die);

		/* if we don't already have this def, add it to the list 
		 */
		if( sdp && (sym_lkup(stp, sdp->s_name) == NULL) && sdp->s_size) {
			sdp->s_next = stp->st_list;
			stp->st_list = sdp;
		}
		else
			sym_free(sdp);

		dwarf_dealloc(dbg,dtypebuf[i],DW_DLA_TYPENAME);
	}

	dwarf_dealloc(dbg, dtypebuf, DW_DLA_LIST);
}


/*
 * init_funcname64() -- Grab functions and put them on the symaddr_t list
 */
void
init_funcname64(Dwarf_Die newset)
{
	char *str;
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
									dwarf_dealloc(dbg, str, DW_DLA_STRING);
								}
								break;
						}
					}
					dwarf_dealloc(dbg, atlist[i], DW_DLA_ATTR);
				}
			}
			dwarf_dealloc(dbg, atlist, DW_DLA_LIST);

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
				sym_cnt++;
			}
		}

		cldres = dwarf_child(newset, &tmpset, &err);
		if (cldres == DW_DLV_OK) {
			init_funcname64(tmpset);
		}

		sibres = dwarf_siblingof(dbg, newset, &tmpset, &err);
		newset = tmpset;
	}
}

/*
 * sym_funcs64() -- 
 * 
 *   Dump into the symaddr_ptr for a symtab the set of non-external
 *   static functions in the DWRF information.
 */
static void
sym_funcs64(symtab_t *stp)
{
	Dwarf_Die head_die;
	Dwarf_Unsigned cu_header_length = 0, abbrev_offset = 0, next_cu_header = 1;
	Dwarf_Half version_stamp = 0, address_size = 0;
	Dwarf_Signed status = 0, sres = 0;

	/* Loop through all of the compilation units until we find all
	 * of the function (subprogram) headers.
	 */
	for (status = dwarf_next_cu_header(dbg, &cu_header_length,&version_stamp,
			&abbrev_offset, &address_size, &next_cu_header, &err);
		status == DW_DLV_OK;
		status = dwarf_next_cu_header(dbg, &cu_header_length, &version_stamp,
			&abbrev_offset, &address_size, &next_cu_header, &err)) {
				sres = dwarf_siblingof(dbg, 0, &head_die, &err);
				if (sres == DW_DLV_OK) {
					init_funcname64(head_die);
				}
	}
}

/*
 * sym_addrs64()
 */
static void
sym_addrs64(Elf *elfp, symtab_t *stp)
{
	Elf_Scn *esp;
	Elf64_Ehdr *ehdrp;
	Elf64_Shdr *eshdrp;
	Elf_Data *edp;
	int stabno, dstabno;
	Elf64_Sym *sp;
	int i, cnt, ndx; 

	
	/* now pull out the symbol table information 
	 */
	if( (ehdrp = elf64_getehdr(elfp)) == NULL )
		return;

	stabno = dstabno = 0;

	for( i = 0 ; i < ehdrp->e_shnum ; i++ ) {

		/* get a section 
		 */
		if( (esp = elf_getscn(elfp,i)) == NULL ) {
			printf("unable to get section %d\n",i);
			continue;
		}

		if( (eshdrp = elf64_getshdr(esp)) == NULL ) {
			printf("unable to get section header %d\n",i);
			continue;
		}

		if( eshdrp->sh_type == SHT_SYMTAB )
			stabno = i;

		if( eshdrp->sh_type == SHT_DYNSYM )
			dstabno = i;
	}

	if( stabno == 0 )
		stabno = dstabno;

	if( stabno == 0 )
		return;

	/* pull up the symbol table 
	 */
	if( (esp = elf_getscn(elfp,stabno)) == NULL )
		return;

	if( (eshdrp = elf64_getshdr(esp)) == NULL )
		return;

	if( (edp = elf_getdata(esp, NULL)) == NULL )
		return;

	ndx = eshdrp->sh_link;
	cnt = edp->d_size / sizeof(*sp);
	sp = (Elf64_Sym *)edp->d_buf;

	sp++;
	cnt--;

	/* Finally, put them into the main list.
	 */
	for (i = 0; i < cnt; i++, sp++) {
		if (ELF32_ST_TYPE(sp->st_info) != STT_FUNC) {
			slist2->s_next = (symaddr_t *)malloc(sizeof(symaddr_t));
			slist2->s_next->s_lowpc = sp->st_value;
			slist2->s_next->s_highpc = 0;
			slist2->s_next->s_name = strdup(elf_strptr(elfp,ndx,sp->st_name));
			slist2->s_next->s_next = (symaddr_t *)NULL;
			slist2 = slist2->s_next;
			addr_cnt++;
			sym_cnt++;
		}
	}
}

/*
 * sym_defload64()
 */
symtab_t *
sym_defload64(Elf *elfp)
{
	int i;
	symtab_t *stp;

	/* get a return buffer 
	 */
	if( (stp = malloc(sizeof(*stp))) == NULL ) {
		return NULL;
	}

	stp->st_flags = ST_64BIT;
	stp->symaddr_ptr = (symaddr_t *)NULL;
	stp->symfunc_ptr = (symaddr_t *)NULL;

	/* Initialize the top of the symaddr list.
	 */
	sym_cnt = 1;
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

	/* Initialize the definitions, addresses and functions.
	 * The functions and addresses will be on the same list.
	 */
	sym_defs64(elfp,stp);
	sym_addrs64(elfp,stp);
	sym_funcs64(stp);

	/* Now that we have our list, sort it appropriately.
	 */
	sym_sort_list(stp);

	dtp = malloc(sizeof(*dtp));
	bzero(dtp, sizeof(*dtp));
	dw_init_types();
	dw_init_globals();
	dw_init_vars();

	return (stp);
}

/*
 * free_current_module_dwarf_memory64() -- 
 * 
 *   free the dwarf memory objects associated with the current module.
 *   Called when switching modules.
 */
void
free_current_module_dwarf_memory64(void)
{
	if (linebuf != NULL && linecount != 0) {
		int line_to_free;

		/* free previous line number information 
		 */
		for (line_to_free = 0; line_to_free < linecount; line_to_free++) {
			dwarf_dealloc (dbg, linebuf[line_to_free], DW_DLA_LINE);
		}

		dwarf_dealloc(dbg, linebuf, DW_DLA_LIST);
		linecount = 0;
	}
}

/*
 * set_current_module64()
 *
 *   Get cu info for loc. If loc is FROM current module, return NULL,
 *   if an error occurred; return -1; otherwise setup new current
 *   module and return new_cu_die.
 */
set_current_module64(kaddr_t loc)
{
	Dwarf_Arange arange;
	Dwarf_Error error;
	Dwarf_Unsigned cu_offset;
	Dwarf_Die new_cu_die;
	int ares;

	/* Check to see if loc is in current module
	 */
	if (current_module_arange_info != NULL) {
		Dwarf_Addr start;
		Dwarf_Unsigned length;
		Dwarf_Off cu_die_offset;
		int ares;

		ares =  dwarf_get_arange_info (current_module_arange_info,
						  &start, &length, &cu_die_offset, &error);

		if(ares != DW_DLV_OK) {
			return(-1);
		}
		if (loc >= start && loc < start + length) {
			/* We don't have to do anything. We're still in the same 
			 * module as before.
			 */
			return(NULL);
		}
	}
	line_numbers_valid = 0;

	/* first, get a handle on the aranges and find out how many of them
	 * there are 
	 */
	if (aranges_get_info_for_loc == NULL) {
		int gares = dwarf_get_aranges (dbg, &aranges_get_info_for_loc,
				&current_module_arange_count, &error);
		if(gares != DW_DLV_OK) {
			/* no aranges present, can't do much 
			 */
			return(-1);
		}
	}

	/* now try to find an arange that matches the PC given 
	 */
	ares = dwarf_get_arange (aranges_get_info_for_loc,
				   current_module_arange_count, loc, &arange, &error);

	if (ares != DW_DLV_OK) {
		/* no match, can't get line number information for this PC 
		 */
		return(-1);
	}

	ares = dwarf_get_cu_die_offset (arange, &cu_offset, &error);
	if (ares == DW_DLV_ERROR) {
		/* can't get the compilation unit offset, something is amiss 
		 */
		if (DEBUG(K, DC_SYM, 1)) {
			fprintf (errorfp, "set_current_module64: dwarf_get_cu_die_offset: "
				"could not get compilation unit offset! ares=%d, error=%d\n", 
				ares, error);
		}
		return(-1);
	}
	if (ares ==  DW_DLV_NO_ENTRY) {
		/* compilation unit does not exist, no debug info here 
		 */
		return(-1);
	}

	/* get the compilation unit die 
	 */
	ares = dwarf_offdie(dbg, cu_offset, &new_cu_die, &error);
	if(ares != DW_DLV_OK) {
		new_cu_die = 0;
		/* can't read the compilation unit header, something amiss 
		 */
		if (DEBUG(K, DC_SYM, 1)) {
			fprintf (errorfp, "set_current_module64: dwarf_offdie: "
				"could not get compilation unit offset! ares=%d, error=%d\n", 
				ares, error);
		}
		return(-1);
	}

	/* setup the new current module (free old current module resources).
	 */
	current_module_arange_info = arange;
	dwarf_dealloc(dbg, module_die, DW_DLA_DIE);
	free_current_module_dwarf_memory64();
	module_die = new_cu_die;
	return(1);
}

/*
 * get_line64()
 */
Dwarf_Line
get_line64(kaddr_t loc)
{
	Dwarf_Line line;
	Dwarf_Unsigned lineno, what_line;
	int pcres, lres, lires;
	kaddr_t pc;

	if (set_current_module64(loc) == -1) {
		return(NULL);
	}

	/* If loc was not from the current module, setup line number info
	 * for new module.
	 */
	if (line_numbers_valid == 0) {
		what_line = 0;
		lres = dwarf_srclines(module_die, &linebuf, &linecount, &err);
		if (lres != DW_DLV_OK) {
			linecount = 0;
			linebuf = 0;
			return(NULL);
		}
		line_numbers_valid = 1;
	}

	/* Get the line.
	 */
	what_line = 0;
	do {
		if (what_line >= linecount) {
			break;
		}
		line = linebuf[what_line++];
		pcres = dwarf_lineaddr(line, &pc, &err);
		if (pcres == DW_DLV_ERROR) {
			if (DEBUG(K, DC_SYM, 1)) {
				fprintf(errorfp, "get_line64: dwarf_lineaddr: line=0x%xd, "
					"pcres=%d\n", line, pcres);
			}
			return(NULL);
		} 
		else if (pcres == DW_DLV_NO_ENTRY) {
			pc = 0;
		}
		if (pc > loc) {
			line = linebuf[what_line - 2];
		}
	} while (loc > pc);
	return(line);
}

/*
 * get_srcfile64()
 */
char *
get_srcfile64(kaddr_t pc)
{
	char *filename, *src_file;
	Dwarf_Line line;
	int fres;

	/* Get the line (and set the current module)
	 */
	if ((line = get_line64(pc)) == NULL) {
		return(NULL);
	}

	if (set_current_module64(pc) == -1) {
		return(NULL);
	}

	/* Get the filename for the current module
	 */
	fres = dwarf_linesrc(line, &filename, &err);
	if(fres != DW_DLV_OK) {
		return(NULL);
	}
	src_file = (char *)alloc_block(strlen(filename) + 1, B_TEMP);
	strcpy(src_file, filename);
	dwarf_dealloc(dbg, filename, DW_DLA_STRING);

	return(src_file);
}

/*
 * get_lineno64()
 */
int
get_lineno64(kaddr_t pc)
{
	Dwarf_Line line;
	Dwarf_Unsigned lineno;
	int lires;

	/* Get the line.
	 */
	if ((line = get_line64(pc)) == NULL) {
		return(NULL);
	}

	/* Now get the line number.
	 */
	lires = dwarf_lineno(line, &lineno, &err);
	if(lires != DW_DLV_OK) {
		lineno = 0;
	}
	return(lineno);
}
