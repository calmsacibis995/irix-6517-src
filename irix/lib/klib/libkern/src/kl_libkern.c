#ident "$Header: "

#include <stdio.h>
#define _KERNEL 1
#include <sys/types.h>
#include <sys/pcb.h>
#undef _KERNEL
#include <stdlib.h>
#include <fcntl.h>
#include <klib/klib.h>

/**
 ** This module contains initialization functions for libkern
 **
 **/

int mem_validity_check = 0;     /* Is it ok to check for valid physmem?      */

/*
 * klib_utsname_init() -- initialize utsname struct
 */
k_ptr_t
kl_get_utsname(int flag)
{
	int size;
	kaddr_t utsname;
	k_ptr_t utsp;

	if (!(utsname = kl_sym_addr("utsname"))) {
		return((k_ptr_t)NULL);
	}
	if ((size = kl_struct_len("utsname")) == 0) {
		return((k_ptr_t)NULL);
	}
	if (!(utsp = kl_alloc_block(size, flag))) {
		return((k_ptr_t)NULL);
	}
	kl_get_block(utsname, size, utsp, "utsname");
	if (KL_ERROR) {
		kl_free_block(utsp);
		return((k_ptr_t)NULL);
	}
	return(utsp);
}

/*
 * klib_set_machine_type() -- set machine type based on utsname info
 */
static void
klib_set_machine_type(k_ptr_t utsnamep)
{
	char ip[3];

	ip[0] = CHAR(utsnamep, "utsname", "machine")[2];
	if (CHAR(utsnamep, "utsname", "machine")[3]) {
		ip[1] = CHAR(utsnamep, "utsname", "machine")[3];
		ip[2] = 0;
	}
	else {
		ip[1] = 0;
	}
	K_IP = atoi(ip);
}

/*
 * klib_set_os_revision()
 */
static void
klib_set_os_revision(k_ptr_t utsnamep)
{
	if (!strncmp(CHAR(utsnamep, "utsname", "release"), "6.5", 3)) {
		K_IRIX_REV = IRIX6_5;
	}
	else {
		K_IRIX_REV = IRIX_SPECIAL;
	}
}

/*
 * kl_set_icrashdef()
 */
int
kl_set_icrashdef(char *k_icrashdef)
{
	K_ICRASHDEF = (char *)malloc(strlen(k_icrashdef) + 1);
	if (K_ICRASHDEF == NULL) {
		fprintf(KL_ERRORFP, "out of memory mallocing %ld bytes for "
			"icrashdef\n", (long)strlen(k_icrashdef) + 1);
		return(1);
	}
	strcpy(K_ICRASHDEF, k_icrashdef);
	return(0);
}

/*
 * libkern_free()
 */
void
libkern_free(void *p)
{
	libkern_info_t *lcp = (libkern_info_t *)p;

	if (lcp->k_sysinfo) {
		free(lcp->k_sysinfo);
	}
	if (lcp->k_kerninfo) {
		free(lcp->k_kerninfo);
	}
	if (lcp->k_kerndata) {
		free(lcp->k_kerndata);
	}
	if (lcp->k_pdeinfo) {
		free(lcp->k_pdeinfo);
	}
	if (lcp->k_struct_sizes) {
		free(lcp->k_struct_sizes);
	}
	free(lcp);
}

/*
 * libkern_init()
 */
int
libkern_init()
{
	int icrash_len, size, tlen;
	kaddr_t kmp, kernel_magic, S_end, S__end;
	kaddr_t icrash_addr, taddr, paddr, naddr = 0;
	k_ptr_t icrashdef, utsnamep;

	KLP->k_libkern.k_free = libkern_free;
	KLP->k_libkern.k_data = malloc(sizeof(libkern_info_t));

	/* Allocate space for the various control structures we will be
	 * initializing (sysinfo_s, kerninfo_s, kerndata_s, and pdeinfo_s).
	 * If we haven't allocated a klib_s struct yet or any of the control 
	 * structs have already been initialized, then we have a real 
	 * problem (and need to bail right away).
	 */
	if (!KLP | (K_FLAGS & (K_SYSINFO_ALLOCED|K_KERNINFO_ALLOCED|
						   K_KERNDATA_ALLOCED|K_PDEINFO_ALLOCED))) {
		return(1);
	}

	if (!(LIBKERN_DATA->k_sysinfo = (sysinfo_t*)malloc(sizeof(sysinfo_t)))) {
		return(1);
	}
	bzero(LIBKERN_DATA->k_sysinfo, sizeof(sysinfo_t));
	K_FLAGS |= K_SYSINFO_ALLOCED;

	if (!(LIBKERN_DATA->k_kerninfo = (kerninfo_t*)malloc(sizeof(kerninfo_t)))) {
		return(1);
	}
	bzero(LIBKERN_DATA->k_kerninfo, sizeof(kerninfo_t));
	K_FLAGS |= K_KERNINFO_ALLOCED;

	if (!(LIBKERN_DATA->k_kerndata = (kerndata_t*)malloc(sizeof(kerndata_t)))) {
		return(1);
	}
	bzero(LIBKERN_DATA->k_kerndata, sizeof(kerndata_t));
	K_FLAGS |= K_KERNDATA_ALLOCED;
	
	if (!(LIBKERN_DATA->k_pdeinfo = (pdeinfo_t*)malloc(sizeof(pdeinfo_t)))) {
		return(1);
	}
	bzero(LIBKERN_DATA->k_pdeinfo, sizeof(pdeinfo_t));
	K_FLAGS |= K_PDEINFO_ALLOCED;

	/* Set the various struct sizes
	 */
	if (klib_set_struct_sizes()) {
		return(1);
	}

	/* Determine if there is a ram_offset (note that it doesn't matter 
	 * if the kl_sym_addr() function fails here, so no need to check for
	 * failure).
	 */
	K_RAM_OFFSET = kl_sym_addr("_physmem_start");

	/* Before we do anything else, we have to determine what the master 
	 * nasid is (with systems that contain more than one node). This 
	 * is necessary because, if the kernel is mapped, translation of K2 
	 * addresses will not be correct when the master nasid is non-zero. 
	 * If this is a core dump, we can just get the master nasid from the 
	 * dump header. Otherwise, we have to use the global variable 
	 * master_nasid.
	 */

	if (PTRSZ64) {
		if (K_DUMP_HDR) {
			K_MASTER_NASID = K_DUMP_HDR->dmp_master_nasid;
		}
		else if (taddr = kl_sym_addr("master_nasid")) {

			/* Although the address of master_nasid is in K2 space,
			 * it is not actually a part of the mapped K2 segment.
			 * We can treat the address as we would a K0 address and
			 * go from there. Actually, since this is a live system,
			 * we let the /dev/mem driver do the translation work of
			 * this address for us.
			 */

			short nasid;

			if (ACTIVE) {
				kl_readmem(taddr, 2, &nasid); 
				K_MASTER_NASID = nasid;
			}
			else {
				fprintf(KL_ERRORFP, 
					"\nNOTICE: master_nasid not set!\n");
			}
		}
		else if (K_IP == 27) {
			fprintf(KL_ERRORFP, 
				"\nNOTICE: master_nasid not set!\n");
		}
		else {
			/*
			 * Master Nasid is an invalid concept under other
			 * platforms. So set it equal to zero.
			 */
			K_MASTER_NASID = 0;
		}
	}

	/* Make sure we are using the right namelist (check to make sure 
	 * the kernel_magic, _end, and end match).
	 */
	S_end = kl_sym_addr("end");
	S__end = kl_sym_addr("_end");
	kmp = kl_sym_addr("kernel_magic");

	/* Because we haven't set the machine specific stuff yet, we need to
	 * manually calculate the physical address of kernel_magic and make a 
	 * call to kl_readmem() directly. We can't use kl_get_block() or 
	 * kl_virtop() as they use macros that rely on global variables that 
	 * haven't been set yet!
	 */
	if (PTRSZ64) {
		if (ACTIVE) {
			paddr = kmp - ((!K_RAM_OFFSET || ACTIVE) ? 0 : K_RAM_OFFSET);
			
			/* If this isn't a mapped kernel, we have to convert the K0
			 * virtual address to a physical address.
			 */
			if (kmp < 0xc000000000000000) {
				paddr &= 0xffffffffff;
			}
		}
		else {
			naddr = ((k_uint_t)K_MASTER_NASID) << 32;
			paddr = (kmp & 0x0000000000ffffff) | naddr;
		}
		size = 8;
	}
	else {
		paddr = ((kmp & 0x1fffffff) -
			 ((!K_RAM_OFFSET || ACTIVE) ? 0 : K_RAM_OFFSET));
		size = 4;
	}
	kl_readmem(paddr, size, &kernel_magic); 
	if (KL_ERROR && !(K_FLAGS & K_IGNORE_FLAG)) {
		fprintf(KL_ERRORFP, 
			"\n%s: error reading the kernel_magic value ", KL_PROGRAM);
		if (COMPRESSED) {
			fprintf(KL_ERRORFP, "from the vmcore image!\n");
		}
		else {
			fprintf(KL_ERRORFP, "from the core image!\n");
		}
		return(1);
	}
	if (size == 4) {
		kernel_magic = (kernel_magic >> 32);
	}
	if ((kernel_magic != S_end) && (kernel_magic != S__end) && 
		!(K_FLAGS & K_IGNORE_FLAG)) {
		fprintf(KL_ERRORFP, 
			"\n%s: namelist and corefile do not match!\n", KL_PROGRAM);
		return(1);
	}

	/* If the icrashdef symbol name and struct definition are not in
	 * the symbol table, then bail.
	 */
	if (!(icrash_addr = kl_sym_addr("icrashdef"))) {
		fprintf(KL_ERRORFP,
			"\n%s: icrashdef not in symbol table!\n", KL_PROGRAM);
		return(1);
	}
	if (!(icrash_len = kl_struct_len("icrashdef_s"))) {
		fprintf(KL_ERRORFP,
			"\n%s: icrashdef_s struct definition not available!\n", KL_PROGRAM);
		return(1);
	}

	/* Allocate space to hold the icrashdef_s struct
	 */
	icrashdef = malloc(icrash_len);
	bzero(icrashdef, icrash_len);

	/* Load in the icrashdef struct. If the K_ICRASHDEF_FLAG is set, 
	 * read in the structure from the binary file on disk. Otherwise,
	 * read it in from system memory.
	 */
	if (K_FLAGS & K_ICRASHDEF_FLAG) {
		int ifp;

		if ((ifp = open(K_ICRASHDEF, O_RDONLY)) == -1) {
			fprintf(KL_ERRORFP, "\nUnable to open icrashdef file %s.\n", 
				K_ICRASHDEF);
			return(1);
		}
		if (read(ifp, icrashdef, icrash_len) != icrash_len) {
			fprintf(KL_ERRORFP, "\nError reading in icrashdef file \'%s\'.\n",
				K_ICRASHDEF);
		}
	}
	else {

		/* Just like with kernel_magic, we have to do a manual calculation
		 * of the physical address. After we get the icrashdef struct, we 
		 * can read memory in a normal maner.
		 */
		if (PTRSZ64) {
			if (ACTIVE) {
				paddr = icrash_addr - ((!K_RAM_OFFSET || 
							ACTIVE) ? 0 : K_RAM_OFFSET);

				/* If this isn't a mapped kernel, we have to convert the K0
				 * virtual address to a physical address.
				 */
				if (icrash_addr < 0xc000000000000000) {
					paddr &= 0xffffffffff;
				}
			}
			else {
				naddr = ((k_uint_t)K_MASTER_NASID) << 32;
				paddr = (icrash_addr & 0x0000000000ffffff) | naddr;
			}
		}
		else {
			paddr = ((icrash_addr & 0x1fffffff) -
				 ((!K_RAM_OFFSET || ACTIVE) ? 0 : K_RAM_OFFSET));
		}
		kl_readmem(paddr, icrash_len, icrashdef);
	}

	/* Get the page size from the icrashdef struct
	 */
	K_PAGESZ = KL_UINT(icrashdef, "icrashdef_s", "i_pagesz");

	/* We need to make sure that the icrashdef struct actually contains
	 * data. In some core dumps, there was a page of memory saved, but
	 * it contained nothing but NULLS or bogus data. We would die later 
	 * anyway, but this allows us to die with a more meaningful error message.
	 */
	if (!K_PAGESZ || 
		((K_PAGESZ != 4096) && (K_PAGESZ != 16384))) {
		fprintf(KL_ERRORFP, "\npagesz (%d) is invalid!\n", K_PAGESZ);
		fprintf(KL_ERRORFP, "namelist and corefile may not match!\n");
		return(1);
	}

	/* Now get the rest of the platform specific stuff from the icrashdef
	 * struct
	 */
	K_PNUMSHIFT    = KL_UINT(icrashdef, "icrashdef_s", "i_pnumshift");
	K_USIZE        = KL_UINT(icrashdef, "icrashdef_s", "i_usize");
	K_UPGIDX       = KL_UINT(icrashdef, "icrashdef_s", "i_upgidx");
	K_KSTKIDX      = KL_UINT(icrashdef, "icrashdef_s", "i_kstkidx");
	K_KUBASE       = KL_UINT(icrashdef, "icrashdef_s", "i_kubase");
	K_KUSIZE       = KL_UINT(icrashdef, "icrashdef_s", "i_kusize");
	K_K0BASE       = KL_UINT(icrashdef, "icrashdef_s", "i_k0base");
	K_K0SIZE       = KL_UINT(icrashdef, "icrashdef_s", "i_k0size");
	K_K1BASE       = KL_UINT(icrashdef, "icrashdef_s", "i_k1base");
	K_K1SIZE       = KL_UINT(icrashdef, "icrashdef_s", "i_k1size");
	K_K2BASE       = KL_UINT(icrashdef, "icrashdef_s", "i_k2base");
	K_K2SIZE       = KL_UINT(icrashdef, "icrashdef_s", "i_k2size");
	K_TO_PHYS_MASK = KL_UINT(icrashdef, "icrashdef_s", "i_tophysmask");
	K_KPTEBASE     = KL_UINT(icrashdef, "icrashdef_s", "i_kptebase");
	K_KERNSTACK 	= KL_UINT(icrashdef, "icrashdef_s", "i_kernstack");
	if (PTRSZ32) {
		/* Have to strip off the high order bits for these...
		 */
		K_KPTEBASE = (K_KPTEBASE & 0xffffffff);
		K_KERNSTACK = (K_KERNSTACK & 0xffffffff);
	}
	K_PFN_MASK     = KL_UINT(icrashdef, "icrashdef_s", "i_pfn_mask");
	K_PDE_PG_CC    = KL_UINT(icrashdef, "icrashdef_s", "i_pde_pg_cc");
	K_PDE_PG_M     = KL_UINT(icrashdef, "icrashdef_s", "i_pde_pg_m");
	K_PDE_PG_N     = KL_UINT(icrashdef, "icrashdef_s", "i_pde_pg_n");
	K_PDE_PG_D     = KL_UINT(icrashdef, "icrashdef_s", "i_pde_pg_d");
	K_PDE_PG_VR    = KL_UINT(icrashdef, "icrashdef_s", "i_pde_pg_vr");
	K_PDE_PG_G     = KL_UINT(icrashdef, "icrashdef_s", "i_pde_pg_g");
	K_PDE_PG_NR    = KL_UINT(icrashdef, "icrashdef_s", "i_pde_pg_nr");
	K_PDE_PG_SV    = KL_UINT(icrashdef, "icrashdef_s", "i_pde_pg_sv");
	K_PDE_PG_EOP   = KL_UINT(icrashdef, "icrashdef_s", "i_pde_pg_eop");

	/* Gather info on mapped kernel addresses
	 */
	K_MAPPED_RO_BASE = KL_UINT(icrashdef, "icrashdef_s",
					"i_mapped_kern_ro_base");
	K_MAPPED_RW_BASE = KL_UINT(icrashdef, "icrashdef_s",
					"i_mapped_kern_rw_base");
	K_MAPPED_PAGE_SIZE = KL_UINT(icrashdef, "icrashdef_s",
					  "i_mapped_kern_page_size");

	free(icrashdef);

	/* Set the maximum physical memory size
	 */
	K_MAXPHYS = K_TO_PHYS_MASK;
	K_MAXPFN = K_MAXPHYS >> K_PNUMSHIFT;

	/* Determine the shift values for the pte related stuff.
	 */
	K_PFN_SHIFT = kl_shift_value(K_PFN_MASK);
	K_PG_CC_SHIFT = kl_shift_value(K_PDE_PG_CC);
	K_PG_M_SHIFT = kl_shift_value(K_PDE_PG_M);
	K_PG_VR_SHIFT = kl_shift_value(K_PDE_PG_VR);
	K_PG_G_SHIFT = kl_shift_value(K_PDE_PG_G);
	K_PG_NR_SHIFT = kl_shift_value(K_PDE_PG_NR);
	K_PG_D_SHIFT = kl_shift_value(K_PDE_PG_D);
	K_PG_SV_SHIFT = kl_shift_value(K_PDE_PG_SV);
	K_PG_EOP_SHIFT = kl_shift_value(K_PDE_PG_EOP);

	/* Check the size of the pde struct here. If we can't determine the
	 * size of a pde then we WILL be blowing up shortly, so return now.
	 */
	if (PDE_SIZE <= 0) {
		fprintf(KL_ERRORFP, "\nERROR: Could not figure out size of pde.!\n");
		return(1);
	}
	
	K_REGSZ = 8;		     		/* We only support 64-bit cpus  */

	K_NBPW = (K_PTRSZ / 8);    		/* number of bytes per word  */
	K_NBPC = K_PAGESZ;	     		/* number of bytes per 'click'  */
	K_NCPS = (K_NBPC / PDE_SIZE);   /* # of clicks per segment */
	K_NBPS = (K_NCPS * K_NBPC); 	/* number of bytes per segment */

	K_KPTE_USIZE = (K_KUSIZE / K_NCPS * K_NBPC);
	K_KPTE_SHDUBASE = (K_KPTEBASE - K_KPTE_USIZE);

	/* kernstack is the top address of the kernel stack.  We need the
	 * start address in all the trace related routines.
	 */
	K_KERNELSTACK = K_KERNSTACK - K_PAGESZ;

	/* Check to see if this kernel supports stack extensions (should be
	 * for 32 bit kernels only).
	 */
	if (kl_sym_addr("Stackext_freecnt")) {
		K_EXTUSIZE++;
		K_EXTSTKIDX = K_KSTKIDX + 1;
		K_KEXTSTACK = K_KERNELSTACK - K_PAGESZ;
	}

	/* Initialize key global values 
	 */
	if (taddr = kl_sym_addr("physmem")) {
		kl_get_block(taddr, 4, &K_PHYSMEM, "physmem");
	}
	if (taddr = kl_sym_addr("numcpus")) {
		kl_get_block(taddr, 4, &K_NUMCPUS, "numcpus");
	}

	/* Determine what type of panic this is...
	 */
	if (ACTIVE) {
		/* There is no panic, we're looking at the live system...
		 */
		if (taddr = kl_sym_addr("maxcpus")) {
			kl_get_block(taddr, 4, &K_MAXCPUS, "maxcpus");
		}
		K_PANIC_TYPE = no_panic;
	}
	else {
		/* If a platform supports NMIs, we have to check and see if the
		 * maxcpus value has been moved to nmi_maxcpus (a side effect of
		 * an NMI dump). We also need to set the panic type as nmi_panic.
		 */
		if (taddr = kl_sym_addr("nmi_maxcpus")) {
			kl_get_block(taddr, 4, &K_MAXCPUS, "nmi_maxcpus");
			if (K_MAXCPUS == 0) {

				/* This wasn't an NMI dump, so maxcpus is valid. 
				 */
				if (taddr = kl_sym_addr("maxcpus")) {
					kl_get_block(taddr, 4, &K_MAXCPUS, "maxcpus");
					K_PANIC_TYPE = reg_panic;
				}
			}
			else {
				K_PANIC_TYPE = nmi_panic;
			}
		}
		else if (taddr = kl_sym_addr("maxcpus")) {
			kl_get_block(taddr, 4, &K_MAXCPUS, "numcpus");
			K_PANIC_TYPE = reg_panic;
		}
	}

	if (taddr = kl_sym_addr("numnodes")) {
		kl_get_block(taddr, 4, &K_NUMNODES, "numnodes");
	}
	if (taddr = kl_sym_addr("maxnodes")) {
		kl_get_block(taddr, 4, &K_MAXNODES, "maxnodes");
	}
	if (taddr = kl_sym_addr("nasid_shift")) {
		kl_get_block(taddr, 4, &K_NASID_SHIFT, "nasid_shift");
	}
	if (taddr = kl_sym_addr("nasid_bitmask")) {
		kl_get_block(taddr, 8, &K_NASID_BITMASK, "nasid_bitmask");
	}
	if (taddr = kl_sym_addr("slot_shift")) {
		kl_get_block(taddr, 4, &K_SLOT_SHIFT, "slot_shift");
	}
	if (taddr = kl_sym_addr("slot_bitmask")) {
		kl_get_block(taddr, 8, &K_SLOT_BITMASK, "slot_bitmask");
	}
	if (taddr = kl_sym_addr("parcel_shift")) {
		kl_get_block(taddr, 4, &K_PARCEL_SHIFT, "parcel_shift");
	}
	if (taddr = kl_sym_addr("parcel_bitmask")) {
		kl_get_block(taddr, 4, &K_PARCEL_BITMASK, "parcel_bitmask");
	}
	if (taddr = kl_sym_addr("slots_per_node")) {
		kl_get_block(taddr, 4, &K_SLOTS_PER_NODE, "slots_per_node");
	}
	if (taddr = kl_sym_addr("parcels_per_slot")) {
		kl_get_block(taddr, 4, 
				 &K_PARCELS_PER_SLOT, "parcels_per_slot");
	}
	if (K_NASID_SHIFT) {
		K_MEM_PER_NODE = (k_uint_t)1 << K_NASID_SHIFT;
		K_MEM_PER_SLOT = 
			(k_uint_t)K_MEM_PER_NODE / K_SLOTS_PER_NODE;
	}

	/* Initialize the utsname struct and fill in the relevant information
	 * from there.
	 */
	if (utsnamep = kl_get_utsname(K_TEMP)) {
		klib_set_machine_type(utsnamep);
		klib_set_os_revision(utsnamep);
		kl_free_block(utsnamep);
	}
	else {
		fprintf(KL_ERRORFP, "Could not read in utsname struct\n");
		return(1);
	}

	/* Get all the useful stuff from the var struct
	 */
	if (taddr = kl_sym_addr("v")) {
		k_ptr_t varp;

		if (!(tlen = kl_struct_len("var"))) {
			return(1);
		}
		if (!(varp = kl_alloc_block(tlen, K_TEMP))) {
			return(1);
		}
		kl_get_block(taddr, tlen, varp, "var");
		K_NPROCS = KL_INT(varp, "var", "v_proc");
		kl_free_block(varp);
	}

	if (taddr = kl_sym_addr("syssegsz")) {
		kl_get_block(taddr, 4, &K_SYSSEGSZ, "syssegsz");
	}

	/* Capture the addresses of certain key pieces of kernel data
	 */
	K_ERROR_DUMPBUF = kl_sym_addr("error_dumpbuf");
	K_LBOLT = kl_sym_addr("lbolt");
	K_NODEPDAINDR = kl_sym_addr("Nodepdaindr");
	K_PDAINDR = kl_sym_addr("pdaindr");
	K_PID_ACTIVE_LIST = kl_sym_addr("pid_active_list");
	if (!K_PID_ACTIVE_LIST) {
		/* XXX - To allow things to work properly with older dumps
		 */
		K_PID_ACTIVE_LIST = kl_sym_addr("pidactive");
	}
	K_STHREADLIST = kl_sym_addr("sthreadlist");
	K_STRST = kl_sym_addr("strst");
	K_TIME = kl_sym_addr("time");
	K_XTHREADLIST = kl_sym_addr("xthreadlist");

	/* In these cases, the sym address is a pointer to the actual
	 * pointer value we want.
	 */
	K_KPTBL = kl_sym_pointer("kptbl");
	K_ACTIVEFILES = kl_sym_pointer("activefiles");
	K_MLINFOLIST = kl_sym_pointer("mlinfolist");
	K_PIDTAB = kl_sym_pointer("pidtab");
	K_PFDAT = kl_sym_pointer("pfdat");
	K_PUTBUF = kl_sym_pointer("putbuf");

	/* Determine the size of pidtab[]
	 */
	if (taddr = kl_sym_addr("pidtabsz")) {
		kl_get_block(taddr, 4, &K_PIDTABSZ, "pidtabsz");
	}

	/* Get the base pid
	 */
	if (taddr = kl_sym_addr("pid_base")) {
		kl_get_block(taddr, 2, &K_PID_BASE, "pid_base");
	}

	if (!ACTIVE) {
		int cpu;
		k_ptr_t pdap;

		/* Try and load the kernel page table (for faster lookups).
		 * Note that this does NOT make sense when analyzing a live
		 * system (the target keeps moving).
		 */
		K_KPTBLP = malloc(K_SYSSEGSZ * PDE_SIZE);
		if (!K_KPTBLP) {
			fprintf(KL_ERRORFP, "error allocating kptbl\n");
			if (!(K_FLAGS & K_IGNORE_FLAG)) {
				return(1);
			}
		}
		kl_get_block(K_KPTBL, (K_SYSSEGSZ * PDE_SIZE), 
				 K_KPTBLP, "pde in kptbl");
		if (KL_ERROR) {
			fprintf(KL_ERRORFP, "error initializing kptbl\n");
			if (!(K_FLAGS & K_IGNORE_FLAG)) {
				return(1);
			}
		}

		/* Get the pointer to the tlb dump area plus the number of TLB
		 * entries and enttry size. Then calculate the size of the total
		 * TLB dump area (for all CPUs). Note that this can only be done
		 * after we have determined what type of system this is.
		 */
		if ((K_IP == 19) || (K_IP == 22) || (K_IP == 20)) {

			/* The R4000 chip has 48 TLB entries. Each TLB entry maps
			 * two consecutive virtual pages to physical pages.
			 */
			K_NTLBENTRIES = 48;
			K_TLBENTRYSZ = (K_REGSZ + (2 * PDE_SIZE));
		}
		else if (K_IP == 21 || K_IP == 26) {

			/* The TFP chip has 384 (128 * 3) TLB entries. There is a
			 * one to one mapping of virtual pages to physical pages.
			 */
			K_NTLBENTRIES = 128;
			K_TLBENTRYSZ = (2 * K_REGSZ * 3);
		}
		else if ((K_IP == 25) || (K_IP == 28) || (K_IP == 27) || 
				 (K_IP == 30) || (K_IP == 32) || (K_IP == 29)) {

			/* The R10000 chip has 64 TLB entries. Each TLB entry 
			 * maps two consecutive virtual pages to physical pages.
			 */
			K_NTLBENTRIES = 64;
			if (K_IP == 32 && (taddr = kl_sym_addr("is_r4600_flag"))) {
				int is_flag;
				
				kl_get_block(taddr, 4, &is_flag, "is_r4600_flag");
				if(is_flag) {
					K_NTLBENTRIES = 48;
				}
			}
			K_TLBENTRYSZ = (K_REGSZ + (2 * PDE_SIZE));
		}
		K_TLBDUMPSIZE = (K_NTLBENTRIES * K_TLBENTRYSZ) + K_REGSZ;

		/* Get a copy of dumpregs
		 */
		if (taddr = kl_sym_addr("dumpregs")) {
			K_DUMPREGS = malloc(NJBREGS * K_REGSZ);
			kl_get_block(taddr, (NJBREGS * K_REGSZ), K_DUMPREGS, "dumpregs");
		}

		/* See if there was a dumpproc. If there was, get the
		 * address of the proc.
		 */
		if (taddr = kl_sym_addr("dumpproc")) {

			kl_get_kaddr(taddr, (void*)&K_DUMPPROC, "dumpproc");
			if (K_DUMPPROC) {
				k_ptr_t dproc;
				kaddr_t kthread;

				/* Perform a little sanity check to make sure that
				 * dumpproc is valid. Note that we shouldn't fail just
				 * because there is some problem with dumpproc. We are
				 * fairly late in the initialization stage and it just
				 * might be that the rest of the dump is OK. If not,
				 * we'll blow up anyway later on. :]
				 */
				if ((kaddr_t)K_DUMPPROC < K_K0BASE) {
					fprintf(KL_ERRORFP,
						"\nERROR: dumpproc = 0x%llx. This is not a "
						"valid kernel address!\n", K_DUMPPROC);
					fprintf(KL_ERRORFP,
						"%s may be corrupted!\07\n", K_NAMELIST);
					K_DUMPPROC = 0;
				}
			}
		}

		/* Determine which CPU initiated the system panic/dump. This
		 * is mostly done so that virtop() can properly translate 
		 * the kernelstack address for dumpproc when there are more
		 * than two kthreads associated with it (unlikely but possible).
		 */
		K_DUMPCPU = -1;
		pdap = kl_alloc_block(PDA_S_SIZE, K_TEMP);
		for (cpu = 0; cpu < K_MAXCPUS; cpu++) {

			/* Get the pda_s for this cpu and see if the cpu was the
			 * one that initiated the panic.
			 */
			kl_get_pda_s((kaddr_t)cpu, pdap);
			if (KL_ERROR) {
				continue;
			}
			if (KL_UINT(pdap, "pda_s", "p_panicking")) {

				/* This is a pda for a cpu that was panicking. We
				 * need to check and see if this is the cpu that
				 * actually initiated the dump. We check the
				 * p_va_panicspin field in the pda_s struct to
				 * make sure that this is the one (zero means it
				 * is).
				 */
				if (KL_INT(pdap, "pda_s", "p_va_panicspin")) {
					continue;
				}

				K_DUMPCPU = cpu;
				K_DUMPKTHREAD = kl_kaddr(pdap, "pda_s", "p_curkthread");

				/* This SHOULD be the CPU we want. If there is a
				 * dumpproc, we need to make sure that curkthread
				 * for this CPU relates to it (just in case...).
				 */
				if (K_DUMPPROC) {
					int kcnt;
					k_ptr_t procp, utp;
					kaddr_t kthread;

					/* Just in case, we have to make sure that we were
					 * able to get both dumpproc and dumpkthread structs
					 * from the dump.
					 */
					procp = kl_get_proc(K_DUMPPROC, 2, 0);
					if (!KL_ERROR) {
						utp = kl_get_uthread_s(K_DUMPKTHREAD, 2, 0);
					}
					if (KL_ERROR) {
						K_DUMPPROC = 0;
						K_DUMPKTHREAD = 0;
					}
					else {
						if (KL_INT(procp, "proc", "p_pid") !=
									kl_uthread_to_pid(utp)) {
							fprintf(KL_ERRORFP, "WARNING: curkthread "
								"(0x%llx) for panicing CPU and dumpproc "
								"(0x%llx) have different PIDs!\n", 
								K_DUMPKTHREAD, K_DUMPPROC);
						}
						kl_free_block(procp);
						kl_free_block(utp);
					}
				}
				break;
			}
		}
		kl_free_block(pdap);

		/* At this point, we should have a valid dumpkthread
		 * (if there was one). We now need to set defkthread 
		 * equal to it.
		 */
		if (K_DUMPKTHREAD) {
			if (kl_set_defkthread(K_DUMPKTHREAD)) {
				fprintf(KL_ERRORFP, "\nNOTICE: Could not set defkthread\n");
			}
		}
	}

	kl_init_hwgraph();

#ifdef MEMCHECK
	if (K_IP == 27) {
		if (!map_node_memory) {

			/* Hack alert! We have to set this flag to allow the checking
			 * for valid physical addresses. We just trust that the
			 * addresses we have translated, read, etc. are OK. This is
			 * necessary because the kl_virtop() routine (which calls
			 * valid_physmem() is called before all the necessary global
			 * variables are set.
			 */
			mem_validity_check = 1;
		}
		else {

			/* If there was an error, print an error message. By leaving
			 * mem_validity_check == 0, it MIGHT be possible to get
			 * something useful from the dump (although it is doubtful
			 * since we failed to successfully read the hwgraph and
			 * that indicates some sort of fundamental problem with the
			 * dump).
			 */
			fprintf(KL_ERRORFP, "\nNOTICE: There was a problem mapping node "
				"memory. Physical memory validation\n");
			fprintf(KL_ERRORFP, "        has been turned off.\n");
		}
	}
#endif /* MEMCHECK */
	return(0);
}
