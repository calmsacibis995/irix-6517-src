/* nextract.c -
 *	Extract the text and data from niblet and put them into an
 *	array of unsigned integers to be compiled into the PROM.
 */

#include <ar.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <scnhdr.h>
#include <filehdr.h>
#include <getopt.h>
#include <string.h>
#include <libelf.h>
#include <unistd.h>
#include <malloc.h>
#include <stdlib.h>


#define NUM_SECTS	5
#define NUM_REAL_SECTS	3

#define LINE_SIZE	4

#define TEXT_S          0
#define DATA_S          1
#define RDATA_S		2
#ifdef BSS_S
#undef BSS_S
#endif
#define BSS_S		3
#define SBSS_S		4

#ifdef COFF
char coff_scn_names[NUM_SECTS][8] = {
	".text",
	".data",
	".rdata",
	".bss",
	".sbss"
} ;

#endif

char *elf_scn_names[NUM_SECTS] = {
	".text",
	".data",
	".rodata",
	".bss",
	".sbss"
} ;


typedef unsigned long long addr_type;
typedef unsigned int scn_data;
typedef scn_data *scn_data_ptr;

struct obj_inf {
	unsigned int scn_size[NUM_SECTS];
	addr_type scn_addr[NUM_SECTS];
	addr_type scn_end[NUM_SECTS];
	scn_data_ptr scn_ptr[NUM_SECTS];
} obj;


struct obj_inf obj;
extern char *optarg;
extern int optind, opterr;

/* prototypes */
static void	read_file(struct obj_inf *, char *);
#ifdef COFF
static void	coff_read_file(struct obj_inf *, char *);
#endif
static void	elf_read_file(struct obj_inf *, char *);
static int	elf32_read_file(struct obj_inf *, Elf *);
static int	elf64_read_file(struct obj_inf *, Elf *);
static void	elf32_generate_syms(Elf *, struct obj_inf *obj);
static void	elf64_generate_syms(Elf *, struct obj_inf *obj);
static void	elf32_get_entry(Elf *, char *, addr_type *);
static void	elf64_get_entry(Elf *, char *, addr_type *);
static void	make_data(struct obj_inf *, int);
static void	read_EMPTY(struct obj_inf *, int j);
static void	print_line(unsigned int *, int, int, unsigned int);
extern Elf64_Ehdr * elf64_getehdr(Elf *);
extern Elf64_Shdr * elf64_getshdr(Elf_Scn *);


main(int argc, char **argv)
{
	int errflag = 0;
	int swizzle = 0;
	int c;

        while ((c = getopt(argc, argv, "s:")) != -1)
	    switch (c) {
		case 's':
			swizzle = atoi(optarg);
			break;
		case '?':
			errflag++;
			break;
	    }

	if (errflag || (optind != argc - 1)) { /* Bad opt or wrong # files */
            fprintf(stderr, 
		     "usage: extract [-s number_to_xor] file\n");
	    exit(1);
	}

	read_file(&obj, argv[optind]);

	make_data(&obj, swizzle);

	exit(0);
}



static void
elf32_get_entry(Elf *elf, char *name, addr_type *value)
{
	Elf_Scn *scn;
	int ndx, i, symtabsize;
	Elf32_Shdr *symtabshdr32;
	Elf32_Sym *symtab;

	ndx = elf32_getehdr(elf)->e_shstrndx;

	/* find the symbol table section */
	scn = NULL;
	do {
		if ((scn = elf_nextscn(elf, scn)) == NULL)
			break;
		else
			symtabshdr32 = elf32_getshdr(scn);
	} while (strcmp(".symtab",
		 elf_strptr(elf, ndx, symtabshdr32->sh_name)));
	if (scn == NULL) {
		fprintf(stderr, "Cannot find symbol table in ELF file\n");
		exit(1);
	}

	/* find the symbol table */
	symtab = (Elf32_Sym *)(elf_getdata(scn, NULL)->d_buf);
	symtabsize = (elf_getdata(scn, NULL)->d_size)/sizeof(Elf32_Sym);
	/* keep looking at entries until the right one is found */
	for(i = 0; i < symtabsize; i++)
	if (strcmp(name,
		   elf_strptr(elf, symtabshdr32->sh_link, symtab[i].st_name))
	    == 0)
		break;
		
	if (i == symtabsize) {
		fprintf(stderr, "nextract: Fatal error - couln't find symbol: %s\n", name);
		exit(1);
	}
	else
		*value = symtab[i].st_value;

}

static void
elf64_get_entry(Elf *elf, char *name, addr_type *value)
{
	Elf_Scn *scn;
	int ndx, i, symtabsize;
	Elf64_Shdr *symtabshdr64;
	Elf64_Sym *symtab;

	ndx = ((Elf64_Ehdr *)elf64_getehdr(elf))->e_shstrndx;

	/* find the symbol table section */
	scn = NULL;
	do {
		if ((scn = elf_nextscn(elf, scn)) == NULL)
			break;
		else
			symtabshdr64 = elf64_getshdr(scn);
	} while (strcmp(".symtab",
		 elf_strptr(elf, ndx, symtabshdr64->sh_name)));
	if (scn == NULL) {
		fprintf(stderr, "Cannot find symbol table in ELF file\n");
		exit(1);
	}

	/* find the symbol table */
	symtab = (Elf64_Sym *)(elf_getdata(scn, NULL)->d_buf);
	symtabsize = (elf_getdata(scn, NULL)->d_size)/sizeof(Elf64_Sym);
	/* keep looking at entries until the right one is found */
	for(i = 0; i < symtabsize; i++) {
	    if (strcmp(name,
		       elf_strptr(elf, symtabshdr64->sh_link, symtab[i].st_name)) == 0)
		break;
	}
		
	if (i == symtabsize) {
		fprintf(stderr, "nextract: Fatal error - couln't find symbol: %s\n", name);
		exit(1);
	}
	else {
		*value = symtab[i].st_value;
	    }
}


static void
elf32_generate_syms(Elf *elf, struct obj_inf *obj)
{
	addr_type nib_obj_start, nib_text_entry;
	addr_type nib_slave_entry;
	addr_type nib_exc_start, nib_exc_end;
	addr_type nib_text_start;
#if 0
	addr_type nib_text_end, nib_data_start, nib_data_end;
#endif

	elf32_get_entry(elf, "nib_obj_start", &nib_obj_start);
	elf32_get_entry(elf, "nib_text_entry", &nib_text_entry);
	elf32_get_entry(elf, "nib_slave_entry", &nib_slave_entry);
	elf32_get_entry(elf, "nib_exc_start", &nib_exc_start);
	elf32_get_entry(elf, "nib_exc_end", &nib_exc_end);
	elf32_get_entry(elf, "nib_text_start", &nib_text_start);

/* We need to get Nible't text start address so we don't copy the exception
 *	handlers too, but the end of text and start and end of data
 *	should come from the object file.
 */

#ifdef BIG_MISTAKE
	elf32_get_entry(elf, "nib_text_end", &nib_text_end);
	elf32_get_entry(elf, "nib_data_start", &nib_data_start);
	elf32_get_entry(elf, "nib_data_end", &nib_data_end);
#endif

	printf("/* nib_code.c: Linked Niblet code stuffed into an array.*/\n");
	printf("\n");
	printf("/* Start of niblet code and entry point */\n");
	printf("unsigned int nib_obj_start = 0x%x;\n", nib_obj_start);
	printf("unsigned int nib_text_entry = 0x%x;\n\n", nib_text_entry);
	printf("/* Start of niblet code and entry point */\n");
	printf("unsigned int nib_slave_entry = 0x%x;\n\n", nib_slave_entry);
	printf("/* Link addresses and object offset for Niblet exception handlers */\n");
	printf("unsigned int nib_exc_start = 0x%x;\n", nib_exc_start);
	printf("unsigned int nib_exc_end = 0x%x;\n", nib_exc_end);
	printf("unsigned int nib_exc_off = 0x%x;\n\n",
						nib_exc_start - nib_obj_start);
	printf("/* Link addresses and object offset for Niblet text */\n");
	printf("unsigned int nib_text_start = 0x%x;\n", nib_text_start);
	printf("unsigned int nib_text_end = 0x%x;\n", obj->scn_end[TEXT_S]);
	printf("unsigned int nib_text_off = 0x%x;\n\n",
					nib_text_start - nib_obj_start);
	printf("/* Link addresses and object offset for Niblet data */\n");
	printf("unsigned int nib_data_start = 0x%x;\n", obj->scn_addr[DATA_S]);
	printf("unsigned int nib_data_end = 0x%x;\n", obj->scn_end[DATA_S]);
	printf("unsigned int nib_data_off = 0x%x;\n\n",
					obj->scn_addr[DATA_S] - nib_obj_start);
	printf("/* BSS and SBSS addrs. */\n");
	printf("unsigned int nib_bss_start = 0x%x;\n", obj->scn_addr[BSS_S]);
	printf("unsigned int nib_bss_end = 0x%x;\n", obj->scn_end[BSS_S]);
	printf("unsigned int nib_sbss_start = 0x%x;\n", obj->scn_addr[SBSS_S]);
	printf("unsigned int nib_sbss_end = 0x%x;\n\n", obj->scn_end[SBSS_S]);

	return;
}

static void
elf64_generate_syms(Elf *elf, struct obj_inf *obj)
{
	addr_type nib_obj_start, nib_text_entry;
	addr_type nib_slave_entry;
	addr_type nib_exc_start, nib_exc_end;
	addr_type nib_text_start;
#if 0
	addr_type nib_text_end nib_data_start, nib_data_end;
#endif

	elf64_get_entry(elf, "nib_obj_start", &nib_obj_start);
	elf64_get_entry(elf, "nib_text_entry", &nib_text_entry);
	elf64_get_entry(elf, "nib_slave_entry", &nib_slave_entry);
	elf64_get_entry(elf, "nib_exc_start", &nib_exc_start);
	elf64_get_entry(elf, "nib_exc_end", &nib_exc_end);
	elf64_get_entry(elf, "nib_text_start", &nib_text_start);

/* We need to get Nible't text start address so we don't copy the exception
 *	handlers too, but the end of text and start and end of data
 *	should come from the object file.
 */

#ifdef BIG_MISTAKE
	elf64_get_entry(elf, "nib_text_end", &nib_text_end);
	elf64_get_entry(elf, "nib_data_start", &nib_data_start);
	elf64_get_entry(elf, "nib_data_end", &nib_data_end);
#endif

	printf("/* nib_code.c: Linked Niblet code stuffed into an array.*/\n");
	printf("\n");
	printf("/* Start of niblet code and entry point */\n");
	printf("unsigned long long nib_obj_start = 0x%llx;\n", nib_obj_start);
	printf("unsigned long long nib_text_entry = 0x%llx;\n\n", nib_text_entry);
	printf("/* Start of niblet code and entry point */\n");
	printf("unsigned long long nib_slave_entry = 0x%llx;\n\n", nib_slave_entry);
	printf("/* Link addresses and object offset for Niblet exception handlers */\n");
	printf("unsigned long long nib_exc_start = 0x%llx;\n", nib_exc_start);
	printf("unsigned long long nib_exc_end = 0x%llx;\n", nib_exc_end);
	printf("unsigned long long nib_exc_off = 0x%llx;\n\n",
						nib_exc_start - nib_obj_start);
	printf("/* Link addresses and object offset for Niblet text */\n");
	printf("unsigned long long nib_text_start = 0x%llx;\n", nib_text_start);
	printf("unsigned long long nib_text_end = 0x%llx;\n", obj->scn_end[TEXT_S]);
	printf("unsigned long long nib_text_off = 0x%llx;\n\n",
					nib_text_start - nib_obj_start);
	printf("/* Link addresses and object offset for Niblet data */\n");
	printf("unsigned long long nib_data_start = 0x%llx;\n", obj->scn_addr[DATA_S]);
	printf("unsigned long long nib_data_end = 0x%llx;\n", obj->scn_end[DATA_S]);
	printf("unsigned long long nib_data_off = 0x%llx;\n\n",
					obj->scn_addr[DATA_S] - nib_obj_start);
	printf("/* BSS and SBSS addrs. */\n");
	printf("unsigned long long nib_bss_start = 0x%llx;\n", obj->scn_addr[BSS_S]);
	printf("unsigned long long nib_bss_end = 0x%llx;\n", obj->scn_end[BSS_S]);
	printf("unsigned long long nib_sbss_start = 0x%llx;\n", obj->scn_addr[SBSS_S]);
	printf("unsigned long long nib_sbss_end = 0x%llx;\n\n", obj->scn_end[SBSS_S]);

	return;
}

static void
read_file(struct obj_inf *obj, char *name)
{
        int fd;
        char magic[4];
        char ElfIdent[4] = { 0x7f, 'E', 'L', 'F' };

        if ((fd = open(name, O_RDONLY)) == -1) {
                fprintf(stderr, "Cannot open file: %s\n", name);
                exit (1);
        }

        if ((read(fd, &magic, 4)) != 4) {
                /* not ELF - let coff_read_file take care of it */
                close(fd);
#ifdef COFF
                coff_read_file(obj, name);
#else
		fprintf(stderr, "Coff file formats not handled\n");
#endif
        }

        close(fd);
        if (memcmp(magic, ElfIdent, 4) == 0)
                elf_read_file(obj, name);
        else {
#ifdef COFF	    
                coff_read_file(obj, name);
#else 
		fprintf(stderr, "Coff fiel formats not handlerd\n");
#endif		
	    }
	
}

static void
elf_read_file(struct obj_inf *obj, char *name)
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

	if ((fd = open(name, O_RDONLY)) == -1) {
		fprintf(stderr, "Cannot open file: %s\n", name);
		exit(1);
	}

	if (elf_version(EV_CURRENT) == NULL) {
		fprintf(stderr, "ELF library out of date.\n");
		exit(1);
	}


	if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
		fprintf(stderr,"Could not open ELF file: %s\n", name);
		close(fd);
		exit(1);
	}

	if ((elf_ident = elf_getident(elf, NULL)) == NULL) {
		fprintf(stderr, "Could not find ELF identification: %s\n", name);
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
	if (retval)     exit(1);
}

static int
elf32_read_file(struct obj_inf *obj, Elf *elf)
{
	Elf_Scn *scn;
	Elf32_Ehdr *ehdr32;
	Elf32_Shdr *shdr32;
	int ndx, j;
	int file_ok = 0;

	/* find the elf header & the section header table */
	if ((ehdr32 = elf32_getehdr(elf)) == NULL) {
		fprintf(stderr, "Can't get ELF header.\n");
		return(-1);
	}

	/* get the index of the section header string table */
	ndx = ehdr32->e_shstrndx;

	for (j = 0; j < NUM_SECTS; j++) {
		scn = NULL;
		do {
			if ((scn = elf_nextscn(elf, scn)) == NULL)
				break;
			else
				shdr32 = elf32_getshdr(scn);
		} while (strcmp(elf_scn_names[j],
			 elf_strptr(elf, ndx, shdr32->sh_name)));
		if (scn == NULL)
			read_EMPTY(obj, j);
		else {
			obj->scn_addr[j] = shdr32->sh_addr;
			obj->scn_size[j] = shdr32->sh_size;
			obj->scn_end[j] = shdr32->sh_addr +
							shdr32->sh_size - 1;
			if (j < NUM_REAL_SECTS) {
				obj->scn_ptr[j] = (scn_data_ptr) malloc(
							obj->scn_size[j] + 1);
				memcpy(obj->scn_ptr[j],
				       elf_getdata(scn, NULL)->d_buf,
				       obj->scn_size[j]);
			}
			else /* don't need to read scns like .bss */
				obj->scn_ptr[j] = NULL;
			file_ok = 1;
		}
        }/* for j */

	if (!file_ok) {
		fprintf(stderr, "No sections were found\n");
		return(-1);
	}

	elf32_generate_syms(elf, obj);
	return(0);
}

static int
elf64_read_file(struct obj_inf *obj, Elf *elf)
{
	Elf_Scn *scn;
	Elf64_Ehdr *ehdr64;
	Elf64_Shdr *shdr64;
	int ndx, j;
	int file_ok = 0;

	/* find the elf header & the section header table */
	if ((ehdr64 = elf64_getehdr(elf)) == NULL) {
		fprintf(stderr, "Can't get ELF header.\n");
		return(-1);
	}

	/* get the index of the section header string table */
	ndx = ehdr64->e_shstrndx;

	for (j = 0; j < NUM_SECTS; j++) {
		scn = NULL;
		do {
			if ((scn = elf_nextscn(elf, scn)) == NULL)
				break;
			else
				shdr64 = elf64_getshdr(scn);
		} while (strcmp(elf_scn_names[j],
			 elf_strptr(elf, ndx, shdr64->sh_name)));
		if (scn == NULL)
			read_EMPTY(obj, j);
		else {
			obj->scn_addr[j] = shdr64->sh_addr;
			obj->scn_size[j] = shdr64->sh_size;
			obj->scn_end[j] = shdr64->sh_addr +
							shdr64->sh_size - 1;
			if (j < NUM_REAL_SECTS) {
				obj->scn_ptr[j] = (scn_data_ptr) malloc(
							obj->scn_size[j] + 1);
				memcpy(obj->scn_ptr[j],
				       elf_getdata(scn, NULL)->d_buf,
				       obj->scn_size[j]);
			}
			else /* don't need to read scns like .bss */
				obj->scn_ptr[j] = NULL;
			file_ok = 1;
		}
        }/* for j */

	if (!file_ok) {
		fprintf(stderr, "No sections were found\n");
		return(-1);
	}

	elf64_generate_syms(elf, obj);
	return(0);
}


static void
read_EMPTY(struct obj_inf *obj, int j)
{
	obj->scn_addr[j] = 0;
	obj->scn_size[j] = 0;
	obj->scn_end[j] = 0;
	obj->scn_ptr[j] = 0;
}

static void
make_data(struct obj_inf *obj, int swizzle)
{
	int i, j, k;
	addr_type high_addr, low_addr;
	int real_num = NUM_REAL_SECTS;
	unsigned int cur_scn;
	addr_type cur_addr, next_scn_start, cur_scn_end;
	scn_data line_buf[LINE_SIZE];
	unsigned cur_index, cur_scn_ptr;
	int cur_line;
	int scn_swizzle;

	int sorted[NUM_REAL_SECTS];

	for (i= 0; i < NUM_REAL_SECTS; i++)
		sorted[i] = i;

	/* do a cheesy bubble sort in case we add more sections */
	for (i = 0; i < NUM_REAL_SECTS; i ++)
		for (j = NUM_REAL_SECTS - 1; j > 0; j--) {
			if (obj->scn_addr[sorted[j]] < 	
					obj->scn_addr[sorted[j-1]]){
				k = sorted[j];
				sorted[j] = sorted[j-1];
				sorted[j-1] = k;
			}
		}	

	for (i = 0; i < real_num; )
		if (obj->scn_size[sorted[i]] == 0) {
			for (j = 1; j <= real_num; j++)
				sorted[j - 1] = sorted[j];
			real_num--;
		} else {
			i++;
	}
	high_addr = obj->scn_end[sorted[real_num - 1]];
	low_addr = obj->scn_addr[sorted[0]];

#ifdef DEBUG
	fprintf(stderr, "File starts at 0x%x\n", obj->scn_addr[sorted[0]]);
	fprintf(stderr, "Ends at 0x%x\n", obj->scn_end[sorted[real_num - 1]]);

#ifdef COFF
	for (i = 0; i < real_num; i++)
		fprintf(stderr, "section %d (%s) starts at 0x%x\n",
					sorted[i], coff_scn_names[sorted[i]], 
					obj->scn_addr[sorted[i]]);
#else
	for (i = 0; i < real_num; i++)
		fprintf(stderr, "section %d (%s) starts at 0x%x\n",
					sorted[i], elf_scn_names[sorted[i]], 
					obj->scn_addr[sorted[i]]);
#endif
#endif

	cur_scn = 0;
	cur_addr = low_addr;
	cur_scn_ptr = 0;
	cur_index = 0; /* Big endian */
	cur_line = 0;

	printf("/* Niblet's stuffed into this array: */\n");
	printf("unsigned int niblet_code[] = {\n");

	for (i = 0; i < LINE_SIZE; i++)
		line_buf[i] = 0;

	while(cur_addr <= high_addr) {
	    next_scn_start = ((cur_scn + 1) == real_num)? high_addr + 1 :
				obj->scn_addr[sorted[cur_scn+1]];
	    cur_scn_end = obj->scn_end[sorted[cur_scn]];
	    if (sorted[cur_scn] == TEXT_S)
		scn_swizzle = 0;
	    else
		scn_swizzle = swizzle;
	    while (cur_addr < next_scn_start) {
		if (cur_addr < cur_scn_end)
		    line_buf[cur_index] =
				obj->scn_ptr[sorted[cur_scn]][cur_scn_ptr];
		else
		    line_buf[cur_index] = 0;

		if (cur_index == LINE_SIZE - 1) {	/* Big endian */
		/* If all eight words are full */
			print_line(line_buf, LINE_SIZE, swizzle,
				cur_addr - cur_index * sizeof(unsigned int));
			cur_line++;
		}
		cur_addr+= 4;
		cur_scn_ptr += 1;
		cur_index = (cur_index + 1)  % LINE_SIZE;	/* Big endian */
	    }
#ifdef DEBUG
	    fprintf(stderr, 
	    	"Current address is %x, starting section %d at line %d\n",
					cur_addr, cur_scn, cur_line);
#endif
	    cur_scn++;
	    cur_scn_ptr = 0;
	}
	if (cur_index != 0) { /* Big endian */
		for (i = cur_index; i >= 0; i--)
			line_buf[i] = 0;
		print_line(line_buf, LINE_SIZE, scn_swizzle,
				cur_addr - cur_index * sizeof(unsigned int));
	}

	printf("\t0x00000000\n};\n");
}


static void
print_line(unsigned int *line_buf, int line_size, int swizzle, unsigned int addr)
{
	int i;

	printf("/* %8x */", addr);
	for (i = 0; i < LINE_SIZE; i++) {
		printf("\t0x%x,", line_buf[i^swizzle]);
	}
	printf("\n");
	
}

#ifdef COFF
static void
coff_get_entry(LDFILE *ldptr, char *name, unsigned int *value)
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
                if (FAILURE == ldtbread(ldptr, i, &symbol)) {
			fprintf(stderr, "nextract: Fatal error - couldn't open symbol table: %s", name);
			exit(1);
		}
                namep = ldgetname(ldptr, &symbol);
                if (!strcmp(namep, name)) {
                        *value = symbol.value;
                        break;
                }
        }
        if (i >= symmax) {
                if (FAILURE == ldtbread(ldptr, i, &symbol)) {
			fprintf(stderr, "nextract: Fatal error - couldn't find symbol: %s", name);
			exit(1);
		}
                *value = 0;
	}
        return;
}

static void
coff_generate_syms(LDFILE *o_file, struct obj_inf *obj)
{
	unsigned int nib_obj_start, nib_text_entry;
	unsigned int nib_slave_entry;
	unsigned int nib_exc_start, nib_exc_end;
	unsigned int nib_text_start, nib_text_end;
	unsigned int nib_data_start, nib_data_end;

	coff_get_entry(o_file, "nib_obj_start", &nib_obj_start);
	coff_get_entry(o_file, "nib_text_entry", &nib_text_entry);
	coff_get_entry(o_file, "nib_slave_entry", &nib_slave_entry);
	coff_get_entry(o_file, "nib_exc_start", &nib_exc_start);
	coff_get_entry(o_file, "nib_exc_end", &nib_exc_end);
	coff_get_entry(o_file, "nib_text_start", &nib_text_start);

/* We need to get Nible't text start address so we don't copy the exception
 *	handlers too, but the end of text and start and end of data
 *	should come from the object file.
 */

#ifdef BIG_MISTAKE
	coff_get_entry(o_file, "nib_text_end", &nib_text_end);
	coff_get_entry(o_file, "nib_data_start", &nib_data_start);
	coff_get_entry(o_file, "nib_data_end", &nib_data_end);
#endif

	printf("/* nib_code.c: Linked Niblet code stuffed into an array.*/\n");
	printf("\n");
	printf("/* Start of niblet code and entry point */\n");
	printf("unsigned int nib_obj_start = 0x%x;\n", nib_obj_start);
	printf("unsigned int nib_text_entry = 0x%x;\n\n", nib_text_entry);
	printf("/* Start of niblet code and entry point */\n");
	printf("unsigned int nib_slave_entry = 0x%x;\n\n", nib_slave_entry);
	printf("/* Link addresses and object offset for Niblet exception handlers */\n");
	printf("unsigned int nib_exc_start = 0x%x;\n", nib_exc_start);
	printf("unsigned int nib_exc_end = 0x%x;\n", nib_exc_end);
	printf("unsigned int nib_exc_off = 0x%x;\n\n",
						nib_exc_start - nib_obj_start);
	printf("/* Link addresses and object offset for Niblet text */\n");
	printf("unsigned int nib_text_start = 0x%x;\n", nib_text_start);
	printf("unsigned int nib_text_end = 0x%x;\n", obj->scn_end[TEXT_S]);
	printf("unsigned int nib_text_off = 0x%x;\n\n",
					nib_text_start - nib_obj_start);
	printf("/* Link addresses and object offset for Niblet data */\n");
	printf("unsigned int nib_data_start = 0x%x;\n", obj->scn_addr[DATA_S]);
	printf("unsigned int nib_data_end = 0x%x;\n", obj->scn_end[DATA_S]);
	printf("unsigned int nib_data_off = 0x%x;\n\n",
					obj->scn_addr[DATA_S] - nib_obj_start);
	printf("/* BSS and SBSS addrs. */\n");
	printf("unsigned int nib_bss_start = 0x%x;\n", obj->scn_addr[BSS_S]);
	printf("unsigned int nib_bss_end = 0x%x;\n", obj->scn_end[BSS_S]);
	printf("unsigned int nib_sbss_start = 0x%x;\n", obj->scn_addr[SBSS_S]);
	printf("unsigned int nib_sbss_end = 0x%x;\n\n", obj->scn_end[SBSS_S]);

	return;
}

static void
coff_read_file(struct obj_inf *obj, char *name)
{

	LDFILE *o_file;
	int j;
	SCNHDR scn_hdr;
	int file_ok;

	file_ok = 0;

	o_file = ldopen(name, (LDFILE *)NULL);

	if (o_file == (LDFILE *)NULL) {
		exit(1);
	}

	for (j = 0; j < NUM_SECTS; j++) {
	    if (ldnshread(o_file, coff_scn_names[j], &scn_hdr) == SUCCESS) {
		obj->scn_addr[j] = scn_hdr.s_vaddr;
		obj->scn_size[j] = scn_hdr.s_size;
		obj->scn_end[j] = scn_hdr.s_vaddr +
						scn_hdr.s_size - 1;
		ldnsseek(o_file, coff_scn_names[j]);
		if (j < NUM_REAL_SECTS) {
			obj->scn_ptr[j] = (unsigned *)malloc(obj->scn_size[j]);
			FREAD((char *)obj->scn_ptr[j], 1,
						obj->scn_size[j], o_file);
		}
		else
			obj->scn_ptr[j] = NULL;
		file_ok = 1;
	    } else
		read_EMPTY(obj, j);
	}
	if (!file_ok) {
		fprintf(stderr, "No sections were found\n");
		exit(1);
	}

	coff_generate_syms(o_file, obj);
}

#endif
