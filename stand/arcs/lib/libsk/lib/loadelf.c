/*
 * Elf loader code. This file is compiled twice, once for 32 bit objects
 * and once for 64 bit objects. Edit only the file loadelf.c
 * The files loadelf32.c and loadelf64.c are generated from loadelf.c
 * The token ELFSIZE should be used in this file anywhere a 32 or 64 would
 * normally be used. The makefile will insert the appropriate value depending
 * on which file is being generated.
 * similarly, the token PTRFMT will be replaced with the printf format
 * required to print out a pointer sized value, i.e. ll on 64 bit,
 * void on 32 bit
 */

#ident "$Revision: 1.58 $"

#include <sys/sbd.h>
#include <arcs/types.h>
#include <arcs/errno.h>
#include <arcs/eiob.h>
#include <elf.h>
#include <libsc.h>
#include <libsk.h>


#include <sys/EVEREST/evaddrmacros.h>	/* XXX -- needs to be only EVEREST */

#include "../../libsk/lib/loadelf.h"

/* Only include proper loader code -- 64 bit proms cannot really load
 * 32 bit programs (TFP for sure, and it is messy on the R4000), plus
 * we need to save the space.  32 bit proms need to be able to load
 * anything.
 */

/*#define DEBUG 1*/
#if IP26 && !DEBUG			/* some arch are short of prom space */
#define dprintf(x)
#else
#define dprintf(x) if (Debug) printf x 
#endif

extern int do_elf_relocELFSIZE(ElfELFSIZE_Shdr *, int, void *, void *, void *, __uint64_t);

extern MEMORYDESCRIPTOR *mem_contains(unsigned long, unsigned long);
extern MEMORYDESCRIPTOR *mem_getblock(void);
extern void mem_list(void);

extern int Verbose;
static unsigned int file_offset;

/* local function ptr for loadelfELFSIZEdynamic ... 
   this is set to a function that can load dynamic elf binaries - 
   if not set, an error will return on trying to load a dynmaic elf binary  */
static LONG (*loadelfELFSIZEdynamic)(char *filename,ULONG fd, ULONG *pc, 
				     ElfELFSIZE_Ehdr *elfhdr,
				     ElfELFSIZE_Phdr *pgmhdr) = NULL;

/* turns loading of dynamic elf binaries on 
   this function sets the loadelfELFSIZEdynamic function ptr
   to a function that can load dynamic elf binaries */
void set_loadelfdynamicELFSIZE(LONG (*func)(char *filename,ULONG fd, ULONG *pc, 
					    ElfELFSIZE_Ehdr *elfhdr,
					    ElfELFSIZE_Phdr *pgmhdr)) 
{

  loadelfELFSIZEdynamic = func;

}

static int
load_elf_struct(char *filename, ULONG *fd, int size, int offset, void *buf)
{
    LARGE off;
    ULONG cnt;
    int err;
#if !LIBSL && !ELF_2_COFF
    struct eiob *io;
#endif

    off.hi = 0;
    off.lo = file_offset;

#ifdef ELF_2_COFF
	/* for elf2coff program, we don't care about bootp stuff
	 * so comment out the following bootp()-related code
	 */
#else	/* ! ELF_2_COFF */
#if !LIBSL
    /* 
     * sash links only with libsc and libsl, so there is no get_eiob
     */
    dprintf(("loading ELF structure at offset 0x%x into memory at 0x%x\n", offset, buf));

    /* we can't do a reverse seek in bootp() because tftp doesn't 
     * support it. So we'll have to close the file and reopen it
     * in order to seek backward.
     * The ELF spec doesn't make any promises about what order the
     * various data structures will come in within the file, so barring
     * loading the entire file into memory and then dealing with it
     * there, this seemed like the quickest workaround. If you have
     * any better ideas please speak up.
     */
    if ((io = get_eiob(*fd)) == NULL)
	return(EBADF);

    /* if this is a bootp filesystem file */
    if (io->fsstrat == bootp_strat)
#endif	/* ! LIBSL */
    {
	if (offset < off.lo) {
	    int vsave = Verbose;
	    dprintf(("need to seek backward to offset %d (currently at %d).\n \
 Closing and reopening file\n", offset, off.lo));
	    Close(*fd);
	
	    Verbose = 0;		/* only one 'Obtaining...' message */
	    err = Open(filename, OpenReadOnly, fd);
	    Verbose = vsave;
	    file_offset = off.hi = off.lo = 0;

	    if (err != ESUCCESS) {
		load_error(filename, "Error reopening file in reverse seek.");
		return(err);
	    }
	}
    }
#endif	/* ! ELF_2_COFF */

    if(offset != off.lo) {
	off.hi = 0;
	off.lo = offset;

	if ((err = Seek(*fd, &off, SeekAbsolute)) != ESUCCESS) {
		load_error(filename, "Error seeking to %d.", offset);
		return(err);
	}
    }

    if ((err = Read(*fd, buf, size, &cnt)) != ESUCCESS) {
	load_error(filename, "Problem reading ELF structure at offset %d.",
		   offset);
	return (err);
    }
    
    if (cnt != size) {
	load_error(filename, "Problem reading ELF structure. %d read of %d.",
		   cnt, size);
	return (EIO);
    }
    file_offset = offset + size;

    return(ESUCCESS);
}

/* can be used by external functions ... */
int
load_elf_structELFSIZE(char *filename, ULONG *fd, int size, int offset, 
		       void *buf) {

  return(load_elf_struct(filename,fd,size,offset,buf));

}

static int
malloc_load_elf_struct(char *filename, ULONG *fd, int size, int offset, void **buf)
{
	*buf = dmabuf_malloc(size);
	if(*buf == 0) {
		return ENOMEM;
	}
	return load_elf_struct(filename, fd, size, offset, *buf);
}

static LONG
loadelfELFSIZE_noreloc(char *filename,
			ULONG fd,
			ULONG *pc,
			ElfELFSIZE_Ehdr *elfhdr,
			ElfELFSIZE_Phdr *pgmhdr)
{
	ULONG laddr, lowaddr, hiaddr;
	int np=0, err, x, offset, size;
	__psunsigned_t buf, bufp;

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
		if (pgmhdr[x].p_type != PT_LOAD)
			continue;

		/*
		 * Handle mapped kernels.  We always use direct-mapped
		 * memory, but we need to load it into our local node's
		 * memory.
		 */
		if (Debug)
			printf("Translated 0x%x ", pgmhdr[x].p_vaddr);

		pgmhdr[x].p_vaddr = kdmtolocal((void *)(__psint_t)pgmhdr[x].p_vaddr);

		if (Debug)
			printf("to 0x%x\n", pgmhdr[x].p_vaddr);


#ifdef EVEREST 
		/* We want to transfer the data into memory using coherent 
	 	 * I/O.  We therefore force all of the sections to start on
	 	 * cacheable addresses.
	 	 */
#if _MIPS_SIM != _ABI64
		/*
	 	 * Compress (potential) 64 bit address to 32 bits so we
	 	 * don't have to change all the drivers, etc...
	 	 */
		pgmhdr[x].p_vaddr = KPHYSTO32K0(pgmhdr[x].p_vaddr);
#endif
		pgmhdr[x].p_vaddr = K1_TO_K0(pgmhdr[x].p_vaddr);
#else
#if ELFSIZE == 64 && _MIPS_SIM != _ABI64	/* use R4000 compat space on 32 -> 64 jump */
	    	pgmhdr[x].p_vaddr = KPHYSTO32K0(pgmhdr[x].p_vaddr);
#endif
#endif /* EVEREST */

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
		err = load_elf_struct(filename, &fd, pgmhdr[x].p_filesz -
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

	/* Handle mapped kernels - does nothing if entry isn't K2 space */
	elfhdr->e_entry = kdmtolocal((void *)(__psint_t)elfhdr->e_entry);

#if ELFSIZE == 64 && _MIPS_SIM != _ABI64
	if ((elfhdr->e_entry & 0x9000000000000000LL) == 0x9000000000000000LL)
		elfhdr->e_entry = KPHYSTO32K1(elfhdr->e_entry);
	else
		elfhdr->e_entry = KPHYSTO32K0(elfhdr->e_entry);
#endif
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

#if !ELF_2_COFF
static int found_entry;
static unsigned long entry_offset;
static int entry_seg;
static
int
find_entry(ElfELFSIZE_Shdr *scnhdr, int rel_indx, void *symtab, char *strtab)
{
	static char *entry_tab[] = {
		"__start",
		"startsc",
		"start"
	};
	int symtab_size;
	ElfELFSIZE_Sym *symp;
	int i, j;

	if(! found_entry) {
		symtab_size = (int)(scnhdr[scnhdr[rel_indx].sh_link].sh_size
					/ scnhdr[scnhdr[rel_indx].sh_link].sh_entsize);
		for(i = 0, symp = (ElfELFSIZE_Sym *)symtab;
				i < symtab_size && !found_entry;
							i++, symp++) {
			for(j = 0;
				!found_entry && j < sizeof(entry_tab) / sizeof(entry_tab[0]);
							j++) {
				if(strcmp(strtab + symp->st_name, entry_tab[j]) == 0) {
					entry_offset = symp->st_value;
					entry_seg = symp->st_shndx;
					found_entry = 1;
				}
			}
		}
	}

	return 0;
}

static int code_seg_seen;
static unsigned long long code_start;

/* top_addr is the top address to be relocated to, if non-NULL */
static LONG
loadelfELFSIZE_reloc(char *filename,
		       ULONG fd,
		       ULONG *pc,
		       ElfELFSIZE_Ehdr *elfhdr,
                       __psunsigned_t top_addr)
{
	int size;
	ElfELFSIZE_Shdr *scnhdr;
	MEMORYDESCRIPTOR *m;
	ULONG laddr, lowaddr, hiaddr;
	unsigned long long virt_addr;
	ElfELFSIZE_Shdr *sp;
	__psunsigned_t imagesize = 0;
	unsigned int i;
#define FIRST_LEGAL_INDEX 0
	int symtab_indx = FIRST_LEGAL_INDEX - 1,
		rel_indx = FIRST_LEGAL_INDEX - 1,
		strtab_indx = FIRST_LEGAL_INDEX - 1;
	void *reltab = 0, *symtab = 0, *strtab = 0;
	int err = ESUCCESS;

	found_entry = 0;
	code_seg_seen = 0;
	dprintf(("loading ElfELFSIZE relocatable\n"));
	if (elfhdr->e_phnum != 0) {
		load_error(filename, "Elf Program header shows some segments"
			   " in a relocatable file.");
		err = EBADF;
		goto done;
	}


	if((ks = dmabuf_malloc(elfhdr->e_shnum * sizeof(struct ks))) == 0) {
		load_error(filename, "Ran out of memory.");
		err = ENOMEM;
		goto done;
	}
	bzero(ks, elfhdr->e_shnum * sizeof(struct ks));

	/* read in section header */
	size = elfhdr->e_shentsize * elfhdr->e_shnum;
	dprintf(("about to read 0x%x bytes from ELF header at offset 0x%x\n",
		size, elfhdr->e_shoff));
	scnhdr = 0;
	if (Verbose)
		printf("%PTRFMTd",size);
	if ((err = malloc_load_elf_struct(filename, &fd, size,
			       elfhdr->e_shoff, (void **)(&scnhdr))) != ESUCCESS) {
		if(scnhdr)
			dmabuf_free(scnhdr);
		Close(fd);
		return(err);
	}

	/* identify loadable sections */
#define IS_LOADABLE(sp) (((sp)->sh_flags & SHF_ALLOC) && ((sp)->sh_type == SHT_PROGBITS || (sp)->sh_type == SHT_NOBITS))
	dprintf(("adding up image size\n"));
	for(imagesize = i = 0, sp = scnhdr; i < elfhdr->e_shnum; i++, sp++) {
		if(IS_LOADABLE(sp)) {
			imagesize += sp->sh_size + (sp->sh_addralign - 1);
			dprintf(("imagesize = 0x%x\n", imagesize));
		}
	}


	/* load it at top_add if its !NULL else find big free area of memory */
	if (!top_addr) {
		if ((m = mem_getblock()) == 0) {
			load_error(filename, "No free memory descriptors available.");
#ifdef	DEBUG
			if (Debug) mem_list();
#endif	/* DEBUG */
			err = ENOMEM;
			goto done;
		}

		laddr = arcs_ptob(m->BasePage + m->PageCount) - imagesize;
	}
	else {
		laddr = top_addr - imagesize;
#ifdef _PAGESZ
		laddr &= ~(_PAGESZ-1);		/* page aligning loadaddr */
#endif
	}

	/* is it big enough? */
	if (!mem_contains(laddr, imagesize)) {
		load_error(filename, "Text start 0x%x, size "
			   "0x%x doesn't fit in a FreeMemory area.",
			   laddr, imagesize);
		err = ENOMEM;
		goto done;
	}

	/* now read loadable segments into that free chunk of memory */
#if ELFSIZE == 64 && _MIPS_SIM != _ABI64
#define K064BASE 0xa800000000000000LL
	virt_addr = K064BASE | laddr;
#else
#ifdef IP28
	/* Run relocatable programs uncached as they are almost always
	 * machine independent things like sash and fx.  They are compiled
	 * w/o the d$ speculation war and are at risk if run full cached.
	 * Running uncached is a bit perf hit, but most of their work is
	 * in in the prom cached, so it is tolerable.  When instructions
	 * are uncached, the T5 just runs slow.
	 */
	virt_addr = PHYS_TO_K1(laddr);
#else
	virt_addr = PHYS_TO_K0(laddr);
#endif /* IP28 */
#endif
	for(i = 0, sp = scnhdr; i < elfhdr->e_shnum; i++, sp++) {
		if(IS_LOADABLE(sp)) {
			dprintf(("alignment is 0x%x ", sp->sh_addralign));
			dprintf(("rounding addr up from 0x%llx ", virt_addr));
			virt_addr = (virt_addr + (sp->sh_addralign - 1)) & ~(sp->sh_addralign - 1);
			dprintf(("to 0x%llx ", virt_addr));
			if(code_seg_seen == 0 && ((sp->sh_flags & SHF_EXECINSTR) == SHF_EXECINSTR)) {
				code_seg_seen = 1;
				code_start = virt_addr;
			}
			if (sp->sh_type == SHT_NOBITS) {
				bzero((void *)PHYS_TO_K0(virt_addr), sp->sh_size);
			}
			else {
				if (Verbose && sp->sh_size)
					printf("+%PTRFMTd",sp->sh_size);
				err = load_elf_struct(filename, &fd,
						sp->sh_size, sp->sh_offset,
						(void *)PHYS_TO_K0(virt_addr));
				if (err != ESUCCESS) {
					load_error(filename,
					"Load failed from offset %d.",
					sp->sh_offset);

					goto done;
				}
			}
			ks[i].ks_raw = virt_addr;
			dprintf(("ks[%d].ks_raw == 0x%llx\n", i, ks[i].ks_raw));
			virt_addr += sp->sh_size;
		}
	}

	/* now loop thru all relocation sections and patch up
	 * all references
	 */
	for(i = 0, sp = scnhdr; err == 0 && i < elfhdr->e_shnum; i++, sp++) {
		switch(sp->sh_type) {
		case SHT_REL:
		case SHT_RELA:
			/* found a relocation section, now find which section
			 * is meant to be relocated by the info in the
			 * relocation section.  Only interested in relocating
			 * sections which are loaded into memory
			 */
			if(IS_LOADABLE(scnhdr + sp->sh_info)) {
				/* found a loaded section to be relocated, so
				 * read in the relocation information and
				 * process the relocation entries
				 */
				rel_indx = i;
				if(IS_LOADABLE(sp)) {
					reltab = (void *)PHYS_TO_K0(ks[rel_indx].ks_raw);
				}
				else {
					/* if not loaded, then load it */
					if((err = malloc_load_elf_struct(filename, &fd, sp->sh_size, sp->sh_offset, &reltab)) != ESUCCESS) {
						load_error(filename,"Load failed from offset %d.",sp->sh_offset);
						goto done;
					}
				}

				/* got the relocation info; now get the
				 * string table and symtable referred to
				 * by the relocation info
				 */

				if(strtab_indx != scnhdr[sp->sh_link].sh_link) { /* already got it? */
					/* No, throw away old one, if any */
					if(strtab_indx >= FIRST_LEGAL_INDEX) {
						if(IS_LOADABLE(scnhdr + strtab_indx)) {
							/* don't throw it away */
						}
						else { /* throw it away */
							dmabuf_free(strtab);
							strtab =  0;
							ks[strtab_indx].ks_raw = 0;
							strtab_indx = FIRST_LEGAL_INDEX - 1;
						}
					}
					/* now get one */
					strtab_indx = scnhdr[sp->sh_link].sh_link;
					if(IS_LOADABLE(scnhdr + strtab_indx)) {
						/* it's already loaded into memory */
						strtab = (void *)PHYS_TO_K0(ks[strtab_indx].ks_raw);
					}
					else { /* load it into memory */
						if((err = malloc_load_elf_struct(filename, &fd, scnhdr[strtab_indx].sh_size, scnhdr[strtab_indx].sh_offset, &strtab)) != ESUCCESS) {
							load_error(filename, "String table load failed.");
							goto done;
						}

					}
				}
				if(symtab_indx != sp->sh_link) { /* already got it? */
					/* No, throw away old one, if any */
					if(symtab_indx >= FIRST_LEGAL_INDEX) {
						if(IS_LOADABLE(scnhdr + symtab_indx)) {
							/* don't throw it away */
						}
						else { /* throw it away */
							dmabuf_free(symtab);
							symtab = 0;
							ks[symtab_indx].ks_raw = 0;
							symtab_indx = FIRST_LEGAL_INDEX - 1;
						}
					}
					/* now get one */
					symtab_indx = sp->sh_link;
					if(IS_LOADABLE(scnhdr + symtab_indx)) {
						/* it's already loaded into memory */
						symtab = (void *)PHYS_TO_K0(ks[symtab_indx].ks_raw);
					}
					else { /* load it into memory */
						if((err = malloc_load_elf_struct(filename, &fd, scnhdr[symtab_indx].sh_size, scnhdr[symtab_indx].sh_offset, &symtab)) != ESUCCESS) {
							load_error(filename, "Symbol table load failed.");
							goto done;
						}

					}
				}
				find_entry(scnhdr, rel_indx, symtab, strtab);
				err = do_elf_relocELFSIZE(scnhdr, rel_indx,
					reltab, strtab, symtab, ks[sp->sh_info].ks_raw);
				if (err) {
					load_error(filename,"Relocation failed.");
					goto done;
				}
				if(!IS_LOADABLE(sp)) {
					dmabuf_free(reltab);
					ks[rel_indx].ks_raw = 0;
				}
				rel_indx = FIRST_LEGAL_INDEX - 1;
				reltab = 0;
			}
			break;
		default:
			break;
		}
	}

done:
	dprintf(("err is 0x%x\n", err));
	if(scnhdr)
		dmabuf_free(scnhdr);
	if(ks)
		dmabuf_free(ks);
	if(rel_indx >= FIRST_LEGAL_INDEX && (!IS_LOADABLE(scnhdr + symtab_indx) || err)) {
		dmabuf_free(reltab);
	}
	if(symtab_indx >= FIRST_LEGAL_INDEX && (!IS_LOADABLE(scnhdr + symtab_indx) || err)) {
		dmabuf_free(symtab);
	}
	if(strtab_indx >= FIRST_LEGAL_INDEX && (!IS_LOADABLE(scnhdr + strtab_indx) || err)) {
		dmabuf_free(strtab);
	}
	Close (fd);
	FlushAllCaches();		/* does arch dependent flush */
	if(err != ESUCCESS) {
		/*XXX dmabuf_free(loaded code + data + other stuff? ) */
		return err;
	}

	if(elfhdr->e_entry != 0) {
		/* then use e_entry as an offset within the first code segment */
		elfhdr->e_entry += code_start;
	}
	else if(found_entry) {
		elfhdr->e_entry = ks[entry_seg].ks_raw + entry_offset;
	}
	else { /* use offset zero within the first code segment */
		elfhdr->e_entry = code_start;
	}
	lowaddr = laddr;
	hiaddr = laddr + imagesize - 1;
#if ELFSIZE == 64 && _MIPS_SIM != _ABI64
	if ((elfhdr->e_entry & 0x9000000000000000LL) == 0x9000000000000000LL)
		elfhdr->e_entry = KPHYSTO32K1(elfhdr->e_entry);
	else
		elfhdr->e_entry = KPHYSTO32K0(elfhdr->e_entry);
#endif
	if ( Verbose )
		printf(" entry: 0x%PTRFMTx\n", elfhdr->e_entry);
	*pc = elfhdr->e_entry;

	dprintf(("list init %x %x %x\n", lowaddr, hiaddr, hiaddr - lowaddr));
	if (err = elf_list_init(lowaddr, hiaddr))
		return (err);

	return(0);
}
#endif

LONG
loadelfELFSIZE(char *filename,
	       ULONG fd,
	       ULONG *pc,
	       ElfELFSIZE_Ehdr *elfhdr,
               __psunsigned_t load_addr)
{
    int size, err;
    ULONG cnt;
    ElfELFSIZE_Phdr *pgmhdr;

    dprintf(("loading ELFSIZE bit ELF\n"));

    /*
     * load the remainder of the ELF header
     */
    if ((err = Read(fd, (char*)elfhdr + ELFHDRBYTES,
		    sizeof(ElfELFSIZE_Ehdr) - ELFHDRBYTES, &cnt)) ||
	(cnt != sizeof(ElfELFSIZE_Ehdr) - ELFHDRBYTES)) {
	load_error (filename, "Problem reading ELF header, err %d"
		    " cnt %d.", err, cnt);
	Close(fd);
	return (err);
    }

    file_offset = sizeof(ElfELFSIZE_Ehdr);

    /*
     * load the program headers
     */
	dprintf(("elfhdr is 0x%x\n", elfhdr));
	if(size = elfhdr->e_phentsize * elfhdr->e_phnum ) {
		dprintf(("about to load program headers\n"));
		if((pgmhdr = (ElfELFSIZE_Phdr *) dmabuf_malloc(size)) == 0) {
		Close(fd);
		return ENOMEM;
		}

		dprintf(("load pgm headers at file offset %x\n",
		       elfhdr->e_phoff));

		if ((err = load_elf_struct(filename, &fd, size,
				       elfhdr->e_phoff, pgmhdr)) != ESUCCESS) {
		dmabuf_free(pgmhdr);
		Close(fd);
		return(err);
		}

		dprintf(("header entry size %d\n", elfhdr->e_phentsize));
		dprintf(("%d entries in file\n", elfhdr->e_phnum));
		if (Debug) {
			for(cnt = 0; cnt < elfhdr->e_phnum; cnt++) {
				printf("%d: type %x, offset %PTRFMTx, vaddr %PTRFMTx, "
					"file size %PTRFMTx, mem size %PTRFMTx, "
					"flags %x, align %PTRFMTx\n",
					cnt, 
					pgmhdr[cnt].p_type,
					pgmhdr[cnt].p_offset,
					pgmhdr[cnt].p_vaddr,
					pgmhdr[cnt].p_filesz,
					pgmhdr[cnt].p_memsz,
					pgmhdr[cnt].p_flags,
					pgmhdr[cnt].p_align);
			}
		}

	}
	else pgmhdr = 0;
    
	if(elfhdr->e_type == ET_EXEC)
		return loadelfELFSIZE_noreloc(filename, fd, pc, elfhdr, pgmhdr);
	else if(elfhdr->e_type == ET_REL) {
#if ELF_2_COFF
		printf("can't elf2coff a relocatable elf file\n");
		return ENOEXEC; /* if it's relocatable ELF */
#else
		if(pgmhdr)
			dmabuf_free(pgmhdr);
		return loadelfELFSIZE_reloc(filename, fd, pc, elfhdr, load_addr);
#endif
	}
        else if (elfhdr->e_type ==  ET_DYN) {

	        dprintf(("loadelfELFSIZE: loadelfELFSIZEdynamic = 0x%X\n",
			 loadelfELFSIZEdynamic));

	        /* if the function ptr to loadelfELFSIZEdynamic is null,
		   we dont handle loading dynamic elf binaries ... */
	        if (loadelfELFSIZEdynamic == NULL) {
		  /* we dont handle dynamic binaries ... */
		  printf("cannot load a dynamic elf binary ...\n");
		  Close(fd);
		  return ENOEXEC; 
		}
		else
		  /* call the function */
		  return( (*loadelfELFSIZEdynamic)(filename,fd,pc,elfhdr,pgmhdr) );

        }

       /* if it's not fixed or relocatable ELF , or dynamic */
	Close(fd);
	return ENOEXEC; 
}



