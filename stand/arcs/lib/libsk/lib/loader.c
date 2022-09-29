/*
 * loader.c
 *
 * ARCS load, invoke and execute functions. This code will load and
 * relocate an object file into the 4 MB of free space defined by
 * the ARCS spec. Files to be loaded and relocated must be loaded
 * on the UNIX host with the options: -G 0 -EL -r -d, so that the 
 * relocation information will be included. At this time, if relocation
 * records are not found within the object file, the object will be
 * loaded at the fixed address specified in the object file's a.out
 * header. This will allow loading of dprom, which depends on being
 * loaded at a specified address. An option to the boot command, -a,
 * has also been added to support loading without relocation. The
 * functions load_abs, invoke_abs and exec_abs have been added to the
 * private vector to support boot -a. For now they just call the ARCS
 * functions, but will need to be modified if the check for relocation
 * records is removed when position independent code is supported.
 *
 * WARNING - loading a file with -r causes 'undefined symbols' diagnostics
 * to be supressed. If a program is loaded and contains undefined symbols,
 * these will show up as a bad section error during the load. 
 *
 * Copyright 1991, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */


#ident "$Revision: 1.94 $"

/* Only include propper loader code -- 64 bit proms cannot really load
 * 32 bit programs (TFP for sure, and it is messy on the R4000), plus
 * we need to save the space.  32 bit proms need to be able to load
 * anything.
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

#if LOAD_COFF
#include <aouthdr.h>
#include <filehdr.h>
#include <scnhdr.h>
#include <sym.h>
#include <arcs/dload.h>
#endif

#include <elf.h>		/* always need this for ELFHDRSIZE */

#ifdef LIBSL
#include <arcs/spb.h>
#include <arcs/pvector.h>
#endif

#include <arcs/types.h>
#include <arcs/errno.h>
#include <arcs/restart.h>
#include <arcs/io.h>
#include <arcs/eiob.h>
#include <libsc.h>
#include <libsk.h>

/*
 * TODO:
 *
 * - Add md_alloc and md_dealloc as private vectors, so that they
 *	can be used by other programs - have requested that they go
 *	into spec.
 * - Invoke deallocs mem desc for now. ARCS should specify a
 *	separate invoke routine which saves state of mem desc list.
 * - Look into fd issues from ARCS pg. 89.
 * - Check all return values.
 */

/*#define DEBUG 1*/
#if IP26 && !DEBUG			/* some arch are short of prom space */
#define dprintf(x)
#else
#define dprintf(x) if (Debug) printf x 
void listlist(void);
#endif

/* number of bytes to read from a bootfile header to determine
 * if it is elf or not, and if so 32 or 64 bit
 */

#ifdef	ULOAD
/* If ULOAD is defined, then if the data space is located
 * at a user address, the loader will try to map the tlb.
 * This allows us to generate mod exceptions on writes to
 * the data space when running from a dprom, which don't
 * work so well when data is in the prom.  This code assumes
 * that data is on separate pages from both text and bss.
 * The IP12prom/Makefile for the uprom target creates 
 * appropriate addresses.
 */
#include <sys/sysmacros.h>
#include <sys/param.h>
#endif	/* ULOAD */

/* #define	DEBUG */

extern MEMORYDESCRIPTOR *mem_getblock(void);
extern MEMORYDESCRIPTOR *mem_contains(unsigned long, unsigned long);
extern void mem_list(void);
extern int _end[];
extern int _ftext[];

LONG swapendian;	/* for loading opposite endian files */
#if LOAD_COFF
static int fixed;	/* load at fixed address or relocate */
#endif

/* singly linked list for LoadedProgram memory descriptors */

typedef struct load_list {
	struct load_list *next;
	MEMORYDESCRIPTOR *m;
	ULONG lowaddr;
} ld_t;

int envdirty;		/* if true EnterInteractiveMode inits more */

static ld_t *lroot, *llist, *ldbg;

static char *argvtab[] = {
    "ConsoleIn",
    "ConsoleOut",
    "SystemPartition",
    "OSLoader",
    "OSLoadPartition",
    "OSLoadFilename",
    "OSLoadOptions",
    0
};

#define STYPof(x) ((x) & ~S_NRELOC_OVFL)

#if LOAD_COFF
static LONG load_noreloc(CHAR *, struct execinfo *, ULONG, ULONG *);
static LONG load_reloc(CHAR *, struct execinfo *, int, ULONG *, ULONG);
#endif

static LONG loadit(CHAR *, ULONG *, struct string_list *, ULONG);
static LONG executeit(ULONG, ULONG, struct string_list *, struct string_list *);
static ld_t *list_init(long, long);
static LONG list_free(ld_t *);
static LONG dbg_loaded;

/*
 * load_init - initialize top of free memory pointer.
 */

void
load_init(void)
{
	lroot = llist = 0;
	dbg_loaded = 0;
	ldbg = 0;
}

/* 
 * ARCS Load Function - load an object file and relocate it
 *	to the specified address.
 */

LONG
Load(CHAR *Path, ULONG TopAddr, ULONG *ExecAddr, ULONG *LowAddr)
{
	static struct string_list argv;
	ULONG pc;
	char *cp;
	int err;
	extern int _prom;

#ifdef DEBUG
	printf("Load: Path=%s, TopAddr=0x%x\n", Path, TopAddr);
#endif /*DEBUG*/

#if LIBSL
	if(sgivers() >= 3) {
		return __TV->Load(Path, TopAddr, ExecAddr, LowAddr);
	}
#endif

	if(_argvize(expand(Path, 0), &argv) == 0)
		return (EINVAL);

	/*
	 * If a .dbg file is loaded, then symmon will probably
	 * get loaded as well. Save the infomation about the
	 * .dbg file so that the memory descriptors can be
	 * freed. This is necessary because symmon doesn't
	 * actually return here after a continue, but instead
	 * jumps back to the .dbg program. When the .dbg program
	 * returns, it must free both memory descriptors.
	 */

	if ((cp = rindex(Path,'.')) && !strcasecmp(cp, ".dbg"))
		dbg_loaded = 1;

	/*  Ok, it looks like we are going to load something.  Set
	 * the envdirty to 1, which will cause EnterInteractiveMode()
	 * to do atleast modest init.
	 */
	envdirty = 1;

	if (err = loadit(Path, &pc, &argv, TopAddr))
		return(err);

	*ExecAddr = pc;
	*LowAddr = llist->lowaddr;
	return(ESUCCESS);
}

/* 
 * SGI Private Load Absolute Function - load an object file 
 * 	without relocation.
 */

LONG
load_abs(CHAR *Path, ULONG *ExecAddr)
{
	ULONG lowaddr;
	MEMORYDESCRIPTOR *m;

	/* 
	 * Figure out what TopAddr should be using descriptor list.
	 * Assume our default load is to the top of the largest
	 * block of remaining free memory
	 */
	if (!(m = mem_getblock())) {
		load_error (Path,"No free memory descriptors available.");
#ifdef	DEBUG
		if (Debug) mem_list();
#endif	/* DEBUG */
		return (ENOMEM);
	}

#if LOAD_COFF
	fixed = 1;
#endif
	return(Load(Path, PHYS_TO_K0(arcs_ptob(m->BasePage + m->PageCount)), 
		    ExecAddr, &lowaddr));
}

/*
 * ARCS Invoke Function - invoke a function loaded by ARCS Load.
 */

LONG
Invoke(ULONG ExecAddr, ULONG StackAddr, LONG Argc, CHAR *Argv[], CHAR *Envp[])
{
	struct string_list exec_environ;
	struct string_list exec_argv;	
	register char **wp, *cp;
	int err;
	ld_t *l;

#ifdef	DEBUG
	printf ("Invoke: ExecAddr=0x%x, StackAddr=0x%x\n", ExecAddr, StackAddr);
#endif	/*DEBUG*/

#if LIBSL
	if(sgivers() >= 3) {
		return __TV->Invoke(ExecAddr, StackAddr, Argc, Argv, Envp);
	}
#endif
	if (!ExecAddr) {
		printf("Cannot invoke addr 0.\n");
		return(EINVAL);
	}

	/* Find the mem node for this address. */
	for (l = lroot; l; l = l->next)
		if (kdmtolocalphys((void *)ExecAddr) >= arcs_ptob(l->m->BasePage) && 
		    kdmtolocalphys((void *)ExecAddr) <= 
		    arcs_ptob(l->m->BasePage + l->m->PageCount))
			break;

	if (!l) {
		printf("Cannot invoke 0x%x: no memory descriptor found.\n",
		       ExecAddr);
#ifdef	DEBUG
		if (Debug) mem_list();
#endif
		return(EINVAL);
	}

	/* Add args; argv[0] already contains path */
	init_str(&exec_argv);
	if (Argc) {
		for (wp = Argv; wp && *wp; wp++)
			if (new_str1(*wp, &exec_argv))
				return(ENOMEM); 
	}

	for (wp = argvtab; *wp; wp++) {
	    if (!find_str(*wp, &exec_argv)&&(cp = getenv((char *)*wp)))
		if (new_str2(*wp, cp, &exec_argv))
		    return(ENOMEM);
	}

	/* Init environment */
	init_str(&exec_environ);
	if (Envp) {
		for (wp = Envp; wp && *wp; wp++)
			if (new_str1(*wp, &exec_environ))
				return(ENOMEM);
	} else { 
		for (wp = environ; wp && *wp; wp++)
			if (new_str1(*wp, &exec_environ))
				return(ENOMEM);
	}
	if (err = executeit(ExecAddr, StackAddr, &exec_argv, &exec_environ))
		return (err);

	/* NEEDS WORK: dealloc mem desc for now. Should have separate
		routine to call for exit and save mem desc. */
	if (list_free (l))
		return (ENOMEM);

	/* See notes in Load() */
	if (ldbg) {
		if (list_free(ldbg)) 
			return (ENOMEM);
		ldbg = 0;
	}
	return (ESUCCESS);
}


/*
 * SGI Private Invoke Absolute Function - Invoke function loaded 
 *	by SGI loadabs function.
 */

LONG
invoke_abs(ULONG ExecAddr, ULONG StackAddr, LONG Argc, CHAR *Argv[],
	   CHAR *Envp[])
{
	return(Invoke(ExecAddr, StackAddr, Argc, Argv, Envp));
}

/*
 * ARCS Execute Function - Load, relocate and execute object file.
 */

LONG
Execute (CHAR *Path, LONG Argc, CHAR *Argv[], CHAR *Envp[])
{
	ULONG TopAddr, ExecAddr, LowAddr;
	MEMORYDESCRIPTOR *m;
	int err;

#ifdef	DEBUG	
	printf("Execute: path=%s\n", Path);
#endif

#if LIBSL
	if(sgivers() >= 3) {
		return __TV->Execute(Path, Argc, Argv, Envp);
	}
#endif

	/* 
	 * Figure out what TopAddr should be using descriptor list.
	 * Assume our default load is to the top of the largest
	 * block of remaining free memory.
	 */
	if (!(m = mem_getblock())) {
		printf ("Cannot execute %s.\n"
			"No free memory descriptors available.\n", Path);
#ifdef	DEBUG
		if (Debug) mem_list();
#endif	/* DEBUG */
		return(ENOMEM);
	}

	TopAddr = arcs_ptob(m->BasePage + m->PageCount);

#ifdef	DEBUG
	printf ("Execute: TopAddr=0x%x\n", TopAddr);
#endif	/*DEBUG*/

	if (err = Load(Path, TopAddr, &ExecAddr, &LowAddr)) {
#if !ELF_2_COFF
	    if(err == ENOENT)
		{
#endif	/* ELF_2_COFF */
		dprintf(("Load err %d\n", err));
#if !ELF_2_COFF
		}
	    else	/* we found it, so say why we couldn't execute it */
		    printf("Unable to execute %s:  %s\n", Path,arcs_strerror(err));
#endif	/* ELF_2_COFF */
	    return(err);
	}

	/* TODO: TEMPORARY check - do something real, soon */
	if (swapendian) {
		printf("WARNING: file loaded but not executed: "
		       "opposite endian\n");
		return(ESUCCESS);
	}

	if (err = Invoke (ExecAddr, 0, Argc, Argv, Envp)) {
	    dprintf(("Invoke err %d\n", err));
	    return(err);
	}

	return(ESUCCESS);
}

/*
 * SGI Private Execute Absolute Function - Load and execute 
 *	object file without relocation.
 */

LONG
exec_abs(CHAR *Path, LONG Argc, CHAR *Argv[], CHAR *Envp[])
{
#if LOAD_COFF
	fixed = 1;
#endif
	return(Execute(Path, Argc, Argv, Envp));
}


int
elf_list_init(ULONG lowaddr, ULONG hiaddr)
{
	if (hiaddr) {
		if (list_init(lowaddr & ~(ARCS_NBPP-1),
			hiaddr - (lowaddr & ~(ARCS_NBPP-1))) == 0)
				return (ENOMEM);
	}
	return(0);
}

/* Data for loadbin
 */
union commonhdr {
#if LOAD_COFF
	struct execinfo execinfo;
#endif
#if LOAD_ELF32
	Elf32_Ehdr elfhdr32;
#endif
#if LOAD_ELF64
	Elf64_Ehdr elfhdr64;
#endif
};

/*
 * loadbin - Load binary image. If there are relocation records
 * in the object file, relocate it. Otherwise, load at the fixed
 * address in the a.out header.
 */

/* ARGSUSED3 (used for coff only) also used by elf2coff */
LONG
loadbin(CHAR *filename, ULONG *pc, ULONG top_addr)
{
	union commonhdr *commonhdr;
	ULONG fd, cnt;
	int tries = 0;
	LONG err;
#if LOAD_COFF
	register size_t hdrsize;
	int totalrelocs, scn;
	static struct execinfo eee;
	struct execinfo *ei = &eee;
#endif

	swapendian = 0;
	dprintf(("enter loadbin %s\n", filename));

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

	commonhdr = dmabuf_malloc(sizeof(union commonhdr));
	if (commonhdr == 0) {
		Close(fd);
		return(ENOMEM);
	}

	/* read the first ELFHDRBYTES bytes of the file to determine if the
	 * file is ELF or COFF, and if elf, whether 32 or 64 bit
	 */

	if ((err = Read(fd, commonhdr, ELFHDRBYTES, &cnt)) ||
	    (cnt != ELFHDRBYTES)) {
		load_error(filename,"Problem reading file magic id, err %d"
			   " cnt %d.", err, cnt);
		err = err ? err : ENOEXEC;
		goto loaderr;
	}

#if LOAD_ELF32 || LOAD_ELF64
#if LOAD_ELF32
	if (IS_ELF(commonhdr->elfhdr32)) {
#elif LOAD_ELF64
	if (IS_ELF(commonhdr->elfhdr64)) {
#endif
		int class;

		/* 32 bit or 64 bit?
		 */
#if LOAD_ELF32
		class = commonhdr->elfhdr32.e_ident[EI_CLASS];
#else
		class = commonhdr->elfhdr64.e_ident[EI_CLASS];
#endif

#if LOAD_ELF32
		if (class == ELFCLASS32) {
			err = loadelf32(filename,fd,pc,&(commonhdr->elfhdr32),
					top_addr);
			dmabuf_free(commonhdr);
			return(err);
		}
#endif

#if LOAD_ELF64
		if (class == ELFCLASS64) {
			err = loadelf64(filename,fd,pc,&(commonhdr->elfhdr64),
					top_addr);
			dmabuf_free(commonhdr);
			return(err);
		}
#endif
		/* give a more useful message if we only support one class, 
		 * and it's the other */
#if !LOAD_ELF32
		if (class == ELFCLASS32)
			load_error(filename, "Can't boot 32 bit ELF");
#endif
#if !LOAD_ELF64
		if (class == ELFCLASS64)
			load_error(filename, "Can't boot 64 bit ELF");
#endif
		else
			load_error(filename, "unknown ELF class %d.", class);
		dmabuf_free(commonhdr);
		Close(fd);
		return(ENOEXEC);
	}
#endif

#if LOAD_COFF
	/* load the rest of the COFF header */
	/* 
	 * Read the file header and the aouthdr and figure out how 
	 * many sections.
	 */

	hdrsize = sizeof(struct filehdr) + sizeof(struct aouthdr);
	if ((err = Read(fd, (char*)commonhdr + ELFHDRBYTES,
			hdrsize - ELFHDRBYTES, &cnt)) ||
	    (cnt != hdrsize - ELFHDRBYTES)) {
	    load_error(filename,"Problem reading coff header, err %d"
			" cnt %d.", err, cnt);
	    err = err ? err : ENOEXEC;
	    goto loaderr;
	}

	ei = &(commonhdr->execinfo);
	ei->sh = 0;		/* just in case we just got garbage */

	/* Determine whether file is of same or opposite endianness */

#ifdef	MIPSEL
	if (!IS_MIPSELMAGIC(ei->fh.f_magic)) {
		if (!IS_MIPSEBMAGIC(nuxi_s(ei->fh.f_magic))) {
			load_error(filename,
				"Illegal f_magic number 0x%x, expected MIPSELMAGIC or MIPSEBMAGIC.",
				ei->fh.f_magic);
			err = ENOEXEC;
			goto loaderr;
		} else 
			swapendian = 1;	
	}
#else	/* MIPSEL */
	if (!IS_MIPSEBMAGIC(ei->fh.f_magic)) {
		if (!IS_MIPSELMAGIC(nuxi_s(ei->fh.f_magic))) {
			load_error(filename,"Illegal f_magic number 0x%x, expected MIPSELMAGIC or MIPSEBMAGIC.",
				ei->fh.f_magic);
			err = ENOEXEC;
			goto loaderr;
		} else
			swapendian = 1;	
	}
#endif	/* MIPSEL */

	/* Swap header entries for loading opposite endian files. */

	if (swapendian) {
		ei->fh.f_nscns = nuxi_s(ei->fh.f_nscns);
		ei->fh.f_symptr = nuxi_l (ei->fh.f_symptr);
		ei->ah.magic = nuxi_s (ei->ah.magic);
		ei->ah.text_start = nuxi_l (ei->ah.text_start);
		ei->ah.data_start = nuxi_l (ei->ah.data_start);
		ei->ah.bss_start = nuxi_l (ei->ah.bss_start);
		ei->ah.entry = nuxi_l (ei->ah.entry);
		ei->ah.tsize = nuxi_l (ei->ah.tsize);
		ei->ah.dsize = nuxi_l (ei->ah.dsize);
		ei->ah.bsize = nuxi_l (ei->ah.bsize);
		ei->ah.entry = nuxi_l (ei->ah.entry);
	}

	/* Read the section headers */

	hdrsize = ei->fh.f_nscns * sizeof(struct scnhdr);
	ei->sh = (struct scnhdr *) dmabuf_malloc(hdrsize);

	if (ei->sh == 0) {
		err = ENOMEM;
		goto loaderr;
	}
  
	if (err = Read(fd, ei->sh, hdrsize, &cnt)) {
		load_error(filename,
			"Problem reading section headers, n sections=%d.", 
			ei->fh.f_nscns);
		goto loaderr;
	}
	if (cnt != hdrsize) {
		load_error(filename,
			"Incorrect section header size 0x%x, expected 0x%x.", 
			cnt, hdrsize);
		err = EIO;
		goto loaderr;
	}

	/* 
	 * Compute number of relocation records in sections. If there
	 * aren't any relocation records, assume that the file should
	 * be loaded without relocation.
	 */

	totalrelocs = 0;
	for (scn=0; (scn < ei->fh.f_nscns); scn++) {
		/* Swap header entries for loading opposite endian files. */
		if (swapendian) {
			ei->sh[scn].s_nreloc = nuxi_s (ei->sh[scn].s_nreloc);
			ei->sh[scn].s_scnptr = nuxi_l (ei->sh[scn].s_scnptr);
			ei->sh[scn].s_size = nuxi_l (ei->sh[scn].s_size);
			ei->sh[scn].s_flags = nuxi_l (ei->sh[scn].s_flags);
			ei->sh[scn].s_relptr = nuxi_l (ei->sh[scn].s_relptr);
			ei->sh[scn].s_vaddr = nuxi_l (ei->sh[scn].s_vaddr);
		}
		totalrelocs += ei->sh[scn].s_nreloc;
	}

	if (fixed || !totalrelocs) {
		ULONG laddr = KDM_TO_PHYS(ei->ah.text_start);

		/* 
		 * If the address doesn't fall within a FreeMemory descriptor,
		 * then don't allow loading.
		 */
		if (!mem_contains(laddr,ei->ah.tsize+ei->ah.dsize+ei->ah.bsize)){
			load_error(filename,
				"Text start 0x%x, size 0x%x doesn't fit in a FreeMemory area.\n",
				laddr, ei->ah.tsize+ei->ah.dsize+ei->ah.bsize);
#ifdef	DEBUG
			if (Debug) mem_list();
#endif
			err = ENOMEM;
			goto loaderr;
		}
		else {
			err = load_noreloc(filename, ei, fd, pc);
			dprintf(("load noreloc err %d\n", err));
		}
	}
	else {
		err = load_reloc(filename, ei, fd, pc, top_addr);
		dprintf(("load reloc err %d\n", err));
	}
#else
	Close(fd);
	return(ENOEXEC);
#endif

loaderr:
#if LOAD_COFF
	if (ei && ei->sh) dmabuf_free(ei->sh);
#endif
	dmabuf_free(commonhdr);
	Close(fd);
	dprintf(("loadbin: return %d\n", err));
	return(err);
}

#if LOAD_COFF
/*
 * load_noreloc - Load binary without relocating it.
 */

static LONG
load_noreloc(CHAR *filename, struct execinfo *ei, ULONG fd, ULONG *pc)
{
	static char *starterr = "Error reading %s section: "
				"start 0x%x, size 0x%x.";
	static char *counterr = "Error reading %s section: "
				"cnt=0x%x, expected 0x%x.";
	static char *rangeerr = "Range check failure: %s "
				"start 0x%x, size 0x%x.";
	static char *overwrit = "section would overwrite an already "
				"loaded program.";
	static char *text = "text";
	static char *data = "data";
	static char *bss  = "bss";
	ULONG cnt;
	LARGE off;
	LONG err;
#ifdef	ULOAD
	unsigned int tlblo_attrib;
#endif

	if (ei->ah.magic != OMAGIC) {
		load_error(filename, "Illegal a_magic number 0x%x, expected OMAGIC.",
				ei->ah.magic);
		return (ENOEXEC);
	}

	off.hi = 0;
	off.lo = ei->sh[0].s_scnptr;
	if (err = Seek (fd, &off, SeekAbsolute)) {
		load_error (filename,"Error seeking to text offset 0x%x.", off.lo);
		return (err);
	}

#ifdef EVEREST 
	/* We want to transfer the data into memory using coherent 
	 * I/O.  We therefore force all of the sections to start on
	 * cacheable addresses.
	 */ 	
	ei->ah.text_start &= 0x9fffffff;
	ei->ah.data_start &= 0x9fffffff;
	ei->ah.bss_start  &= 0x9fffffff;
#endif /* EVEREST */
	
	if ( Verbose )
		printf("%d", ei->ah.tsize);

	if (range_check(ei->ah.text_start, ei->ah.tsize)) {
		load_error(filename, rangeerr, text,
			ei->ah.text_start, ei->ah.tsize);
		printf("Text %s", overwrit);
		return(ENOMEM); 
	}

	if (err = Read(fd, (void *)ei->ah.text_start, ei->ah.tsize, &cnt)) {
		load_error(filename, starterr, text,
			ei->ah.text_start, ei->ah.tsize);
		return(err);
	}
	if (cnt != ei->ah.tsize) {
		load_error(filename, counterr, text,
			cnt, ei->ah.tsize);
		return(EIO);
	}
	if ( Verbose )
		printf("+%d", ei->ah.dsize);
#ifdef	ULOAD
	if (IS_KUSEG(ei->ah.data_start)) {
		/* try to map the data space of the file into the tlb
		 */
		unsigned page, lastpage;
		int idx;

		page = btoct(KDM_TO_PHYS(ei->ah.data_start));
		lastpage = btoc(KDM_TO_PHYS(ei->ah.data_start+ei->ah.dsize));
#if R4000 || R10000
		if (lastpage - page > (NTLBENTRIES * 2)) {
#else	/* TFP */
		<<<This code is not ported to TFP>>>
#endif	/* R4000 || R10000 */
			printf ("\nToo may pages to map to tlb.\n");
			return (EINVAL);
		}
#if R4000 || R10000
		tlblo_attrib = TLBLO_UNCACHED | TLBLO_V | TLBLO_G;
		for (idx = 0; page < lastpage; idx++, page += 2)
			tlbwired (idx, 0, vfntov(page),
			    (page << TLBLO_PFNSHIFT) | tlblo_attrib,
			    ((page + 1) << TLBLO_PFNSHIFT) | tlblo_attrib);
#else	/* TFP */
		<<<This code is not ported to TFP>>>
#endif	/* R4000 || R10000 */
	}
	ei->ah.data_start |= K1BASE;
#endif	/* ULOAD */

	if (range_check(ei->ah.data_start, ei->ah.dsize)) {
		load_error(filename, rangeerr,data,
			ei->ah.data_start, ei->ah.dsize);
		printf("Data %s", overwrit);
		return(ENOMEM); 
	}

	if (err = Read(fd, (void *)ei->ah.data_start, ei->ah.dsize, &cnt)) {
		load_error(filename, starterr, data,
			ei->ah.data_start, ei->ah.dsize);
		return(err);
	}
	if (cnt != ei->ah.dsize) {
		load_error(filename, counterr, data, cnt, ei->ah.dsize);
		return(EIO);
	}
	if ( Verbose )
		printf("+%d", ei->ah.bsize);

	if (range_check(ei->ah.bss_start, ei->ah.bsize)) {
		load_error(filename, rangeerr, bss,
			ei->ah.bss_start, ei->ah.bsize);
		printf("Bss %s", overwrit);
		return(ENOMEM);
	}

	bzero((void*)ei->ah.bss_start, ei->ah.bsize);
	if ( Verbose )
		printf(" entry: 0x%x\n", ei->ah.entry);
	*pc = ei->ah.entry;

#if !ELF_2_COFF
	FlushAllCaches();
#endif

	if (list_init (ei->ah.text_start & ~(ARCS_NBPP-1),
		ei->ah.bss_start + ei->ah.bsize - 
		(ei->ah.text_start & ~(ARCS_NBPP-1))) == 0)
		return(ENOMEM);
	return(0);
}

/* 
 * load_reloc - Load binary and relocate it.
 */
/*ARGSUSED*/
static LONG
load_reloc(CHAR *filename, struct execinfo *ei, int fd, ULONG *pc, ULONG addr)
{
	char *textstart, *datastart;
	LONG textsize, datasize;
	__psunsigned_t mem;
	ULONG entry;
	int status;
	ld_t *l;

	if (!ei->fh.f_symptr) {
		printf("Cannot load and relocate %s.\n", filename);
		printf("Relocation entries found, but symbol table missing.\n");
		printf("File has probably been stripped.\n");
		return(EIO);
	}

	/* 
	 * Compute text and data starting locations, from the
	 * 4MB of free space.
	 */

	textsize = ei->ah.tsize;
	datasize = ei->ah.dsize + ei->ah.bsize;

	/* 
	 * Calculate memory location for load. Load into kseg0
	 * as per ARCS rev 1.00, section 3.3.2.3.
	 */

	mem = PHYS_TO_K0(addr) - (textsize + datasize);
	if ((__psint_t)mem & 3)
		mem = ((__psint_t)mem - ((__psint_t)mem & 3));
	textstart = (char *) mem;
	datastart = (char *) (mem + textsize);

	if ((l = list_init (mem & ~(ARCS_NBPP-1),
	    (long)(datastart + datasize - (mem & ~(ARCS_NBPP-1))))) == 0)
		return(ENOMEM);


	/* Call dynamic load function to relocate code. */
	entry = (ULONG) dload(fd, textstart, datastart, ei, &status);

	if (entry) {
		*pc = entry;
#ifdef	DEBUG
		printf("test1:dload successful, entry=0x%x, teststart=0x%x, datastart=0x%x\n",
		entry, textstart, datastart);
#endif	/*DEBUG*/
	}
	else {
		list_free(l);
		return(EINVAL);
	}
	if (Verbose)
		printf(" entry: 0x%x\n", entry);
	return(0);
}
#endif

/*
 * loadit - Check the path and load the file if we can.
 */

static LONG
loadit(CHAR *Path, ULONG *pc, struct string_list *exec_argv, ULONG top_addr)
{
	static struct string_list path_list;
	int pathcnt, i, err;
	static char path[256];
	char *cp;

#ifdef SN0
	if (index(Path, '/') || index(Path, '('))
#else
	if (index(Path, '('))
#endif
	{
		/*
		 * file had a device specified, so just try booting it
		 */
		if (err = loadbin(Path, pc, top_addr)) {
		    dprintf(("loadit: loadbin1 failed %d\n", err));
		    return(err);
		}
	}
	else {
		/*
		 * no device specified so try searching path
		 */
		if ((cp = (char *)makepath()) == NULL) {
			load_error(Path,
				"No default device and path in environment.");
			return(EINVAL);
		}
		pathcnt = _argvize(cp, &path_list);
		for (i = 0; i < pathcnt; i++) {
			strncpy(path, path_list.strptrs[i], sizeof(path));
			strncat(path, Path, sizeof(path));

			err = loadbin(path, pc, top_addr);
			if (err == ESUCCESS)
				break;
			if (err == ENOMEM || err == ENOEXEC)
				return(err);
		}
		if (i >= pathcnt) {
			dprintf(("loadit: path not found\n"));
			return(ENOENT);
		}
		/*
		 * replace argv[0] with full pathname, so booted program
		 * can figure out device it was booted off of
		 */

		/* 
		 * NEEDS WORK - this won't really work, since path is 
		 * 	from the stack. 
		 */

		set_str(path, 0, exec_argv);
	}

#if LOAD_COFF
	fixed = 0;
#endif
	return (ESUCCESS);
}

/*
 * executeit - Execute the loaded binary.
 */

static LONG
executeit(ULONG pc, ULONG sp, struct string_list *exec_argv, struct string_list *exec_environ)
{
#ifdef	DEBUG
	printf("executeit: pc=0x%x, sp=0x%x\n", pc, sp);
#endif

	_scandevs();		/* last chance for interrupt */

	rbsetbs(BS_RFINISHED);

	/* If sp is 0, client_start will start the new program with
	 * the current sp.  client_start returns the return value of
	 * the program that it called, but currently that status is
	 * never examined.
	 */
	client_start(exec_argv->strcnt,
		    exec_argv->strptrs, exec_environ->strptrs,
		    pc, sp);

	/*  
	 * Close all but stdin + stdout.  The client may actually change
	 * these on us, if they are sneaky.  This might have to be addressed
	 * eventually.
	 */
	close_noncons();

	return (ESUCCESS);
}

/*
 * list_init - create a load list node for the loaded program.
 *
 * returns ptr to ld_t OK, 0 on error.
 */
static ld_t *
list_init(long lowaddr, long size)
{
	char *me = "Cannot create load list node, out of memory.\n";
	ld_t *l, *ltmp;

#ifdef	DEBUG
	printf ("list_init: lowaddr=0x%x, size=0x%x\n", lowaddr, size);
#endif	/*DEBUG*/

	/* Go to end of list */
	l = lroot;
	if (l)
		while (l->next)
			l = l->next;

	/* 
	 * Malloc a LoadedProgram list node for multiple Load/Invoke.
	 */
	if (l) {
		if ((l->next = malloc(sizeof(ld_t))) == 0) {
			printf(me);
			return(0);
		}
		ltmp = l;
		l = l->next;
	} else {
		if ((lroot = malloc(sizeof(ld_t))) == 0) {
			printf(me);
			return(0);
		}
		ltmp = l;
		l = lroot;
	}
	if ((l->m = md_alloc((unsigned long)KDM_TO_PHYS(lowaddr), (unsigned long)arcs_btop (size), LoadedProgram)) == 0) {
#ifdef	DEBUG
		if (Debug) mem_list();
#endif	/* DEBUG */
		if (ltmp) {
			free (ltmp->next);
			ltmp->next = 0;
		}
		return(0);
	}
	l->lowaddr = lowaddr;
	l->next = 0;
#ifdef	DEBUG
	listlist();
#endif
	llist = l;
	if (dbg_loaded) {
		dbg_loaded = 0;
		ldbg = l;
	}
	return(l);
}

/*
 * list_free - free the load_list node and the memory descriptor.
 *
 * returns 0 if OK, 1 if error.
 */
static LONG
list_free(ld_t *l)
{
	ld_t *l1, *l2;

	/* 
	 * If there's only 1 node in list, free mem descriptor and 
	 * and reset llist.
	 */
	if ((l == lroot) && (l->next == 0)) {
		if (md_dealloc ((unsigned long)l->m->BasePage, (unsigned long)l->m->PageCount)) {
			printf ("Error freeing memory descriptor at 0x%x\n",
				l->m->BasePage);
			return(1);
		}
		free (l);
		lroot = 0;
		return(0);
	} 
	/*
	 * If l is the lroot, free the node and reset lroot to
	 * the next node in the list.
	 */
	if (l == lroot) {
		if (md_dealloc ((unsigned long)l->m->BasePage, (unsigned long)l->m->PageCount)) {
			printf("Error freeing memory descriptor at 0x%x\n",
			       l->m->BasePage);
			return(1);
		}
		lroot = l->next; 
		free(l);
		return(0);
	}
	/*
	 * Otherwise, search list and free the node.
	 */
	l1 = lroot;
	while (l1) {
		l2 = l1->next;
		if (l2 == l) {
			if (md_dealloc ((unsigned long)l->m->BasePage, (unsigned long)l->m->PageCount)) {
				printf("Error freeing memory descriptor at 0x%x\n",
					l->m->BasePage);
				return(1);
			}
			l1->next = l2->next;
			free(l2);
			return(0);
		}
		l1 = l1->next;
	}
	printf("Error freeing load list node.\n");
	return(1);
}

void
load_error(char *filename, char *fmt, ...)
{
	va_list ap;


	printf("\nCannot load %s.\n", filename);

	/*CONSTCOND*/
	va_start(ap,fmt);
	vprintf(fmt,ap);
	va_end(ap);

	putchar('\n');

	return;
}

#if DEBUG && LOAD_COFF
printhdr (struct filehdr *hdr)
{
	printf ("filehdr:\n");
	printf ("	magic=0x%x, nscns=%d, timdat=0x%x\n",
		hdr->f_magic, hdr->f_nscns, hdr->f_timdat);
	printf ("	symptr=0x%x, nsyms=%d, opthdr=0x%x, flags=0x%x\n",
		hdr->f_symptr, hdr->f_nsyms, hdr->f_opthdr, hdr->f_flags); 	
	printf ("\n");
}

printaouthdr (struct filehdr *hdr)
{ 
	struct aouthdr *aout;

	aout = (struct aouthdr *) ((char *)hdr + sizeof(struct filehdr));
	printf ("aouthdr:\n");
	printf ("	magic=0x%x, vstamp=0x%x\n", aout->magic, aout->vstamp);
	printf ("	tsize=0x%x, dsize=0x%x, bsize=0x%x\n", aout->tsize,
		aout->dsize, aout->bsize);
	printf ("	entry=0x%x, tstart=0x%x, dstart=0x%x, bstart=0x%x\n",
		aout->entry, aout->text_start, aout->data_start, 
		aout->bss_start);
	printf ("\n");
}

printscns (struct filehdr *hdr)
{
	struct scnhdr *scn;
	int nscns = ((struct filehdr *)hdr)->f_nscns;
	int i;

	scn =(struct scnhdr *)
		((char *)hdr + sizeof(struct filehdr) + hdr->f_opthdr);

	for (i=0;i++, nscns--;scn++) {
		printf ("scnhdr %d:\n", i);
		printf ("	name=%s, paddr=0x%x, vaddr=0x%x, size=0x%x\n",
			scn->s_name, scn->s_paddr, scn->s_vaddr, scn->s_size);
		printf ("	scnptr=0x%x, relptr=0x%x, lnnoptr=0x%x\n",
			scn->s_scnptr, scn->s_relptr, scn->s_lnnoptr); 
		printf ("	nreloc=%d, nlnno=%d, flags=0x%x\n",
			scn->s_nreloc, scn->s_nlnno, scn->s_flags);
	}
	printf ("\n");
}
#endif

#if DEBUG
void
listlist(void)
{
	ld_t *l;

	printf ("listlist\n");
	l = lroot;
	while(l) {
		printf ("l=0x%x, BasePage=0x%x, lowaddr=0x%x, next=0x%x\n", 
			l, l->m->BasePage, l->lowaddr, l->next);
		l = l->next;
	}
}
#endif	/*DEBUG*/
