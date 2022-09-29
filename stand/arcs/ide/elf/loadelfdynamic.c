#include <sys/sbd.h>
#include <arcs/types.h>
#include <arcs/errno.h>
#include <arcs/eiob.h>
#include <elf.h>
#include <libsc.h>
#include <libsk.h>

#include "ide_elf.h"

/* Only include proper loader code -- 64 bit proms cannot really load
 * 32 bit programs (TFP for sure, and it is messy on the R4000), plus
 * we need to save the space.  32 bit proms need to be able to load
 * anything.
 */

/* #define DEBUG 1 */
#if IP26 && !DEBUG			/* some arch are short of prom space */
#define dprintf(x)
#else
#define dprintf(x) if (Debug) printf x 
#endif

/* external functs - XXX fix this w/ proper include file !!! */
/* defined in stand/arcs/lib/libsk/lib/loadelf.c ... */
extern int load_elf_structELFSIZE(char *filename, ULONG *fd, int size, int offset, 
				  void *buf);

/* was defined extern in loadelf.c this way .. */
extern MEMORYDESCRIPTOR *mem_contains(unsigned long, unsigned long);

/* globals defined in stand/arcs/ide/elf/elf.c */
extern struct symbol_table _core_symbol_table;
extern char* _module_gp_ptr;

/* globals used in loadelfdynamic.c ... */
static ElfELFSIZE_Got* got_ELFSIZE = NULL;      /* ptr to GOT table */
static ElfELFSIZE_Sym* dynsym_ELFSIZE = NULL;   /* ptr to symbol table */
static char* strtab_ELFSIZE = NULL;             /* ptr to string table */
static int got_index_ELFSIZE = -1;              /* index to the first global GOT entry */
static int dynsym_index_ELFSIZE = -1;           /* index to the first global dynsym entry */
static int dynsym_no_ELFSIZE = -1;              /* total number of dynsym entries */
#if _MIPS_SIM == _ABI64
static ElfELFSIZE_Addr gp_value_ELFSIZE = 0x0; /* gp value in reginfo section */
#else /* 32 bit */
static ElfELFSIZE_Sword gp_value_ELFSIZE = 0x0; /* gp value in reginfo section */
#endif

static LONG
loadelfELFSIZE_dynamic(char *filename,
			ULONG fd,
			ULONG *pc,
			ElfELFSIZE_Ehdr *elfhdr,
			ElfELFSIZE_Phdr *pgmhdr)
{
	ULONG laddr, lowaddr, hiaddr;
	int np=0, err, x, offset, size;
	__psunsigned_t buf, bufp;

	/* new defs */
	int options_offset;
	int reginfo_found;
	ElfELFSIZE_Dyn elf_dyn_entry;
	ElfELFSIZE_RegInfo elf_reginfo_entry;
	Elf_Options elf_options_entry;

#ifdef ELF_2_COFF
	static ULONG text_start, data_start, bss_start;
	static ULONG tsize, dsize, bsize;
	static char *coff_blk[2];
#endif

	lowaddr = (ULONG)-1; /* max addr */
	hiaddr = 0;

	/* loop through program segments and load ones marked PT_LOAD
	 */
	for (x=0; x < elfhdr->e_phnum; x++) {

	        dprintf(("loadelf_dynamic: program header type = 0x%X\n",
			 pgmhdr[x].p_type));

		/* DYNAMIC */
		if (pgmhdr[x].p_type == PT_DYNAMIC) {
		  dprintf(("loadelf_dynamic: got dynamic prog segment\n"));
		  dprintf(("loadelf_dynamic: p_filesz = 0x%X\n",
			   pgmhdr[x].p_filesz));
		  dprintf(("loadelf_dynamic: p_memsz = 0x%X\n",
			   pgmhdr[x].p_memsz));
		  dprintf(("loadelf_dynamic: sizeof elf_dyn_entry = 0x%X\n",
			   sizeof(elf_dyn_entry)));

		  offset = pgmhdr[x].p_offset;
		  /* search to get the entries we need ... */
		  while (1) {
		    err = load_elf_structELFSIZE(filename, &fd, 
						 sizeof(elf_dyn_entry),
						 offset, (void*)&elf_dyn_entry);
		    if (err != ESUCCESS) {
		      dprintf(("loadelf_dynamic: could not read dynamic\n"));
		      return(err);
		    }

		    dprintf(("loadelf_dynamic: elf_dyn_entry.d_tag= 0x%X\n",
			     elf_dyn_entry.d_tag));
		    dprintf(("loadelf_dynamic: elf_dyn_entry.d_un.d_val= 0x%X\n",
			     elf_dyn_entry.d_un.d_val));
		    dprintf(("loadelf_dynamic: elf_dyn_entry.d_un.d_ptr= 0x%X\n",
			     elf_dyn_entry.d_un.d_ptr));
		    
		    /* process a PLTGOT entry if found - not mandatory*/
		    if (elf_dyn_entry.d_tag == DT_PLTGOT) 
		      /* set got */
		      got_ELFSIZE = (ElfELFSIZE_Got*) elf_dyn_entry.d_un.d_ptr;

		    /* process a SYMTAB entry - mandatory */
		    if (elf_dyn_entry.d_tag == DT_SYMTAB) 
		      /* set dynsym */
		      dynsym_ELFSIZE = (ElfELFSIZE_Sym*) elf_dyn_entry.d_un.d_ptr;

		    /* process a STRTAB entry - mandatory */
		    if (elf_dyn_entry.d_tag == DT_STRTAB) 
		      /* set strtab */
		      strtab_ELFSIZE = (char*) elf_dyn_entry.d_un.d_ptr;

		    /* process a MIPS_LOCAL_GOTNO entry - mandatory */
		    if (elf_dyn_entry.d_tag == DT_MIPS_LOCAL_GOTNO) 
		      /* set got_index */
		      got_index_ELFSIZE = elf_dyn_entry.d_un.d_val;

		    /* process a MIPS_GOTSYM entry - mandatory */
		    if (elf_dyn_entry.d_tag == DT_MIPS_GOTSYM) 
		      /* set dynsym_index */
		      dynsym_index_ELFSIZE = elf_dyn_entry.d_un.d_val;

		    /* process a MIPS_SYMTABNO entry - mandatory */
		    if (elf_dyn_entry.d_tag == DT_MIPS_SYMTABNO) 
		      /* set dynsym_no */
		      dynsym_no_ELFSIZE = elf_dyn_entry.d_un.d_val;

		    offset +=  sizeof(elf_dyn_entry);
		    /* DT_NULL marks end of dynamic array*/
		    if (elf_dyn_entry.d_tag ==  DT_NULL) {
		      dprintf(("loadelf_dynamic: searched all entries ...\n"));
		      break;
		    }
		  }

		}

#if OLD_REGINFO
		/* REGINFO */
		if (pgmhdr[x].p_type == PT_MIPS_REGINFO) {
		  offset = pgmhdr[x].p_offset;
		  err = load_elf_structELFSIZE(filename, &fd,
					       sizeof(elf_reginfo_entry),
					       offset, (void*)&elf_reginfo_entry);
		  if (err != ESUCCESS) {
		    dprintf(("loadelf_dynamic: could not read dynamic\n"));
		    return(err);
		  }
		  /* set gp value of module */
		  gp_value_ELFSIZE = elf_reginfo_entry.ri_gp_value;
		}
#endif /* OLD_REGINFO */

		/* OPTIONS - hold REGINFO for both 32 & 64 bit */
		if (pgmhdr[x].p_type == PT_MIPS_OPTIONS) {

		  options_offset = pgmhdr[x].p_offset;
		  offset = 0;
		  reginfo_found = 0;
		  /* go thru all option sections */
		  while (offset < pgmhdr[x].p_memsz) {

		    /* read option header */
		    err = load_elf_structELFSIZE(filename, &fd,
						 sizeof(elf_options_entry),
						 options_offset + offset, 
						 (void*)&elf_options_entry);
		    if (err != ESUCCESS) {
		      dprintf(("loadelf_dynamic: could not read dynamic\n"));
		      return(err);
		    }
		    /* process reginfo entry */
		    if (elf_options_entry.kind == ODK_REGINFO) {
		      
		      err = load_elf_structELFSIZE(filename, &fd,
						   sizeof(elf_reginfo_entry),
						   options_offset + offset + sizeof(elf_options_entry),
						   (void*)&elf_reginfo_entry);
		      if (err != ESUCCESS) {
			dprintf(("loadelf_dynamic: could not read dynamic\n"));
			return(err);
		      }
		      /* set gp value of module */
		      reginfo_found = 1;
		      gp_value_ELFSIZE = elf_reginfo_entry.ri_gp_value;
		      /* done */
		      break;
		    }
		    /* offset for next option entry */
		    offset += sizeof(elf_options_entry.size);
		  }
		}

		/* LOAD */
		if (pgmhdr[x].p_type != PT_LOAD) 
			continue;

		laddr = KDM_TO_PHYS(pgmhdr[x].p_vaddr);
		if (!mem_contains(laddr, pgmhdr[x].p_memsz)) {
			load_error(filename, "Text start 0x%x, size "
				   "0x%x doesn't fit in a FreeMemory area.",
				   laddr, pgmhdr[x].p_memsz);
			Close (fd);
			dmabuf_free(pgmhdr);
			return (ENOMEM);
		}
	
		/* Make sure target of the load is in free memory.
		 */
		if (range_check((__psunsigned_t)pgmhdr[x].p_vaddr,
				pgmhdr[x].p_memsz)) {
			load_error(filename,
				"Range check failure: Section start 0x%x, "
				"size 0x%x.\nSection would overwrite an "
				"already loaded program.",
				(__psunsigned_t)pgmhdr[x].p_vaddr,
				pgmhdr[x].p_memsz);
			dmabuf_free(pgmhdr);
			Close(fd);
			return(ENOMEM); 
		}

		if (Verbose && pgmhdr[x].p_filesz) {
			if (np++)
				putchar('+');
			printf("%PTRFMTd",pgmhdr[x].p_filesz);
		}

#ifdef ELF_2_COFF
		bufp = buf = (__psunsigned_t)dmabuf_malloc(pgmhdr[x].p_filesz);
#else
		bufp = buf = (__psunsigned_t)pgmhdr[x].p_vaddr;
#endif
		/*
		 * Check for overlap of this segment and the already
		 * xferred elf and program headers.  We can save some
		 * time if we can use bcopy here.
		 */
		if (pgmhdr[x].p_offset == (int)0) {
			/* do elf hdr */
			if (pgmhdr[x].p_filesz <= sizeof (ElfELFSIZE_Ehdr)) {
				size = pgmhdr[x].p_filesz;
			} else {
				size = sizeof (ElfELFSIZE_Ehdr);
			}
			(void) bcopy((void *)elfhdr, (void *)bufp, size);
			bufp += size;

			/* do pgm hdr if it's aligned with end of elf hdr */
			if (elfhdr->e_phoff == (int)(bufp - buf)) {
				if (pgmhdr[x].p_filesz < (size +
					(elfhdr->e_phentsize * elfhdr->e_phnum))) {
					size = pgmhdr[x].p_filesz - size;
				} else {
					size = elfhdr->e_phentsize *
						elfhdr->e_phnum;
				}
				(void) bcopy((void *)pgmhdr, (void *)bufp,
					size);
				bufp += size;
			}
			offset = bufp - buf;
		} else {
			offset = pgmhdr[x].p_offset;
		}
		err = load_elf_structELFSIZE(filename, &fd, pgmhdr[x].p_filesz -
					     (bufp - buf), offset, (void *)bufp);
		if (err != ESUCCESS) {
			dmabuf_free(pgmhdr);
			Close(fd);
			return(err);
		}

		/* zero any trailing bytes that weren't stored in the file,
		 * which very well may be .bss.
		 */
		err = pgmhdr[x].p_memsz - pgmhdr[x].p_filesz;
#ifdef ELF_2_COFF
		switch(pgmhdr[x].p_flags) {
#define R_X 5 /* 101 == r-x  means CODE */
#define RW_ 6 /* 110 == rw- means DATA */
#define RWX 7 /* 111 == rwx  means CODE */
		case R_X:
		case RWX:
			text_start = laddr;
			tsize = pgmhdr[x].p_filesz;
			coff_blk[0] = (char *)buf;
			break;
		case RW_:
			data_start = laddr;
			dsize = pgmhdr[x].p_filesz;
			coff_blk[1] = (char *)buf;
			break;
		default:
			printf("elf2coff: unknown p_flags in header segment: 0x%x\n", pgmhdr[x].p_flags);
			break;
		}
#endif
		if (err > 0) {
			if (Verbose)
				printf("+%d",err);
#ifdef ELF_2_COFF
			bss_start = laddr + pgmhdr[x].p_filesz;
			bsize = err;
#else
			bzero((char*)(__psunsigned_t)
			      (pgmhdr[x].p_vaddr+pgmhdr[x].p_filesz), err);
#endif
		}
		if (laddr < lowaddr)
			lowaddr = laddr;
		if (laddr + pgmhdr[x].p_memsz > hiaddr)
			hiaddr = laddr + pgmhdr[x].p_memsz - 1;

		dprintf(("loaded segment %d at %PTRFMTx, length %PTRFMTx\n", x,
	        	pgmhdr[x].p_vaddr, pgmhdr[x].p_memsz));
	}

	if ( Verbose )
		printf(" entry: 0x%PTRFMTx\n", elfhdr->e_entry);
	*pc = elfhdr->e_entry;
	dmabuf_free(pgmhdr);
	Close(fd);

#if !ELF_2_COFF
	/* save kernel name for run-time symbol table mgmt in the kernel */
	setenv("kernname", filename);

	FlushAllCaches();		/* does arch dependent flush */
#endif
	dprintf(("list init %x %x %x\n", lowaddr, hiaddr, hiaddr - lowaddr));
	if (err = elf_list_init(lowaddr, hiaddr))
		return (err);

#ifdef ELF_2_COFF
	{
	extern dump_output_file(ULONG, ULONG, ULONG,
					ULONG, ULONG, ULONG, ULONG, char **);
	dump_output_file(elfhdr->e_entry, text_start, tsize,
				data_start, dsize, bss_start, bsize, coff_blk);
	}
#endif
	return(0);
}

LONG
ide_loadelfELFSIZEdynamic(char *filename,
			  ULONG fd,
			  ULONG *pc,
			  ElfELFSIZE_Ehdr *elfhdr,
			  ElfELFSIZE_Phdr *pgmhdr)
{
    int size, err;
    ULONG cnt;
    int notresolved;

    int i,count;
    ElfELFSIZE_Addr addr;
    char buf[50];

    /* Debug = 1; */
    dprintf(("debugging on!\n"));
    
    dprintf(("loading ELFSIZE bit ELF\n"));

    dprintf(("loadelfELFSIZE: loading dynamic ...\n"));
    
    err = loadelfELFSIZE_dynamic(filename, fd, pc, elfhdr, pgmhdr);

    /* return on error XXX close fd here ? */
    if (err)
      return(err);
    
    /* print out gp value */
    dprintf(("loadelfELFSIZE: gp_value = 0x%X\n",gp_value_ELFSIZE));
    /* must have a gp value */
    if (gp_value_ELFSIZE == 0x0) {
      printf("loadelfELFSIZE: fatal error: found no gp value\n");
      return(ENOEXEC);
    } 
    /* store gp value in module's argv[] - so it will appear in the stack */
    sprintf(buf,"%x",gp_value_ELFSIZE);
    strcpy(_module_gp_ptr,buf);

    /* make sure we have a got table */
    if (got_ELFSIZE == NULL)
      printf("loadelfELFSIZE: no GOT table found ...\n");
    
    /* print out address of symbol table */
    dprintf(("loadelfELFSIZE: dynsym = 0x%X\n",dynsym_ELFSIZE));
    /* must have a symbol table */
    if (dynsym_ELFSIZE == NULL) {
      printf("loadelfELFSIZE: fatal error: found no symbol table\n");
      return(ENOEXEC);
    }
    
    /* print out address of string table */
    dprintf(("loadelfELFSIZE: strtab = 0x%X\n",strtab_ELFSIZE));
    /* must have a string table */
    if (strtab_ELFSIZE == NULL) {
      printf("loadelfELFSIZE: fatal error: found no string table\n");
      return(ENOEXEC);
    }
    
    /* print out got_index */
    dprintf(("loadelfELFSIZE: got_index = 0x%X\n",got_index_ELFSIZE));
    /* must have a valid got_index */
    if (got_index_ELFSIZE == -1) {
      printf("loadelfELFSIZE: fatal error: found no LOCAL_GOTNO\n");
      return(ENOEXEC);
    }
    
    /* print out dynsym_index */
    dprintf(("loadelfELFSIZE: dynsym_index = 0x%X\n",dynsym_index_ELFSIZE));
    /* must have a valid dynsym_index */
    if (dynsym_index_ELFSIZE == -1) {
      printf("loadelfELFSIZE: fatal error: found no GOTSYM\n");
      return(ENOEXEC);
    }
    
    /* print out dynsym_no */
    dprintf(("loadelfELFSIZE: dynsym_no = 0x%X\n",dynsym_no_ELFSIZE));
    /* must have a valid dynsym_no */
    if (dynsym_no_ELFSIZE == -1) {
      printf("loadelfELFSIZE: fatal error: found no SYMTAB_NO\n");
      return(ENOEXEC);
    }
    
    /* now fix up the GOT table */
    count = 0;
    notresolved = 0;
    for (i=dynsym_index_ELFSIZE ; i<dynsym_no_ELFSIZE ; i++) {

      /* search for undefined GOT table entries */
      if (dynsym_ELFSIZE[i].st_shndx == STN_UNDEF) {

	/* look up symbol in core's symbol table by string name */
	if (resolve_symbolELFSIZE(&_core_symbol_table, strtab_ELFSIZE+
				  dynsym_ELFSIZE[i].st_name, &addr)) {
	  printf("loadelfELFSIZE: could not resolve symbol %s\n",
		 strtab_ELFSIZE+dynsym_ELFSIZE[i].st_name);
	  notresolved++;
	}
	/* resolved symbol successfully */
	else {
	  /* set GOT entry to new address */
	  got_ELFSIZE[got_index_ELFSIZE+count].g_index = addr;
#if 0
	  dprintf(("set symbol %s with got index %d at 0x%X to addr 0x%X\n",
		   strtab_ELFSIZE+dynsym_ELFSIZE[i].st_name,got_index_ELFSIZE+count,
		   &got_ELFSIZE[got_index_ELFSIZE+count].g_index, addr));
#endif
	}

      }
      count++;
    }
    
    /* fail if any 1 symbol did not resolve */
    if (notresolved) { 
      printf("loadelfELFSIZE: could not resolve %d symbols\n",notresolved);
      return(ENOEXEC);
    }

    /* done */
    return(0);
    
}



