/*
 * symbol.c - routines for interrogating kernel symbol table
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <libelf.h>
#include <fcntl.h>
#include <sym.h>
#include <symconst.h>
#include <dwarf.h>
#include <libdwarf.h>

#define perrorx(s)	perror(s), exit(1)
#define fatal(m)	fprintf(stderr, "ERROR - %s\n", m), exit(1)

#ifdef TIMER
extern void cycletrace(char*);
#define TSTAMP(s)       cycletrace(s);
#else
#define TSTAMP(s)
#endif


/* prototypes */


extern char* strspace(void);
extern Elf_Scn *find_section(Elf *, char *);


typedef __psunsigned_t    symaddr_t;


#define LISTINIT        20000
#define LISTINCR        6000

typedef struct {
	char		*namep;
	symaddr_t	addr;
} symlist_t;

static int		symcnt=0;                  /* number of symbols */
static int		symlistsize = 0;           /* sized of malloc'ed memory for list */
static symlist_t	*symlist = NULL;           /* list of symbol address */
static symlist_t	*symnext = NULL;           /* next free slot */
static void		*magic_addr;
static void		*end_addr;
static void		*begin_addr;


static void	traverse(Dwarf_Die);
static void	process_die(Dwarf_Die);
static void	listadd(char *, symaddr_t);
char*		symtab_name(symaddr_t);

static	Dwarf_Debug dwarf;	/* be passed around recursively in traverse */
static	Dwarf_Error derror;	/* dummy var for libdwarf error management  */


static int
rdelfsymtab(int fd)
{
	Elf *elf;
	int symtab_size, i, type;
	char *name;

	if (elf_version(EV_CURRENT) == EV_NONE) {
		fprintf(stderr, "rofiler out of date with respect to current ELF version");
		exit(1);
	}


	if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL)
		return 1;

	/*
	 * Scan looking for functions and the special symbol "_etext" which
	 * shows up as a STT_SECTION symbol.  For each function symbol (and
	 * "_etext") pass them to listadd() if they don't show up as already
	 * in the symbol table (list_repeated()).
	 */
#if (_MIPS_SZPTR == 64)
	{
	Elf_Scn *symtab_scn = find_section(elf, ".symtab");
	Elf64_Shdr *symtab_shdr = elf64_getshdr(symtab_scn);
	Elf64_Sym *symtab;

	symtab = (Elf64_Sym *)(elf_getdata(symtab_scn, NULL)->d_buf);
	symtab_size = (int) ((elf_getdata(symtab_scn, NULL)->d_size)
			     / sizeof(Elf64_Sym));

	for (i = 0; i < symtab_size; i++) {
		__uint64_t addr;

		type = ELF64_ST_TYPE(symtab[i].st_info);
		if (type != STT_OBJECT && type != STT_SECTION)
			continue;

		addr = symtab[i].st_value;
		if (addr == 0)
			continue;

		name = elf_strptr(elf, symtab_shdr->sh_link,
				  symtab[i].st_name);

		if (type == STT_SECTION && strcmp(name, "end") == 0)
			end_addr = (void*)addr;
		else if (type == STT_SECTION && strcmp(name, "ftext") == 0)
			begin_addr = (void*)addr;
		else if (type == STT_OBJECT && strcmp(name, "kernel_magic") == 0)
			magic_addr = (void*)addr;

	}
	}
#else /* _MIPS_SZPTR == 32 */
	{
	Elf_Scn *symtab_scn = find_section(elf, ".symtab");
	Elf32_Shdr *symtab_shdr = elf32_getshdr(symtab_scn);

	Elf32_Sym *symtab;

	symtab = (Elf32_Sym *)(elf_getdata(symtab_scn, NULL)->d_buf);
	symtab_size = (elf_getdata(symtab_scn, NULL)->d_size)
			/ sizeof(Elf32_Sym);

	for(i = 0; i < symtab_size; i++) {
		__uint32_t addr;

		type = ELF32_ST_TYPE(symtab[i].st_info);
		if (type != STT_OBJECT && type != STT_SECTION)
			continue;

		addr = symtab[i].st_value;
		if (addr == 0)
			continue;

		name = elf_strptr(elf, symtab_shdr->sh_link,
				  symtab[i].st_name);
		if (type == STT_SECTION && strcmp(name, "end") == 0)
			end_addr = (void*)addr;
		else if (type == STT_SECTION && strcmp(name, "ftext") == 0)
			begin_addr = (void*)addr;
		else if (type == STT_OBJECT && strcmp(name, "kernel_magic") == 0)
			magic_addr = (void*)addr;
	}
	}
#endif /* _MIPS_SZPTR == 32 */
	elf_end(elf);

	return 0;
}
static int
rdsymtab(int fd)
{
	Dwarf_Unsigned	cu_offset;
	Dwarf_Unsigned	cu_header_length;
	Dwarf_Unsigned	abbrev_offset;
	Dwarf_Half	version_stamp;
	Dwarf_Half	address_size;
	Dwarf_Die	first_die;

	if (dwarf_init(fd, DW_DLC_READ, NULL, NULL, &dwarf, &derror) != DW_DLV_OK)
		return 1;

	while (dwarf_next_cu_header(dwarf, &cu_header_length, &version_stamp, &abbrev_offset, &address_size, &cu_offset,
	     					&derror) == DW_DLV_OK) {
		if (cu_offset == DW_DLV_BADOFFSET)
			return 1;

		/* process a single compilation unit in .debug_info */
		if (dwarf_siblingof(dwarf, NULL, &first_die, &derror) != DW_DLV_OK)
			return 1;

		traverse(first_die);
	}

	dwarf_finish(dwarf, &derror);

	return 0;
}


static void	
traverse(Dwarf_Die die)
{
	Dwarf_Die	next_die;

	if (die != NULL) {
		process_die(die);
		if (dwarf_child(die, &next_die, &derror) == DW_DLV_OK)
			traverse(next_die);
		if (dwarf_siblingof(dwarf, die, &next_die, &derror) == DW_DLV_OK)
			traverse(next_die);
		dwarf_dealloc(dwarf, die, DW_DLA_DIE);
	}
}


static void	
process_die(Dwarf_Die die)
{
	Dwarf_Half	tag;
	Dwarf_Bool	hasattr;
	Dwarf_Addr	addr;
	Dwarf_Attribute	attr;
	Dwarf_Locdesc	*llbuf;
	Dwarf_Signed	locCount;
	char		*name;

	/* see if the symbol in the die is wanted  & get values */
	if (dwarf_tag(die, &tag, &derror) != DW_DLV_OK)
		return;

	switch (tag) {
	case DW_TAG_subprogram:
		if ((dwarf_hasattr(die, DW_AT_low_pc, &hasattr, &derror) != DW_DLV_OK) ||  !hasattr ||  (dwarf_lowpc(die,
		     &addr, &derror) != DW_DLV_OK))
			return;
		break;
	case DW_TAG_variable:
		if ((dwarf_hasattr(die, DW_AT_location, &hasattr, &derror) != DW_DLV_OK) ||  !hasattr ||  (dwarf_attr(die,
		     DW_AT_location, &attr, &derror) != DW_DLV_OK) ||  (dwarf_loclist(attr, &llbuf, &locCount, &derror) !=
		    DW_DLV_OK) ||  (locCount == DW_DLV_NOCOUNT) ||  (llbuf->ld_s[0].lr_atom != DW_OP_addr))
			return;
		addr = llbuf->ld_s[0].lr_number;
		dwarf_dealloc(dwarf, llbuf, DW_DLA_LOCDESC);
		break;
	default:
		return;
	}
	if (dwarf_diename(die, &name, &derror) != DW_DLV_OK) {
		return;
	}

	listadd(name, addr);
}



static int	
compar(const void *x, const void *y)
{
	return(int)((*(symlist_t * )x).addr - (*(symlist_t * )y).addr);
}


static void	
listadd(char *name, symaddr_t addr)
{
	if ((void*)addr < begin_addr || (void*)addr > end_addr)
		return;
	if (symcnt >= symlistsize) {
		TSTAMP("expand list");
		symlistsize += LISTINCR;
		symlist = (symlist_t * )
		realloc(symlist, symlistsize * sizeof(symlist_t));
		if (symlist == NULL)
			perrorx("cannot allocate memory");
		symnext = &symlist[symcnt];
	}
	(*symnext).addr  = addr;
	(*symnext).namep  = strspace();
	strcpy((*symnext).namep, name);
	symnext++;
	symcnt++;
}


static void
listinit(void)
{
	symlist = (symlist_t * )malloc(LISTINIT * sizeof (symlist_t));
	if (symlist == NULL)
		perrorx("cannot allocate memory");
	symnext = symlist;
	symlistsize = LISTINIT;

}


void
loadsymtab (char *namelist, void *kmagic_addr, void *kend_addr)
{
	int		fd;

	if (symcnt)			/* one time only */
		return;
	
	TSTAMP("loadsymtab");
	listinit();
	if ((fd = open(namelist, O_RDONLY)) < 0)
		perrorx("cannot open unix file");

	if (rdelfsymtab(fd) != 0)
		perrorx("rdelfsymtab");
	if (kmagic_addr != magic_addr || kend_addr != end_addr)
		fatal("Wrong unix file. It does not match the running system");

	TSTAMP("rdsymtab");
	if (rdsymtab(fd) < 0)
		perrorx("unable to load unix symbol table");
	close(fd);
	
	listadd("BEGIN", (symaddr_t)begin_addr);
	listadd("END", (symaddr_t)end_addr);

	TSTAMP("start symtab sort");
	qsort(symlist, symcnt, sizeof (symlist_t), compar);
	TSTAMP("complete symtab sort");
	symlist[symcnt].addr = symlist[symcnt-1].addr + 10000;
	symcnt++;
}


static int	
search_compar(const void *x, const void *y)
{
	symlist_t	*sx, *sy;

	sx = (symlist_t * )x;
	sy = (symlist_t * )y;	/* sy is the pointer into symyab */
	if (sx->addr >= sy->addr) {
		if (sx->addr < (sy+1)->addr)
			return(0);
		else
			return(1);
	} 
	return(-1);
}

#if (_MIPS_SZPTR == 64)
#define SPEC	"0x%llx"
#else
#define SPEC	"0x%x"
#endif

char*
symtab_name(symaddr_t addr)
{
	static char	name[100];
	char		offset[32];
	symlist_t	sym, *p;

	if (addr < symlist[0].addr || addr >= symlist[symcnt-1].addr) {
		sprintf(name, SPEC, addr);
	} else {
		sym.addr = addr;
		p = (symlist_t*)bsearch((void*)&sym, symlist, symcnt, sizeof(symlist_t), search_compar);
		strcpy(name, p->namep);
		if (addr != p->addr) {
			sprintf(offset, "+" SPEC, addr - p->addr);
			strcat(name, offset);
		}
	}
	return(name);
}
