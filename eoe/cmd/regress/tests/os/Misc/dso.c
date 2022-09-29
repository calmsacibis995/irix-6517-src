#include <libelf.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <limits.h>

extern int optind;

void handle_elf(Elf *ed, Elf32_Phdr **tph, Elf32_Phdr **dph);
void dopheaders(Elf *ed, int nph, Elf32_Phdr **tph, Elf32_Phdr **dph);
void dotest(void *);

int Verbose = 0;
char file1[PATH_MAX], file2[PATH_MAX];

#define Vprintf if (Verbose) printf

static
void
die(char * format, ...)
{
	va_list ap;
	char string[1000];

	strcpy(string, sys_errlist[errno]);
	strcat(string, ": ");
	va_start(ap, format);
	strcat(string, format);
	strcat(string, "\n");
	vfprintf(stderr, string, ap);
	va_end(ap);

	exit(1);
}

Elf *
openelf(char *file)
{
	Elf 		*ed;
	int 		fd;
	Elf_Cmd		cmd;

	if ((fd = open(file, O_RDONLY)) == -1)
		die(file);
	
	cmd = ELF_C_READ;
	if ((ed = elf_begin(fd, cmd, (Elf *) 0)) == NULL)
		die("Elf begin");

	if (elf_kind(ed) != ELF_K_ELF)
		die("%s:Cannot handle non-elf files (archives, coff, plain files, etc ...)", file);
	return(ed);
}




void
handle_elf(Elf *ed, Elf32_Phdr **tph, Elf32_Phdr **dph)
{
	Elf32_Ehdr 	*eh;
	
	if ((eh = elf32_getehdr(ed)) == NULL)
		die("elf32_getehdr");


	Vprintf("Class:\t\t");
	switch (eh->e_ident[EI_CLASS]) {
		case ELFCLASSNONE : Vprintf("NONE\n"); break;
		case ELFCLASS32   : Vprintf("32-BIT\n"); break;
		case ELFCLASS64   : Vprintf("64-BIT\n"); break;
		default		  : Vprintf("UNKNOWN\n"); break;
	}
	Vprintf("Type:\t\t");
	switch (eh->e_type) {
		case ET_NONE	  : Vprintf("NONE\n"); break;
		case ET_REL	  : Vprintf("RELOCATABLE\n"); break;
		case ET_EXEC	  : Vprintf("EXEC\n"); break;
		case ET_DYN	  : Vprintf("DYNAMIC\n"); break;
		case ET_CORE	  : Vprintf("CORE\n"); break;
		default		  : Vprintf("UNKNOWN\n"); break;
	}
	if (eh->e_type != ET_DYN) {
		printf("Object not Dynamic\n");
		exit(-1);
	}
	Vprintf("# Pheaders: \t\t%d\n", eh->e_phnum);
	dopheaders(ed, eh->e_phnum, tph, dph);
}


printpheader(Elf32_Phdr *ph)
{
	switch (ph->p_type) {
		case PT_NULL:         printf("%-12s", "NULL"); break;
		case PT_LOAD:         printf("%-12s", "LOAD"); break;
		case PT_DYNAMIC:      printf("%-12s", "DYN"); break;
		case PT_INTERP:       printf("%-12s", "INTERP"); break;
		case PT_NOTE:         printf("%-12s", "NOTE"); break;
		case PT_PHDR:         printf("%-12s", "PHDR"); break;
		case PT_SHLIB:        printf("%-12s", "SHLIB"); break;
		case PT_MIPS_REGINFO: printf("%-12s", "REGINF"); break;
		case PT_MIPS_RTPROC:  printf("%-12s", "RTPROC"); break;
		default:              printf("%-12x", ph->p_type);
	}
	printf("%-12#x%-19#x%-18#x\n%-12#x%-12#x",
		ph->p_offset,
		ph->p_vaddr,
		ph->p_paddr,
		(unsigned long)ph->p_filesz,
		(unsigned long)ph->p_memsz);
	switch (ph->p_flags) {
		case 0:              printf("%-19s", "---"); break;
		case PF_X:           printf("%-19s", "--x"); break;
		case PF_W:           printf("%-19s", "-w-"); break;
		case PF_W+PF_X:      printf("%-19s", "-wx"); break;
		case PF_R:           printf("%-19s", "r--"); break;
		case PF_R+PF_X:      printf("%-19s", "r-x"); break;
		case PF_R+PF_W:      printf("%-19s", "rw-"); break;
		case PF_R+PF_W+PF_X: printf("%-19s", "rwx"); break;
		default:             Vprintf("%-19d", ph->p_flags);
	}
	printf("%-18#x\n\n", (unsigned long)ph->p_align);
}


void
dopheaders(Elf *ed, int nph, Elf32_Phdr **tph, Elf32_Phdr **dph)
{
	Elf32_Phdr 	*ph;
	int		i;


	Vprintf(" ***** PROGRAM EXECUTION HEADER *****\n");
	Vprintf("Type        Offset      Vaddr              Paddr\n");
	Vprintf("Filesz      Memsz       Flags              Align\n\n");

	if ((ph = elf32_getphdr(ed)) == NULL) {
		printf("No program headers\n");
		return;
	}

	for (i = 0; i < nph; i++) {
		if (ph == NULL) 
			die("Premature EOF on program exec header");
		if (ph->p_flags == PF_R+PF_X) {
			if (*tph == NULL) {
				*tph = ph;
				Vprintf("Setting tph to 0x%x\n", ph);
			}
			else printf("Warning: > 1 text section\n");
		}
		else if (ph->p_flags == PF_R+PF_W) {
			if (*dph == NULL)  {
				*dph = ph;
				Vprintf("Setting dph to 0x%x\n", ph);
			}
			else printf("Warning: > 1 data section\n");
		}
		if (Verbose) printpheader(ph);
		ph++;
	}
}


dload(char *dso, char *entry)
{
	int		*foo;
	void	        (*bar)(int);

	foo = dlopen(dso, RTLD_LAZY);
	if (foo == 0)
		printf("dlopen: %s: %s\n", dso, dlerror());
	else {
		bar = dlsym(foo, entry);
		if (bar == 0)
			printf("dlsym: %s %s: %s\n", dso, entry, dlerror());
		else {
			(*bar)(Verbose);
			Vprintf("appears to work!\n");
		}
	}
}


void
dotest(void *vaddr)
{
	int fd;

	if ((fd = open("./mmap.file", O_RDWR | O_CREAT, 0644)) < 0)
		die("mmap.file open");
	if (mmap((void *) vaddr, 0x80000,
			PROT_NONE, MAP_SHARED|MAP_FIXED|MAP_AUTOGROW,
				fd, 0) == (void *) -1)
		die("mmap");
	Vprintf("Done mmap\n");

	dload("./x.so", "x");
	dload("./y.so", "y");
}

main(int argc, char **argv)
{
	Elf		*ed1, *ed2; /* elf descriptor */
	Elf32_Phdr 	*tph1, *dph1,*tph2, *dph2;
	int 		c, errflg = 0;
	int		nodataclash = 0, notextclash = 0;

	while ((c = getopt(argc, argv, "v")) != -1)
		switch (c) {
		case 'v':
			Verbose++;
			break;
		case '?':
			errflg++;
			break;
		}

	if (errflg) {
		fprintf(stderr, "Usage: dso [-v] <file1> <file2>\n");
		exit(1);
	}

	if ((argc - optind) == 0) {
		/* no file was specified */
		strcpy(file1, "x.so");
		strcpy(file2, "y.so");
	} else {
		fprintf(stderr, "Usage: dso [-v] <file1> <file2>\n");
		exit(1);
	}

	if (elf_version(EV_CURRENT) == EV_NONE)
		die("Elf version");
	ed1 = openelf(file1);
	ed2 = openelf(file2);

	tph1 = dph1 = tph2 = dph2 = NULL;
	handle_elf(ed1, &tph1, &dph1);
	handle_elf(ed2, &tph2, &dph2);

		
	if (Verbose) {
		printf("Text in %s:\n", file1);
		printpheader(tph1);
		printf("Should clash with text in %s\n", file2);
		printpheader(tph2);
		printf("Data in %s:\n", file1);
		printpheader(dph1);
		printf("Should clash with data in %s\n", file2);
		printpheader(dph2);
	}
	if ((tph1->p_vaddr != tph2->p_vaddr)
	   || (tph1->p_memsz != tph2->p_memsz)) {
		Vprintf("Text regions do not clash\n");
		notextclash = 1;
	}
	if ((dph1->p_vaddr != dph2->p_vaddr)
	   || (dph1->p_memsz != dph2->p_memsz)) {
		Vprintf("Data regions do not clash\n");
		nodataclash = 1;
	}

	if ((nodataclash == 1) && (notextclash == 1)) {
		printf("No clashes in either text or data --- "
			" This test is not effective\n");
		printf("Please re-check compilation of shared objects (%s & %s)\n", file1, file2);
		exit(-1);
	}

	if (!notextclash) dotest((void*) tph1->p_vaddr);
	if (!nodataclash) dotest((void*) dph1->p_vaddr);

	elf_end(ed1);
	elf_end(ed2);
	exit(0);
}
