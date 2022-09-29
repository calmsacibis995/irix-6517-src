/*
 * "readsym.c"
 *
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libelf.h>
#include <dwarf.h>
#include <libdwarf.h>
#include <sym.h>
#include <symconst.h>
#include "setsym.h"
#include "readsym.h"
#include "elfsubr.h"

static int symmax;
static int namesize;
static struct dbstbl64 *dbstab64;
static struct dbstbl32 *dbstab32;
static char *nametab;
static int bits;

static struct dbstbl64 *db64;	/* holds current table location */
static struct dbstbl32 *db32;	/* holds current table location */
static int nameoffset;		/* holds current offset in name table */

static int fd;			/* keep these static so they don't have to  */
static Elf *elf;		/* be passed around recursively in traverse */
static Dwarf_Debug dwarf;


static Dwarf_Error derror;	/* dummy var for libdwarf error management  */

/* prototypes */
static void	traverse(Dwarf_Die);
static void	process_die(Dwarf_Die);

static void	hash_init(void);
static void	hash_insert64(struct dbstbl64 *);
static void	hash_insert32(struct dbstbl32 *);
static int	hash_repeated64(__uint64_t, char *);
static int	hash_repeated32(__uint32_t, char *);

static void	do_mdebug(Elf_Scn *, Elf32_Shdr *);

void
init_read_symtab(int f_fd, Elf *f_elf, int f_symmax, int f_namesize,
	    void *f_dbstab, char *f_nametab, int f_bits)
{
	fd = f_fd;
        elf = f_elf;
	symmax = f_symmax;
	namesize = f_namesize;
	dbstab64 = (struct dbstbl64 *) f_dbstab;
	dbstab32 = (struct dbstbl32 *) f_dbstab;
	nametab = f_nametab;
	bits = f_bits;
	db64 = dbstab64;
	db32 = dbstab32;
	nameoffset = 0;

	hash_init();
}

int
elf_read_symtab(void)
{

	if (bits == ELF_MIPS64) {
		Elf_Scn *symtab_scn = find_section(elf, ".symtab");
	   	Elf64_Shdr *symtab_shdr = elf64_getshdr(symtab_scn);

		Elf64_Sym *symtab;
	     	int symtab_size, i;

		symtab = (Elf64_Sym *)(elf_getdata(symtab_scn, NULL)->d_buf);
		symtab_size = (elf_getdata(symtab_scn, NULL)->d_size)
				/ sizeof(Elf64_Sym);

		for(i = 0; i < symtab_size; i++) {
			__uint64_t addr;
			char *name;

			if ((ELF64_ST_TYPE(symtab[i].st_info) != STT_FUNC) &&
			  (ELF64_ST_TYPE(symtab[i].st_info) != STT_OBJECT))
				continue;

			addr = symtab[i].st_value;

			if (addr == 0)
				continue;

			name = elf_strptr(elf, symtab_shdr->sh_link, symtab[i].st_name);

			if (hash_repeated64(addr, name))
				continue;

			/* see if there is space left in the table */
			if (db64 >= &dbstab64[symmax])
				xerror("symbol table overflow", OPELF, elf, fd);
	
			/* get the wanted information */
			db64->addr = addr;
			db64->noffst = nameoffset;
			hash_insert64(db64);
			db64++;


			/* see if there is space left in the name table */
			if ((nameoffset + strlen(name)) >= namesize)
				xerror("nametab overflow", OPELF, elf, fd);

			while (*name != '\0')
				nametab[nameoffset++] = *name++;
			nametab[nameoffset++] = '\0';
		}

		return(db64 - dbstab64);
	}

	else  { /* bits == ELF_MIPS32 */
		Elf_Scn *symtab_scn = find_section(elf, ".symtab");
	   	Elf32_Shdr *symtab_shdr = elf32_getshdr(symtab_scn);
		Elf_Scn *mdebug_scn = find_section(elf, ".mdebug");
	   	Elf32_Shdr *mdebug_shdr = elf32_getshdr(mdebug_scn);
		Elf32_Sym *symtab;
	     	int symtab_size, i;

		symtab = (Elf32_Sym *)(elf_getdata(symtab_scn, NULL)->d_buf);
		symtab_size = (elf_getdata(symtab_scn, NULL)->d_size)
				/ sizeof(Elf32_Sym);

		for(i = 0; i < symtab_size; i++) {
			__uint32_t addr;
			char *name;

			if ((ELF32_ST_TYPE(symtab[i].st_info) != STT_FUNC) &&
			  (ELF32_ST_TYPE(symtab[i].st_info) != STT_OBJECT))
				continue;

			addr = symtab[i].st_value;

			if (addr == 0)
				continue;

			name = elf_strptr(elf, symtab_shdr->sh_link, symtab[i].st_name);

			if (hash_repeated32(addr, name))
				continue;

			/* see if there is space left in the table */
			if (db32 >= &dbstab32[symmax])
				xerror("symbol table overflow", OPELF, elf, fd);
	
			/* get the wanted information */
			db32->addr = addr;
			db32->noffst = nameoffset;
			hash_insert32(db32);
			db32++;

			/* see if there is space left in the name table */
			if ((nameoffset + strlen(name)) >= namesize)
				xerror("nametab overflow", OPELF, elf, fd);

			while (*name != '\0')
				nametab[nameoffset++] = *name++;
			nametab[nameoffset++] = '\0';
		}

		if (mdebug_scn && mdebug_shdr)
			(void) do_mdebug(mdebug_scn, mdebug_shdr);

		return(db32 - dbstab32);
	}

}


int
dwarf_read_symtab(int Verbose)
{
	Dwarf_Unsigned cu_offset;
        Dwarf_Unsigned cu_header_length;
        Dwarf_Unsigned abbrev_offset;
        Dwarf_Half version_stamp;
        Dwarf_Half address_size;
	Dwarf_Die first_die;

	if (dwarf_init(fd, DW_DLC_READ, NULL, NULL, &dwarf, &derror) !=
							DW_DLV_OK) {
		if (Verbose)
			printf("Could not find DWARF debugging info; using ELF only.\n");
	}
	else {

		while (dwarf_next_cu_header(dwarf, &cu_header_length,
						&version_stamp, &abbrev_offset,
						&address_size,
						&cu_offset,
						&derror) == DW_DLV_OK) {
		    if (cu_offset == DW_DLV_BADOFFSET)
        	                xerror("could not get dwarf_cu_header", OPELFDW, dwarf, fd);
			/* process a single compilation unit in .debug_info */
			if (dwarf_siblingof(dwarf, NULL, &first_die, &derror) !=
								DW_DLV_OK)
				xerror("could not get dwarf_die", OPELFDW, dwarf, fd);
	 		traverse(first_die);
        	}

		dwarf_finish(dwarf, &derror);
	}

	if (bits == ELF_MIPS64)
		return(db64 - dbstab64);
	else /* bits == ELF_MIPS32 */
		return(db32 - dbstab32);
}

static void
traverse(Dwarf_Die die)
{
	Dwarf_Die next_die;

	if (die != NULL) {
		process_die(die);
		if (dwarf_child(die, &next_die, &derror) == DW_DLV_OK)
			traverse(next_die);
		if (dwarf_siblingof(dwarf, die, &next_die, &derror)
							== DW_DLV_OK)
			traverse(next_die);
	}
}

static void
process_die(Dwarf_Die die)
{
	char *name;
	Dwarf_Addr addr;
	Dwarf_Bool hasattr;
	Dwarf_Half tag;
	Dwarf_Signed locCount;
	Dwarf_Locdesc *llbuf;
	Dwarf_Attribute attr;

	/* see if the symbol in the die is wanted  & get values */
	if (dwarf_tag(die, &tag, &derror) != DW_DLV_OK)
		return;
	switch (tag) {
	case DW_TAG_subprogram:
		if ((dwarf_hasattr(die, DW_AT_low_pc, &hasattr, &derror) !=
								DW_DLV_OK) ||
		    !hasattr ||
		    (dwarf_lowpc(die, &addr, &derror) != DW_DLV_OK))
			return;
		break;
	case DW_TAG_variable:
		if ((dwarf_hasattr(die, DW_AT_location, &hasattr, &derror) !=
								DW_DLV_OK) ||
		    !hasattr ||
		    (dwarf_attr(die, DW_AT_location, &attr, &derror) !=
								DW_DLV_OK) ||
		    (dwarf_loclist(attr, &llbuf, &locCount, &derror) !=
								DW_DLV_OK) ||
		    (locCount == DW_DLV_NOCOUNT) ||
		    (llbuf->ld_s[0].lr_atom != DW_OP_addr))
			return;
		addr = llbuf->ld_s[0].lr_number;
		dwarf_dealloc(dwarf, llbuf, DW_DLA_LOCDESC);
		break;
	default:
		return;
	}

	if (dwarf_diename(die, &name, &derror) != DW_DLV_OK)
		return;
	if (addr == 0)
		return;

	if (bits == ELF_MIPS64) {

                /* check to see if this is a repeat */
		if (hash_repeated64(addr, name))
			return;

		/* see if there is space left in the table */
		if (db64 >= &dbstab64[symmax])
			xerror("symbol table overflow", OPELFDW, dwarf, elf, fd);
	
		/* get the wanted information */
		db64->addr = addr;
		db64->noffst = nameoffset;
		hash_insert64(db64);
		db64++;
	}

	else /* bits == ELF_MIPS32 */ {

                /* check to see if this is a repeat */
		if (hash_repeated32(addr, name))
			return;

		/* see if there is space left in the table */
		if (db32 >= &dbstab32[symmax])
			xerror("symbol table overflow", OPELFDW, dwarf, elf, fd);
	
		/* get the wanted information */
		db32->addr = addr;
		db32->noffst = nameoffset;
		hash_insert32(db32);
		db32++;
	}

	/* see if there is space left in the name table */
	if ((nameoffset + strlen(name)) >= namesize)
		xerror("nametab overflow", OPELFDW, dwarf, elf, fd);

	while (*name != '\0')
		nametab[nameoffset++] = *name++;
	nametab[nameoffset++] = '\0';
}







/* Hash table stuff.  To ensure no repeated entries between DWARF & ELF */

#define HASHSIZE 0x00010000
#define HASHSHIFTCONST 16
#define A 0.6180339887498948482
#define ATwoToW 2654435769
#define Hash(ki) ((unsigned long)((ki)*ATwoToW) >> HASHSHIFTCONST)

struct node {
	struct node *hashPtr;
	void *entry;
};

static struct node *HashTable[HASHSIZE];

void
hash_init(void)
{
	int i;

	for (i = 0; i < HASHSIZE; i++) {
		HashTable[i] = NULL;
	}
}

void
hash_insert64(struct dbstbl64 *db)
{
	long i;
	struct node *new, *old;

	/* make node for key */
	new = (struct node *) malloc (sizeof(struct node));
        new->entry = (void *) db;

	/* put node in hash table */
	i = Hash(db->addr);
	old = HashTable[i];
	HashTable[i] = new;
	new->hashPtr = old;

}

int
hash_repeated64(__uint64_t addr, char *name)
{
	struct node *x;
	struct dbstbl64 *ent;

	x = HashTable[Hash(addr)];
	while (x != NULL) {
		ent = (struct dbstbl64 *) x->entry;
		if (ent->addr == addr)
			if (strcmp(name, &nametab[ent->noffst]) == 0)
				return 1;
		x = x->hashPtr;
	}

	return 0;
}

void
hash_insert32(struct dbstbl32 *db)
{
	long i;
	struct node *new, *old;

	/* make node for key */
	new = (struct node *) malloc (sizeof(struct node));
        new->entry = (void *) db;

	/* put node in hash table */
	i = Hash(db->addr);
	old = HashTable[i];
	HashTable[i] = new;
	new->hashPtr = old;

}

int
hash_repeated32(__uint32_t addr, char *name)
{
	struct node *x;
	struct dbstbl32 *ent;

	x = HashTable[Hash(addr)];
	while (x != NULL) {
		ent = (struct dbstbl32 *) x->entry;
		if (ent->addr == addr)
			if (strcmp(name, &nametab[ent->noffst]) == 0)
				return 1;
		x = x->hashPtr;
	}

	return 0;
}

/*
 * do_mdebug()
 * On 32bit platforms, we're still using the ucode compilers to build
 * the kernel, so we need to get our static text/data from the .mdebug
 * section instead of the .dwarf sections.
 */
static void
do_mdebug(Elf_Scn *scnp, Elf32_Shdr *shdrp)
{
	long *buf = (long *)(elf_getdata(scnp, NULL)->d_buf); 
	u_long addr, mdoff = shdrp->sh_offset;
	HDRR *hdrp;
	SYMR *symbase, *symp, *symend;
	FDR *fdrbase, *fdrp;
	int i, j;
	char *strbase, *str;
	int ifd;

	/* get header */
	addr = (__psunsigned_t)buf;
	hdrp = (HDRR *)addr;

	/* setup base addresses */
	addr = (u_long)buf + (u_long)(hdrp->cbFdOffset - mdoff);
	fdrbase = (FDR *)addr;
	addr = (u_long)buf + (u_long)(hdrp->cbSymOffset - mdoff);
	symbase = (SYMR *)addr;
	addr = (u_long)buf + (u_long)(hdrp->cbSsOffset - mdoff);
	strbase = (char *)addr;

#define KEEPER(a,b)	((a == stStaticProc && b == scText) || \
			 (a == stStatic && (b == scData || b == scBss || \
					    b == scSBss || b == scSData)))

	for (fdrp = fdrbase; fdrp < &fdrbase[hdrp->ifdMax]; fdrp++) {
		str = strbase + fdrp->issBase + fdrp->rss;

		/* local symbols for each fd */
		for (symp = &symbase[fdrp->isymBase];
		     symp < &symbase[fdrp->isymBase+fdrp->csym];
		     symp++) {
			if (KEEPER(symp->st, symp->sc)) {
				if (symp->value == 0)
					continue;

				str = strbase + fdrp->issBase + symp->iss;
				
				if (hash_repeated32(symp->value, str))
					continue;

				if (db32 >= &dbstab32[symmax]) {
					xerror("symbol table overflow",
					       OPELF, elf, fd);
				}
				
				db32->addr = symp->value;
				db32->noffst = nameoffset;

				hash_insert32(db32);
				db32++;

				if ((nameoffset + strlen(str)) >= namesize)
					xerror("nametab overflow",
					       OPELF, elf, fd);

				while (*str != '\0')
					nametab[nameoffset++] = *str++;
				nametab[nameoffset++] = '\0';
			}
		}
	}
}
