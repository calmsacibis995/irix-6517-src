#ident "$Revision: 1.4 $"

#include "lboot.h"
#include <libelf.h>

static void
elf32_scan(char *, void (*fun)(char *, void *, void *, void *),
	   void *, void *, void *, Elf *, Elf32_Ehdr *);
static void
elf64_scan(char *, void (*fun)(char *, void *, void *, void *),
	   void *, void *, void *, Elf *, Elf64_Ehdr *);

/*
 * symbol_scan
 *
 * For each global procedure symbol found in the archive or ELF object 'fname',
 * call 'fun' with the name of the symbol, 'arg0', 'arg1', and 'arg2'.
 */
void
symbol_scan(char *fname, void (*fun)(char *, void *, void *, void *),
	    void *arg0, void *arg1, void *arg2)
{
	Elf *arf, *elf;
	Elf_Cmd cmd;
	Elf32_Ehdr *e32hdr;
	Elf64_Ehdr *e64hdr;
	int fd;

	if (elf_version(EV_CURRENT) == EV_NONE) {
		fprintf(stderr,
		    "lboot out of date with respect to current ELF version\n");
		return;
	}
	if ((fd=open(fname,O_RDONLY)) == -1) {
		fprintf(stderr, "could not open %s/%s\n",
				slash_boot, fname);
		return;
	}
	cmd = ELF_C_READ;
	arf = elf_begin(fd, cmd, (Elf *)NULL);
	while ((elf = elf_begin(fd, cmd, arf)) != NULL) {
		if ((e32hdr = elf32_getehdr(elf)) != NULL) {
			elf32_scan(fname, fun, arg0, arg1, arg2, elf, e32hdr);
		} else if ((e64hdr = elf64_getehdr(elf)) != NULL) {
			elf64_scan(fname, fun, arg0, arg1, arg2, elf, e64hdr);
		}
		cmd = elf_next(elf);
		elf_end(elf);
	}
	elf_end(arf);
	close (fd);
}

static void
elf32_scan(char *fname, void (*fun)(char *, void *, void *, void *),
	    void *arg0, void *arg1, void *arg2, Elf *elf, Elf32_Ehdr *e32hdr)
{
	Elf_Scn *stringscn, *symscn;
	Elf32_Shdr *stringshdr, *symshdr;
	Elf_Data *stringdata, *symdata;
	Elf32_Sym *sym;
	int i;
	unsigned long count;
	char *procname;

	if (((stringscn = elf_getscn(elf, e32hdr->e_shstrndx)) == NULL) ||
	    ((stringshdr = elf32_getshdr(stringscn)) == NULL) ||
	    (stringshdr->sh_type != SHT_STRTAB)) {
		fprintf(stderr, "cannot get string header from %s/%s\n",
				slash_boot, fname);
		return;
	}
	stringdata = (Elf_Data *)NULL;
	if (((stringdata = elf_getdata(stringscn, stringdata)) == NULL) ||
	    (stringdata->d_size == 0)) {
		fprintf(stderr, "no data in string table of %s/%s\n",
				slash_boot, fname);
		return;
	}
	symscn = (Elf_Scn *)NULL;
	while ((symscn = elf_nextscn(elf, symscn)) != NULL) {
		if ((symshdr = elf32_getshdr(symscn)) == 0) {
			fprintf(stderr, "cannot get section header of %s/%s\n",
					slash_boot, fname);
			return;
		}
		if (symshdr ->sh_type == SHT_SYMTAB)
			break;
	}
	symdata = (Elf_Data *)NULL;
	if ((symdata = elf_getdata(symscn, symdata)) == NULL) {
		fprintf(stderr, "no data in symbol table of %s/%s\n",
				slash_boot, fname);
		return;
	}
	count = symdata->d_size/sizeof(Elf32_Sym);
	sym = symdata->d_buf;

	sym++; 	/* first member holds the number of symbols */

	for (i = 1; i  < count; i++, sym++) {

		/* if it's not global text or data, continue... */
		if (((ELF32_ST_TYPE(sym->st_info) != STT_FUNC) &&
		     (ELF32_ST_TYPE(sym->st_info) != STT_OBJECT)) ||
		    (ELF32_ST_BIND(sym->st_info) != STB_GLOBAL) ||
		    (sym->st_shndx == SHN_UNDEF))
			continue;

		procname = elf_strptr(elf, symshdr->sh_link,
				      sym->st_name);
#ifdef DEBUG
fprintf(stderr, "%d. value 0x%x size %d type %d bind %d index %d name %s\n",
		i, sym->st_value, sym->st_size,
		ELF32_ST_TYPE(sym->st_info), ELF32_ST_BIND(sym->st_info),
		sym->st_shndx, procname);
#endif
		(*fun)(procname, arg0, arg1, arg2);
	}
}

static void
elf64_scan(char *fname, void (*fun)(char *, void *, void *, void *),
	    void *arg0, void *arg1, void *arg2, Elf *elf, Elf64_Ehdr *e64hdr)
{
	Elf_Scn *stringscn, *symscn;
	Elf64_Shdr *stringshdr, *symshdr;
	Elf_Data *stringdata, *symdata;
	Elf64_Sym *sym;
	int i;
	unsigned long count;
	char *procname;

	if (((stringscn = elf_getscn(elf, e64hdr->e_shstrndx)) == NULL) ||
	    ((stringshdr = elf64_getshdr(stringscn)) == NULL) ||
	    (stringshdr->sh_type != SHT_STRTAB)) {
		fprintf(stderr, "cannot get string header from %s/%s\n",
				slash_boot, fname);
		return;
	}
	stringdata = (Elf_Data *)NULL;
	if (((stringdata = elf_getdata(stringscn, stringdata)) == NULL) ||
	    (stringdata->d_size == 0)) {
		fprintf(stderr, "no data in string table of %s/%s\n",
				slash_boot, fname);
		return;
	}
	symscn = (Elf_Scn *)NULL;
	while ((symscn = elf_nextscn(elf, symscn)) != NULL) {
		if ((symshdr = elf64_getshdr(symscn)) == 0) {
			fprintf(stderr, "cannot get section header of %s/%s\n",
					slash_boot, fname);
			return;
		}
		if (symshdr ->sh_type == SHT_SYMTAB)
			break;
	}
	symdata = (Elf_Data *)NULL;
	if ((symdata = elf_getdata(symscn, symdata)) == NULL) {
		fprintf(stderr, "no data in symbol table of %s/%s\n",
				slash_boot, fname);
		return;
	}
	count = symdata->d_size/sizeof(Elf64_Sym);
	sym = symdata->d_buf;

	sym++; 	/* first member holds the number of symbols */

	for (i = 1; i  < count; i++, sym++) {

		/* if it's not global text or data, continue... */
		if (((ELF64_ST_TYPE(sym->st_info) != STT_FUNC) &&
		     (ELF64_ST_TYPE(sym->st_info) != STT_OBJECT)) ||
		    (ELF64_ST_BIND(sym->st_info) != STB_GLOBAL) ||
		    (sym->st_shndx == SHN_UNDEF))
			continue;

		procname = elf_strptr(elf, symshdr->sh_link,
				      sym->st_name);
#ifdef DEBUG
fprintf(stderr, "%d. value 0x%llx size %lld type %d bind %d index %d name %s\n",
		i, sym->st_value, sym->st_size,
		ELF64_ST_TYPE(sym->st_info), ELF64_ST_BIND(sym->st_info),
		sym->st_shndx, procname);
#endif
		(*fun)(procname, arg0, arg1, arg2);
	}
}
