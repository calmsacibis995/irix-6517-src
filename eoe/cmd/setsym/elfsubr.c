/*
 * "elfsubr.c"
 *
 */
#include <libelf.h>
#include <string.h>
#include "elfsubr.h"

int
sym_val64(Elf *elf, char *sym_name, Elf64_Addr *value)
{
	/* this static is here so that if the same file is repeatedly checked,
	   the code isn't repeated */
	static Elf *oldelf = NULL;
	static Elf_Scn *symtab_scn;
	static Elf64_Shdr *symtab_shdr;

	Elf64_Sym *symtab;
	int symtab_size, i;

	/* see if we have correct symtab */
	if (elf != oldelf) {
		symtab_scn = find_section(elf, ".symtab");
		symtab_shdr = elf64_getshdr(symtab_scn);
	}

	symtab = (Elf64_Sym *)(elf_getdata(symtab_scn, NULL)->d_buf);
	symtab_size = (elf_getdata(symtab_scn, NULL)->d_size)
			/ sizeof(Elf64_Sym);

	/* keep looking at entries until the right one is found */
	for(i = 0; i < symtab_size; i++)
		if (strcmp(sym_name,
		    elf_strptr(elf, symtab_shdr->sh_link, symtab[i].st_name))
		    == 0)
			break;

	if (i == symtab_size)
		return 1;

	*value = symtab[i].st_value;
	return 0;
}

int
sym_val32(Elf *elf, char *sym_name, Elf32_Addr *value)
{
	/* this static is here so that if the same file is repeatedly checked,
	   the code isn't repeated */
	static Elf *oldelf = NULL;
	static Elf_Scn *symtab_scn;
	static Elf32_Shdr *symtab_shdr;

	Elf32_Sym *symtab;
	int symtab_size, i;

	/* see if we have correct symtab */
	if (elf != oldelf) {
		symtab_scn = find_section(elf, ".symtab");
		symtab_shdr = elf32_getshdr(symtab_scn);
	}

	symtab = (Elf32_Sym *)(elf_getdata(symtab_scn, NULL)->d_buf);
	symtab_size = (elf_getdata(symtab_scn, NULL)->d_size)
			/ sizeof(Elf32_Sym);

	/* keep looking at entries until the right one is found */
	for(i = 0; i < symtab_size; i++)
		if (strcmp(sym_name,
		    elf_strptr(elf, symtab_shdr->sh_link, symtab[i].st_name))
		    == 0)
			break;

	if (i == symtab_size)
		return 1;

	*value = symtab[i].st_value;
	return 0;
}

int
elf_type(Elf *elf)
{
	/* these statics are here so that if the same file is repeatedly
	   checked, the code isn't repeated */
	static Elf *oldelf = NULL;
	static int oldtype;

	if (elf == oldelf)
		return(oldtype);
	/* otherwise do calculations */
	else
	{
		unsigned char MipsElf32Ident[EI_NIDENT] = {
			0x7f, 'E', 'L', 'F',
			ELFCLASS32, ELFDATA2MSB, EV_CURRENT,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		};
		unsigned char MipsElf64Ident[EI_NIDENT] = {
			0x7f, 'E', 'L', 'F',
			ELFCLASS64, ELFDATA2MSB, EV_CURRENT,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		};
		char *elf_ident;

		oldelf = elf;
		if ((elf_ident = elf_getident(elf, NULL)) == NULL)
			return((oldtype = ELF_NONE));

		if (memcmp(MipsElf32Ident, elf_ident, EI_NIDENT) == 0)
			return((oldtype = ELF_MIPS32));
		else if (memcmp(MipsElf64Ident, elf_ident, EI_NIDENT) == 0)
			return((oldtype = ELF_MIPS64));
		else
			return((oldtype = ELF_UNKNOWN));
	}
}





/*
 * returns section with the name of scn_name, & puts its header in shdr64 or
 * shdr32 based on elf's file type
 *
 */
Elf_Scn *
find_section(Elf *elf, char *scn_name)
{
	Elf64_Ehdr *ehdr64;
	Elf32_Ehdr *ehdr32;
	Elf_Scn *scn = NULL;
	Elf64_Shdr *shdr64;
	Elf32_Shdr *shdr32;

	if (elf_type(elf) == ELF_MIPS64) {
		if ((ehdr64 = elf64_getehdr(elf)) == NULL)
			return(NULL);
		do {
			if ((scn = elf_nextscn(elf, scn)) == NULL)
				break;
			if ((shdr64 = elf64_getshdr(scn)) == NULL)
				return(NULL);
		} while (strcmp(scn_name, elf_strptr(elf, ehdr64->e_shstrndx,
							  shdr64->sh_name)));
	}
	else if (elf_type(elf) == ELF_MIPS32) {
		if ((ehdr32 = elf32_getehdr(elf)) == NULL)
			return(NULL);
		do {
			if ((scn = elf_nextscn(elf, scn)) == NULL)
				break;
			if ((shdr32 = elf32_getshdr(scn)) == NULL)
				return(NULL);
		} while (strcmp(scn_name, elf_strptr(elf, ehdr32->e_shstrndx,
							  shdr32->sh_name)));
	}
	else
		scn = NULL;

	return(scn);	
}

char *
find_sym_offset64(Elf *elf,
	char *symname,
	Elf64_Off *offset,
	Elf_Scn *datascn,
	Elf_Scn *sdatascn,
	Elf_Scn *bssscn)
{
	Elf64_Off data_offset;
	Elf64_Addr data_addr;
	Elf64_Word data_size;
	Elf64_Addr addr;

	data_offset = elf64_getshdr(datascn)->sh_offset;
	data_addr = elf64_getshdr(datascn)->sh_addr;
	data_size = elf64_getshdr(datascn)->sh_size;

	if (sym_val64(elf, symname, &addr) != 0)
		return "symbol not found";
	/* 
	 *	Original code from kevinm
	 * There seems to be a little bit of alignment padding 
	 * between the data and sdata sections in elf32.  That
	 * prevents us from directly seeking to an sdata symbol 
	 * using the data section as a base.  So if the symbol 
	 * shows up within the sdata section, use sdata as the 
	 * base for seeking in the file.
	 */
	if (addr < data_addr || addr >= data_addr + data_size) {
		if (sdatascn) {
			Elf64_Off sdata_offset = elf64_getshdr(sdatascn)->sh_offset;
			Elf64_Addr sdata_addr = elf64_getshdr(sdatascn)->sh_addr;
			Elf64_Word sdata_size = elf64_getshdr(sdatascn)->sh_size;
			if (addr < sdata_addr || addr >= sdata_addr + sdata_size) {
				Elf64_Addr bss_addr = bssscn ? elf64_getshdr(bssscn)->sh_addr : 0;
				Elf64_Word bss_size = bssscn ? elf64_getshdr(bssscn)->sh_size : 0;
			/*
			   Check if the value is in the bss ... if it is and 
			   your setsym'ing the kernel - you need to turn 
			   on the the special flag to make sure that the 
			   variable(s) initialized to zero are placed in the 
			   data section.  Without the flag variables that 
			   are initialized to zero will be placed in the bss.
			 */
				if (addr >= bss_addr && addr <= bss_addr + bss_size)
					return "Initialized variable in bss ";
				else
					return "symbol not in sdata";
			}
			*offset = (addr - sdata_addr) + sdata_offset;
		} else
			return "symbol not in data";
	} else
		*offset = (addr - data_addr) + data_offset;
	return NULL;
}

char *
find_sym_offset32(Elf *elf,
	char *symname,
	Elf32_Off *offset,
	Elf_Scn *datascn,
	Elf_Scn *sdatascn,
	Elf_Scn *bssscn)
{
	Elf32_Off data_offset;
	Elf32_Addr data_addr;
	Elf32_Word data_size;
	Elf32_Addr addr;

	data_offset = elf32_getshdr(datascn)->sh_offset;
	data_addr = elf32_getshdr(datascn)->sh_addr;
	data_size = elf32_getshdr(datascn)->sh_size;

	if (sym_val32(elf, symname, &addr) != 0)
		return "symbol not found";

	if (addr < data_addr || addr >= data_addr + data_size) {
		/* There seems to be a little bit of alignment padding 
		 * between the data and sdata sections in elf32.  That
		 * prevents us from directly seeking to an sdata symbol 
		 * using the data section as a base.  So if the symbol 
		 * shows up within the sdata section, use sdata as the 
		 * base for seeking in the file.
		 */
		if (sdatascn) {
		    Elf32_Off sdata_offset = elf32_getshdr(sdatascn)->sh_offset;
		    Elf32_Addr sdata_addr = elf32_getshdr(sdatascn)->sh_addr;
		    Elf32_Word sdata_size = elf32_getshdr(sdatascn)->sh_size;
		    if (addr < sdata_addr || addr >= sdata_addr + sdata_size)
			return "symbol not in sdata";
		    *offset = (addr - sdata_addr) + sdata_offset;
		} else
		    return "symbol not in data";
	} else 
		*offset = (addr - data_addr) + data_offset;
	return NULL;
}
