/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#if defined(IP30)

#include <arcs/types.h>
#include <sys/cpu.h>
#include <sys/mips_addrspace.h>
#include <parser.h>
#include <libsc.h>
#include <libsk.h>
#include <ctype.h>
#include <sys/RACER/sflash.h>
#include <sys/RACER/IP30nvram.h>

#include "flash_prom_cmd.h"

static int _fl_copy(__psunsigned_t src, __psunsigned_t dst, int len);

#define	FCMD_CHECK	0x1

/*
 * command structure
 */
typedef struct flash_cmd flash_cmd_t;

struct flash_cmd {
	char		*cmd;
	int		(*func)(int, char **, flash_cmd_t *);
	char		*usage;
	unsigned	flags;
};

#define FLASH_USAGE(_p)    FPR_PR(("flash %s %s\n", (_p)->cmd, (_p)->usage))
SETUP_TIME

#ifdef FDEBUG
unsigned long long
_cksum64(unsigned long long *lp, int len, unsigned long long sum)
{
	len &= ~0x7;	/* trucate to mod(8)==0 */
	while (len) {
	    sum = sum + *lp++;
	    len -= sizeof (*lp);
	}
	return(sum);
}
#endif

static ulong
running_prom(void)
{
	ulong pc, ppc;
	extern ulong get_pc(void);

	pc = get_pc();
	if (IS_COMPAT_PHYS(pc))
	    ppc = COMPAT_TO_PHYS(pc);
	else
	    ppc = KDM_TO_PHYS(pc);

	return(IS_PROM_PADDR(ppc)?ppc:0);
}

static unsigned
running_rprom(void)
{
	ulong ppc = running_prom();
	return((ppc >= SFLASH_RPROM_PADDR &&
		ppc < (SFLASH_RPROM_PADDR + SFLASH_RPROM_SIZE)));
}

static unsigned
running_fprom(void)
{
	ulong ppc = running_prom();
	return((ppc >= SFLASH_FPROM_PADDR &&
		ppc < (SFLASH_FPROM_PADDR + SFLASH_FPROM_SIZE)));
}
/******************************************************************/
#include <arcs/errno.h>
#include <arcs/eiob.h>

extern MEMORYDESCRIPTOR *mem_contains(unsigned long, unsigned long);
extern MEMORYDESCRIPTOR *mem_getblock(void);
extern void mem_list(void);

int
_fl_get(		char *path,
			__psunsigned_t *get_buf,
			int *get_size)
{
#define READ_SIZE (16 * 1024)

	MEMORYDESCRIPTOR *m;
	__psunsigned_t bufp, ptr, buf_lim;
	ULONG fd, cnt, total;
	LONG err;

	if (!(m = mem_getblock())) {
	    FPR_ERR(("No free memory descriptors available.\n"));
	    mem_list();
	    return(ENOMEM);
	}

	FPR_HI(("  memblock base=%#x count=%ld\n",
	    arcs_ptob(m->BasePage), arcs_ptob(m->PageCount)));
	bufp = arcs_ptob(m->BasePage);
	bufp = PHYS_TO_K0(bufp);

	err = Open(path, OpenReadOnly, &fd);
	if (err) {
	    FPR_ERR(("Open error %ld for %s\n", err, path));
	    return(err);
	}

	cnt = 0;
	total = 0;
	ptr = bufp;
	buf_lim = bufp + arcs_ptob(m->PageCount); 

	do {
	    err = Read(fd, (void *)ptr, READ_SIZE, &cnt);
	    if (err != ESUCCESS) {
		FPR_ERR(("Read error  %ld for %s\n", err, path));
		goto bail;
	    }
	    total += cnt;
	    ptr += cnt;
	    if (ptr > buf_lim) {
		FPR_ERR(("File too big > %d\n", arcs_ptob(m->PageCount)));
		err = EBADF;
		goto bail;
	    }
	} while (cnt);

	*get_buf = bufp;
	*get_size = total;
bail:
	Close(fd);
	return(err);
}

#ifdef FDEBUG
#include <elf.h>
extern int load_elf_struct64(char *, ULONG *, int, int, void *);

union commonhdr {
    Elf64_Ehdr elfhdr64;
};

/*
 * load the pgm section into load_buf and add a ~_cksum_fl16 at end
 */
static int
_fl_load(		char *path,
			__psunsigned_t exe_base,
			__psunsigned_t *load_buf,
			int *load_size)
{
	__psunsigned_t bufp, boffset;
	MEMORYDESCRIPTOR *m;
	ULONG fd, cnt;
	LONG err;
	union commonhdr *commonhdr = 0;
	Elf64_Ehdr *elfhdr;
	Elf64_Phdr *pgmhdr = 0;
	unsigned int size;
#ifdef XXX
	Elf64_Shdr *shdr;
#endif

	/* XXX */
	if (!(m = mem_getblock())) {
	    FPR_ERR(("No free memory descriptors available.\n"));
	    mem_list();
	    return(FL_ERR);
	}
	FPR_HI(("  memblock base=%#x count=%ld\n",
	    arcs_ptob(m->BasePage), arcs_ptob(m->PageCount)));
	/*
	 * use the memory from the top
	 */
	bufp = arcs_ptob(m->BasePage + m->PageCount);
	bufp = PHYS_TO_K0(bufp - SFLASH_MAX_SIZE);
	/*
	 * initialize possbile holes with all 1's
	 */
	{
	    int64_t	*lp = (int64_t*) bufp;
	    
	    cnt = SFLASH_RPROM_SIZE + FLASH_HEADER_SIZE;
	    while (cnt) {
	    	*lp++ = -1;
		cnt -= sizeof(int64_t);
	    }
	}

	/* load */
	err = Open(path, OpenReadOnly, &fd);
	if (err) {
	    FPR_ERR(("Open error %ld for %s\n", err, path));
	    return(err);
	}
	commonhdr = dmabuf_malloc(sizeof(union commonhdr));
	if (commonhdr == 0) {
	    Close(fd);
	    FPR_ERR(("cant dmabuf_malloc commondr (%d)\n",
	    	sizeof(union commonhdr)));
	    return(FL_ERR);
	}
	elfhdr = &(commonhdr->elfhdr64);
	err = load_elf_struct64(path, &fd, sizeof(Elf64_Ehdr), 0, elfhdr);
	if (err != ESUCCESS) {
	    FPR_ERR(("%s: cant read elf64 header err=%i\n", err));
	    goto bail;
	}
	if (!IS_ELF(commonhdr->elfhdr64) ||
	    commonhdr->elfhdr64.e_ident[EI_CLASS] != ELFCLASS64) {
	    FPR_ERR(("Not elf class 64\n"));
	    goto bail;
	}
	FPR_HI(("file fd %li, Elf 64\n", fd));
	
	FPR_HI(("    type %#x mach %#x version %u\n",
	       elfhdr->e_type,
	       elfhdr->e_machine,
	       elfhdr->e_version));
	FPR_HI(("    entry-addr %#x hsize %u flg %#x\n",
	       elfhdr->e_entry,
	       elfhdr->e_ehsize,
	       elfhdr->e_flags));
	FPR_HI(("    program off %#llx size %#x num %#x\n",
	       elfhdr->e_phoff,
	       elfhdr->e_phentsize,
	       elfhdr->e_phnum));
	FPR_HI(("    section off %#llx size %#x num %#x\n\n",
	       elfhdr->e_shoff,
	       elfhdr->e_shentsize,
	       elfhdr->e_shnum));
	size = elfhdr->e_phentsize * elfhdr->e_phnum;
	if (size > 0) {
	    FPR_HI(("about to load program headers\n"));
	    pgmhdr = (Elf64_Phdr *) dmabuf_malloc(size);
	    if (pgmhdr == 0) {
		FPR_ERR(("cant alloc pgmhdr, size=%d\n", size));
		goto bail;
	    }
	    err = load_elf_struct64(path, &fd, size, elfhdr->e_phoff, pgmhdr);
	    if (err != ESUCCESS) {
		FPR_ERR(("load_elf_struct() failed err=%d\n", err));
		goto bail;
	    }
	    /* dump program headers */
	    FPR_HI(("\nheader entry size %d\n", elfhdr->e_phentsize));
	    FPR_HI(("%d entries in file\n", elfhdr->e_phnum));
	    for(cnt = 0; cnt < elfhdr->e_phnum; cnt++) {
		FPR_MSG(("program %d:  size %d vaddr %x\n",
			cnt, pgmhdr[cnt].p_memsz, pgmhdr[cnt].p_vaddr));
		FPR_HI(("     type %x, offset %llx",
		       pgmhdr[cnt].p_type,
		       pgmhdr[cnt].p_offset));
		FPR_HI((" file size %llx, mem size %llx, "
		       " flags %x, align %llx\n",
		       pgmhdr[cnt].p_filesz,
		       pgmhdr[cnt].p_memsz,
		       pgmhdr[cnt].p_flags,
		       pgmhdr[cnt].p_align));
	    }
	} else {
	    FPR_ERR(("no program header!\n"));
	    goto bail;
	}
	for(size = 0, cnt = 0; cnt < elfhdr->e_phnum; cnt++) {
	    __psunsigned_t boffset, beg_paddr;

	    /* check PT_LOAD type */
	    if (pgmhdr[cnt].p_type != PT_LOAD)
		continue;

	    if (IS_COMPAT_PHYS(pgmhdr[cnt].p_vaddr))
		beg_paddr = COMPAT_TO_PHYS(pgmhdr[cnt].p_vaddr);
	    else
		beg_paddr = KDM_TO_PHYS(pgmhdr[cnt].p_vaddr);
	    /* check vaddr+memsz range */
	    if (!IS_PROM_PADDR(beg_paddr) &&
		!IS_PROM_PADDR(beg_paddr+pgmhdr[cnt].p_memsz-2))
		continue;
	    if (beg_paddr < exe_base) {
		FPR_ERR(("Bad (p_vaddr=0x%llx paddr=0x%x) < exe_base=0x%x\n",
			pgmhdr[cnt].p_vaddr, beg_paddr, exe_base));
		err = ENOMEM;
		goto bail;
	    }
	    boffset = (beg_paddr - exe_base);
	    err = load_elf_struct64(path, &fd, pgmhdr[cnt].p_filesz, 
		pgmhdr[cnt].p_offset, (void *)(bufp + boffset)); 
	    if (err != ESUCCESS)
		goto bail;
	    /* size must be begin of bufp to all loaded pgms */
	    if (boffset + pgmhdr[cnt].p_filesz > size)
	    	size = boffset + pgmhdr[cnt].p_filesz;
	}

#ifdef XXX
	/*
	 * dump section headers
	 */
	size = elfhdr->e_shentsize * elfhdr->e_shnum;
	if (size > 0) {
	    FPR_MSG(("about to load section headers size=%#x\n", size));
	    shdr = (Elf64_Shdr *) dmabuf_malloc(size);
	    if (shdr == 0) {
		FPR_ERR(("cant alloc shdr, size=%d\n", size));
		goto bail;
	    }
	    err = load_elf_struct64(path, &fd, size, elfhdr->e_shoff, shdr);
	    if (err != ESUCCESS) {
		FPR_ERR(("load_elf_struct() failed err=%d\n", err));
		goto bail;
	    }
	    FPR_MSG(("section entry size %#d\n", elfhdr->e_shentsize));
	    FPR_MSG(("%#d entries in file\n", elfhdr->e_shnum));
	    for(cnt = 0; cnt < elfhdr->e_shnum; cnt++) {
		printf("[%d]\n", cnt);
		dump_bytes((char *)&shdr[cnt], sizeof(Elf64_Shdr), 1);
		FPR_MSG(("\n"));
		FPR_MSG(("%d: %#x type %#x, offset %#llx, vaddr %#llx, "
		       "section size %#llx, align %#llx, entsize %#llx, "
		       "flags %#llx\n",
		       cnt,
		       shdr[cnt].sh_name,
		       shdr[cnt].sh_type,
		       shdr[cnt].sh_offset,
		       shdr[cnt].sh_addr,
		       shdr[cnt].sh_size,
		       shdr[cnt].sh_addralign,
		       shdr[cnt].sh_entsize,
		       shdr[cnt].sh_flags));
	    }
	    dmabuf_free(shdr);
	}
#endif	/* FDEBUG */
	if (elfhdr->e_type != ET_EXEC) {
	    FPR_MSG(("%s: not executable!\n", path));
	    goto bail;
	}
	if (size & 1)
	    size += 1;

	*load_size = size;
	*load_buf = bufp;
bail:
	if (commonhdr) dmabuf_free(commonhdr);
	if (pgmhdr) dmabuf_free(pgmhdr);
	if (fd) Close(fd);
	return(err);
}

/*
 * syntax: fl load [<file>]	-
 */
static int
fl_load(int argc, char **argv, flash_cmd_t *fcmdp)
{
	__psunsigned_t ld_buf;
	int rv, ld_sz;
	char *path;

	FPR_LO(("load: \n"));
	if (argc <= 0) {
	    /* no path, try env variables */
	    /* XXX for now hardcode is some stuff */
	    path = "bootp()femto.engr:i2/prom";
	} else
	    path = argv[0];
	FPR_LO(("path=%s\n", path));
	ld_sz = 0;
	ld_buf = 0;
	rv = _fl_load(path, SFLASH_EXE_BASE, &ld_buf, &ld_sz);
	if (rv != ESUCCESS || !ld_sz || !ld_buf)
	    FPR_ERR(("No text/data in %s to load\n", path));

	FPR_MSG(("\n%s loaded @0x%x, actual size %d, flash rdup size %d\n",
		path, ld_buf, ld_sz, SFLASH_RDUP(ld_sz)));
	return(rv);
}


/*
 * erase segments from beg_seg to end_seg inclusive
 */
static int
_fl_erase(unsigned beg_seg, unsigned end_seg)
{
	unsigned seg;
	flash_err_t frv;

	if (end_seg - beg_seg >= SFLASH_MAX_SEGS)
	    return(1);
	for (seg = beg_seg; seg <= end_seg; seg++) {
	    frv = flash_erase(seg);
	    if (frv) {
		flash_print_err(frv);
		return(1);
	    }
	}
	return(0);
}

/*
 * syntax: fl get [<file>]	-
 */
static int
fl_get(int argc, char **argv, flash_cmd_t *fcmdp)
{
	__psunsigned_t get_buf;
	int rv, get_sz;
	char *path;

	if (argc <= 0) {
	    /* no path, try env variables */
	    /* XXX for now hardcode is some stuff */
	    path = "bootp()femto.engr:i2/prom";
	} else
	    path = argv[0];
	FPR_PR(("get: %s\n", path));
	get_sz = 0;
	get_buf = 0;
	rv = _fl_get(path, &get_buf, &get_sz);
	if (rv)
	    return(rv);
	FPR_PR(("%d bytes read to 0x%x, sum -r:\n", get_sz, get_buf));
	FPR_PR(("%05u %d\n", flash_cksum_r((void *)get_buf, get_sz, 0), (get_sz + 511)/512));
	return(0);
}
#endif

#define PROM_CMD_OPT_f		0
#define PROM_CMD_OPT_R		1
#define PROM_CMD_OPT_r		2
#define PROM_CMD_OPT_Force	3
/*
 * syntax: fl prom [-r|-R] <file>      -
 */
static int
fl_prom(int argc, char **argv, flash_cmd_t *fcmdp)
{
	char *path;
	char *data;
	flash_header_t *hdr;
	__psunsigned_t ld_buf = 0;
	int ld_sz = 0;
	int flash_n_reset_fprom = running_fprom();
	int opt = PROM_CMD_OPT_f;
	int rv;

	FPR_LO(("prom:\n"));

	if (argc) {
	    if (!strcmp(argv[0], "-r")) {
		opt = PROM_CMD_OPT_r;
		argc--;
		argv++;
	    } else if (!strcmp(argv[0], "-R")) {
		opt = PROM_CMD_OPT_R;
		argc--;
		argv++;
	    } else if (!strcmp(argv[0], "-Force")) {
		opt = PROM_CMD_OPT_Force;
		argc--;
		argv++;
	    }
	}

	if (argc <= 0) {
	    FPR_ERR(("no file specified\n"));
	    return(1);
	} else
	    path = argv[0];

	rv = _fl_get(path, &ld_buf, &ld_sz);
	if (rv) {
	    return(1);
	}
	FPR_MSG(("%d bytes loaded to mem @ 0x%x\n", ld_sz, ld_buf));

	if (opt == PROM_CMD_OPT_Force) {
	    FPR_PR(("Program loaded data to base of flash\n"));
	    goto force_flash;
	}

	rv = _fl_check_bin_image((char*)ld_buf, ld_sz);
	switch (rv) {

	case IP30PROM_BIN_FORMAT: {	/* rprom.bin format */
	    /*
	     * we will be running fprom or dprom, rprom will not
	     * use this fct
	     */
	    if (opt == PROM_CMD_OPT_R || opt == PROM_CMD_OPT_r) {
		FPR_PR(("Programming Rprom\n"));
		rv = _fl_prom((uint16_t *)ld_buf,
			       0,
			       SFLASH_RPROM_SEG,
			       SFLASH_RPROM_SIZE);
		if (rv)
		    goto rprom_failed;
	    }
	    /*
	     * make sure the rprom is good before attempting to
	     * re-flash the fprom section
	     */
	    if (flash_prom_ok((char *)SFLASH_RPROM_ADDR,
			      SFLASH_RPROM_HDR_ADDR) == 0)
		goto bad_rprom;

	    if (opt != PROM_CMD_OPT_r) { 
		FPR_PR(("Programming Fprom\n"));
		rv = _fl_prom((uint16_t *)(ld_buf + SFLASH_RPROM_SIZE),
			       flash_n_reset_fprom,
			       SFLASH_FPROM_SEG,
			       ld_sz - SFLASH_RPROM_SIZE);
		if (rv)
		    goto fprom_failed;
	    }
	    break;
	}

	case FPROM_BIN_FORMAT: {		/* fprom.bin format */

	    if (opt != PROM_CMD_OPT_f) {
		FPR_ERR(("bad option %s\n", argv[-1]));
		return(1);
	    }
	    /*
	     * make sure the rprom is good before attempting to
	     * re-flash the fprom section
	     */
	    if (flash_prom_ok((char *)SFLASH_RPROM_ADDR,
			      SFLASH_RPROM_HDR_ADDR) == 0)
		goto bad_rprom;

	    FPR_PR(("Programming Fprom\n"));
	    rv = _fl_prom((uint16_t *)ld_buf,
			   flash_n_reset_fprom,
			   SFLASH_FPROM_SEG,
			   ld_sz);
	    if (rv)
		goto fprom_failed;
	    break;
	}

	default: {
	    if (!yes("Unrecognized binary image, program anyway"));
		return(1);
force_flash:
	    if (!yes("This is VERY risky! Are you REALLY REALLY sure"))
		return(1);

	    rv = _fl_prom((uint16_t *)ld_buf,
			   flash_n_reset_fprom,
			   SFLASH_RPROM_SEG,
			   MIN(ld_sz, SFLASH_MAX_SIZE));
	    if (rv)
		goto force_failed;
	    break;
	}
	} /* switch */
	return(0);

rprom_failed:
	FPR_ERR(("Programming Rprom failed\n"));
	return(1);
fprom_failed:
	FPR_ERR(("Programming Fprom failed\n"));
	return(1);
force_failed:
	FPR_ERR(("Programming Flash failed\n"));
	return(1);
bad_rprom:
	FPR_ERR(("Rprom section is bad cannot proceed\n"));
	return(1);
}

/*
 * syntax: fl log [inval] [<entry_offset>]
 */
/*ARGSUSED*/
static int
fl_log(int argc, char **argv, flash_cmd_t *fcmdp)
{
	flash_pds_ent_t *ent = 0;
	char *data;
	int invalidate = 0;
	int len, i;
	int hoffset; 	

	while (argc--) {
	    if ( !strcmp("inval", *argv)) {
		invalidate = 1;
	    } else {
		hoffset = atoi(*argv)/sizeof(vu_short);
		ent = (flash_pds_ent_t *)(SFLASH_PDS_ADDR + hoffset);
	    }
	    argv++;
	}

	/*
	 * ent returned is the next entry but data and len
	 * is from the current log entry
	 */
	i = 0;
	data = 0;
	do {
	    ent = flash_pds_get_log(ent, &data, &len, invalidate);
	    if (!data)
		break;
	    FPR_PR(("[0x%04x] ",
	    	((__psint_t)data&0xffff)-FPDS_ENT_DATA_OFFS*sizeof(vu_short)));
	    flash_print_log((void *)data, len);
	    if ((++i % 16) == 0) {
		if (!yes("more"))
		    return(0);
	    }
	} while (ent);
	return(0);
}

/*
 * key=0 ==> first data entry starting from ent
 * ent=0 ==> start searching from begin of PDS
 */
static flash_pds_ent_t *
fl_pds_get_data(char *key, flash_pds_ent_t *ent)
{
    	char *buf;
	int len;
	u_short cksum;
	flash_pds_da0_t *da0;

	ent = flash_pds_find_data(key, ent);
	if (!ent)
	    return(0);
	da0 = (flash_pds_da0_t *)ent->data;
	key = (char *)(da0 + 1);	/* in case key was 0 */
	buf = malloc(da0->datalen + 8); /* datalen and some pad */
	if (buf == 0) {
	    FPR_ERR(("cant malloc %d bytes\n", da0->datalen));
	    return(0);
	}
	len = flash_pds_copy_data(key, buf, 0);
	cksum = flash_cksum_r(buf, len, 0);
	
	FPR_PR(("PDS DATA key=%s: cksum 0x%x (%d bytes) is %s (actual 0x%x)\n",
	    key, da0->datasum, da0->datalen,
	    (da0->datasum != cksum)?"BAD":"OK", cksum));
	free(buf);
	return(ent);
}

#define DATA_CMD_OPT_def	0
#define DATA_CMD_OPT_d		1
#define DATA_CMD_OPT_f		2
#define DATA_CMD_OPT_m		3
#define DATA_CMD_OPT_r		4
/*
 *	"[[-d|-f|-m] <key> [<file>|<range>]]"
 */
static int
fl_data(int argc, char **argv, flash_cmd_t *fcmdp)
{
	int opt = DATA_CMD_OPT_def;
	int len;
	char *key, *file, *buf;
	struct range rg;
	flash_pds_ent_t *ent;
	flash_err_t frv;

	if (argc) {
	    opt = DATA_CMD_OPT_r;

	    if (!strcmp(argv[0], "-d")) {
		opt = DATA_CMD_OPT_d;
		argc--;
		argv++;
	    } else if (!strcmp(argv[0], "-f")) {
		opt = DATA_CMD_OPT_f;
		argc--;
		argv++;
	    } else if (!strcmp(argv[0], "-m")) {
		opt = DATA_CMD_OPT_m;
		argc--;
		argv++;
	    }
	}

	key = argv[0];
	argc--;
	argv++;

	switch (opt) {
	case DATA_CMD_OPT_d:	/* delete option */
	    flash_pds_inval_data(key);
	    break;
	
	case DATA_CMD_OPT_f:	/* add from file option */
	    if (argc == 0) {
		FLASH_USAGE(fcmdp);
		return(-1);
	    }
	    frv = _fl_get(argv[0], (__psunsigned_t *)&buf, &len);
	    if (frv)
		break;
	    frv = flash_pds_set_data(key, buf, len);
	    if (frv)
		flash_print_err(frv);
	    break;

	case DATA_CMD_OPT_m:	/* add from memory option */
	    if (argc == 1 && !parse_range(argv[0], 1, &rg)) {
		FLASH_USAGE(fcmdp);
		return(-1);
	    }
	    frv = flash_pds_set_data(key, (char *)rg.ra_base, rg.ra_count);
	    if (frv)
		flash_print_err(frv);
	    break;

	case DATA_CMD_OPT_r:	/* read <key> option */
	    fl_pds_get_data(key, 0);
	    break;

	default:		/* read all option */
	    ent = 0;
	    while (ent = fl_pds_get_data(0, ent))
		;
	    break;
	}
	return(0);
}

/*
 * syntax: 
 */
/*ARGSUSED*/
static int
fl_resetpds(int argc, char **argv, flash_cmd_t *fcmdp)
{
	/* call flash_pds_init with resetpds flag on */
    	flash_pds_init(1);
	return(0);
}

static struct reg_desc rprom_flags_desc[] = {
    { RPROM_FLG_TEST,		0,	"RPROM TEST",	NULL,	NULL },
    { RPROM_FLG_DBG_MSK,	-4,	"Debug",	"0x%x",	NULL },
    { RPROM_FLG_FVP,		0,	"FPROM Valid Pend",NULL,NULL },
    { RPROM_FLG_VRB_MSK,	0,	"Verbose",	"0x%x",	NULL },
    {0,0,NULL,NULL,NULL}
};
/*
 * syntax: <16 bit flag> 
 */
/*ARGSUSED*/
static int
fl_rpromflg(int argc, char **argv, flash_cmd_t *fcmdp)
{
	ushort oval, nval;

	if (argc) {
	    nval = atoi(*argv);
	}

	oval = flash_get_nv_rpromflg();
	FPR_PR(("Current flag=%R\n", oval, rprom_flags_desc)); 

	if (!argc)
	    return(0);

	/* call flash_pds_init with resetpds flag on */
    	flash_set_nv_rpromflg(nval);

	FPR_PR(("New     flag=%R\n", nval, rprom_flags_desc)); 
	return(0);
}

#ifdef FDEBUG
/*
 * syntax: fl setenv var [str]
 */
/*ARGSUSED*/
static int
fl_senv(int argc, char **argv, flash_cmd_t *fcmdp)
{
	flash_err_t rv;
	short zero = 0;
	char *str = (char *)&zero;

	if (argc > 2 || !argc)
	    return(1);
	if (argc == 2)
	    str = argv[1];
	FPR_LO(("senv: var %s str %s\n", argv[0], str));

	START_TIME;
	rv = flash_setenv(argv[0], str);
	STOP_TIME("setenv ")

	FPR_PR(("rv=0x%x\n",rv));
	return(rv); 
}

/*
 * syntax: fl unsetenv var
 */
/*ARGSUSED*/
static int
fl_uenv(int argc, char **argv, flash_cmd_t *fcmdp)
{
	int rv;

	if (argc != 1)
	    return(1);

	/*
	 * 0 for string entry implies unset
	 */
	START_TIME;
	rv = flash_setenv(argv[0], 0);
	STOP_TIME("unsetenv ");

	FPR_PR(("rv=0x%x\n",rv));
	return(rv); 
}

/*
 * syntax: fl getenv var
 */
/*ARGSUSED*/
static int
fl_genv(int argc, char **argv, flash_cmd_t *fcmdp)
{
	flash_pds_ent_t *p;

	if (argc != 1)
	    return(1);
	FPR_LO(("genv: var %s\n", argv[0]));

	START_TIME;
	p = flash_findenv(0, argv[0]);
	STOP_TIME("getenv ");

	if (p)
	    FPR_PR(("%s\n", p->data));
	return(0);
}

/*
  syntax: fl pds [all | env | log]
 */
/*ARGSUSED*/
static int
fl_pds(int argc, char **argv, flash_cmd_t *fcmdp)
{
	u_short filter = FPDS_FILTER_VALID;
	flash_pds_ent_t *ent = 0;
	int len;
	char msg[514];

	FPR_PDS(("fl_pds:\n"));
	if (argc >= 1) {
	    if (!strcmp(argv[0], "all"))
		filter = FPDS_FILTER_ALL;
	    else if (!strcmp(argv[0], "env"))
		filter = FPDS_ENT_ENV_TYPE;
	    else if (!strcmp(argv[0], "log"))
		filter = FPDS_ENT_LOG_TYPE;
	    else if (!strcmp(argv[0], "data"))
		filter = FPDS_ENT_DAT_TYPE;
	    else if (!strcmp(argv[0], "compress")) {
		vu_short *rv;

		rv = flash_pds_compress(0);
		FPR_PR(("flash_pds_compress rv = 0x%x\n", rv));
		return(0);
	    }
	}

	while (ent = flash_pds_ent_next(ent, filter)) {
	    __psunsigned_t offset;

	    len = ent->pds_hlen*sizeof(uint16_t);
	    bcopy((void *)ent->data, msg, len);
	    msg[len] = 0;

	    offset = ((__psunsigned_t)ent - (__psunsigned_t)SFLASH_PDS_ADDR);
	    switch (FPDS_ENT_TYPE(ent)) {
		case FPDS_ENT_ENV_TYPE: {
		    printf("env: offs 0x%06x: %s: len %d: %s\n",
		    	0xffffff&offset,
			(ent->valid==FPDS_ENT_VALID)?"VAL":"INV",
			len, msg);
		    break;
		}
		case FPDS_ENT_LOG_TYPE: {
		    printf("log: offs 0x%06x: %s: len %d:\n",
			0xffffff&offset,
			(ent->valid==FPDS_ENT_VALID)?"VAL":"INV",
			len);
		    flash_print_log((void *)msg, len);
		    break;
		}
		case FPDS_ENT_DA0_TYPE: {
		    flash_pds_da0_t *da0 = (flash_pds_da0_t *)ent->data;
		    printf("da0: offs 0x%06x: %s: len %d:\n",
			0xffffff&offset,
			(ent->valid==FPDS_ENT_VALID)?"VAL":"INV",
			len);
		    printf("  len %d, cksum %d (0x%x): key %s\n",
		    	da0->datalen, da0->datasum, da0->datasum,
			(char *)(da0 + 1));
		    break;
		}
		case FPDS_ENT_DA1_TYPE: {
		    printf("da1: offs 0x%06x: %s: len %d:\n",
			0xffffff&offset,
			(ent->valid==FPDS_ENT_VALID)?"VAL":"INV",
			len);
		    break;
		}
		default: {
		    printf("unknown: offs 0x%06x: %s: type 0x%x (%d bytes):\n",
			0xffffff&offset,
			(ent->valid==FPDS_ENT_VALID)?"VAL":"INV",
		    	ent->pds_type_len & FPDS_ENT_TYPE_MASK,
			ent->pds_hlen * sizeof(uint16_t));
		    break;
		}
	    }	
	}
	return(0);
}


/*
 * syntax: fl erase (<SEG> | all) [<END_SEG>]
 */
/*ARGSUSED*/
static int
fl_erase(int argc, char **argv, flash_cmd_t *fcmdp)
{
	unsigned beg_seg, end_seg;

	if (argc == 1) {
	    if (!strcmp(argv[0], "all")) {
		beg_seg = 0;
		end_seg = SFLASH_MAX_SEGS - 1;
		FPR_PR(("flash: erase all segments"));
	    } else {
		beg_seg = atoi(argv[0]);
		end_seg = beg_seg;
		FPR_PR(("flash: erase segment %d @%#x",
		       beg_seg, SFLASH_SEG_ADDR(beg_seg)));
	    }
	} else if (argc == 2) {
	    beg_seg = atoi(argv[0]);
	    end_seg = atoi(argv[1]);
	    FPR_PR(("flash: erase segments %d - %d", beg_seg, end_seg));
	} else {
	    FLASH_USAGE(fcmdp);
	    return(0);
	}
	if (!yes(""))
	    return(1);

	if (_fl_erase(beg_seg, end_seg))
	    FPR_ERR(("Error: unable to complete erase\n"));
	else
	    FPR_MSG(("Erase completed\n"));
	return(0);
}

/*
 * syntax: fl fprom [<SEG> [wd-pattern]]
 */
/*ARGSUSED*/
static int
fl_writest(int argc, char **argv, flash_cmd_t *fcmdp)
{
	int seg = 2, n;
	unsigned *u1, test=0;
	unsigned short *fp, *src_buf, *tp = (unsigned short *)&test;
	flash_err_t frv;

	if (argc >= 1)
	    seg = atoi(argv[0]);
	if (argc >= 2)
	    test = (unsigned)atoi(argv[1]);

	src_buf = (unsigned short *)malloc(SFLASH_SEG_SIZE);
	if (!src_buf) {
	    FPR_ERR(("flash: src buffer alloc failed\n"));
	    return(0);
	}
	n = SFLASH_SEG_SIZE/2;
	while (n--) 
	    src_buf[n] = tp[n&1];

	fp = (unsigned short *)SFLASH_SEG_ADDR(seg);
	FPR_PR(("  write segment %d@%#x from %#x with %#x",
	    seg, fp, src_buf, test));
	if (!yes(""))
	    return(1);

	START_TIME;
	frv = flash_cp(src_buf, fp, SFLASH_SEG_SIZE/2);
	STOP_TIME("\tsegment write");
	if (frv)
	    flash_print_err(frv);

	n = SFLASH_SEG_SIZE/4;
	u1 = (unsigned *)src_buf;
	START_TIME;
	while (n--) {
	    if (*u1++ == test)
		continue;
	    FPR_ERR(("flash: memory write failure @ %#x\n", u1-1));
	    free(src_buf);
	    return(0);
	}
	STOP_TIME("\tmemory buffer K1 read/test");

	n = SFLASH_SEG_SIZE/4;
	u1 = (unsigned *)SFLASH_SEG_ADDR(seg);
	START_TIME;
	while (n--) {
	    if (*u1++ == test)
		continue;
	    FPR_ERR(("flash: segment write failure @ %#x\n", u1-1));
	    free(src_buf);
	    return(0);
	}
	STOP_TIME("\tflash segment K1 read/test");

	free(src_buf);
	return(0);
}

/*ARGSUSED*/
static int
_fl_copy(__psunsigned_t src, __psunsigned_t dst, int hlen)
{
	int n;
	long long lsum;
	unsigned short *sp, *dp, sum;
	long long *lsp = (long long *)src;
	long long *ldp = (long long *)dst;
	flash_err_t frv;

	n = hlen;
	START_TIME;
	frv = flash_cp((uint16_t *)src, (vu_short *)dst, hlen);
	STOP_TIME("flash copy ");
	if (frv)
	    flash_print_err(frv);

	/* verify the copy */
	n = hlen;

	START_TIME;
	while (n > 3) {
	    n -= 4;
	    if (*lsp++ == *ldp++)
		continue;
	    FPR_ERR((" lcompare failure src/dst %04x(%#x)/%04x(%#x)\n",
		    *(lsp-1), lsp-1, *(ldp-1), ldp-1));
	    return(0);
	}
	if (n) {	/* we have some odd hwds, 1,2, or 3 */
	    sp = (unsigned short *)lsp;
	    dp = (unsigned short *)ldp;

	    while (n--) {
		if (*sp++ == *dp++)
		    continue;
		FPR_ERR((" hcompare failure src/dst %04x(%#x)/%04x(%#x)\n",
			*(sp-1), sp-1, *(dp-1), dp-1));
		return(0);
	    }
	}
	STOP_TIME("src/dst cmp ");

	START_TIME;
	sum = 0xffff & ~_cksum1((void *)src, hlen*2, 0);
	STOP_TIME("in_cksum src");
	printf(" in_cksum src=%#x\n", sum);
	START_TIME;
	sum = 0xffff & ~_cksum1((void *)dst, hlen*2, 0);
	STOP_TIME("in_cksum dst");
	printf(" in_cksum dst=%#x\n", sum);
	START_TIME;
	lsum = _cksum64((void *)src, hlen*2, 0);
	STOP_TIME("cksum64 src");
	printf("cksum64 src=%#llx\n", lsum);
	START_TIME;
	lsum = _cksum64((void *)dst, hlen*2, 0);
	STOP_TIME("cksum64 dst");
	printf("cksum64 dst=%#llx\n", lsum);
	return(0);
}

/*
 * syntax: fl copy <SRC> <DST> <len>
 */
/*ARGSUSED*/
static int
fl_copy(int argc, char **argv, flash_cmd_t *fcmdp)
{
	int len;
	long long lsum;
	__psint_t src, dst;

	if (argc != 3) {
	    FLASH_USAGE(fcmdp);
	    return(0);
	}

	/* switch over to command parser  for addr/RANGE parsing*/
	atob_ptr(argv[0], &src);
	atob_ptr(argv[1], &dst);
	len = atoi(argv[2]);
	if (!IS_KSEGDM(src) || !IS_KSEGDM(src+len)) {
	    FPR_ERR(("src addr %#x not K0/1\n", src));
	    return(0);
	}
	if (!IS_FLASH_ADDR(dst) || !IS_FLASH_ADDR(dst + len - 2)) {
	    FPR_ERR(("dst addr %#x len %d not flash\n", dst, len));
	    return(0);
	}
	if (len & 1) {
	    FPR_ERR(("len not even bytes, flash is 16bit device\n"));
	}

	FPR_PR(("flash: copy %#x -> %#x for %d bytes",
	    src, dst, len));
	if (!yes(""))
	    return(1);
	return(_fl_copy(src, dst, len/2));
}

/*
 * syntax: fl dump [<SEG> | <ADDR>]
 */
/*ARGSUSED*/
static int
fl_dump(int argc, char **argv, flash_cmd_t *fcmdp)
{
	static __psint_t addr;
	static int len;
	static int addr_flg = 1;

	if (argc >= 3)
	    addr_flg = 0;
	if (argc >= 2)
	    len = atoi(argv[1]);
	if (!len)
	    len = 0x80;
	if (argc >= 1)
	    atob_ptr(argv[0], &addr);
	if (addr >= 0 && addr < SFLASH_MAX_SEGS)
	    addr = (__psint_t)SFLASH_SEG_ADDR(addr);
	else if (!IS_KSEGDM(addr) && !IS_COMPAT_PHYS(addr)) {
	    FPR_ERR(("Bad addr: %#x\n", addr));
	    return(0);
	}
	dump_bytes((char *)addr, len, addr_flg);
	addr += len;
	printf("\n");
	return(0);
}

/*
 * syntax: fl 1fe
 *		set the flash_mem_base to 1fe
 */
/*ARGSUSED*/
static int
fl_1fe(int argc, char **argv, flash_cmd_t *fcmdp)
{
	flash_mem_base = FLASH_MEM_ALT_BASE;

	FPR_PR(("flash_mem_base set to 0x%x\n", flash_mem_base));
	return(0);
}

/*
 * syntax: fl 1fc
 *		set the flash_mem_base to 1fc
 */
/*ARGSUSED*/
static int
fl_1fc(int argc, char **argv, flash_cmd_t *fcmdp)
{
	flash_mem_base = FLASH_MEM_BASE;
	FPR_PR(("flash_mem_base set to 0x%x\n", flash_mem_base));

	if (running_rprom() || running_fprom())
	    FPR_PR(("CAUTION: You can toast the prom you are executing!!!\n"));
	return(0);
}
/*
 * syntax: fl time <usecs>
 */
/*ARGSUSED*/
static int
fl_time(int argc, char **argv, flash_cmd_t *fcmdp)
{
	int i = atoi(argv[0]);
	ulong t10k, t10k_ns, ns;
	vu_long hticks;
#ifdef US_DELAY_DEBUG
	__uint32_t before, after;
#endif
	extern __uint32_t us_before, us_after;
	extern ulong decinsperloop, uc_decinsperloop;
	extern ulong _ticksper1024inst(void);
	extern ulong delay_calibrate(void);

	FPR_PR(("time: us_delay(%d): ", i));
	START_TIME;
	us_delay(i);
	SAMPLE_TICKS(hticks);
#ifdef IP30
	ns = hticks * HEART_COUNT_NSECS;
#else
	ns = hticks * 100;
#endif
#ifdef US_DELAY_DEBUG
	before = us_before;
	after = us_after;
	FPR_PR(("C0_COUNT: took a little less than 0x%x - 0x%x = "
                        "%u ticks.\n",
                        (__psunsigned_t)before,(__psunsigned_t)after,
                        after-before));
#endif
	FPR_PR(("took %d,%03d,%03d nsecs hticks=%u\n",
		ns/1000000, (ns%1000000)/1000, (ns%1000000)%1000, hticks));
	FPR_PR(("uc_decinsperloop=%u decinsperloop=%u\n",
		uc_decinsperloop, decinsperloop));

	START_TIME;
	t10k = _ticksper1024inst();
	SAMPLE_TICKS(hticks);
#ifdef IP30
	ns = hticks * HEART_COUNT_NSECS;
	t10k_ns = t10k * HEART_COUNT_NSECS;
#else
	ns = hticks * 100;
	t10k_ns = t10k * 100;
#endif
	FPR_PR(("_ticksper1024inst took %d,%03d,%03d nsecs hticks=%u\n",
		ns/1000000, (ns%1000000)/1000, (ns%1000000)%1000, hticks));
	FPR_PR(("_ticksper1024inst=%u ticks, %u secs\n",
		t10k, t10k_ns));
	return(0);
}

/*
 * syntax: fl cksum [(<seg> | <addr>) [len]]
 */
/*ARGSUSED*/
static int
fl_cksum(int argc, char **argv, flash_cmd_t *fcmdp)
{
	int len = 0x80000;	/* default is 512K*/
	int cksum, n;
	__psint_t data = 0;
	long long llsum;

	if (argc >= 1) {
	    atob_ptr(argv[0], &data);
	    len = SFLASH_SEG_SIZE;
	}
	if (argc >= 2)
	    len = atoi(argv[1]);

	if (data >= 0 && data < SFLASH_MAX_SEGS)
	    data = (__psint_t)SFLASH_SEG_ADDR(data);
	else if (!IS_KSEGDM(data) || !IS_KSEGDM(data+len)) {
	    FPR_ERR(("Bad addr/len: %#x/%#x\n", data/len));
	    return(0);
	}
#if NOT
	START_TIME;
	cksum = flash_cksum_r((char *)data, len, 0);
	STOP_TIME("sum ");
	FPR_PR(("sum -r %d bytes @%#x\n", len, data));
	FPR_PR(("%05u %d\n", cksum, (len+511)/512));

	START_TIME;
	isum = _cksum32_16((void *)data, len, 0);
	STOP_TIME("cksum32_16 ");
	START_TIME;
	llsum = _cksum64_16((void *)data, len, 0);
	STOP_TIME("cksum64_16 ");
	FPR_PR(("  32bit %#x 64bit %#llx by 16 bit access\n", isum, llsum));
	START_TIME;
	isum = _cksum32((void *)data, len, 0);
	STOP_TIME("cksum32 ");
#endif
	FPR_PR(("%d bytes @%#x:\n", len, data));
	START_TIME;
	cksum = 0xffff & ~_cksum1((void *)data, len, 0);
	STOP_TIME("in_cksum ");
	FPR_PR(("in_cksum %#x\n", cksum));

	START_TIME;
	llsum = _cksum64((void *)data, len, 0);
	STOP_TIME("cksum64 ");
	FPR_PR(("cksum64 %#llx\n", llsum));
	return(0);
}

/*
 * syntax: fl sum-r <addr> <len>
 */
/*ARGSUSED*/
static int
fl_sum_r(int argc, char **argv, flash_cmd_t *fcmdp)
{
	int cksum, len, n;
	__psint_t data = 0;
	long long llsum;

	if (argc < 2) {
	    FLASH_USAGE(fcmdp);
	    return(0);
	}

	atob_ptr(argv[0], &data);
	len = atoi(argv[1]);

	if (data >= 0 && data < SFLASH_MAX_SEGS)
	    data = (__psint_t)SFLASH_SEG_ADDR(data);
	else if (!IS_KSEGDM(data) || !IS_KSEGDM(data+len)) {
	    FPR_ERR(("Bad addr/len: %#x/%#x\n", data/len));
	    return(0);
	}
	START_TIME;
	cksum = flash_cksum_r((char *)data, len, 0);
	STOP_TIME("sum ");
	FPR_PR(("sum -r %d bytes @%#x\n", len, data));
	FPR_PR(("%05u %d\n", cksum, (len+511)/512));
	FPR_PR(("0x%x\n", cksum));

	return(0);
}

static int
fl_log_test(int argc, char **argv, flash_cmd_t *f)
{
    char buf[128];
    char *str;
    int n = 0;

    if (argc)
	str = argv[0];
    else
	str = "flash log test";
    if (argc == 2) {
	TIMEINFO t;

	START_TIME;
	cpu_get_tod(&t);
	STOP_TIME("get tod ");

	n = atoi(argv[1]);
    }
    START_TIME;
    flash_pds_log(str);
    STOP_TIME("log test ");
    START_TIME;
    flash_pds_prf("flash_pds_prf: string test %s=0x%x %s=%d", "A", 0xA, "B", 2);
    STOP_TIME("log1 write ");
    START_TIME;
    flash_pds_write_log(PDS_LOG_TYPE2, 0xa, 0xb, 0xc, 0xd);
    STOP_TIME("log2 write ");

    START_TIME;
    while (n--) {
	flash_pds_prf("%s %8d", str, n);
    }
    STOP_TIME("logN write ");
    return(0);
}

/*
 * test power en-dis able fct by toggling it
 */
static int
fl_power(int argc, char **argv, flash_cmd_t *f)
{
    static int off_state;

    if (off_state) {
	ip30_setup_power();
	off_state = 0;
	FPR_MSG(("Power ints are ON\n"));
    } else {
	ip30_disable_power();
	off_state = 1;
	FPR_MSG(("Power ints are OFF\n"));
    }
    return(0);
}

#define ERASED_UINT64   0xffffffffffffffff
static __psunsigned_t
erase_chk(int seg)
{
    uint64_t *fdp     = (uint64_t *)SFLASH_SEG_ADDR(seg);
    uint64_t *fdp_end = (uint64_t *)SFLASH_SEG_ADDR(seg+1);

    while (fdp < fdp_end) {
	if (*fdp++ == ERASED_UINT64)
	    continue;
	return((__psunsigned_t)(fdp - 1));
    }
    return(0);
}

#define BITSPERSHORT	16
static flash_err_t
ttf_extra(int seg, int sl)
{
    vu_short *fp =  SFLASH_SEG_ADDR(seg);
    vu_short *seg_endp = SFLASH_SEG_ADDR(seg + 1);
    int i, ii;
    vu_short test, actual;
    flash_err_t rv;

    for (i=0; fp < seg_endp; i++) {
	test = 0xffff;
	for (ii=0; ii < BITSPERSHORT; ii++) {
	    if (sl)
		test <<= 1;
	    else
		test >>= 1;
	    rv = flash_cp((void *)&test, fp, 1);
	    actual = *fp;
	    if (rv || test != actual)
		goto failed;
	}
	fp++;
    }
    return(rv);
failed:
    printf("ttf_extfa: failed fp=0x%x i=%d ii=%d\n", fp, i, ii);
    if (rv)
	flash_print_err(rv);
    printf("  test=0x%x actual=0x%x\n", test, actual);
    return(1);
}


/*
 * test time-to-failure of a segment
 */
static int
fl_ttf(int argc, char **argv, flash_cmd_t *f)
{
    int extra = 0, erase_only = 0, set_count = 0, fcnt;
    unsigned n, count = 1;
    __psunsigned_t echk;
    flash_err_t frv;
    char *bufp = 0;


    while (argc--) {
	if ( !strcmp("extra", *argv)) {
	    extra = 1;
	} else if ( !strcmp("erase_only", *argv)) {
	    erase_only = 1;
	} else if ( !strcmp("set_count", *argv)) {
	    set_count = 1;
	} else {
	    count = (unsigned) atoi(*argv);
	}
	argv++;
    }

    if (set_count) {
	fcnt = count;
	fcnt += flash_get_nv_cnt(SFLASH_FPROM_SEG);
	flash_set_nv_cnt(SFLASH_FPROM_SEG, &fcnt);
	printf("new count %d\n", fcnt);
	return(0);
    }

    if (!extra) {
	bufp = (char *)align_malloc(SFLASH_SEG_SIZE, sizeof(__uint64_t));
	if (!bufp) {
	    printf("no mem for %d size\n", SFLASH_SEG_SIZE);
	    return(0);
	}
    }

    printf("flash erase-to-failure test of segment 14,\n"
    	   " %d loops extra=%d erase_only=%d: ",
	   count, extra, erase_only);
    	
    if (!yes("continue"))
	return(0);

    for (n = 0; n < count; n++) {
	printf(" n = %8d: ", n);
	if (extra) {
	    START_TIME;
	    if (ttf_extra(14, n & 1))
		break;
	    STOP_TIME("ttf_extra ");
	} else if (!erase_only) {
	    START_TIME;
	    bcopy((void *)SFLASH_SEG_ADDR(n&0x7), bufp, SFLASH_SEG_SIZE);
	    /* fill the buffer */
	    frv = flash_cp((void *)bufp,
			   SFLASH_SEG_ADDR(14),
			   SFLASH_SEG_SIZE/sizeof(uint16_t));
	    STOP_TIME("segfill ");
	    if (frv) {
		flash_print_err(frv);
		break;
	    }
	}
	frv = flash_erase(14);
	if (frv) {
	    flash_print_err(frv);
	    break;
	}
	fcnt = flash_get_nv_cnt(SFLASH_FPROM_SEG) + 1;
	flash_set_nv_cnt(SFLASH_FPROM_SEG, &fcnt);
	echk = erase_chk(14);
	if (echk) {
	    printf("erase failure at 0x%x\n", echk);
	    break;
	}

    }
    if (bufp)
	align_free(bufp);
    return(0);
}
#endif	/* FDEBUG */

/*ARGSUSED*/
/*
 * repeat flash cmds
 */
static int
fl_repeat(int argc, char **argv, flash_cmd_t *fcmdp)
{
	int flash_cmd(int argc, char **argv, char **envp);
	int count;

	if (argc < 2) {
	    FLASH_USAGE(fcmdp);
	    return(0);
    	}
	count = atoi(argv[0]);
	if (count <= 0) {
	    FLASH_USAGE(fcmdp);
	    return(0);
	}
	argc--;
	argv++;
	while (count--) {
	    flash_cmd(argc, argv, 0);
	    flash_pds_prf("%s: rep count %d", argv[0], count);
	}
	return(0);

}

/*
 * syntax: fl info
 */
/*ARGSUSED*/
static int
fl_info(int argc, char **argv, flash_cmd_t *fcmdp)
{
	unsigned short mfgid, devid;
	int seg = 0, status;

	if (argc >= 1)
	    seg = atoi(argv[0]);
	flash_id(&mfgid, &devid);
	FPR_PR(("Flash Base=0x%x  ID(mfg %#x dev %#x)\n",
		flash_mem_base, mfgid, devid));
	/* check mfg and dev and give appropriate confirm/warning */
	if (mfgid != SFLASH_MFG_ID)
	    FPR_PR(("Warning: unknown mfg id 0x%x, expected 0x%x\n",
		    mfgid, SFLASH_MFG_ID));
	if (devid != SFLASH_DEV_ID)
	    FPR_PR(("Warning: unknown device id 0x%x, expected 0x%x\n",
		    devid, SFLASH_DEV_ID));
	flash_print_status(seg);

	FPR_PR(("\n"));
	FPR_PR(("RPROM "));
	flash_print_prom_info(SFLASH_RPROM_HDR_ADDR,
			      (u_short *)SFLASH_RPROM_ADDR);
	FPR_PR(("\n"));
	FPR_PR(("FPROM "));
	flash_print_prom_info(SFLASH_FPROM_HDR_ADDR,
			      (u_short *)SFLASH_FPROM_ADDR +
			      		 FLASH_HEADER_SIZE/sizeof(uint16_t));
	FPR_PR(("\n"));
	/*
	 * if seg is non-zero it will be used as flag to check the
	 * pds usage data structs
	 */
	flash_print_pds_status(seg);
	return(0);
}

#define FCMD_PROM_USAGE		"[-r|-R] <file>" 
#define FCMD_LOG_USAGE		"[inval] [<entry_offset>]"
#define FCMD_DATA_USAGE		"[[-d|-f|-m] <key> [<file>|<range>]]"
#define FCMD_RESETPDS_USAGE	""
#define FCMD_RPROMFLG_USAGE	"<16-bit flag>"
#define FCMD_INFO_USAGE		"" 
#define FCMD_HELP_USAGE		""

#ifdef FDEBUG
#define FCMD_SENV_USAGE		"<var> [<string>]" 
#define FCMD_GENV_USAGE		"<var>" 
#define FCMD_UENV_USAGE		"<var>" 
#define FCMD_GET_USAGE		"[<file>]" 
#define FCMD_LOAD_USAGE		"[<file>]" 
#define FCMD_ERASE_USAGE	"(<SEG> [<END_SEG>] | all)"
#define FCMD_PDS_USAGE		"[all | env | log | compress ]" 
#define FCMD_1FC_USAGE		"" 
#define FCMD_1FE_USAGE		"" 
#define FCMD_WRITEST_USAGE	"[<SEG> [<wd-pattern>]]" 
#define FCMD_COPY_USAGE		"<SRC> <DST> <len>" 
#define FCMD_DUMP_USAGE		"[(<SEG> | <ADDR>) [<len> [<pr_addr>]]" 
#define FCMD_TIME_USAGE		"<usecs>" 
#define FCMD_CKSUM_USAGE	"[(<SEG> | <ADDR>) [<len>]]" 
#define FCMD_SUM_R_USAGE	"<ADDR> <len>" 
#define FCMD_LOG_TEST_USAGE	"[<string> [<iteration>]]"
#endif	/* FDEBUG */

static int fl_usage(int, char **, flash_cmd_t *);

/*
 * flash commands
 */
static flash_cmd_t fl_cmds[] = {
    {"prom",	fl_prom,    FCMD_PROM_USAGE,     FCMD_CHECK},
    {"log",	fl_log,     FCMD_LOG_USAGE,   	 FCMD_CHECK},
    {"data",	fl_data,    FCMD_DATA_USAGE,   	 FCMD_CHECK},
    {"resetpds",fl_resetpds,FCMD_RESETPDS_USAGE, FCMD_CHECK},
    {"rpromflg",fl_rpromflg,FCMD_RPROMFLG_USAGE, 0},
    {"info",	fl_info,    FCMD_INFO_USAGE,     0},
    {"?",	fl_usage,   FCMD_HELP_USAGE,     0},
    {"help",	fl_usage,   FCMD_HELP_USAGE,     0},
#ifdef FDEBUG
    {"setenv",	fl_senv,    FCMD_SENV_USAGE,	 FCMD_CHECK},
    {"unsetenv",fl_uenv,    FCMD_UENV_USAGE,	 FCMD_CHECK},
    {"getenv",	fl_genv,    FCMD_GENV_USAGE,   	 FCMD_CHECK},
    {"load",	fl_load,    FCMD_LOAD_USAGE,     0},
    {"get",	fl_get,	    FCMD_GET_USAGE,      0},
    {"erase",	fl_erase,   FCMD_ERASE_USAGE,    FCMD_CHECK},
    {"pds",	fl_pds,     FCMD_PDS_USAGE,      FCMD_CHECK},
    {"writest",	fl_writest, FCMD_WRITEST_USAGE,  FCMD_CHECK},
    {"copy",	fl_copy,    FCMD_COPY_USAGE,     FCMD_CHECK},
    {"1fc",	fl_1fc,	    FCMD_1FC_USAGE,      0},
    {"1fe",	fl_1fe,	    FCMD_1FE_USAGE,      0},
    {"dump",	fl_dump,    FCMD_DUMP_USAGE,     0},
    {"time",	fl_time,    FCMD_TIME_USAGE,     0},
    {"cksum",	fl_cksum,   FCMD_CKSUM_USAGE,    0},
    {"sum-r",	fl_sum_r,   FCMD_SUM_R_USAGE,    0},
    {"log_test",fl_log_test,FCMD_LOG_TEST_USAGE, 0},
    {"power",	fl_power,   "power", 0},
    {"ttf",	fl_ttf,     "[extra | erase_only] [<count>]", 0},
#endif	/* FDEBUG */
    {"repeat",	fl_repeat,     "repeat count flash cmd ...", 0},
    { 0 },
};

/*ARGSUSED*/
static int
fl_usage(int argc, char **argv, flash_cmd_t *fcmdp)
{
	for (fcmdp = fl_cmds; fcmdp->cmd; fcmdp++)
	    FLASH_USAGE(fcmdp);
	return(0);
}

/*ARGSUSED*/
int
flash_cmd(int argc, char **argv, char **envp)
{
	flash_cmd_t *fcmdp = fl_cmds;

	if (argc < 2)
	    goto err;

	for (; fcmdp->cmd; fcmdp++) {
	    if ( !strcmp(fcmdp->cmd, argv[1])) {
		if (fcmdp->flags & FCMD_CHECK) {
		    if (!flash_ok()) {
			FPR_ERR(("flash: flash part not found at 0x%x\n",
				flash_mem_base));
			return(0);
		    }
		}
		argc -= 2;
		argv += 2;
		(void)(*fcmdp->func)(argc, argv, fcmdp);
		return(0);
	    }
	}
	FPR_ERR(("unknown flash command\n"));
err:
	return(fl_usage(argc, argv, fl_cmds));
}
#endif /* defined(IP30) */
