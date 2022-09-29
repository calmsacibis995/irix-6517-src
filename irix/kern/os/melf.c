/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 *	SGI module loader ELF routines.
 */

#ident	"$Revision: 1.46 $"

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/ddi.h>
#include <sys/idbg.h>
#include <sys/sysmacros.h>
#include <sys/errno.h>
#include <sys/flock.h>
#include <sys/immu.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/vnode.h>

#include <elf.h>
#include <aouthdr.h>
#include <filehdr.h>
#include <scnhdr.h>
#include <sym.h>
#include <symconst.h>
#include <reloc.h>
#include <sys/mload.h>
#include <sys/mloadpvt.h>
#include <string.h>
#include <sys/inst.h>

#if _MIPS_SIM == _ABI64
#define _64BIT_OBJECTS 1
#endif

#if 0
#define	MTRACE	1
#define	MVERBOSE 1
#endif

#ifdef	MVERBOSE
int mverbose = 1;
#define mvprintf(x)	if (mverbose) printf x
#else	/* !MVERBOSE */
#define mvprintf(x)
#endif	/* MVERBOSE */

/* ELF prototypes */

int  mreadelf (ml_info_t *, vnode_t *);
void mfreeelfobj (ml_info_t *);
int  doelfrelocs (ml_info_t *);
int  msetelfsym (ml_info_t *, int);
static key_section_info *section_key (char *);
int  symcmp(const void *, const void *);
int melf_findaddr(char *, __psunsigned_t **, int, sema_t *);
char * melf_findname(__psunsigned_t *, int, sema_t *);
int melf_findexactname(ml_info_t *, char *, long *);

#ifdef JUMP_WAR
/* these routines/defines are needed to fix up jalr-at-eop problems */
static int melf_scan_badjumps(ADDR, int);
#endif /* JUMP_WAR */

/* bring in the calvary for sorting the /unix symtab */
extern void qsort(void *, size_t, size_t, int (*)(const void *, const void *));

extern sema_t mlinfosem;

/*
 * This section list contains only the ELF sections that we are 
 * concerned with.
 */
static key_section_info wordlist[] = 
{
	{".text",	SEC_TEXT},
	{".rel.text",	SEC_TEXTREL},
	{".rela.text",	SEC_TEXTRELA},
	{".data",	SEC_DATA},
	{".rel.data",	SEC_DATAREL},
	{".rela.data",	SEC_DATARELA},
	{".rodata",	SEC_RODATA},
	{".rel.rodata",	SEC_RODATAREL},
	{".rela.rodata",SEC_RODATARELA},
	{".bss",	SEC_BSS},
	{".symtab",	SEC_SYMTAB},
	{".shstrtab",	SEC_SHSTRTAB},
	{".strtab",	SEC_STRTAB},
};

#define	MREAD(addr, size, offset)	vn_rdwr (UIO_READ, vp, (caddr_t)addr, \
					size, (off_t)offset, UIO_SYSSPACE, 0, \
					0L, cr, &resid, sys_flid)

/*
 * mreadelf - read elf .o file
 */
int
mreadelf (ml_info_t *minfo, vnode_t *vp)
{
	int i, size;
	int error = 0;
	ssize_t resid = 0;
	int tsize = 0; int dsize = 0; int bsize = 0; int rsize = 0;
	char *curtextaddr, *curdataaddr;
	elf_obj_info_t *obj;
	key_section_info *key;
	Elf_Word arch;
	Elf_Shdr *shdr;
	char *sh_name;
	cred_t *cr = get_current_cred();
#ifdef JUMP_WAR
	int align;
#endif /* JUMP_WAR */
#define SECTALIGN       16
#define SECTRND(x)      (((ulong)(x) + (SECTALIGN-1)) & ~(SECTALIGN-1));
#ifdef TRITON
	extern __uint32_t _irix5_mips4;
#endif /* TRITON */

#ifdef	MTRACE
	printf ("mreadelfXX\n");
#endif	/* MTRACE */

	minfo->ml_obj = (elf_obj_info_t *) kern_calloc(1, sizeof(elf_obj_info_t));
	obj = (elf_obj_info_t *)minfo->ml_obj;

	/* read elf header */
	obj->ehdr = kern_malloc (sizeof(Elf_Ehdr));
	if (MREAD(obj->ehdr, sizeof(Elf_Ehdr), 0) != 0 || resid) {
		error = ENOEXEC;
		goto elfdone;
	}

	/* check magic */
	if (obj->ehdr->e_ident[EI_MAG2] != ELFMAG2
	    || obj->ehdr->e_ident[EI_MAG3] != ELFMAG3
	    || obj->ehdr->e_ident[EI_DATA] != ELFDATA2MSB
	    || obj->ehdr->e_type != ET_REL) {
		error = MERR_BADMAGIC;
		goto elfdone;
	}

	if (obj->ehdr->e_type != ET_REL) {
		error = MERR_ET_REL;
		goto elfdone;
	}

#if _MIPS_SIM == _ABI64
	/* A 64 bit kernel requires a 64bit loadable module */
	if (obj->ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
		error = MERR_ELF64;
		goto elfdone;
	}	
#elif _MIPS_SIM == _ABIN32
	/* An N32 bit kernel requires an N32bit loadable module */
	if (!(obj->ehdr->e_flags & EF_MIPS_ABI2)) {
		error = MERR_ELFN32;
		goto elfdone;
	}
#else	/* _ABI64 */
	/* A 32 bit kernel requires a 32bit loadable module */
	if ((obj->ehdr->e_ident[EI_CLASS] != ELFCLASS32) ||
	    (obj->ehdr->e_flags & EF_MIPS_ABI2)) {
		error = MERR_ELF32;
		goto elfdone;
	}	
#endif	/* _ABI64 */

	minfo->flags |= ML_ELF;

	/*
	 * check architecture XXX bad ld puts in 0xf for ARCH!
	 */
	arch = obj->ehdr->e_flags & EF_MIPS_ARCH;
	if (arch != 0 &&
	    arch != EF_MIPS_ARCH &&
	    arch != EF_MIPS_ARCH_2 &&
#if MIPS4_ISA
	    (! (
#ifdef TRITON
		_irix5_mips4 != 0 &&
#endif /* TRITON */
		arch == EF_MIPS_ARCH_4)) &&
#endif
	    arch != EF_MIPS_ARCH_3) {
		error = MERR_BADARCH;
		goto elfdone;
	}

	/* other checks */
	if (obj->ehdr->e_shstrndx == SHN_UNDEF) {
		error = MERR_SHSTR;
		goto elfdone;
	}

	/* read the section headers */
	size = obj->ehdr->e_shnum * obj->ehdr->e_shentsize;
	obj->shdr = (Elf_Shdr *) kern_malloc (size);
	if (MREAD(obj->shdr, size, obj->ehdr->e_shoff) != 0 || resid) {
		error = MERR_BADSH;
		goto elfdone;
	}

	/* read section header string table */
	size = obj->shdr[obj->ehdr->e_shstrndx].sh_size;
	obj->shstringtab = (char *) kern_malloc (size);
	if (MREAD(obj->shstringtab, size, 
		obj->shdr[obj->ehdr->e_shstrndx].sh_offset) != 0 || resid) {
		error = MERR_SHSTR;
		goto elfdone;
	}

	/* mark the sections that we care about */
	for (i=0; i<obj->ehdr->e_shnum; i++) {

		sh_name = obj->shstringtab + obj->shdr[i].sh_name;

		/* check for the sections that we don't support */
		if ((strcmp (sh_name, ".sdata") == 0) ||
		    (strcmp (sh_name, ".sbss") == 0)) {
			error = MERR_GP;
			goto elfdone;
		}

		/* mark the sections that we care about */
		if ((key = section_key (sh_name)) != 0 
			&& obj->key_sections[key->sec] == 0)
			obj->key_sections[key->sec] = i;
	}

	/* read symbol table */
	if (!obj->key_sections[SEC_SYMTAB]) {
		error = MERR_NOSYMS;
		goto elfdone;
	}
	shdr = &obj->shdr[obj->key_sections[SEC_SYMTAB]];
	obj->symtab = (Elf_Sym *) kern_malloc (shdr->sh_size);
	if (MREAD(obj->symtab, shdr->sh_size, shdr->sh_offset) != 0 || resid) {
		error = MERR_SYMTAB;
		goto elfdone;
	}

	/* read string table */
	if (!obj->key_sections[SEC_STRTAB]) {
		error = MERR_NOSTRTAB;
		goto elfdone;
	}
	shdr = &obj->shdr[obj->key_sections[SEC_STRTAB]];
	obj->stringtab = (char *) kern_malloc (shdr->sh_size);
	if (MREAD(obj->stringtab, shdr->sh_size, shdr->sh_offset) != 0 	
		|| resid) {
		error = MERR_STRTAB;
		goto elfdone;
	}

	/* malloc space for the section data pointers */
	obj->scn_dataptrs = kern_calloc(1, (int)sizeof(void *)*SEC_LAST_ENUM);

	/* SARAH - can probably redo scn_dataptrs, so that we don't need to
		keep scn_text, etc. */

	/* Malloc relocation space for text and data section data.
	 * Each section is aligned on a SECTALIGN boundary.  This 
	 * provides a single, contiguous symbol space for the module,
	 * delimited by ml_text and ml_end.
	 */

	if (obj->key_sections[SEC_TEXT])
		tsize = obj->shdr[obj->key_sections[SEC_TEXT]].sh_size
		    + SECTALIGN;
	if (obj->key_sections[SEC_DATA])
		dsize = obj->shdr[obj->key_sections[SEC_DATA]].sh_size
		    + SECTALIGN;
	if (obj->key_sections[SEC_RODATA])
		rsize = obj->shdr[obj->key_sections[SEC_RODATA]].sh_size
		    + SECTALIGN;
	if (obj->key_sections[SEC_BSS])
		bsize = obj->shdr[obj->key_sections[SEC_BSS]].sh_size
		    + SECTALIGN;
	if (!(size = tsize + rsize + dsize + bsize)) {
		error = MERR_NOSEC;
		goto elfdone;
	}

	minfo->ml_text = kern_malloc(size);
	minfo->ml_end = minfo->ml_text + size;

	curtextaddr = (char *)SECTRND(minfo->ml_text);
	curdataaddr = (char *)SECTRND(curtextaddr + tsize);

	/* if there is a text sections, read it */
	if (obj->key_sections[SEC_TEXT]) {
		shdr = &obj->shdr[obj->key_sections[SEC_TEXT]];
		if (MREAD(curtextaddr, shdr->sh_size, shdr->sh_offset) != 0 || resid) {
			error = MERR_BADTEXT;
			goto elfdone;
		}
		obj->text_scn = obj->key_sections[SEC_TEXT];
		obj->scn_dataptrs[obj->text_scn] = curtextaddr;
#ifdef	MTRACE
		mprintf (("SEC_TEXT location = 0x%x\n", curtextaddr));
#endif	/* MTRACE */

	}

#ifdef JUMP_WAR
	/*
	 * Rev 2.2 of the r4k has problems with JALR-at-EOP instructions.
	 * We avoid this situation by attempting to determine a "safe"
	 * realignment.  If melf_scan_badjumps() suggests a non-zero
	 * realignment value, then we must reallocate our text space
	 * and reinitialize the corresponding pointers.
	 */
	if ((align = melf_scan_badjumps((ADDR)curtextaddr, size)) < 0) {
		cmn_err(CE_CONT, "WARNING: branch at EOP cannot be realigned.  Load rejected.\n");
		error = MERR_BADTEXT;	
		goto elfdone;
	} else if (align > 0) {
		char *tmp;

		minfo->ml_base = (char *)kern_malloc(size + align);
		tmp = minfo->ml_text;
		minfo->ml_text = (char *)((caddr_t)minfo->ml_base + align);
		minfo->ml_end = minfo->ml_text + size;
		curtextaddr = (char *)SECTRND(minfo->ml_text);
		(void) bcopy((void *)tmp, (void *)curtextaddr, shdr->sh_size);
		(void) kern_free(tmp);
		curdataaddr = (char *)SECTRND(curtextaddr + tsize);
		obj->scn_dataptrs[obj->text_scn] = curtextaddr;
	} else {
		minfo->ml_base = minfo->ml_text;
	}
#else
	minfo->ml_base = minfo->ml_text;
#endif /* JUMP_WAR */

	/* if there is a text rel section, read it */
	if (obj->key_sections[SEC_TEXTREL]) {
		/* read text relocation section */
		shdr = &obj->shdr[obj->key_sections[SEC_TEXTREL]];
		obj->text_reloctab = (Elf_Rel *) kern_malloc (shdr->sh_size);
		if (MREAD(obj->text_reloctab, shdr->sh_size, shdr->sh_offset) != 0 
			|| resid) {
			error = MERR_BADTEXT;
			goto elfdone;
		}
	}

	/* if there is a text rela section, read it */
	if (obj->key_sections[SEC_TEXTRELA]) {
		shdr = &obj->shdr[obj->key_sections[SEC_TEXTRELA]];
		obj->text_relocatab = (Elf_Rela *) kern_malloc (shdr->sh_size);
		if (MREAD(obj->text_relocatab, shdr->sh_size, shdr->sh_offset) != 0 
			|| resid) {
			error = MERR_BADTEXT;
			goto elfdone;
		}
	}

	/* read data */
	if (obj->key_sections[SEC_DATA]) {
		shdr = &obj->shdr[obj->key_sections[SEC_DATA]];
		if (MREAD(curdataaddr, shdr->sh_size, shdr->sh_offset) != 0 
			|| resid) {
			error = MERR_BADDATA;
			goto elfdone;
		}
		obj->data_scn = obj->key_sections[SEC_DATA];
		obj->scn_dataptrs[obj->data_scn] = curdataaddr;
#ifdef	MTRACE
		mprintf (("SEC_DATA location = 0x%x, dataptrs=0x%x\n", 
			curdataaddr, obj->scn_dataptrs[obj->data_scn]));
#endif	/* MTRACE */
		curdataaddr = (char *)SECTRND(curdataaddr + shdr->sh_size);
	}

	/* if there is a data rel section, read it */
	if (obj->key_sections[SEC_DATAREL]) {
		shdr = &obj->shdr[obj->key_sections[SEC_DATAREL]];
		obj->data_reloctab = (Elf_Rel *) kern_malloc (shdr->sh_size);
		if (MREAD(obj->data_reloctab, shdr->sh_size, shdr->sh_offset) != 0 
			|| resid) {
			error = MERR_BADREL;
			goto elfdone;
		}
	}

	/* if there is a data rela section, read it */
	if (obj->key_sections[SEC_DATARELA]) {
		shdr = &obj->shdr[obj->key_sections[SEC_DATARELA]];
		obj->data_relocatab = (Elf_Rela *) kern_malloc (shdr->sh_size);
		if (MREAD(obj->data_relocatab, shdr->sh_size, shdr->sh_offset) != 0 
			|| resid) {
			error = MERR_BADREL;
			goto elfdone;
		}
	}

	/* read rodata */
	if (obj->key_sections[SEC_RODATA]) {
		shdr = &obj->shdr[obj->key_sections[SEC_RODATA]];
		if (MREAD(curdataaddr, shdr->sh_size, shdr->sh_offset) != 0 
			|| resid) {
			error = MERR_BADDATA;
			goto elfdone;
		}
		obj->rodata_scn = obj->key_sections[SEC_RODATA];
		obj->scn_dataptrs[obj->rodata_scn] = curdataaddr;
#ifdef	MTRACE
		mprintf (("SEC_RODATA location = 0x%x, dataptrs=0x%x\n", 
			curdataaddr, obj->scn_dataptrs[obj->rodata_scn]));
#endif	/* MTRACE */
		curdataaddr = (char *)SECTRND(curdataaddr + shdr->sh_size);
	}

	/* if there is a rodata rel section, read it */
	if (obj->key_sections[SEC_RODATAREL]) {
		shdr = &obj->shdr[obj->key_sections[SEC_RODATAREL]];
		obj->rodata_reloctab = (Elf_Rel *) kern_malloc (shdr->sh_size);
		if (MREAD(obj->rodata_reloctab, shdr->sh_size, shdr->sh_offset) != 0 
			|| resid) {
			error = MERR_BADDATA;
			goto elfdone;
		}
	}

	/* if there is a rodata rela section, read it */
	if (obj->key_sections[SEC_RODATARELA]) {
		shdr = &obj->shdr[obj->key_sections[SEC_RODATARELA]];
		obj->rodata_relocatab = (Elf_Rela *) kern_malloc (shdr->sh_size);
		if (MREAD(obj->rodata_relocatab, shdr->sh_size, shdr->sh_offset) != 0 
			|| resid) {
			error = MERR_BADDATA;
			goto elfdone;
		}
	}

	/* setup bss */
	if (obj->key_sections[SEC_BSS]) {
		shdr = &obj->shdr[obj->key_sections[SEC_BSS]];
		obj->bss_scn = obj->key_sections[SEC_BSS];
		bzero(curdataaddr, shdr->sh_size);
		obj->scn_dataptrs[obj->bss_scn] = curdataaddr;
#ifdef	MTRACE
		mprintf (("SEC_BSS location = 0x%x, dataptrs=0x%x\n", 
			curdataaddr, obj->scn_dataptrs[obj->bss_scn]));
#endif	/* MTRACE */
		curdataaddr = (char *)SECTRND(curdataaddr + shdr->sh_size);
	}

elfdone:
#ifdef	MVERBOSE
	if (error)
		mvprintf (("mreadelf system error=%d\n", error));
#endif	/* MVERBOSE */

	return error;
#undef SECTALIGN
#undef SECTRND
}

/*
 * mreadelfsymtab
 */
int
mreadelfsymtab (ml_info_t *minfo, vnode_t *vp)
{
	int i, size;
	int error = 0;
	ssize_t resid = 0;
	char *sh_name;
	Elf_Word arch;
	Elf_Shdr *shdr;
	elf_obj_info_t *obj;
	key_section_info *key;
	cred_t *cr = get_current_cred();

	minfo->ml_obj = (elf_obj_info_t *) kern_calloc(1, sizeof(elf_obj_info_t));
	obj = (elf_obj_info_t *)minfo->ml_obj;

	/* read elf header */
	obj->ehdr = kern_malloc (sizeof(Elf_Ehdr));
	if (MREAD(obj->ehdr, sizeof(Elf_Ehdr), 0) != 0 || resid) {
		error = ENOEXEC;
		goto elfsymdone;
	}

	/* check magic */

	/* XXX - type will be ET_EXEC, rather than ET_REL. */

	if (obj->ehdr->e_ident[EI_MAG2] != ELFMAG2
	    || obj->ehdr->e_ident[EI_MAG3] != ELFMAG3
	    || obj->ehdr->e_ident[EI_CLASS] != ELFCLASS
	    || obj->ehdr->e_ident[EI_DATA] != ELFDATA2MSB) {
		error = MERR_BADMAGIC; 
		goto elfsymdone;
	}
	minfo->flags |= ML_ELF;

	/*
	 * check architecture XXX bad ld puts in 0xf for ARCH!
	 */
	arch = obj->ehdr->e_flags & EF_MIPS_ARCH;
	if (arch != 0 &&
	    arch != EF_MIPS_ARCH &&
	    arch != EF_MIPS_ARCH_2 &&
#if MIPS4_ISA
	    arch != EF_MIPS_ARCH_4 &&
#endif
	    arch != EF_MIPS_ARCH_3) {
		error = MERR_BADARCH;
		goto elfsymdone;
	}

	/* other checks */
	if (obj->ehdr->e_shstrndx == SHN_UNDEF) {
		error = MERR_SHSTR;
		goto elfsymdone;
	}

	/* read the section headers */
	size = obj->ehdr->e_shnum * obj->ehdr->e_shentsize;
	obj->shdr = (Elf_Shdr *) kern_malloc (size);
	if (MREAD(obj->shdr, size, obj->ehdr->e_shoff) != 0) {
		error = MERR_BADSH;
		goto elfsymdone;
	}

	/* read section header string table */
	size = obj->shdr[obj->ehdr->e_shstrndx].sh_size;
	obj->shstringtab = (char *) kern_malloc (size);
	if (MREAD(obj->shstringtab, size, 
		obj->shdr[obj->ehdr->e_shstrndx].sh_offset) != 0 || resid) {
		error = MERR_SHSTR;
		goto elfsymdone;
	}

	/* mark the sections that we care about */
	for (i=0; i<obj->ehdr->e_shnum; i++) {
		sh_name = obj->shstringtab + obj->shdr[i].sh_name;

		if ((key = section_key (sh_name)) != 0 &&
		    obj->key_sections[key->sec] == 0)
			obj->key_sections[key->sec] = i;
	}
	if (!obj->key_sections[SEC_SYMTAB]) {
		error = MERR_NOSYMS;
		goto elfsymdone;
	}
	if (!obj->key_sections[SEC_STRTAB]) {
		error = MERR_NOSTRTAB;
		goto elfsymdone;
	}

	shdr = &obj->shdr[obj->key_sections[SEC_SYMTAB]];
	obj->symtab = (Elf_Sym *) kern_malloc (shdr->sh_size);
	if (MREAD(obj->symtab, shdr->sh_size, shdr->sh_offset) != 0 || resid) {
		error = MERR_SYMTAB;
		goto elfsymdone;
	}

	shdr = &obj->shdr[obj->key_sections[SEC_STRTAB]];
	obj->stringtab = (char *) kern_malloc (shdr->sh_size);
	if (MREAD(obj->stringtab, shdr->sh_size, shdr->sh_offset) != 0
	    || resid) {
		error = MERR_STRTAB;
		goto elfsymdone;
	}

elfsymdone:
#ifdef  MVERBOSE
	if (error)
		mvprintf (("mreadelfsym system error=%d\n", error));
#endif  /* MVERBOSE */

        return error;

}

/*
 * mfreeelfobj - free elf obj struct
 */
void
mfreeelfobj (ml_info_t *minfo)
{
	elf_obj_info_t *obj = (elf_obj_info_t *)minfo->ml_obj;

	if (!obj)
		return;
	if (obj->ehdr)
		kern_free (obj->ehdr);
	if (obj->shdr)
		kern_free (obj->shdr);
	if (obj->symtab)
		kern_free (obj->symtab);
	if (obj->text_reloctab)
		kern_free (obj->text_reloctab);
	if (obj->text_relocatab)
		kern_free (obj->text_relocatab);
	if (obj->data_reloctab)
		kern_free (obj->data_reloctab);
	if (obj->data_relocatab)
		kern_free (obj->data_relocatab);
	if (obj->rodata_reloctab)
		kern_free (obj->rodata_reloctab);
	if (obj->rodata_relocatab)
		kern_free (obj->rodata_relocatab);
	if (obj->stringtab)
		kern_free (obj->stringtab);
	if (obj->shstringtab)
		kern_free (obj->shstringtab);
	if (obj->scn_dataptrs)
		kern_free (obj->scn_dataptrs);
	kern_free (obj);
	minfo->ml_obj = 0;
}

/*
 * msetelfsym - create an ml symbol table and string table for the module.
 */
int
msetelfsym (ml_info_t *minfo, int noreloc)
{
	int i, size, nsyms = 0;
	long newaddr;
	int bind, type;
	int error = 0;
	int offset = 0;
	elf_obj_info_t *obj = (elf_obj_info_t *)minfo->ml_obj;
	ml_sym_t *tmpsyms = 0;
	Elf_Sym *sym;
	char *tmpstrs = 0, *np;

#ifdef	MTRACE
	printf ("msetelfsym\n");
#endif

	/* 
	 * Set up a small symbol table that contains only stGlobal and stProc
	 * symbols for this module. The symbol table contains only the address 
	 * and the offset into the string table for that symbol. The address
	 * of the symbol is fixed up to be the address of the relocated code.
	 * The string table contains only the strings that go with the symbols
	 * saved.
	 *
	 * If M_SYMDEBUG is true, keep statics in the symbol table also.
	 */
	tmpsyms = (ml_sym_t *) kern_malloc (obj->shdr[obj->key_sections[SEC_SYMTAB]].sh_size);
	tmpstrs = (char *) kern_malloc (obj->shdr[obj->key_sections[SEC_STRTAB]].sh_size);

	size = obj->shdr[obj->key_sections[SEC_SYMTAB]].sh_size / sizeof(Elf_Sym);

	for (i=0; i<size; i++) {
		sym = &obj->symtab[i];
		bind = ELF_ST_BIND (sym->st_info);		
		type = ELF_ST_TYPE (sym->st_info);
		if (minfo->ml_desc->m_flags & M_SYMDEBUG) {
			if ((bind != STB_GLOBAL) && (bind != STB_LOCAL))
				continue;
		} else {
			if (bind != STB_GLOBAL)
				continue;
		}
		if ((type != STT_OBJECT) && (type != STT_FUNC))
			continue;
		if (sym->st_shndx == SHN_UNDEF || sym->st_shndx == SHN_ABS)
			continue;

		/* hack to avoid meddlesoem */
		np = (char *) obj->stringtab + sym->st_name;
		if (np == (char *) 0)
			continue;
		newaddr = sym->st_value;

		if (!noreloc) {
			if (sym->st_shndx == obj->key_sections[SEC_TEXT])
				newaddr += (long)obj->scn_dataptrs[obj->text_scn];
			else if (sym->st_shndx == obj->key_sections[SEC_DATA])
				newaddr += (long)obj->scn_dataptrs[obj->data_scn];
			else if (sym->st_shndx == obj->key_sections[SEC_RODATA])
				newaddr += (long)obj->scn_dataptrs[obj->rodata_scn];
			else if (sym->st_shndx == obj->key_sections[SEC_BSS])
				newaddr += (long)obj->scn_dataptrs[obj->bss_scn];
			else if (sym->st_shndx == SHN_COMMON) {
				error = MERR_COMMON;
				goto done;
			} else {
				error = MERR_SHNDX;
				goto done;
			}
		}
		tmpsyms[nsyms].m_addr = newaddr;
		tmpsyms[nsyms].m_off = offset;

		while (*np != '\0')
			tmpstrs[offset++] = *np++;
		tmpstrs[offset++] = '\0';

		nsyms++;
	}
	minfo->ml_symtab = 0;
	minfo->ml_stringtab = 0;
	minfo->ml_strtabsz = 0;

	if (!nsyms) {
		error = MERR_NOSYMS;
		goto done;
	} else {
		minfo->ml_symtab = kern_malloc (nsyms * sizeof (ml_sym_t));
		minfo->ml_stringtab = kern_malloc (offset);
	}

	(void) qsort((void *)tmpsyms, nsyms, sizeof (ml_sym_t), &symcmp);

	bcopy (tmpsyms, minfo->ml_symtab, nsyms * sizeof (ml_sym_t));
	bcopy (tmpstrs, minfo->ml_stringtab, offset);
	minfo->ml_nsyms = nsyms;
	minfo->ml_strtabsz = offset;

done:
	if (tmpsyms)
		kern_free (tmpsyms);
	if (tmpstrs)
		kern_free (tmpstrs);
	if (error) {
		if (minfo->ml_symtab)
			kern_free (minfo->ml_symtab);
		if (minfo->ml_stringtab)
			kern_free (minfo->ml_stringtab);
	}
	return error;
}

static key_section_info *
section_key (char *str)
{
	int i;

	for (i=0; i<SEC_LAST_ENUM; i++) {
		if (strcmp (str, wordlist[i].name) == 0)
			return &wordlist[i];
	}
	return 0;
}

/*
 * melf_findaddr - given a name, search for symbol's address in module list
 *	type defines which type of modules to examine for symbols
 *
 * return 0 on success, with addr filled in
 * return 1 on failure
 */
int
melf_findaddr(char *name, __psunsigned_t **addr, int type, sema_t *sem)
{
    ml_info_t *m;
    int i;

    /* Search any libraries that might be loaded */
    psema(sem, PZERO);
    m = mlinfolist;
    while (m) {
	if (!type || m->ml_desc->m_type == type) {
		for (i=0; i<m->ml_nsyms; i++) {
			if (strcmp(name, 
			  &m->ml_stringtab[((ml_sym_t *)(m->ml_symtab))[i].m_off]) == 0) {
				*addr = (__psunsigned_t *)((ml_sym_t *)(m->ml_symtab))[i].m_addr;
				vsema(sem);
				return 0;
			}
		}
	}
	m = m->ml_next;
    }

    *addr = 0;
    vsema(sem);
    return 1;
}

/*
 * melf_findname - given an address, search for symbol's name in module list
 *	type defines which type of modules to examine for symbols
 */
char *
melf_findname(__psunsigned_t *addr, int type, sema_t *sem)
{
    ml_info_t *m;
    int i;

    /* Search any libraries that might be loaded */
    psema(sem, PZERO);
    m = mlinfolist;
    while (m) {
	if (!type || m->ml_desc->m_type == type) {
		for (i=0; i<m->ml_nsyms; i++) {
			if (addr == (__psunsigned_t *)((ml_sym_t *)(m->ml_symtab))[i].m_addr) {
				vsema(sem);
			        return &m->ml_stringtab[((ml_sym_t *)(m->ml_symtab))[i].m_off];
			}
		}
	}
	m = m->ml_next;
    }

    vsema(sem);
    return 0;
}

int
melf_findexactname(ml_info_t *minfo, char *name, long *entry)
{
	int sym;
	int found = 0;
	ml_sym_t *symtab = (ml_sym_t *) minfo->ml_symtab;

	for (sym=0; sym<minfo->ml_nsyms; sym++) {
		if (strcmp((char *)(minfo->ml_stringtab + symtab[sym].m_off),
			name) == 0) {
			*entry = symtab[sym].m_addr;
			found = 1;
			break;
		}
	}
	if (((int (*)()) *entry == nodev) || (!found)) {
		if (mloaddebug == MUNDEF)
			mprintf (("mfindname: could not find %s\n", name));
		return 1;
	}
#ifdef	MVERBOSE
	mprintf (("mfindname found: %s 0x%x\n", name, *entry));
#endif	/* MVERBOSE */
	return 0;
}

/*
 * relocation code - borrowed from v4.00 ld/relocate.c, but pared down,
 * 	since we don't support GOT, GP, LIT, CALL, etc.
 */

typedef struct reloc_attr {
    /* number of bytes the relocation value p4-18 of ABI-mips */
    char val_size;
    char val_type;		    /* specifics of how to get value */
#define      VAL_NORMAL	 0   
#define	     VAL_SCNDSP	 1    /* use relative offset from section */
#define	     VAL_BAD	 2
    char addend_type;
#define      ADEND_NONE  0
#define      ADEND_HALF  1
#define      ADEND_WORD  2
#define      ADEND_JAL   3
#define      ADEND_HI    4
#define      ADEND_LO    5
#define      ADEND_XWORD 6
#define      ADEND_BAD   7
#define	     ADEND_ADDR	 8    
} RELC_ATTR;

static const RELC_ATTR relc_attr[_R_MIPS_COUNT_ + 2] = {
    0,  VAL_NORMAL,	ADEND_NONE,	/* R_MIPS_NONE */
    2,  VAL_NORMAL,	ADEND_HALF,	/* R_MIPS_16 */
    4,  VAL_NORMAL,	ADEND_WORD,	/* R_MIPS_32 */
    4,  VAL_NORMAL,	ADEND_NONE,	/* R_MIPS_REL32 */
    4,  VAL_NORMAL,	ADEND_JAL,	/* R_MIPS_26 */
    4,  VAL_NORMAL,	ADEND_HI,	/* R_MIPS_HI16 */
    4,  VAL_NORMAL,	ADEND_LO,	/* R_MIPS_LO16 */
    4,  VAL_NORMAL,	ADEND_BAD,	/* R_MIPS_GPREL16 */
    4,  VAL_BAD,	ADEND_BAD,	/* R_MIPS_LITERAL */
    4,  VAL_BAD,	ADEND_BAD,	/* R_MIPS_GOT16 */
    4,  VAL_NORMAL,	ADEND_BAD,	/* R_MIPS_PC16 */
    4,  VAL_BAD,	ADEND_BAD,	/* R_MIPS_CALL16 */
    4,  VAL_NORMAL,	ADEND_BAD,	/* R_MIPS_GPREL32 */
    0,  VAL_NORMAL,	ADEND_BAD,	/*  */
    0,  VAL_NORMAL,	ADEND_BAD,	/*  */
    0,  VAL_NORMAL,	ADEND_BAD,	/*  */
    4,  VAL_NORMAL,	ADEND_BAD,	/* R_MIPS_SHIFT5 */
    4,  VAL_NORMAL,	ADEND_BAD,	/* R_MIPS_SHIFT6 */
    8,  VAL_NORMAL,	ADEND_XWORD,	/* R_MIPS_64 */
    4,  VAL_BAD,	ADEND_BAD,	/* R_MIPS_GOT_DISP */
    4,  VAL_BAD,	ADEND_BAD,	/* R_MIPS_GOT_PAGE */
    4,  VAL_NORMAL,	ADEND_BAD,	/* R_MIPS_GOT_OFST */
    4,  VAL_BAD,	ADEND_BAD,	/* R_MIPS_GOT_HI16 */
    4,  VAL_BAD,	ADEND_BAD,	/* R_MIPS_GOT_LO16 */
    8,  VAL_NORMAL,	ADEND_XWORD,	/* R_MIPS_SUB */
    4,  VAL_NORMAL,	ADEND_BAD,	/* R_MIPS_INSERT_A */
    4,  VAL_NORMAL,	ADEND_BAD,	/* R_MIPS_INSERT_B */
    4,  VAL_NORMAL,	ADEND_BAD,	/* R_MIPS_DELETE */
    4,  VAL_NORMAL,	ADEND_ADDR,	/* R_MIPS_HIGHER */
    4,  VAL_NORMAL,	ADEND_ADDR,	/* R_MIPS_HIGHEST */
    4,  VAL_BAD,	ADEND_BAD,	/* R_MIPS_CALL_HI16 */
    4,  VAL_BAD, 	ADEND_BAD,	/* R_MIPS_CALL_LO16 */
    4,	VAL_SCNDSP,	ADEND_WORD, 	/* R_MIPS_SCN_DISP */
    0,  VAL_NORMAL,	ADEND_BAD,   	/* should NOT have gotten here, check sys/elf.h */
};

#define OVERFLOW_16(_x) \
    (((__int32_t)(_x) < (__int32_t)0xffff8000) || ((__int32_t)(_x) > (__int32_t)0x00007fff))

#pragma pack(1)
struct unalign_word {
    __uint32_t w;
};
struct unalign_half {
    short h;
};
struct unalign_longlong {
    __uint64_t x;
};
#pragma pack(0)

/*
 * fixup_val
 *
 * This structure contains all of the info needed to handle
 * relocation.  One of these is declared on the stack of 
 * doelfrelocs(), the address of which is passed around to
 * the various subordinate functions.
 */
struct fixup_val {
	int addend, last_addend;
	ADDR fixup_addr;
	long raw;
	short half;
	__uint32_t word;
	__uint64_t xword;
};

/* relocation prototypes */
static int extract_relc_val(long, ADDR, struct fixup_val *);
static int rela_get_addend(int, Elf_Rela *, struct fixup_val *);
static int rel_get_addend(int, int, int, Elf_Rel *, struct fixup_val *);
static int put_relc_val(long, ADDR, struct fixup_val *);
static int get_target_value(ml_info_t *, int, int, ADDR *, elf_obj_info_t *);
static ADDR scn_vaddr_disp (int, elf_obj_info_t *);
static int fixup(ADDR, int, int, struct fixup_val *);

/* 
 * dorelfrelocs - 
 */
int
doelfrelocs(ml_info_t *minfo)
{
	int error = 0;
	int goterror = 0;
	int sec, keyidx, symidx, scn, type;
	int rel, nrel = 0;
	int is_rela;
	ADDR value;
	const RELC_ATTR *rlc_attr;
	Elf_Shdr *shdr;
	Elf_Rel *prlc, *reloc;
	Elf_Rela *prlca, *reloca;
	elf_obj_info_t *obj = (elf_obj_info_t *)minfo->ml_obj;
	struct fixup_val fval;
#ifdef	MVERBOSE
	ADDR vaddr;
#endif

#ifdef	MTRACE
	printf ("doelfrelocs\n");
#endif

	bzero(&fval, sizeof (struct fixup_val));

	for (sec=0; sec<SEC_LAST_ENUM; sec++) {
		switch (sec) {
		case SEC_TEXTREL:
			if (obj->key_sections[SEC_TEXTREL]) {
				reloc = obj->text_reloctab;
				reloca = (Elf_Rela *)reloc;
				scn = obj->text_scn;
				is_rela = 0;
			}
			break;
		case SEC_TEXTRELA:
			if (obj->key_sections[SEC_TEXTRELA]) {
				reloca = obj->text_relocatab;
				reloc = (Elf_Rel *)reloca;
				scn = obj->text_scn;
				is_rela = 1;
			}
			break;
		case SEC_DATAREL:
			if (obj->key_sections[SEC_DATAREL]) {
				reloc = obj->data_reloctab;
				reloca = (Elf_Rela *)reloc;
				scn = obj->data_scn;
				is_rela = 0;
			}
			break;
		case SEC_DATARELA:
			if (obj->key_sections[SEC_DATARELA]) {
				reloca = obj->data_relocatab;
				reloc = (Elf_Rel *)reloca;
				scn = obj->data_scn;
				is_rela = 1;
			}
			break;
		case SEC_RODATAREL:
			if (obj->key_sections[SEC_RODATAREL]) {
				reloc = obj->rodata_reloctab;
				reloca = (Elf_Rela *)reloc;
				scn = obj->rodata_scn;
				is_rela = 0;
			}
			break;
		case SEC_RODATARELA:
			if (obj->key_sections[SEC_RODATARELA]) {
				reloca = obj->rodata_relocatab;
				reloc = (Elf_Rel *)reloca;
				scn = obj->rodata_scn;
				is_rela = 1;
			}
			break;
		default:
			continue;
		};

#ifdef MVERBOSE
		vaddr = (ADDR) obj->scn_dataptrs[scn];
#endif
		keyidx = obj->key_sections[sec];
		shdr = &obj->shdr[keyidx];
		if (is_rela)
			nrel = shdr->sh_size / sizeof (Elf_Rela);
		else
			nrel = shdr->sh_size / sizeof (Elf_Rel);

#ifdef	MTRACE
		printf ("relocs for %s\n", obj->shstringtab + shdr->sh_name);
#endif

		for (rel=0; rel < nrel; rel++) {
			Elf_Sym *sym;
			register int is_composite;

			if (is_rela) {
				prlca = &reloca[rel];
				prlc = (Elf_Rel *)prlca;
				is_composite = (rel + 1 < nrel &&
					(((Elf_Rela *)(&reloca[rel+1]))->r_offset == prlca->r_offset)); 
			} else {
				prlc = &reloc[rel];
				prlca = (Elf_Rela *)prlc;
				is_composite = (rel + 1 < nrel &&
					(((Elf_Rel *)(&reloc[rel+1]))->r_offset == prlc->r_offset)); 

			}

#if _MIPS_SIM == _ABI64
			symidx = prlc->r_sym;
#else
			symidx = ELF_R_SYM(prlc->r_info);
#endif
			sym = &obj->symtab[symidx];
			fval.addend = fval.last_addend;
#if _MIPS_SIM == _ABI64
			type = prlc->r_type;
#else
			type = ELF_R_TYPE(prlc->r_info);
#endif
			rlc_attr = &relc_attr[type];
			fval.raw = (ADDR)obj->scn_dataptrs[scn];

#ifdef	MVERBOSE
			mvprintf (("%d: offset=0x%x addr=0x%x symbol idx=%d, type=%d", 
				rel, prlc->r_offset, 
				prlc->r_offset + vaddr, symidx, type));
			mvprintf ((" %s ", obj->stringtab + sym->st_name));
			mvprintf (("bind=%d type=%d\n", 
				ELF_ST_BIND(sym->st_info),
				ELF_ST_TYPE(sym->st_info)));
#endif	/* MVERBOSE */
	
			/* get raw data to be fixed up */
			if (extract_relc_val(rlc_attr->val_size,
					     prlc->r_offset, &fval))
				return ENOEXEC;

			/* 
			 * find addend, i.e. constant portion of "sym+const" 
			 * in relocatable object
			 */
			if (is_rela)
				rela_get_addend(rlc_attr->addend_type,
					prlca, &fval);
			else
				rel_get_addend(rlc_attr->addend_type, rel,
					nrel, reloc, &fval);

			fval.last_addend = 0;

			/* get final value of sym, as in sym+const */
			if (error = get_target_value(minfo, symidx,
						     rlc_attr->val_type,
						     &value, obj)) {
				if (goterror == 0) {
					goterror = error;
					goto done;
				}
			}

			/* the real thing */
			fval.fixup_addr =
				(ADDR)obj->scn_dataptrs[sym->st_shndx]
					+ prlc->r_offset;

#ifdef _64BIT_OBJECTS
			error = fixup(value, type, prlc->r_type2, &fval);
			if (!error && prlc->r_type2) {
			    switch (prlc->r_ssym) {
			    case RSS_UNDEF:
				value = 0;
				break;
			    case RSS_GP:
			    case RSS_GP0:
				mprintf (("ELF relocation failure - can't handle GP\n"));
				return MERR_GP;
			    case RSS_LOC:
				value = prlc->r_offset;
				break;
			    default:
				mprintf (("ELF relocation failure - unknown r_ssym type\n"));
				return error;
			    }
			    error = fixup(value, prlc->r_type2,
					  prlc->r_type3, &fval);

			    if (!error && prlc->r_type3) {
				error = fixup(0, prlc->r_type3, is_composite,
					      &fval);
			    }
			}
#else
			error = fixup(value, type, is_composite, &fval);
#endif /* _64BIT_OBJECTS */
			if (error)
				return error;

			if (is_composite) {
				fval.last_addend = fval.addend;
				continue;
			} else
				fval.last_addend = 0;

			/* put the final bit back into raw pool */	
			put_relc_val(rlc_attr->val_size,
				     prlc->r_offset, &fval);
		}
	}

done:
	if (goterror)
		return (goterror);

	/* Writeback Dcache, invalidate Icache */
	cache_operation(minfo->ml_text, 
		obj->shdr[obj->key_sections[SEC_TEXT]].sh_size, 
		CACH_ICACHE_COHERENCY);

	return error;
}

static Elf_Rel *
find_lo16 (int hiidx, int numrlc, Elf_Rel *prlc)
{
	register int i, hisymidx;

#if _MIPS_SIM == _ABI64
	hisymidx = ((Elf_Rel *)(&prlc[hiidx]))->r_sym;
#else
	hisymidx = ELF_R_SYM(((Elf_Rel *)(&prlc[hiidx]))->r_info);
#endif

	/*
	 * Starting with the next relocation record, search thru and
	 * look for the next record that is R_MIPS_LO16 and has the same
	 * sym index as the R_MIPS_HI16 record.
	 */
	
	for (i = hiidx+1; i < numrlc; i++) {
#if _MIPS_SIM == _ABI64
		if ( ((Elf_Rel *)(&prlc[i]))->r_type == R_MIPS_LO16 &&
			hisymidx == ((Elf_Rel *)(&prlc[i]))->r_sym) {
#else
		if ( ELF_R_TYPE(((Elf_Rel *)(&prlc[i]))->r_info) == R_MIPS_LO16 &&
			hisymidx == ELF_R_SYM(((Elf_Rel *)(&prlc[i]))->r_info)) {
#endif
			return &prlc[i];
		}
	}
    return (Elf_Rel *)0;
} /* find_lo16 */

static int
put_relc_val(long size, ADDR offset, struct fixup_val *fval)
{
	switch (size) {
	case 4:
		/*SWAP32WORD(word);*/
		bcopy((char *)&fval->word, (char *)fval->raw+offset, size);
		break;
	case 8:
		/*SWAP64WORD(xword);*/
		bcopy((char *)&fval->xword, (char *)fval->raw+offset, size);
		break;
	case 0:
		break;
	case 2:
		/*SWAPHALF(half);*/
		bcopy((char *)&fval->half, (char *)fval->raw+offset, size);
		break;
	case -1:
	default:
		cmn_err_tag (1799, CE_WARN,
	"val_rel_size table inconsistent with number of relocation entries"); 
		return (-1);
	}
	return 0;
}

/*
 * SIDE EFFECT: sets the set of statics: "half, word, lword"
 *	their values are used to compute the correct value
 *	for relocation. They should stay the same while working
 *	on the same relocation entry.
 */
static int
extract_relc_val(long size, ADDR offset, struct fixup_val *fval)
{
	switch (size) {
	case 4:
		bcopy((char *)fval->raw+offset, (char *)&fval->word, size);
		/*SWAP32WORD(word);*/
		break;
	case 8:
		bcopy((char *)fval->raw+offset, (char *)&fval->xword, size);
		/*SWAP64WORD(xword);*/
		break;
	case 0:
		break;
	case 2:
		bcopy((char *)fval->raw+offset, (char *)&fval->half, size);
		/*SWAPHALF(half);*/
		break;
	case -1:
	default:
#ifdef	DEBUG
		cmn_err_tag (1800, CE_WARN, "val_rel_size table inconsistent with number of relocation entries");
#endif	/* DEBUG */
		return -1;
	}
	return 0;
}

static ADDR
scn_vaddr_disp (int scnidx, elf_obj_info_t *obj)
{
	/* SARAH - do some error checking on scnidx */
	return ((ADDR)obj->scn_dataptrs[scnidx]);
}

static ADDR
get_scn_addr (int scnidx, elf_obj_info_t *obj)
{
	/* SARAH - do some error checking on scnidx */
	return ((ADDR)obj->scn_dataptrs[scnidx]);
}

/* 
 * SIDE EFFECT: 
 */
static int
get_target_value(ml_info_t *minfo, int symidx, int val_type,
	ADDR *value, elf_obj_info_t *obj)
{
	register Elf_Sym *psym = &obj->symtab[symidx];
	int rc = 0;

	if (psym->st_shndx != SHN_UNDEF) {
		*value = scn_vaddr_disp (psym->st_shndx, obj);
		if (ELF_ST_TYPE(psym->st_info) == STT_OBJECT ||
			ELF_ST_TYPE(psym->st_info) == STT_FUNC)
			*value += psym->st_value;
	} else {
		char *str = obj->stringtab + psym->st_name;

		/* check symtab modules first */
		if ((rc = st_findaddr(str, (__psunsigned_t **)value)) != 0) {
			/* nothing...try lib modules */
			if (melf_findaddr(str, (__psunsigned_t **)value,
					  M_LIB, &mlinfosem) != 0) {
				cmn_err_tag(1801, CE_WARN, "%s: symbol %s not found",
					minfo->ml_desc->m_fname, str);
				/* only reset rc if st_findaddr returned MERR_FINDADDR */
				if (MERR_FINDADDR == rc)
					rc = MERR_BADLINK;
				return rc;	/* nothing doing */
			}
		}
	}

	switch (val_type) {
	case VAL_NORMAL:
		break;

	case VAL_SCNDSP:
		*value -= get_scn_addr (psym->st_shndx, obj);
		break;

	default:
		return MERR_BADRTYPE;
	}
	return 0;
}


/*
 * SIDE EFFECT: sets the static: addend
 *     It value us used to calculate the 
 *     correct value for relocation. They should remain the same
 *     while working on the same relocation entry.
 */
static int
rela_get_addend (int type, Elf_Rela *prlc, struct fixup_val *fval)
{
Elf64_Sxword r_addend = prlc->r_addend;
    
switch (type) {
	
	case ADEND_NONE:
		break;
	
	case ADEND_HI: 
	case ADEND_WORD:
	case ADEND_JAL:
	case ADEND_LO:
	case ADEND_HALF:
	case ADEND_XWORD:
	case ADEND_ADDR:
		fval->addend += r_addend;
		break;
	
	case ADEND_BAD:
		return (MERR_BADADTYPE);
	}

	return 0;
}


static int
rel_get_addend (int type, int idx, int nrel, Elf_Rel *reloc,
		struct fixup_val *fval)
{
	short addend_lo;
	Elf_Rel *rlc_lo;
    
	switch(type) {
	case ADEND_NONE:
		break;
	
	case ADEND_HALF:
		fval->addend += fval->half;
		break;
	
	case ADEND_WORD:
		fval->addend += fval->word;
		break;
	
	case ADEND_JAL:
		fval->addend += (fval->word & 0x3ffffff) << 2;
		break;

	case ADEND_HI: 
		{
		struct unalign_word lo_word;
	    
		/* calculate addend from lo portion */
		rlc_lo = find_lo16(idx, nrel, reloc);
		if (rlc_lo == (Elf_Rel *)0) 
			return (MERR_BADADTYPE);
	
	    
		lo_word = *(struct unalign_word *)
			((char *)fval->raw + rlc_lo->r_offset);
		/*SWAP32WORD(lo_word);*/
	    
		addend_lo = (short)lo_word.w;
	    
		fval->addend += (fval->word << 16) + addend_lo;
	    
		break;
		}
	
	case ADEND_LO:
		fval->addend += (short)fval->word;
		break;
	
	case ADEND_XWORD:
		fval->addend += fval->xword;
		break;
	
	case ADEND_ADDR:
		fval->addend += (short) (fval->word & 0xffff);
		break;

	case ADEND_BAD:
		return (MERR_BADADTYPE);
	}
#ifdef	MVERBOSE
	mvprintf (("rel_get_addend: addend=0x%x\n", fval->addend));
#endif	/* MVERBOSE */
	return 0;
}

/* 
 * jmp_out_of_range - jmp target must have high bits the same 
 * 	i.e. same 256 megabyte segment 
 */
static int
jmp_out_of_range(ADDR target, ADDR pc)
{

#ifdef _64BIT_OBJECTS
#define JAL_MASK (0xfffffffff0000000LL)
#else
#define JAL_MASK (0xf0000000)
#endif /* _64BIT_OBJECTS */

	if ((target & JAL_MASK) != ((pc+4) & JAL_MASK))
		return 1;

	/* check for hardward bug at end of a 256M segments.  See bug 169884 */
	pc &= 0xfffffff;
	if (pc >= 0xffffff0)
		return 1;
	return 0;
}

/*
 * fixup - fixup of raw data due to relocation type
 */
static
int fixup(ADDR value, int type, int no_update, struct fixup_val *fval)
{
	int ret = 0;

	switch (type) {
	case R_MIPS_NONE:
	case R_MIPS_REL32:
		break;
	
	case R_MIPS_32:
#ifdef	NOTYET
	case R_MIPS_SCN_DISP:
#endif	/* NOTYET */
		value = (__int32_t) (value + (__int32_t)fval->addend);
		if (!no_update)
		    fval->word = value;
		break;
	
	case R_MIPS_26:
		value += (__int32_t)fval->addend;
	
		if (!no_update) {
		    if (jmp_out_of_range(value, fval->fixup_addr)) {
			ret = MERR_JMP256;
			break;
		    }
	       
		    /* TODO: */
		    /*      1. for shared, need to skip gp-prolog */
	
		    fval->word = (fval->word & 0xfc000000) | 
			(0x3ffffff & (value >> 2));
		}
		break;
	
	case R_MIPS_HI16:
		value += (__int32_t)fval->addend;
		if (!no_update)
		    fval->word = (fval->word & 0xffff0000) |
			(((value + 0x8000) >> 16) & 0xffff);
		break;
	
	
	case R_MIPS_LO16:
		value += (short)fval->addend;
		if (!no_update)
		    fval->word = (fval->word & 0xffff0000) | (value & 0xffff);
		break;
	
	case R_MIPS_CALL16:
	case R_MIPS_GOT16:
	case R_MIPS_GOT_DISP:
	case R_MIPS_GOT_PAGE:
	case R_MIPS_GOT_OFST:
	case R_MIPS_CALL_LO16:
	case R_MIPS_GOT_LO16:
	case R_MIPS_LITERAL:
	case R_MIPS_GPREL16:
	case R_MIPS_GPREL32:
	case R_MIPS_CALL_HI16:
	case R_MIPS_GOT_HI16:
		ret = MERR_BADRTYPE;
		break;
	
	case R_MIPS_64:
		value = (Elf64_Xword)value + fval->addend;
		if (!no_update)
		    fval->xword = value;
		break;

	case R_MIPS_SUB:
		value = (Elf64_Xword)value - fval->addend;
		if (!no_update)
		    fval->xword = value;
		break;
		
	case R_MIPS_16:
		value = fval->half + value + (short)fval->addend;
		if (!no_update) {
		    fval->word = value;
		    if (OVERFLOW_16(value))
			ret = MERR_BADRTYPE;
		    fval->half = fval->word;
		}
		break;

#ifdef _64BIT_OBJECTS
	case R_MIPS_HIGHER:
		value += fval->addend;
		if (!no_update)
		    fval->word = (fval->word & 0xffff0000) |
			(((value + 0x80008000LL) >> 32) & 0xffff);
		break;

	case R_MIPS_HIGHEST:
		value += fval->addend;
		if (!no_update)
		    fval->word = (fval->word & 0xffff0000) |
			(((value + 0x800080008000LL) >> 48) & 0xffff);
		break;
#endif	/* _64BIT_OBJECTS */
	    
	case R_MIPS_PC16:
	default:
		ret = MERR_BADRTYPE;
	}

	fval->addend = value;

	return ret;
}

int
symcmp(const void *arg1, const void *arg2)
{
	register ml_sym_t *sym1 = (ml_sym_t *)arg1;
	register ml_sym_t *sym2 = (ml_sym_t *)arg2;

	if (sym1->m_addr < sym2->m_addr)
		return (-1);
	else if (sym1->m_addr == sym2->m_addr)
		return (0);
	else
		return (1);
}

#ifdef JUMP_WAR
/*
 * melf_scan_badjumps()
 *
 * This routine looks through the EOP boundaries of the text
 * section based at textbase.  It will suggest any needed realignment
 * via the return value.
 *
 * Note ADDR is a word-sized, ELF-dependent macro from mloadpvt.h.
 */
#define PGSZ		ctob(1)

extern int is_intdiv(inst_t);		/* these are from os/fault.c */
extern int is_jumpreg(inst_t);
extern int is_condbranch(inst_t);
extern int is_intload(inst_t);
extern int jumpinst_reg(inst_t);
extern int loadinst_reg(inst_t);
extern int branchinst_reg1(inst_t);
extern int branchinst_reg2(inst_t);
extern int is_branch(inst_t);		/* this is from os/trap.c */

static int
melf_scan_badjumps(ADDR text, int size)
{
	int done_flag, reg;
	int is_bad_jmp = 0, offset = 0;
	ADDR base;
	inst_t *inst1, *inst2, *inst3;

	if (text == 0 || size == 0)
		return 0;

	/*
	 * text holds the address of the text section in memory
	 * base holds the alignment address being screened (offset from text)
	 * next holds the address of consecutive page boundaries
	 * inst{1,2,3} holds the address of the instruction at EOP-{1,2,3}
	 */
	base = text;
	do {
		register ADDR next;

		done_flag = 1;
		next = ((base + PGSZ) & ~(PGSZ - 1)) - sizeof (inst_t);
		for ( ; base + size > next; next += PGSZ) {
			inst1 = (inst_t *)(next - base + text);
			inst2 = inst1 - 1;
			inst3 = inst2 - 1;

			if ((ADDR)inst2 < text || (ADDR)inst3 < text)
				return 0;

			is_bad_jmp = is_intdiv(*inst1);
			
			if (is_bad_jmp == 0 && is_jumpreg(*inst1)) {
				reg = jumpinst_reg(*inst1);
				if (((is_intload(*inst2) &&
				      loadinst_reg(*inst2) == reg)) ||
				    (is_intload(*inst3) &&
				     loadinst_reg(*inst3) == reg))
					is_bad_jmp = 1;
			}

			if (is_bad_jmp == 0 && is_condbranch(*inst1)) {
				if (is_intload(*inst2)) {
					reg = loadinst_reg(*inst2);
					if ((branchinst_reg1(*inst1) == reg) ||
					    (branchinst_reg2(*inst1) == reg))
						is_bad_jmp = 1;
				}
			}

			if (is_bad_jmp) {
				offset += sizeof (inst_t);
				base += sizeof (inst_t);

				/* if we can't find a safe spot, punt... */
				if (offset >= PGSZ)
					return (-1);

				done_flag = 0;
				break;
			}
		}
	} while (done_flag != 1);
 
	return (offset);
}
#endif /* JUMP_WAR */
