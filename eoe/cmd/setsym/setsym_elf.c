/*
 * setsym.c
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <getopt.h>
#include <utime.h>
#include <libelf.h>
#include <libdwarf.h>
#include <syslog.h>
#include "setsym.h"
#include "elfsubr.h"
#include "readsym.h"

/* command line flags */
#define NUM_FLAGS	3
#define V_FLAG		0		/* verbose flag */
#define D_FLAG		1		/* dump to stdout flag */
#define E_FLAG		2		/* elf symbols only */

/* symbol offset indexes*/
#define OFF_LIST_SIZE	4
#define SYMMAX		0
#define NAMESIZE	1
#define DBSTAB		2
#define NAMETAB		3

char *off_name[OFF_LIST_SIZE] = {
	"symmax",
	"namesize",
	"dbstab",
	"nametab"
};
char *Cmd;

/* prototypes */
static void	process(char *, int [NUM_FLAGS]);
static void	usage_error(void);
static void	dump_symtab(int, int, int, void *, char *,
				off_t [OFF_LIST_SIZE], int);
static void	write_symtab(int, Elf *, int, int, void *, char *,
				off_t [OFF_LIST_SIZE], int, int, int);
static int	compar64(const void *, const void *);
static int	compar32(const void *, const void *);

int
main(int argc, char **argv)
{
	int c;
	int flag[NUM_FLAGS];
	char *filename = "unix";
	struct stat bsb, asb;

	if ((Cmd = strrchr(argv[0], '/')) == NULL)
		Cmd = argv[0];
	else
		Cmd++;

	/* set all flags to false */
	for (c = 0; c < NUM_FLAGS; c++)
		flag[c] = 0;

	/* read command line */
	while ((c = getopt(argc, argv, "vde")) != -1) {
		switch(c) {
		case 'v':
			(flag[V_FLAG])++;
			break;
		case 'd':
			(flag[D_FLAG])++;
			break;
		case 'e':
			(flag[E_FLAG])++;
			break;
		case '?':
			usage_error();
			break;
		}
	}

	if (argc - optind != 1)
		usage_error();
	else
		filename = argv[optind];

	if (stat(filename, &bsb) != 0)
		bsb.st_mtime = 0;

	process(filename, flag);

	if (stat(filename, &asb) == 0 && asb.st_mtime < bsb.st_mtime) {
		struct utimbuf utb;

		syslog(LOG_WARNING, "WARNING: %s mtime was in the future",
			filename);
		fprintf(stderr, "%s: WARNING: %s mtime was in the future\n",
			argv[0], filename);

		utb.actime = bsb.st_atime;
		utb.modtime = bsb.st_mtime;

		if (utime(filename, &utb) == 0) {
			syslog(LOG_WARNING, "WARNING: %s was modified but mtime was set back", filename);
			fprintf(stderr, "%s: WARNING: %s was modified but mtime was set back\n", argv[0], filename);
		}
	}
	exit(0);
}

static void
process(char *filename, int flag[NUM_FLAGS])
{
	off_t offset[OFF_LIST_SIZE];
	char *err;
	int fd;
	Elf *elf;
	Elf_Scn *datascn;
	Elf_Scn *sdatascn;
	Elf_Scn *bssscn;
	int i, bits;

	void *dbstab;		/* debugging symbol table */
	int symmax;		/* maximum number of symbols in dbstab */
	char *nametab;		/* name string table */
	int namesize;		/* indext to next available byte */


	if (elf_version(EV_CURRENT) == EV_NONE)
		xerror("out of date with respect to current ELF version", NONE);

	if (flag[D_FLAG]) {
		if ((fd = open(filename, O_RDONLY)) == -1)
			xerror("cannot open file", NONE);
		if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL)
			xerror("cannot open ELF file", OP, fd);
	}
	else {
		if ((fd = open(filename, O_RDWR)) == -1)
			xerror("cannot open file", NONE);
		if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL)
			xerror("cannot open ELF file", OP, fd);
	}
	
	/* get .data section */
	datascn = find_section(elf, ".data");
	if (datascn == NULL)
		xerror("could not access the .data section.  Not a valid ELF file", OPELF, elf, fd);

	/* get .sdata section */
	sdatascn = find_section(elf, ".sdata");

	/* get .bss section, if any */
	bssscn = find_section(elf, ".bss");

	/* get file offset and virtual address of data section */
	if ((bits = elf_type(elf)) == ELF_MIPS64) {
		for (i = 0; i < OFF_LIST_SIZE; i++) {
			auto Elf64_Off off;
			err = find_sym_offset64(elf, off_name[i],
					&off, datascn, sdatascn, bssscn);
			if (err) {
				fprintf(stderr, "%s: symbol \'%s\' not found\n",
					Cmd, off_name[i]);
				xerror(err, OPELF, elf, fd);
			}
			offset[i] = (off_t)off;
		}
	} else if (bits == ELF_MIPS32) {
		for (i = 0; i < OFF_LIST_SIZE; i++) {
			auto Elf32_Off off;
			err = find_sym_offset32(elf, off_name[i],
					&off, datascn, sdatascn, bssscn);
			if (err) {
				fprintf(stderr, "%s: symbol \'%s\' not found\n",
					Cmd, off_name[i]);
				xerror(err, OPELF, elf, fd);
			}
			offset[i] = (off_t)off;
		}
	} else
		xerror("not a valid 64/32-bit MIPS ELF file", OPELF, elf, fd);

	/* find size of tables */
	if ((lseek(fd, offset[SYMMAX], SEEK_SET) == -1) ||
	    (read(fd, &symmax, sizeof(symmax)) != sizeof(symmax)))
		xerror("could not seek/read symmax in file", OPELF, elf, fd);
	if ((lseek(fd, offset[NAMESIZE], SEEK_SET) == -1) ||
	    (read(fd, &namesize, sizeof(namesize)) != sizeof(namesize)))
		xerror("could not seek/read namesize in file", OPELF, elf, fd);

	/* allocate space for tables */
	if (bits == ELF_MIPS64)
		dbstab = (void *) calloc(symmax, sizeof(struct dbstbl64));
	else /* bits == ELF_MIPS32 */
		dbstab = (void *) calloc(symmax, sizeof(struct dbstbl32));
	nametab = (char *) calloc(namesize, sizeof(char));
	if ((dbstab == NULL) || (nametab == NULL))
		xerror("could not malloc table", OPELF, elf, fd);

	/* if verbose then write the sizes */
	if (flag[V_FLAG]) {
		printf ("Maximum number of symbols: %d\n", symmax);
		printf ("Maximum size of name table: %d\n", namesize);
	}

	/* either dump symbol table or write into it */
	if (flag[D_FLAG]) {
		elf_end(elf);
		dump_symtab(fd, symmax, namesize, dbstab,
				nametab, offset, bits);
	}
	else {
		write_symtab(fd, elf, symmax, namesize, dbstab, nametab,
				offset, bits, flag[V_FLAG], flag[E_FLAG]);
		elf_end(elf);
	}

	close(fd);
}

static void
dump_symtab(	int fd,
		int symmax,
		int namesize,
		void *dbstab,
		char *nametab,
		off_t offset[OFF_LIST_SIZE],
		int bits)
{
	int i;
	struct dbstbl64 *dbstab64 = (struct dbstbl64 *) dbstab;
	struct dbstbl32 *dbstab32 = (struct dbstbl32 *) dbstab;

	if (bits == ELF_MIPS64) {
		if ((lseek(fd, offset[DBSTAB], SEEK_SET) == -1) ||
		    (read(fd, dbstab, sizeof(struct dbstbl64) * symmax) !=
				      sizeof(struct dbstbl64) * symmax))
			xerror("could not seek/read dbstab", OP, fd);
		if ((lseek(fd, offset[NAMETAB], SEEK_SET) == -1) ||
		    (read(fd, nametab, sizeof(char) * namesize) !=
				       sizeof(char) * namesize))
			xerror("could not seek/read nametab", OP, fd);

		for (i = 0; i < symmax; i++)
			printf("addr=%llx\tnoffst=%d\tname=\"%s\"\n",
				dbstab64[i].addr, dbstab64[i].noffst,
				&nametab[dbstab64[i].noffst]);
	}
	else /* bits == ELF_MIPS32 */ {
		if ((lseek(fd, offset[DBSTAB], SEEK_SET) == -1) ||
		    (read(fd, dbstab, sizeof(struct dbstbl32) * symmax) !=
				      sizeof(struct dbstbl32) * symmax))
			xerror("could not seek/read dbstab", OP, fd);
		if ((lseek(fd, offset[NAMETAB], SEEK_SET) == -1) ||
		    (read(fd, nametab, sizeof(char) * namesize) !=
				       sizeof(char) * namesize))
			xerror("could not seek/read nametab", OP, fd);

		for (i = 0; i < symmax; i++)
			printf("addr=%x\tnoffst=%d\tname=\"%s\"\n",
				dbstab32[i].addr, dbstab32[i].noffst,
				&nametab[dbstab32[i].noffst]);
	}
}

static void
write_symtab(	int fd,
		Elf *elf,
		int symmax,
		int namesize,
		void *dbstab,
		char *nametab,
		off_t offset[OFF_LIST_SIZE],
		int bits,
		int Verbose,
		int ElfOnly)
{
	int symnum;
	struct dbstbl64 *dbstab64 = (struct dbstbl64 *) dbstab;
	struct dbstbl32 *dbstab32 = (struct dbstbl32 *) dbstab;

	init_read_symtab(fd, elf, symmax, namesize, dbstab, nametab, bits);

	symnum = elf_read_symtab();

	if (!ElfOnly)
		symnum = dwarf_read_symtab(Verbose);

	if (bits == ELF_MIPS64) {
		qsort(dbstab, symnum, sizeof(struct dbstbl64), compar64);

		/* print info if verbose flag is on */
		if (Verbose) {
			printf("First symbol %s @ 0x%llx Last %s @ 0x%llx\n",
				&nametab[dbstab64[0].noffst],
				dbstab64[0].addr,
				&nametab[dbstab64[symnum-1].noffst],
				dbstab64[symnum-1].addr);
			printf("Using %d out of %d available symbol entries.\n",
				symnum, symmax);
			printf("Seeking to 0x%llx, writing %d bytes of dbstab.\n",
				offset[DBSTAB],
				sizeof(struct dbstbl64) * symmax);
		}
		if ((lseek(fd, offset[DBSTAB], SEEK_SET) == -1) ||
		    (write(fd, dbstab, sizeof(struct dbstbl64) * symmax) !=
				       sizeof(struct dbstbl64) * symmax))
			xerror("could not seek/write dbstab",  OPELF, elf, fd);
	}
	else /* bits == ELF_MIPS32 */ {
		qsort(dbstab, symnum, sizeof(struct dbstbl32), compar32);

		/* print info if verbose flag is on */
		if (Verbose) {
			printf("First symbol %s @ 0x%x Last %s @ 0x%x\n",
				&nametab[dbstab32[0].noffst],
				dbstab32[0].addr,
				&nametab[dbstab32[symnum-1].noffst],
				dbstab32[symnum-1].addr);
			printf("Using %d out of %d available symbol entries.\n",
				symnum, symmax);
			printf("Seeking to 0x%llx, writing %d bytes of dbstab.\n",
				offset[DBSTAB],
				sizeof(struct dbstbl32) * symmax);
		}
		if ((lseek(fd, offset[DBSTAB], SEEK_SET) == -1) ||
		    (write(fd, dbstab, sizeof(struct dbstbl32) * symmax) !=
				       sizeof(struct dbstbl32) * symmax))
			xerror("could not seek/write dbstab",  OPELF, elf, fd);
	}

	if (Verbose) {
		printf("Using all %d bytes for symbol names.\n", namesize);
		printf("Seeking to 0x%llx, writing %d bytes of nametab.\n",
			offset[NAMETAB], sizeof(char ) * namesize);
	}

	if ((lseek(fd, offset[NAMETAB], SEEK_SET) == -1) ||
	    (write(fd, nametab, sizeof(char) * namesize) !=
				sizeof(char) * namesize))
		xerror("could not seek/write nametab",  OPELF, elf, fd);

	/*
	 * now re-write max symbols "symmax" so that binary search
	 * won't find a bucket of zeros
	 */
	if ((lseek(fd, offset[SYMMAX], SEEK_SET) == -1) ||
	    (write(fd, &symnum, sizeof(symnum)) != sizeof(symnum)))
		xerror("could not seek/write symnum", OPELF, elf, fd);
}

static int
compar64(const void *x, const void *y)
{
	if (((struct dbstbl64 *)x)->addr > ((struct dbstbl64 *)y)->addr)
		return(1);
	else if (((struct dbstbl64 *)x)->addr == ((struct dbstbl64 *)y)->addr)
		return(0);
	return(-1);
}

static int
compar32(const void *x, const void *y)
{
	if (((struct dbstbl32 *)x)->addr > ((struct dbstbl32 *)y)->addr)
		return(1);
	else if (((struct dbstbl32 *)x)->addr == ((struct dbstbl32 *)y)->addr)
		return(0);
	return(-1);
}

void
xerror(char *msg, int closes, ...)
{
	Dwarf_Error derror;
	va_list ap;
	
	va_start(ap, closes);

	fprintf(stderr, "%s: Error: %s.\n", Cmd, msg);
	if (closes == OP)
		close((int) va_arg(ap, int));
	else if (closes == OPELF) {
		elf_end((Elf *) va_arg(ap, Elf *));
		close((int) va_arg(ap, int));
	}
	else if (closes == OPDW) {
		dwarf_finish((Dwarf_Debug) va_arg(ap, Dwarf_Debug), &derror);
		close((int) va_arg(ap, int));
	}
	else if (closes == OPELFDW) {
		dwarf_finish((Dwarf_Debug) va_arg(ap, Dwarf_Debug), &derror);
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
	fprintf(stderr, "Usage: setsym [-[d]|[[v][e]]]] filename\n");
	fprintf(stderr, "              -d: Dump symbols from already processed file\n");
	fprintf(stderr, "              -v: Verbose\n");
	fprintf(stderr, "              -e: ELF symbols only (default is ELF & DWARF)\n");
	fprintf(stderr, "\n");
	exit(1);
}
