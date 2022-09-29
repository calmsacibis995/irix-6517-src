/*
 * setphdr.c
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <getopt.h>
#include <libelf.h>
#include "setsym.h"
#include "elfsubr.h"

/* symbol offset indexes */
#define OFF_LIST_SIZE	3
#define ETEXT		0
#define EDATA		1
#define END		2

char *off_name[OFF_LIST_SIZE] = {
	"ketext",
	"kedata",
	"kend"
};

char *Cmd;
int verbose;
/* prototypes */
static void	process(char *, Elf64_Addr);
static void	usage_error(void);

main(int argc, char **argv)
{
	int c;
	char *filename = "unix";
	Elf64_Addr vaddr = 0;

	if ((Cmd = strrchr(argv[0], '/')) == NULL)
		Cmd = argv[0];

	/* read command line */
	while ((c = getopt(argc, argv, "f:v")) != -1) {
		switch(c) {
		case 'v':
			verbose = 1;
			break;
		case 'f':
			filename = optarg;
			break;
		case '?':
			usage_error();
			break;
		}
	}

	if (argc - optind != 1)
		usage_error();
	vaddr = strtoull(argv[optind], 0, 0);
	if (verbose)
		printf("Looking for program header entry for vaddr 0x%llx in file %s\n",
			vaddr, filename);

	process(filename, vaddr);
	exit(0);
}

static void
process(char *filename, Elf64_Addr vaddr)
{
	int fd;
	Elf *elf;
	Elf_Scn *datascn;
	Elf_Scn *sdatascn;
	Elf_Scn *bssscn;
	char *err;
	int i, foundit;

	if (elf_version(EV_CURRENT) == EV_NONE)
		xerror("out of date with respect to current ELF version", NONE);

	if ((fd = open(filename, O_RDWR)) == -1)
		xerror("cannot open file", NONE);
	if ((elf = elf_begin(fd, ELF_C_RDWR, NULL)) == NULL)
		xerror("cannot open ELF file", OP, fd);
	
	/* get .data section */
	datascn = find_section(elf, ".data");
	if (datascn == NULL)
		xerror("could not access the .data section.  Not a valid ELF file", OPELF, elf, fd);

	/* get .sdata section */
	sdatascn = find_section(elf, ".sdata");

	/* get .bss section */
	bssscn = find_section(elf, ".bss");
	if (bssscn == NULL)
		xerror("could not access the .bss section.  Not a valid ELF file", OPELF, elf, fd);

	if (elf_type(elf) == ELF_MIPS64) {
		Elf64_Addr ketext, kedata, kend;
		Elf64_Phdr *phdr, *pp;
		Elf64_Ehdr *ehdr;
		Elf64_Off offset[OFF_LIST_SIZE];
		Elf_Scn *scn = NULL;

		/* find symbols that denote kernel extents */
		for (i = 0; i < OFF_LIST_SIZE; i++) {
			err = find_sym_offset64(elf, off_name[i],
				&offset[i], datascn, sdatascn, bssscn);
			if (err) {
				fprintf(stderr, "%s: symbol \'%s\' not found\n",
					Cmd, off_name[i]);
				xerror(err, OPELF, elf, fd);
			}
			if (verbose)
				printf("symbol \'%s\' found at offset 0x%llx\n",
					off_name[i], offset[i]);
		}

		if ((ehdr = elf64_getehdr(elf)) == NULL)
			xerror("no ehdr!", OPELF, elf, fd);
		if ((phdr = elf64_getphdr(elf)) == NULL)
			xerror("no phdr!", OPELF, elf, fd);
		foundit = 0;
		for (i = 0; i < ehdr->e_phnum; i++, phdr++) {
			if (verbose)
				printf("phdr %d: type %hd vaddr 0x%llx\n",
					i, phdr->p_type, phdr->p_vaddr);
			if (phdr->p_type != PT_LOAD) {
				/* to permit running this multiple times
				 * on the same binary ... 
				 */
				if (phdr->p_type == PT_NOTE &&
				    phdr->p_vaddr == vaddr) {
					foundit = 1;
				}
				continue;
			}
			if (phdr->p_vaddr == vaddr) {
				/* found it! */
				phdr->p_type = PT_NOTE;
				elf_flagphdr(elf, ELF_C_SET, ELF_F_DIRTY);
				foundit = 1;
			}
		}
		if (!foundit)
			xerror("didn't find matching phdr", OPELF, elf, fd);

		/*
		 * since the ldspec file can't really set end/etext, etc.
		 * loop through all the sections and compute our best take
		 * and set the k* variables to the appropirate values
		 */
		ketext = 0;
		kedata = 0;
		kend = 0;

		while (scn = elf_nextscn(elf, scn)) {
			Elf64_Shdr *shdr = elf64_getshdr(scn);
			char *secname;

			if (shdr->sh_type != SHT_PROGBITS &&
			    shdr->sh_type != SHT_NOBITS)
				continue;
			if (shdr->sh_flags & SHF_MIPS_LOCAL)
				continue;
			if (shdr->sh_flags & SHF_EXECINSTR) {
				if ((shdr->sh_addr + shdr->sh_size) > ketext)
					ketext = shdr->sh_addr + shdr->sh_size;
			}
			/* XXX assumes data > text */
			if (shdr->sh_type == SHT_PROGBITS) {
				if ((shdr->sh_addr + shdr->sh_size) > kedata)
					kedata = shdr->sh_addr + shdr->sh_size;
			}
			if ((shdr->sh_addr + shdr->sh_size) > kend)
				kend = shdr->sh_addr + shdr->sh_size;
		}

		if (verbose)
			printf("ketext 0x%llx kedata 0x%llx kend 0x%llx\n",
				ketext, kedata, kend);
		if (ketext == 0 || kedata == 0 || kend == 0)
			xerror("couldn't find kernel extents", OPELF, elf, fd);

		/* write kernel extent variables */
		if ((lseek(fd, offset[ETEXT], SEEK_SET) == -1) ||
		    (write(fd, &ketext, sizeof(ketext)) != sizeof(ketext)))
			xerror("could not seek/write \'ketext\' in file", OPELF, elf, fd);
		if ((lseek(fd, offset[EDATA], SEEK_SET) == -1) ||
		    (write(fd, &kedata, sizeof(kedata)) != sizeof(kedata)))
			xerror("could not seek/write \'kedata\' in file", OPELF, elf, fd);
		if ((lseek(fd, offset[END], SEEK_SET) == -1) ||
		    (write(fd, &kend, sizeof(kend)) != sizeof(kend)))
			xerror("could not seek/write \'kend\' in file", OPELF, elf, fd);
	} else {
		xerror("only supports 64 bits!", OPELF, elf, fd);
	}
	elf_update(elf, ELF_C_WRITE);
	elf_end(elf);
	close(fd);
}


void
xerror(char *msg, int closes, ...)
{
	va_list ap;
	
	va_start(ap, closes);

	fprintf(stderr, "%s: Error: %s.\n", Cmd, msg);
	if (closes == OP)
		close((int) va_arg(ap, int));
	else if (closes == OPELF) {
		elf_end((Elf *) va_arg(ap, Elf *));
		close((int) va_arg(ap, int));
	}

	va_end(ap);
	exit(1);
}

static void
usage_error(void)
{
	fprintf(stderr, "\n");
	fprintf(stderr, "Usage: setphdr [-v] [-f filename] addr\n");
	fprintf(stderr, "              -v: Verbose\n");
	fprintf(stderr, "\n");
	exit(1);
}
