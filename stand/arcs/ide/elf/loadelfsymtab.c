/*
 * Elf symbol table loader code. This file is compiled twice, once for 32 bit objects
 * and once for 64 bit objects. Edit only the file loadelfsymtab.c
 * The files loadelfsymtab32.c and loadelfsymtab64.c are generated from loadelfsymtab.c
 * The token ELFSIZE should be used in this file anywhere a 32 or 64 would
 * normally be used. The makefile will insert the appropriate value depending
 * on which file is being generated.
 * similarly, the token PTRFMT will be replaced with the printf format
 * required to print out a pointer sized value, i.e. ll on 64 bit,
 * void on 32 bit
 */
#if _MIPS_SIM == _ABI64
#define LOAD_ELF64	1
#else
#define	LOAD_COFF	1
#define LOAD_ELF32	1
#define LOAD_ELF64	1
#endif

#include <sys/types.h>
#include <sys/file.h>
#include <sys/sbd.h>
#include <saio.h>
#include <setjmp.h>
#include <stringlist.h>
#include <pause.h>
#include <stdarg.h>

#include <elf.h>		/* always need this for ELFHDRSIZE */

#include <arcs/types.h>
#include <arcs/errno.h>
#include <arcs/restart.h>
#include <arcs/io.h>
#include <arcs/eiob.h>
#include <libsc.h>
#include <libsk.h>
#include <uif.h>

#include "ide_elf.h"

/* symtab macro is used to access the correct
   ptr in the symbol_table struct */
#define symtab symtab_u.ptrELFSIZE

#if DEBUG
#define dprintf(x) if (Debug) printf x 
#else
#define dprintf(x)
#endif

#define BUSY(X) busy(X)

/* external functions - XXX fix w/ correct header please ! */
int load_elf_structELFSIZE(char *filename, ULONG *fd, int size, int offset, 
			   void *buf);

/*  
 * load_symtab[32,64]: routines to load core's symbol table needed so core can 
 * do symbol look up for module 
 */
int
load_symtabELFSIZE(CHAR *filename, struct symbol_table *st)
{
	ElfELFSIZE_Ehdr *elfhdr;
	ULONG fd, cnt;
	int tries = 0;
	LONG err;

	dprintf(("enter load_symtab %s\n", filename));

	/* If we get an EBUSY it means that the tape is still rewinding.
	 */
	while (((err = Open(filename, OpenReadOnly, &fd)) == EBUSY) &&
	       tries++ < 5 ) {
		if ( tries == 1 && Verbose)
			printf("Waiting for the tape to become available...");
		if (pause(15, "", "\033") == P_INTERRUPT)
			return (EBUSY);
		}

	if (err)
		return (err);

	elfhdr = dmabuf_malloc(sizeof(ElfELFSIZE_Ehdr));
	if (elfhdr == 0) {
		Close(fd);
		return(ENOMEM);
	}

	BUSY(1);

	/* read the first ELFHDRBYTES bytes of the file to determine if the
	 * file is ELF or COFF, and if elf, whether 32 or 64 bit
	 */
	if ((err = Read(fd, elfhdr, ELFHDRBYTES, &cnt)) ||
	    (cnt != ELFHDRBYTES)) {
		load_error(filename,"Problem reading file magic id, err %d"
			   " cnt %d.", err, cnt);
		err = err ? err : ENOEXEC;
		goto loaderr;
	}

	BUSY(1);

	if (IS_ELF(*elfhdr)) {
		int class;

		dprintf(("binary is ELF\n"));

		/* 32 bit or 64 bit?
		 */
		class = elfhdr->e_ident[EI_CLASS];

		if (class == ELFCLASSELFSIZE) {
		        dprintf(("binary is ELFSIZE bit\n"));
			err = loadelfELFSIZE_symtab(filename, fd, elfhdr, st);
			if (err)
			  printf("Could not read symbol table for %s",filename);
			dmabuf_free(elfhdr);
			Close(fd);
			return(err);
		}

		BUSY(1);

		printf("File %s has unknown ELF class %d.", filename, class);
		dmabuf_free(elfhdr);
		Close(fd);
		return(ENOEXEC);
        }
	else
	  printf("File %s must be ELF.",filename);

loaderr:
	dmabuf_free(elfhdr);
	Close(fd);
	dprintf(("loadbin: return %d\n", err));
	return(err);
}

/*
 * loadelfELFSIZE_symtab:
 */
int
loadelfELFSIZE_symtab(char *filename,
		      ULONG fd,
		      ElfELFSIZE_Ehdr *elfhdr,
		      struct symbol_table *st) {
    int size, err;
    ULONG cnt;
    ElfELFSIZE_Shdr *scnhdr;

    /* check for valid symbol table */
    if (st == NULL)
      return EINVAL;

    dprintf(("debugging on!\n"));
    
    dprintf(("loading ELFSIZE bit ELF symbol table\n"));

    BUSY(1);

    /*
     * load the remainder of the ELF header
     */
    if ((err = Read(fd, (char*)elfhdr + ELFHDRBYTES,
		    sizeof(ElfELFSIZE_Ehdr) - ELFHDRBYTES, &cnt)) ||
	(cnt != sizeof(ElfELFSIZE_Ehdr) - ELFHDRBYTES)) {
      printf("Problem reading ELF header, err %d cnt %d for file %s.", 
	     err, cnt, filename);
      return (err);
    }
    
    /*
     * load the section headers
     */
    dprintf(("elfhdr is 0x%x\n", elfhdr));
    if(size = elfhdr->e_shentsize * elfhdr->e_shnum ) {
      dprintf(("about to load section headers\n"));
      if((scnhdr = (ElfELFSIZE_Shdr *) dmabuf_malloc(size)) == 0) {
	return ENOMEM;
      }
      
      dprintf(("load scn headers at file offset %x\n",
	       elfhdr->e_shoff));
      
      BUSY(1);

      if ((err = load_elf_structELFSIZE(filename, &fd, size,
					elfhdr->e_shoff, scnhdr)) != ESUCCESS) {
	dmabuf_free(scnhdr);
	return(err);
      }
      /* debug output */
      dprintf(("section header entry size %d\n", elfhdr->e_shentsize));
      dprintf(("%d section entries in file\n", elfhdr->e_shnum));
      if (Debug) {
	for(cnt = 0; cnt < elfhdr->e_shnum; cnt++) {
	  printf("%d: name %x, type %x, flags %x, addr %PTRFMTx, offset %PTRFMTx, "
		 "size %PTRFMTx, link %x, info %x, "
		 "addralign %PTRFMTx, entsize %x\n",
		 cnt, 
		 scnhdr[cnt].sh_name,
		 scnhdr[cnt].sh_type,
		 scnhdr[cnt].sh_flags,
		 scnhdr[cnt].sh_addr,
		 scnhdr[cnt].sh_offset,
		 scnhdr[cnt].sh_size,
		 scnhdr[cnt].sh_link,
		 scnhdr[cnt].sh_info,
		 scnhdr[cnt].sh_addralign,
		 scnhdr[cnt].sh_entsize);
	}
      }
    }
    else 
      scnhdr = 0;

    /* Read SHT_SYMTAB */
    for(cnt = 0; cnt < elfhdr->e_shnum; cnt++) {
      /* look for the symtab entry */
      if (scnhdr[cnt].sh_type == SHT_SYMTAB) {
	/* allocate memory for symbol table */
	size =  scnhdr[cnt].sh_size;
	if((st->symtab = (ElfELFSIZE_Sym *) dmabuf_malloc(size)) == 0) {
	  return ENOMEM;
	}
	/* read symbol table */
	if ((err = load_elf_structELFSIZE(filename, &fd, size, scnhdr[cnt].sh_offset, 
					  st->symtab)) != ESUCCESS) {
	  dmabuf_free(st->symtab);
	  return(err);
	}

	BUSY(1);

	/* set index in symtab to first C symbol */
	st->symtab_c_index = scnhdr[cnt].sh_info;
	/* calc number of entries in symbol table */
	st->symtab_entry_no = scnhdr[cnt].sh_size / scnhdr[cnt].sh_entsize;
	/* done */
	break;
      }
    }
    if (cnt >= elfhdr->e_shnum) {
      printf("Could not find a symbol table for file %s\n",filename);
      return(ENOEXEC);
    }
    dprintf(("Symbol Table: loaded symtab at 0x%X \n", st->symtab));
    dprintf(("Symbol Table: # of entries %d\n",st->symtab_entry_no));
    dprintf(("Symbol Table: first C symbol is at index %d\n",st->symtab_c_index));    

    /* Read SHT_STRTAB */
    for(cnt = 0; cnt < elfhdr->e_shnum; cnt++) {
      /* look for the strtab entry */
      if (scnhdr[cnt].sh_type == SHT_STRTAB) {
	/* allocate memory for string table */
	size =  scnhdr[cnt].sh_size;
	if((st->strtab = (char *) dmabuf_malloc(size)) == 0) {
	  return ENOMEM;
	}
	/* read string table */
	if ((err = load_elf_structELFSIZE(filename, &fd, size, scnhdr[cnt].sh_offset, 
					  st->strtab)) != ESUCCESS) {
	  dmabuf_free(st->strtab);
	  return(err);
	}

	BUSY(1);

	/* done */
	break;
      }
    }
    if (cnt >= elfhdr->e_shnum) {
      printf("Could not find a string table for file %s\n",filename);
      return(ENOEXEC);
    }
    dprintf(("Symbol Table: loaded strtab at 0x%X\n",st->strtab));    

    /* success */
    return 0;
}


/* 
 * resolve_symbol: looks up the symbol_name in the st symbol table
 *                 and returns its address in addr ...
 */
int 
resolve_symbolELFSIZE(struct symbol_table* st, char* symbol_name, 
		      ElfELFSIZE_Addr* addr) {

  int i;

  /* init return value */
  *addr = NULL;

  /* make sure we have a symbol table */
  if (st == NULL) {
    printf("resolve_symbol: symbol table is NULL\n");
    return(1);
  }

  /* make sure we have a valid symtab */
  if (st->symtab == NULL) {
    printf("resolve_symbol: symbol table has no symtab\n");
    return(1);
  }
    
  /* make sure we have a valid strtab */
  if (st->strtab == NULL) {
    printf("resolve_symbol: symbol table has no strtab\n");
    return(1);
  }

  /* now do linear search for symbol */
  dprintf(("resolve_symbol: searching for symbol %s\n", symbol_name));
  for (i=0 ; i<st->symtab_entry_no ; i++) {
    dprintf(("resolve_symbol: comparing to symbol %s\n",
	     st->strtab+st->symtab[i].st_name));
    if (strcmp(st->strtab+st->symtab[i].st_name, symbol_name) == 0) {
      /* success */
      *addr = st->symtab[i].st_value;
      dprintf(("resolve_symbol: found returning addr = 0x%X\n",addr));
      return(0);
    }
  }

  /* not found */
  return(1);

}


/*
 * dumps symbol table 
 */
void
dump_symbol_tableELFSIZE(struct symbol_table *st) {

  int i;

  if (st == NULL)
    return;

  if (st->symtab == NULL) {
    printf("symbol table has no symtab !\n");
    return;
  }
    
  if (st->strtab == NULL) 
    printf("warning: symbol table has no strtab ... will print indexes instead of names\n");

  printf("Symbol Table\n");
  printf("# of entries = %d\n",st->symtab_entry_no);
  
  printf("index:\taddress\t\tbind\ttype\tname\n");
  printf("======\t=======\t\t====\t====\t====\n");
  for (i=0 ; i<st->symtab_entry_no ; i++) {
    printf("%d:\t",i);
    /* addr */
    printf("0x%X\t",st->symtab[i].st_value);
    /* bind */
    switch (ELFELFSIZE_ST_BIND(st->symtab[i].st_info)) {
    case STB_LOCAL:
      printf("LOCAL"); break;
    case STB_GLOBAL:
      printf("GLOBAL"); break;
    case STB_WEAK:
      printf("WEAK  "); break;      
    default:
      printf("%d   ",ELFELFSIZE_ST_BIND(st->symtab[i].st_info)); break;
    }
    printf("\t");
    /* type */
    switch (ELFELFSIZE_ST_TYPE(st->symtab[i].st_info)) {
    case STT_NOTYPE:
      printf("NOTYPE "); break;
    case STT_OBJECT:
      printf("OBJECT "); break;
    case STT_FUNC:
      printf("FUNC   "); break;      
    case STT_SECTION:
      printf("SECTION"); break;      
    case STT_FILE:
      printf("FILE   "); break;      
    default:
      printf("%d    ",ELFELFSIZE_ST_TYPE(st->symtab[i].st_info)); break;
    }
    printf("\t");
    /* print name */
    if (st->strtab) 
      printf("%s",st->strtab+st->symtab[i].st_name);
    else
      printf("%d",st->symtab[i].st_name);
    printf("\n");
  }

}
