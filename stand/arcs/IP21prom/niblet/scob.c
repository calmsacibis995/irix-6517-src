/* scob.c -
 * 	Create a "cob" which along with niblet makes an executable
 *	containing a mini-kernel and programs to run on it.
 *	Scob makes a "static" cob.  This contains all of the possible
 *	niblet tests.  Dcob later combines these into dynamic cobs
 *	which can be used to run niblet with a subset of the tests.

REVISION LOG
------------
3/25/94  RAR  Removed writing of #defines to nib_procs.h file, with the
               exception of the NUM_PROCS define which is required for
	       RAW_NIBLET mode.
3/28/94  RAR  Added "-f" option to allow list of tests to come from a file.
3/30/94  RAR  Fixed data section pointer bug by the addition of a data_
               offset_addr variable and the creation of the data section
               pointer a special case.  Problem is that the portion of the
               code which writes the sections does them section at a
               time/test by test, whereas the portion of the code which
               writes the static_table does it test at time/section by
	       section.  Thus, all offset_addr's were wrt the text section,
	       throwing off not only the data section pointers but also
	       the following test's text section pointer.  This situation
	       is only noticeable when there are tests in the testdomain
	       which have non-zero length .data sections.  NOTE: Similar
	       problems will arise if the BSS or SHARED sections ever
	       exist.
5/2/94  RAR   Fixed bug in the code which set the .data pointer.
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <ar.h>
#include <stdio.h>
#if COFF
#include <syms.h>
#endif
#include <scnhdr.h>
#include <filehdr.h>
#if COFF
#include <ldfcn.h> 
#endif
#include <getopt.h>
#include <libelf.h>
#include <string.h>
#include <malloc.h>
#include "niblet.h"
#include "cob.h"

#define DEFAULT_FILE_NAME	"nib_procs"

struct obj_inf objs[MAX_FILES];

/* Section names */
char scn_names[NUM_SECTS][LABEL_LENGTH] = {
        ".text",
        ".data",
        ".bss",
        "SHARED"
};

static void	read_file(struct obj_inf *, char *, int);
static void	elf_read_file(struct obj_inf *, char *, int);
static int	elf64_read_file(struct obj_inf *, Elf *);
static int	elf32_read_file(struct obj_inf *, Elf *);
#ifdef COFF
static void	coff_get_entry(LDFILE *, char *, int *);
static void	coff_read_file(struct obj_inf *, char *, int);
#endif
static void	read_SHARED(struct obj_inf *, int);
static void	read_EMPTY(struct obj_inf *, int);
static void	make_defs(struct obj_inf *, int, char *);
static void 	prg_code(struct obj_inf *, int, char *);
extern Elf64_Ehdr * elf64_getehdr(Elf *);
extern Elf64_Shdr * elf64_getshdr(Elf_Scn *);

int swizzle = 0;

extern char *optarg;
extern int optind, opterr;

main(int argc, char **argv)
{
	int i;
	int num_prgs=0;
	char base_name[FILENAME_MAX];
	char whole_name[FILENAME_MAX];
	int errflag = 0;
	int c;
	char *test_file = NULL;  /* Pointer to name of file specified via the "-f" option */
	char line[256];          /* Storage to hold a line of input from a file */
	char test_name[256];     /* Storage to hold name of test */
	FILE *tfptr = NULL;      /* File ptr for list of tests */

	strcpy(base_name, DEFAULT_FILE_NAME);

	while ((c = getopt(argc, argv, "o:f:")) != -1)
		switch (c) {

		case 'o':
		  strcpy(base_name, optarg);
		  break;
		case 'f':
		  test_file = optarg;
		  /* Open text data file for reading */
		  if ((tfptr = fopen(test_file, "r")) == NULL) {
		    fprintf(stderr, "%s: can't open %s\n", argv[0], test_file);
		    exit(1);
		  }
		  
		  printf("scob: Reading-\n");
		  while (fgets(line, 255, tfptr) != NULL) { /* Read to end of file */
		    test_name[0] = NULL;
		    strncat(test_name, line, (strlen(line)-1)); /* Remove trailing '\n' */
		    read_file(&(objs[num_prgs]), test_name, 99);
		    num_prgs++;
		  }
		  
		  fclose(tfptr);
		  if (ferror(tfptr)) {
		    fprintf(stderr, "%s: error writing file %s\n", argv[0], test_file);
		    exit(2);
		  }
		  
		  break;
		case '?':
		  errflag++;
		  break;
		} /* Switch */
	
	if (test_file == NULL) { /* then we haven't already read the list from a file */
	  num_prgs = argc - optind;
	
	  if (num_prgs > MAX_FILES) {
	    printf(" Can't cob more than %d files\n", MAX_FILES);
	    exit(1);
	  }

	}

	if (num_prgs == 0 || errflag) {
	  fprintf(stderr, "usage: scob [-o <outfile>] [-f <infile> | <test1>... <testn>]\n");
	  exit(1);
	}

	if (test_file == NULL) { /* then we haven't already read the list from a file */
	  printf("scob: Reading-\n");
	  for (i = 0; i < num_prgs; i++) {
	    read_file(&(objs[i]), argv[i + optind], 99);
	  }
	}
#ifdef DEBUG
	for (i = 0; i < num_prgs; i ++) {
		int i, j;
		printf("file: %s\n", objs[i].name);
		printf("--------------------------------\n");
		for (j = 0; j < NUM_SECTS; j++) {
			printf("Section: %s\n", scn_names[j]);
			printf("Size: %d\n", objs[i].scn_size[j]);
 			printf("Addr: 0x%lx\n", objs[i].scn_addr[j]);
			printf("End: 0x%lx\n", objs[i].scn_end[j]);
		}
		printf("Text starts with:\n");
		for (k = 0; k < 8; k++) {
#ifdef 32BIT
			printf("%x\t", objs[i].scn_ptr[TEXT_S][k]);
#else if 64BIT
			printf("%llx\t", objs[i].scn_ptr[TEXT_S][k]);
#endif
			if ((k % 4) == 3)
				printf("\n");
		}
	}
#endif
	strcpy(whole_name, base_name);
	strcat(whole_name, ".h");
	make_defs(objs, num_prgs, whole_name);
	strcpy(whole_name, base_name);
	strcat(whole_name, ".c");
	prg_code(objs, num_prgs, whole_name);

	return 0;
}



static void
read_file(struct obj_inf *obj, char *name, int max_length)
{
	int fd;
	char magic[4];
	char ElfIdent[4] = { 0x7f, 'E', 'L', 'F' };

	strncpy(obj->name, name, max_length);

	printf("\t%s...\n", obj->name);
	
	if ((fd = open(name, O_RDONLY)) == -1) {
		fprintf(stderr, "Cannot open file: %s\n", obj->name);
		exit (1);
	}

	if (read(fd, magic, 4) != 4) {
#if COFF		
		/* not ELF - let coff_read_file take care of it */
		close(fd);
		coff_read_file(obj, name, max_length);
#else 
		fprintf(stderr, "Bad magic number.  Coff file?\n");
#endif
	}

	close(fd);
	if (memcmp(magic, ElfIdent, 4) == 0)
		elf_read_file(obj, name, max_length);
	else {
#if COFF
		coff_read_file(obj, name, max_length);
#else 
		fprintf(stderr, "No coff read\n");
#endif
	}
	
	
}

static void
elf_read_file(struct obj_inf *obj, char *name, int max_length)
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

	int fd;
	Elf *elf;
	int Elf64;
	char *elf_ident;
	int retval;
	unsigned elf_v;  /* Margie added */

	if ((fd = open(name, O_RDONLY)) == -1) {
		fprintf(stderr, "Cannot open file: %s\n", obj->name);
		exit(1);
	}

	/* MARGIE added following if expr */
	if ((elf_v = elf_version(EV_CURRENT)) == NULL) {
		fprintf(stderr, "Bad elf version: %x\n", elf_v);
		exit(1);
	}

	if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
		fprintf(stderr,"Could not open ELF file: %s\n", obj->name);
		close(fd);
		exit(1);
	}

	if ((elf_ident = elf_getident(elf, NULL)) == NULL) {
		fprintf(stderr, "Could not find ELF identification: %s\n", obj->name);
		elf_end(elf);
		close(fd);
		exit(1);
	}

	Elf64 = 0;
	if (memcmp(MipsElf32Ident, elf_ident, EI_NIDENT) != 0) {
		if (memcmp(MipsElf64Ident, elf_ident, EI_NIDENT) == 0)
			Elf64 = 1;
		else {
			fprintf(stderr, "Not a valid MIPS ELF file.\n");
			elf_end(elf);
			close(fd);
			exit(1);
		}
	}

	if (Elf64)
		retval = elf64_read_file(obj, elf);
	else
		retval = elf32_read_file(obj, elf);

	elf_end(elf);
	close(fd);
	/* exit if an error occured */
	if (retval)	exit(1);
}


static int
elf64_read_file(struct obj_inf *obj, Elf *elf)
{
 	Elf_Scn *scn;
	Elf64_Ehdr *ehdr64;
	Elf64_Shdr *shdr64;
	int ndx, j;

	/* find the elf header & the section header table */
	if ((ehdr64 = elf64_getehdr(elf)) == NULL) {
		fprintf(stderr, "Can't get ELF header.\n");
		return(-1);
	}

	obj->entry_addr = ehdr64->e_entry;

	/* Margie added following line */
	ndx = ehdr64->e_shstrndx;
	for (j = 0; j < NUM_SECTS; j++) {
		if (strcmp(scn_names[j], "SHARED") == 0)
			read_SHARED(obj, j);
		else {
			scn = NULL;
			do {
				if ((scn = elf_nextscn(elf, scn)) == NULL)
					break;
				else
					shdr64 = elf64_getshdr(scn);

			} while (strcmp(scn_names[j], 
					elf_strptr(elf, ndx, shdr64->sh_name)));

			if (scn == NULL)
				read_EMPTY(obj, j);
			else {
				obj->scn_addr[j] = shdr64->sh_addr;
				obj->scn_size[j] = shdr64->sh_size;
				obj->scn_end[j] = shdr64->sh_addr +
							shdr64->sh_size - 1;
				obj->start_addr[j] = obj->end_addr[j] = 0;
				obj->real_size[j] = 0;
				if (j < NUM_REAL_SECTS) {
					obj->scn_ptr[j] = (unsigned *) malloc(
							obj->scn_size[j] + 1);
							/* "+1" for align */
					memcpy(obj->scn_ptr[j],
					       elf_getdata(scn, NULL)->d_buf,
					       obj->scn_size[j]);
				}
				else { /* don't need to read scns like .bss */
					obj->scn_ptr[j] = NULL;
				}
				if (obj->scn_size[j] & 7)
					obj->scn_size[j] += 8 -
							(obj->scn_size[j] % 8);
					/* Doubleword align the next guy. */
			} /* else regular section */
		} /* non-SHARED section */
	}/* for j */
	return(0);
}



static void
read_SHARED(struct obj_inf *obj, int j)
{
	obj->scn_addr[j] = SHARED_VADDR;
	obj->scn_size[j] = SHARED_LENGTH;
	obj->scn_end[j] = SHARED_VADDR + SHARED_LENGTH - 1;
	obj->real_size[j] = 0;
	obj->scn_ptr[j] = 0;
		
}

static void
read_EMPTY(struct obj_inf *obj, int j)
{
	obj->scn_addr[j] = 0;
	obj->scn_size[j] = 0;
	obj->scn_end[j] = 0;
	obj->start_addr[j] = obj->end_addr[j] = 0;
	obj->real_size[j] = 0;
	obj->scn_ptr[j] = (unsigned int *)NULL;
}

/* Make process table */
static void
make_defs(struct obj_inf *objs, int num_prgs, char *def_file_name) 
{

    FILE *defs;
    int i, j, k;
    printf("scob: Writing header file, %s\n", def_file_name);
    defs = fopen(def_file_name, "w");

    /* Write defines for each test's position in the structure */
    fprintf(defs, "#define\tNUM_PROCS\t%d\n", num_prgs);
#if !defined(RAW_NIBLET)
    for (i = 0; i < num_prgs; i ++) {
	fprintf(defs, "#define\t");
	/* Find the last part of the path name. */
	for (j = strlen(objs[i].name) - 1;
					 j >= 0 && objs[i].name[j] != '/';
					j--)
		;
	/* Uppercase the test name for the define */
	for (k = j + 1; objs[i].name[k] != '\0'; k++)
		fputc(toupper(objs[i].name[k]), defs);
	fprintf(defs, "\t%d\n", i);
    }
#endif
    fclose(defs);
}


static void
prg_code(struct obj_inf *objs, int num_prgs, char *code_file_name)
{
    FILE *prg;
    int i, j, k;
    int offset_addr;
    int data_offset_addr;

    printf("scob: Writing program data, %s\n", code_file_name);

    prg = fopen(code_file_name, "w");

    fprintf(prg, "/* This file was generated by scob.  It contains Niblet\n");
    fprintf(prg, " * programs and a data structure which the dynamic cob\n");
    fprintf(prg, " * program uses to find and prepare them.\n */");
    fprintf(prg, "\n\n#include \"../cob.h\"\n\n");

    fprintf(prg, "unsigned int nib_prgs[] = {\n");

    offset_addr = 0;
    for (i = 0; i < NUM_REAL_SECTS; i++) {
	if (i == DATA_S) /* Set data_offset_addr = end of text section */
	  data_offset_addr = offset_addr * 4;
	fprintf(prg, "\n/* Section: %s */", scn_names[i]);
	for (j = 0; j < num_prgs; j++) {
	    fprintf(prg, "\n/* Program: %s */\n", objs[j].name);
	    if (objs[j].scn_size[i] != 0) {
		for (k = 0; k < (objs[j].scn_size[i] / sizeof(int)); k++) {
		    if (k == 0 && i == 0 && j == 0)
			fprintf(prg, "/*       0:*/\t0x%x", 
						(objs[j].scn_ptr[i])[k^swizzle]);
		    else if (k % 4 == 0)
			fprintf(prg, ",\n/*%8x:*/\t0x%x", (offset_addr + k) * 4,
						(objs[j].scn_ptr[i])[k^swizzle]);
		    else
			fprintf(prg, ",    \t0x%x", (objs[j].scn_ptr[i])[k^swizzle]);

		}
		/* Margie added "/ 4" because we are using offset_addr as a word address! */
		offset_addr += (objs[j].scn_size[i] / 4);
	    } /* if objs[j].scn_size...  */
	} /* for j */
    } /* for i */
    fprintf(prg, "\n};\n\n");

    offset_addr = 0;
    fprintf(prg, "struct obj_inf static_table[] = {\n");
    for (i = 0; i < num_prgs; i++) {
	fprintf(prg, "    {\n");
	fprintf(prg, "\t\"%s\",\n", objs[i].name);

	/* Section Size (scn_size) */
	fprintf(prg, "\t{ ");
	for (j = 0; j < NUM_SECTS; j++) 
		fprintf(prg, "0x%x%s", objs[i].scn_size[j],
						j == NUM_SECTS - 1 
						? " },\t/* scn_size */\n"
						: ",\t");

	/* Section address (scn_addr) */
	fprintf(prg, "\t{ ");
	for (j = 0; j < NUM_SECTS; j++)  {
	    fprintf(prg, "0x%llx", objs[i].scn_addr[j]);
	    if (j == NUM_SECTS - 1 ) {
		fprintf(prg, "%s",  "},\t/* scn_addr */\n" );
	    }
	    else {
		fprintf(prg, "%s", ",\t");
	    }

	}
#if 0
		fprintf(prg, "0x%llx%s", objs[i].scn_addr[j],
						j == NUM_SECTS - 1 
						? " },\t/* scn_addr */\n" 
						: ",\t");
#endif
	
	/* Section end (scn_end) */
	fprintf(prg, "\t{ ");
	for (j = 0; j < NUM_SECTS; j++)  {
	    fprintf(prg, "0x%llx", objs[i].scn_end[j]);
	    if (j == NUM_SECTS - 1) {
		fprintf(prg, "%s"," },\t/* scn_end */\n");
	    }
	    else {
		fprintf(prg, "%s", ",\t");
	    }
	}

	    
#if 0
		fprintf(prg, "0x%llx%s", objs[i].scn_end[j],
						j == NUM_SECTS - 1 
						? " },\t/* scn_end */\n" 
						: ",\t");
#endif

	/* Section pointer (scn_ptr) */
	fprintf(prg, "\t{\n");
	for (j = 0; j < NUM_SECTS; j++) {

	  if (j == DATA_S) { /* Kludge fix for data section pointer bug */
	    fprintf(prg, "\t\tnib_prgs + 0x%x%s",
		    (data_offset_addr / sizeof(unsigned int)), ",\n");
	    data_offset_addr += objs[i].scn_size[j];
	  } else { /* Original code */
	    fprintf(prg, "\t\tnib_prgs + 0x%x%s",
		    (offset_addr / sizeof(unsigned int)),
		    j == NUM_SECTS - 1 ? "\n\t},\n": ",\n");
	    if (j < NUM_REAL_SECTS)
	      offset_addr += objs[i].scn_size[j];
	    /* Don't go adding space for BSS, SHARED, etc. */
	  }

	}

	/* start_page, num_pages, end_page */
	for (j = 0; j < 3; j++) {
		fprintf(prg, "\t{ ");
		for (k = 0; k < NUM_SECTS; k++)
			fprintf(prg, "0x0%s",
					k == NUM_SECTS - 1 ? " },\n" : ",");
	}
	fprintf(prg, "\t0x0,\t/* Page table address place holder */\n");
	fprintf(prg, "\t0x0,\t/* Page table end place holder */\n");  /* MARGIE added this! */
	fprintf(prg, "\t0x%llx\t/* Entry address */\n", objs[i].entry_addr);

        if (i == num_prgs - 1)
		fprintf(prg, "    }\n");
	else
		fprintf(prg, "    },\n");
    } /* for i */
    fprintf(prg, "};\n\n");  /* End of the horrendous table */

    fclose(prg);
}


static int
elf32_read_file(struct obj_inf *obj, Elf *elf)
{
	Elf_Scn *scn;
	Elf32_Ehdr *ehdr32;
	Elf32_Shdr *shdr32;
	int ndx, j;

	/* find the elf header */
	if ((ehdr32 = elf32_getehdr(elf)) == NULL) {
		fprintf(stderr, "Can't get ELF header.\n");
		return(-1);
	}

	obj->entry_addr = ehdr32->e_entry;

	/* Margie added following line */
	ndx = ehdr32->e_shstrndx;
	for (j = 0; j < NUM_SECTS; j++); {
		if (strcmp(scn_names[j], "SHARED") == 0)
			read_SHARED(obj, j);
		else {
			scn = NULL;
			do {
				if ((scn = elf_nextscn(elf, scn)) == NULL)
					break;
				else
					shdr32 = elf32_getshdr(scn);

			} while (strcmp(scn_names[j],
				 elf_strptr(elf, ndx, shdr32->sh_name)));
			if (scn == NULL)
				read_EMPTY(obj, j);
			else {
				obj->scn_addr[j] = shdr32->sh_addr;
				obj->scn_size[j] = shdr32->sh_size;
				obj->scn_end[j] = shdr32->sh_addr +
							shdr32->sh_size - 1;
				obj->start_addr[j] = obj->end_addr[j] = 0;
				obj->real_size[j] = 0;
				if (j < NUM_REAL_SECTS) {
					obj->scn_ptr[j] = (unsigned *) malloc(
							obj->scn_size[j] + 1);
							/* "+1" for align */
					memcpy(obj->scn_ptr[j],
					       elf_getdata(scn, NULL)->d_buf,
					       obj->scn_size[j]);
				}
				else { /* don't need to read scns like .bss */
					obj->scn_ptr[j] = NULL;
				}
				if (obj->scn_size[j] & 7)
					obj->scn_size[j] += 8 -
							(obj->scn_size[j] % 8);
					/* Doubleword align the next guy. */
			} /* else regular section */
		} /* non-SHARED section */
	}/* for j */
	return(0);
}

#ifdef COFF
void
coff_get_entry(LDFILE *ldptr, char *name, int *value)
{
	PDR ppd;
	long i;
	long  symmax, symmaxlocal, symmaxextern;
	SYMR symbol;
	char *namep;

	symmaxlocal = SYMHEADER(ldptr).isymMax;
	symmaxextern = SYMHEADER(ldptr).iextMax;
	symmax = symmaxlocal + symmaxextern;

	for (i = 0; i < symmax; i++) {
		if (ldtbread(ldptr, i, &symbol) == FAILURE) {
			*value = 0;
			return;
		}
		namep = ldgetname(ldptr, &symbol);
		if (!strcmp(namep, name)) {
			*value = symbol.value;	
			break;
		}
	}
	if (i >= symmax)
		*value = 0;
	return;
}
#endif

#ifdef COFF
static void
coff_read_file(struct obj_inf *obj, char *name, int max_length)
{
	LDFILE *o_file;
	int j;
	SCNHDR scn_hdr;

	o_file = ldopen(obj->name, (LDFILE *)NULL);

	if (o_file == (LDFILE *)NULL) {
		fprintf(stderr, "Cannot open file: %s\n", obj->name);
		exit (1);
	}

	if(ldreadst(o_file,-1) == FAILURE) {
		/* This binary has no symbol table */
		obj->entry_addr = 0;
	} else {
		coff_get_entry(o_file, "ENTRY", &(obj->entry_addr));	
	}

	for (j = 0; j < NUM_SECTS; j++) {
	    if (strcmp(scn_names[j], "SHARED") == 0)
		read_SHARED(obj, j);
	    else if (ldnshread(o_file, scn_names[j], &scn_hdr) == SUCCESS) {
		obj->scn_addr[j] = scn_hdr.s_vaddr;
		obj->scn_size[j] = scn_hdr.s_size;
		obj->scn_end[j] = scn_hdr.s_vaddr + 
						scn_hdr.s_size - 1;
		obj->start_addr[j] = obj->end_addr[j] = 0;
		obj->real_size[j] = 0;
		if (j < NUM_REAL_SECTS) {
			ldnsseek(o_file, scn_names[j]);
			obj->scn_ptr[j] = 
				(unsigned *)malloc(obj->scn_size[j] + 1);
				/* Plus 1 so we can doubleword align `em */
			FREAD((char *)obj->scn_ptr[j], obj->scn_size[j], 
				1, o_file);
		} else {
			/* Don't need to read "fake" sections like BSS */
			obj->scn_ptr[j] = (unsigned *)NULL;
		}
		if (obj->scn_size[j] & 7)
			obj->scn_size[j] += 8 - (obj->scn_size[j] % 8);
			/* Doubleword align the next guy. */
	    } else
		read_EMPTY(obj, j);
	} /* for j */
	ldclose(o_file);
}
#endif
