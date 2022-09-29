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

#define NONE		0x0		/* "..." = <nothing>		*/
#define OP		0x1		/* "..." = open()		*/
#define OPELF		0x2		/* "..." = elf_begin(), open()	*/
#define OK		0x4
#define ELF_NONE	0
#define ELF_UNKNOWN	1
#define ELF_MIPS32	2
#define ELF_MIPS64	3

char *Cmd;
int verbose;
/* prototypes */
static void process(char *);
static void	usage_error(void);
extern int elf_type(Elf *elf);
extern Elf_Scn *find_section(Elf *elf, char *scn_name);
extern void xerror(char *msg, int closes, ...);

int
main(int argc, char **argv)
{
	int c;
	char *filename = NULL;

	if ((Cmd = strrchr(argv[0], '/')) == NULL)
		Cmd = argv[0];
	else
		Cmd++;

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

	if (filename == NULL)
		usage_error();

	process(filename);
	return 0;
}

static void
process(char *filename)
{
	int fd;
	Elf *elf;
	Elf_Scn *options;

	if (elf_version(EV_CURRENT) == EV_NONE)
		xerror("out of date with respect to current ELF version", NONE);

	if ((fd = open(filename, O_RDWR)) == -1)
		xerror("cannot open file", NONE);
	if ((elf = elf_begin(fd, ELF_C_RDWR, NULL)) == NULL)
		xerror("cannot open ELF file", OP, fd);
	
	/* get .MIPS_options section */
	options = find_section(elf, ".MIPS.options");
	if (options == NULL)
		xerror("could not access the .MIPS.options section.  Not a valid ELF file", OPELF|OK, elf, fd);

	if (elf_type(elf) == ELF_MIPS64) {
		Elf_Data *opdata = NULL;
		Elf_Options *optptr;
		int off = 0;

		opdata = elf_getdata(options, opdata);
		if (opdata && (opdata->d_size > 0)) {
			while (off < opdata->d_size) {
				optptr = (Elf_Options *)((char *)opdata->d_buf + off);
				off += optptr->size;
				if (optptr->kind == ODK_EXCEPTIONS) {
					if (verbose)
						printf("found exceptions! - currently 0x%x\n", optptr->info);
					optptr->info |= OEX_PAGE0;
					break;
				}
			}
		}

	} else {
		xerror("only supports 64 bits!", OPELF|OK, elf, fd);
	}
	elf_flagelf(elf, ELF_C_SET, ELF_F_DIRTY|ELF_F_LAYOUT);
	if (elf_update(elf, ELF_C_WRITE) < 0) {
		unlink(filename);
		xerror("elf_update error - binary trashed! will remove!",
			OPELF, elf, fd);
	}
	elf_end(elf);
	close(fd);
}

void
xerror(char *msg, int closes, ...)
{
	va_list ap;
	
	va_start(ap, closes);

	fprintf(stderr, "%s: %s: %s.\n", Cmd, closes & OK ? "Note" : "Error", msg);
	if (closes & OPELF)
		elf_end((Elf *) va_arg(ap, Elf *));
	if (closes & OP)
		close((int) va_arg(ap, int));

	va_end(ap);
	exit(closes & OK ? 0 : 1);
}

static void
usage_error(void)
{
	fprintf(stderr, "\n");
	fprintf(stderr, "Usage: setex [-v] [-f filename] value\n");
	fprintf(stderr, "              -v: Verbose\n");
	fprintf(stderr, "\n");
	exit(1);
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
