/*
 * symbol.c - routines for interrogating kernel symbol table
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libelf.h>
#include "prf.h"
#include "elfsubr.h"
#include <fcntl.h>
#include <sym.h>
#include <symconst.h>

/* prototypes */

extern int list_repeated(symaddr_t);

#include <dwarf.h>
#include <libdwarf.h>

static void traverse(Dwarf_Die, void (*)(char *, symaddr_t));
static void process_die(Dwarf_Die, void (*)(char *, symaddr_t));

static Dwarf_Debug dwarf;	/* be passed around recursively in traverse */
static Dwarf_Error derror;	/* dummy var for libdwarf error management  */

int
rdsymtab(int fd, void (*fun)(char *, symaddr_t))
{
	Dwarf_Unsigned cu_offset;
        Dwarf_Unsigned cu_header_length;
        Dwarf_Unsigned abbrev_offset;
        Dwarf_Half version_stamp;
        Dwarf_Half address_size;
	Dwarf_Die first_die;

	if (dwarf_init(fd, DW_DLC_READ, NULL, NULL, &dwarf, &derror)
							!= DW_DLV_OK)
		return 1;

	while (dwarf_next_cu_header(dwarf, &cu_header_length,
					&version_stamp, &abbrev_offset,
					&address_size, &cu_offset,
					&derror) == DW_DLV_OK) {
                if (cu_offset == DW_DLV_BADOFFSET)
			return 1;

		/* process a single compilation unit in .debug_info */
		if (dwarf_siblingof(dwarf, NULL, &first_die, &derror)
							!= DW_DLV_OK)
			return 1;

		traverse(first_die, fun);
        }

	dwarf_finish(dwarf, &derror);
	return 0;
}

static void
traverse(Dwarf_Die die, void (*fun)(char *, symaddr_t))
{
	Dwarf_Die next_die;

	if (die != NULL) {
		process_die(die, fun);
                if (dwarf_child(die, &next_die, &derror) == DW_DLV_OK)
                        traverse(next_die, fun);
                if (dwarf_siblingof(dwarf, die, &next_die, &derror)
                                                        == DW_DLV_OK)
                        traverse(next_die, fun);
		dwarf_dealloc(dwarf, die, DW_DLA_DIE);
	}
}

static void
process_die(Dwarf_Die die, void (*fun)(char *, symaddr_t))
{
	Dwarf_Half tag;
	Dwarf_Bool hasattr;
	Dwarf_Addr addr;
	char *name;

	/* see if the symbol in the die is wanted  & get values */
	if ((dwarf_tag(die, &tag, &derror) != DW_DLV_OK) ||
	    (tag != DW_TAG_subprogram) ||
	    (dwarf_hasattr(die, DW_AT_low_pc,&hasattr,&derror) != DW_DLV_OK) ||
	    (!hasattr))
		return;
	if (dwarf_diename(die, &name, &derror) != DW_DLV_OK)
		name = "";
	if (dwarf_lowpc(die, &addr, &derror) != DW_DLV_OK)
		addr = 0;
	fun(name, addr);
}

int
rdelfsymtab(int fd, void (*fun)(char *, symaddr_t))
{
	Elf *elf;
	int type;

	if (elf_version(EV_CURRENT) == EV_NONE) {
		printf("profiler out of date with respect to current ELF version");
		return 1;
	}


	if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL)
		return 1;

	type = elf_type(elf);

	/*
	 * Scan looking for functions and the special symbol "_etext" which
	 * shows up as a STT_SECTION symbol.  For each function symbol (and
	 * "_etext") pass them to fun() if they don't show up as already
	 * in the symbol table (list_repeated()).
	 */
	if (type == ELF_MIPS64)
	{
	Elf_Scn *symtab_scn = find_section(elf, ".symtab");
	Elf64_Shdr *symtab_shdr = elf64_getshdr(symtab_scn);
	Elf64_Sym *symtab;
	int symtab_size, i;

	symtab = (Elf64_Sym *)(elf_getdata(symtab_scn, NULL)->d_buf);
	symtab_size = (int) ((elf_getdata(symtab_scn, NULL)->d_size)
			     / sizeof(Elf64_Sym));

	for (i = 0; i < symtab_size; i++) {
		__uint64_t addr;
		char *name;

		if (ELF64_ST_TYPE(symtab[i].st_info) != STT_FUNC
		    && ELF64_ST_TYPE(symtab[i].st_info) != STT_SECTION)
			continue;

		addr = symtab[i].st_value;
		if (addr == 0 || list_repeated(addr))
			continue;

		name = elf_strptr(elf, symtab_shdr->sh_link,
				  symtab[i].st_name);
		if (ELF64_ST_TYPE(symtab[i].st_info) == STT_SECTION
		    && strcmp(name, "_etext") != 0)
			continue;

		/* get the wanted information */
		fun(name, addr);
	}
	}
	else if (type == ELF_MIPS32)
	{
	Elf_Scn *symtab_scn = find_section(elf, ".symtab");
	Elf32_Shdr *symtab_shdr = elf32_getshdr(symtab_scn);

	Elf32_Sym *symtab;
	int symtab_size, i;

	symtab = (Elf32_Sym *)(elf_getdata(symtab_scn, NULL)->d_buf);
	symtab_size = (elf_getdata(symtab_scn, NULL)->d_size)
			/ sizeof(Elf32_Sym);

	for(i = 0; i < symtab_size; i++) {
		__uint32_t addr;
		char *name;

		if (ELF32_ST_TYPE(symtab[i].st_info) != STT_FUNC
		    && ELF32_ST_TYPE(symtab[i].st_info) != STT_SECTION)
			continue;

		addr = symtab[i].st_value;
		if (addr == 0 || list_repeated(addr))
			continue;

		name = elf_strptr(elf, symtab_shdr->sh_link,
				  symtab[i].st_name);
		if (ELF32_ST_TYPE(symtab[i].st_info) == STT_SECTION
		    && strcmp(name, "_etext") != 0)
			continue;

		/* get the wanted information */
		fun(name, addr);
	}
	}
	else {
		fprintf(stderr, "unknown elf type for namelist\n");
		exit(EXIT_FAILURE);
	}

	elf_end(elf);

	return 0;
}
