
#include <stdio.h>
#include <libelf.h>
#include <fcntl.h>
#include <string.h>
#include <bstring.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>

#undef DEBUG

#define FMT_HEX		16
#define FMT_DECIMAL	10
#define FMT_NULL	0
#define FMT_END		-1

struct init_data_ELFSIZE_ {
	union {
		Elf_ELFSIZE__Addr _name;
		char *_string;
	} d_ELFSIZE__un;
#define d_ELFSIZE__name	d_ELFSIZE__un._name
#define d_ELFSIZE__string	d_ELFSIZE__un._string

	app_ELFSIZE__long_t	d_ELFSIZE__value;
	int		d_ELFSIZE__format;
};

static void elf_ELFSIZE__resolve_strings(
	char *, Elf *, struct init_data_ELFSIZE_ *);
static void elf_ELFSIZE__get_shdr_and_data(
	char *, Elf *, Elf_ELFSIZE__Shdr **, Elf_Data **, Elf_ELFSIZE__Addr);

#define ADDR_COVERED(shdr, addr)				\
			((shdr)->sh_addr <= (addr) &&			\
			(shdr)->sh_addr + (shdr)->sh_size > (addr))

Elf_ELFSIZE__Sym *
elf_ELFSIZE__findsym(
	char *fname,
	char *srchname,
	Elf *elf)
{
	Elf_Data *symdata = NULL;
	Elf_ELFSIZE__Shdr *symshdr = NULL;
	Elf_Scn *symscn = NULL;
	Elf_ELFSIZE__Sym *sym;
	int i;
	size_t count;
	char *symname;

#ifdef DEBUG
	fprintf(stderr, "Search for %s\n", srchname);
#endif
	symscn = (Elf_Scn *)NULL;
	while ((symscn = elf_nextscn(elf, symscn)) != NULL) {
		if ((symshdr = elf_ELFSIZE__getshdr(symscn)) == 0) {
			fprintf(stderr, "cannot get section header of %s\n",
					fname);
			exit(1);
		}
		if (symshdr->sh_type == SHT_SYMTAB ||
		    symshdr->sh_type == SHT_DYNSYM)
			break;
	}

	if (symscn == NULL) {
		fprintf(stderr, "No symbol table section in %s\n", fname);
		exit(1);
	}

	symdata = (Elf_Data *)NULL;
	if ((symdata = elf_getdata(symscn, symdata)) == NULL) {
		fprintf(stderr, "no data in symbol table of %s\n",
				fname);
		exit(1);
	}

	count = symdata->d_size/sizeof(Elf_ELFSIZE__Sym);

	sym = symdata->d_buf;
	sym++;		/* first member holds the number of symbols */

	for (i = 1; i  < count; i++, sym++) {
		symname = elf_strptr(elf, symshdr->sh_link, sym->st_name);
		if (strcmp(symname, srchname) == 0) {
#ifdef DEBUG
			fprintf(stderr,
			"%d. val 0x%LFMTx size %LFMTd type %d name %s\n",
			i, sym->st_value, sym->st_size,
			ELF_ELFSIZE__ST_TYPE(sym->st_info), symname);
#endif
			return sym;
		}
	}
	fprintf(stderr, "Could not find symbol %s in file %s\n",
		srchname, fname);
	exit(1);
	/* NOTREACHED */
}

void
elf_ELFSIZE__read_program_data(
	char *fname,
	Elf *elf,
	Elf_ELFSIZE__Sym *sym)
{
	Elf_Data *scndata = NULL;
	Elf_ELFSIZE__Shdr *shdr = NULL;
	Elf_ELFSIZE__Addr addr;
	Elf_ELFSIZE__Off scn_data_offset;
	char *scn_data_ptr;
	char *scn_data_end;
	struct init_data_ELFSIZE_ *data_ELFSIZE_;
	size_t elem_count;
	int i;

	addr = sym->st_value;
	elf_ELFSIZE__get_shdr_and_data(fname, elf, &shdr, &scndata, addr);

	scn_data_offset = sym->st_value - shdr->sh_addr;
	scn_data_ptr = (char *)scndata->d_buf + scn_data_offset;

	/* Scan through the section data, looking for the end marker.
	 * The scn_data_ptr has already been adjusted to point to
	 * start of the initialized data corresponding to the array
	 * we are trying to decode.
	 */
	data_ELFSIZE_ = (struct init_data_ELFSIZE_ *)scn_data_ptr;

	/* just so we don't go off the end */
	scn_data_end = (char *)scndata->d_buf + scndata->d_size;
	elem_count = 0;

	for(i = 0; ; i++) {
		elem_count++;
		if (data_ELFSIZE_[i].d_ELFSIZE__format == -1)
			break;
#ifdef DEBUG
		fprintf(stderr, "name 0x%LFMTx, value 0x%LFMTx, format %d\n",
			data_ELFSIZE_[i].d_ELFSIZE__name,
			data_ELFSIZE_[i].d_ELFSIZE__value,
			data_ELFSIZE_[i].d_ELFSIZE__format);
#endif
		if ((char *)data_ELFSIZE_ >= scn_data_end) {
			fprintf(stderr,
			"Could not find SENTINAL value in initialized data\n");
			exit(1);
		}
	}

	data_ELFSIZE_ = memalign(sizeof(app_ELFSIZE__ptr_t),
				 elem_count * sizeof(*data_ELFSIZE_));
	if (data_ELFSIZE_ == NULL) {
		fprintf(stderr, "Cannot allocate %d bytes for data\n",
			elem_count * sizeof(*data_ELFSIZE_));
		exit(1);
	}

	bcopy(scn_data_ptr, data_ELFSIZE_,
	     (int)(elem_count * sizeof(*data_ELFSIZE_)));
#ifdef DEBUG
	fprintf(stderr, "Found section addr 0x%LFMTx, fileoff 0x%LFMTx\n",
			shdr->sh_addr, shdr->sh_offset);
	fprintf(stderr, "scn_data_offset 0x%LFMTx, scn_data_ptr 0x%x\n",
			scn_data_offset, scn_data_ptr);
#endif

	elf_ELFSIZE__resolve_strings(fname, elf, data_ELFSIZE_);

	for (i = 0; ; i++) {
		if (data_ELFSIZE_[i].d_ELFSIZE__format == FMT_END)
			break;
		if (data_ELFSIZE_[i].d_ELFSIZE__format == FMT_HEX)
			printf("#define	%s	0x%LFMTx\n",
				data_ELFSIZE_[i].d_ELFSIZE__string,
				data_ELFSIZE_[i].d_ELFSIZE__value);
		else if (data_ELFSIZE_[i].d_ELFSIZE__format == FMT_DECIMAL)
			printf("#define	%s	%LFMTd\n",
				data_ELFSIZE_[i].d_ELFSIZE__string,
				data_ELFSIZE_[i].d_ELFSIZE__value);
		else if (data_ELFSIZE_[i].d_ELFSIZE__format == FMT_NULL)
			printf("#define	%s\n",
				data_ELFSIZE_[i].d_ELFSIZE__string);
		else {
			fprintf(stderr, "Unrecognized print format\n");
			fprintf(stderr, "Formats must be 10, 16, or 0\n");
			exit(1);
		}
	}
}

static void
elf_ELFSIZE__resolve_strings(
	char *fname,
	Elf *elf,
	struct init_data_ELFSIZE_ *data_ELFSIZE_)
{
	Elf_ELFSIZE__Addr addr;
	Elf_ELFSIZE__Shdr *shdr;
	Elf_Data *scndata;
	Elf_ELFSIZE__Off scn_data_offset;
	char *scn_data_ptr;
	int i;

	addr = data_ELFSIZE_[0].d_ELFSIZE__name;

	elf_ELFSIZE__get_shdr_and_data(fname, elf, &shdr, &scndata, addr);

	for (i = 0; ; i++) {
		size_t len;
		char *nstring;

		if (data_ELFSIZE_[i].d_ELFSIZE__format == -1)
			break;

		addr = data_ELFSIZE_[i].d_ELFSIZE__name;

		if (!ADDR_COVERED(shdr, addr))
			elf_ELFSIZE__get_shdr_and_data(fname, elf, &shdr,
						&scndata, addr);

		scn_data_offset = addr - shdr->sh_addr;
		scn_data_ptr = (char *)scndata->d_buf + scn_data_offset;

		len = strlen(scn_data_ptr);
		if ((nstring = malloc(len+1)) == NULL) {
			fprintf(stderr, "Cannot allocate %d bytes\n", len+1);
			exit(1);
		}
		strcpy(nstring, scn_data_ptr);
		nstring[len] = '\0';
		/* Replace the file ptr in data_ELFSIZE_[i].name with a
		 * pointer to the string.
		 */
		data_ELFSIZE_[i].d_ELFSIZE__string = nstring;
#ifdef DEBUG
		fprintf(stderr, "STRING %s\n", nstring);
#endif
	}
}

static void
elf_ELFSIZE__get_shdr_and_data(
	char *fname,
	Elf *elf,
	Elf_ELFSIZE__Shdr **shdrpp,
	Elf_Data **datapp,
	Elf_ELFSIZE__Addr addr)
{
	Elf_Scn *scn;
	Elf_ELFSIZE__Shdr *shdr;
	Elf_Data *scndata;

	scn = (Elf_Scn *)NULL;
	while ((scn = elf_nextscn(elf, scn)) != NULL) {
		if ((shdr = elf_ELFSIZE__getshdr(scn)) == 0) {
			fprintf(stderr, "cannot get section header of %s\n",
					fname);
			exit(1);
		}
		if (shdr->sh_type != SHT_PROGBITS)
			continue;
		if (ADDR_COVERED(shdr, addr))
			break;
	}

	if (scn == NULL) {
		fprintf(stderr, "No section containing 0x%LFMTx in %s\n",
			addr, fname);
		exit(1);
	}

	scndata = (Elf_Data *)NULL;
	if ((scndata = elf_getdata(scn, scndata)) == NULL) {
		fprintf(stderr, "Cannot get section data for %s\n",
				fname);
		exit(1);
	}

	*shdrpp = shdr;
	*datapp = scndata;
}
