
#include <stdio.h>
#include <libelf.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

char *namelist[] = {
	"addrs",
	0,
};

static void elf64_scan(char *, Elf *);
extern Elf64_Sym * elf64_findsym(char *, char *, Elf *);
extern void elf64_read_program_data(char *, Elf *, Elf64_Sym *);

static void elf32_scan(char *, Elf *);
extern Elf32_Sym * elf32_findsym(char *, char *, Elf *);
extern void elf32_read_program_data(char *, Elf *, Elf32_Sym *);

main(int argc, char **argv)
{
	char *fname;
	Elf *elf;
	Elf_Cmd cmd;
	int fd;

	if (argc != 2) {
		fprintf(stderr, "Usage: read_offsets <filename>\n");
		exit(1);
	}
	fname = argv[1];

	if (elf_version(EV_CURRENT) == EV_NONE) {
		fprintf(stderr,
		    "out of date with respect to current ELF version\n");
		exit(1);
	}
	if ((fd = open(fname, O_RDONLY)) == -1) {
		fprintf(stderr, "could not open %s\n", fname);
		exit(1);
	}

	cmd = ELF_C_READ;
	elf = elf_begin(fd, cmd, (Elf *)NULL);
	if (elf != NULL) {
		if (elf32_getehdr(elf) != NULL)
			elf32_scan(fname, elf);
		else if (elf64_getehdr(elf) != NULL)
			elf64_scan(fname, elf);
	}
	elf_end(elf);
	exit(0);
	/* NOTREACHED */
}

static void
elf64_scan(char *fname, Elf *elf)
{
	Elf64_Sym *sym;
	int list;
	char *cursearch;

	for (list = 0; list < sizeof(namelist) / sizeof(namelist[0]); list++) {
		cursearch = namelist[list];
		if (cursearch == NULL)
			break;

		if ((sym = elf64_findsym(fname, cursearch, elf)) == NULL) {
			fprintf(stderr, "Cannot find symbol %s in %s\n",
					cursearch, fname);
			exit(1);
		}
		elf64_read_program_data(fname, elf, sym);
	}
}

static void
elf32_scan(char *fname, Elf *elf)
{
	Elf32_Sym *sym;
	int list;
	char *cursearch;

	for (list = 0; list < sizeof(namelist) / sizeof(namelist[0]); list++) {
		cursearch = namelist[list];
		if (cursearch == NULL)
			break;

		if ((sym = elf32_findsym(fname, cursearch, elf)) == NULL) {
			fprintf(stderr, "Cannot find symbol %s in %s\n",
					cursearch, fname);
			exit(1);
		}
		elf32_read_program_data(fname, elf, sym);
	}
}
