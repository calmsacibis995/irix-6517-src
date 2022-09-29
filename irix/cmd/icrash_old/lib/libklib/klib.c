#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libklib/RCS/klib.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <stdio.h>
#define _KERNEL 1
#include <sys/types.h>
#include <sys/pcb.h>
#undef _KERNEL
#include <stdlib.h>
#include <fcntl.h>
#include <elf.h>
#include <errno.h>

#include "klib.h"
#include "klib_extern.h"

/**
 ** This module contains initialization functions for libklib
 **
 **/

#define IS_ELF32(hdr)   (IS_ELF(hdr) && (hdr).e_ident[EI_CLASS] == ELFCLASS32)

int mem_validity_check = 0;     /* Is it ok to check for valid physmem?      */
k_uint_t klib_debug;	/* Global debug flag used by klib routines and app */

/*
 * free_klib()
 */
void
free_klib(klib_t *klp) 
{
	if (K_NAMELIST(klp)) {
		free(K_NAMELIST(klp));
	}
	if (K_COREFILE(klp)) {
		free(K_COREFILE(klp));
	}
	if (K_DUMP_HDR(klp)) {
		free((void*)K_DUMP_HDR(klp));
	}
	if (K_CORE_FD(klp)) {
		close(K_CORE_FD(klp));
	}
	free(klp);
}

/*
 * klib_check_dump_size() 
 *
 *   Check to make sure the size of the dump header is valid
 */
int
klib_check_dump_size(klib_t *klp)
{
    /*
     * Here we see if we should ignore the OS, or if the
     * page size is undetermined.
     */
    if (COMPRESSED(klp) && (K_DUMP_HDR(klp)->dmp_pages == 1)) 
    {
	    KL_SET_ERROR(KLE_SHORT_DUMP);		
	    return(1);
    }
    return(0);
}


/*
 * klib_open()
 */
klib_t *
klib_open(char *k_namelist, char *k_corefile, char *k_program, 
	char *k_icrashdef, int32 rwflag, int flags)
{
	int core_fd;
	klib_t *klp;

	klp = (klib_t *)malloc(sizeof(klib_t));
	if (!klp) {
		fprintf(stderr,"out of memory! malloc failed on %d bytes!",
			(int)sizeof(klib_t));
		return((klib_t*)NULL);
	}
	bzero(klp, sizeof(*klp));

	/* We need to set KL_ERRORFP first thing, just in case there is an
	 * error during startup. We set it equal to stderr - it can be
	 * modified by the application using libklib.
	 */
	KL_ERRORFP = stderr;

	if (flags & K_IGNORE_FLAG) {
		K_FLAGS(klp) = (flags|K_IGNORE_MEMCHECK);
	}
	else {
		K_FLAGS(klp) = flags;
	}

	/* Make sure we were passed pointers to both k_namelist and k_corefile
	 */
	if (k_namelist == NULL) {
		fprintf(KL_ERRORFP, "NULL pointer provided as namelist name\n");
		free_klib(klp);
		return((klib_t*)NULL);
	}
	if (k_corefile == NULL) {
		fprintf(KL_ERRORFP,"NULL pointer provided as corefile name\n");
		return((klib_t*)NULL);
	}

	/* Copy k_namelist, k_corefile, and icrashdef to klp
	 */
	K_NAMELIST(klp) = (char *)malloc(strlen(k_namelist) + 1);
	if (K_NAMELIST(klp) == NULL) {
		fprintf(KL_ERRORFP, "out of memory mallocing %ld bytes for name\n",
				(long)strlen(k_namelist) + 1);
		free_klib(klp);
		return((klib_t*)NULL);
	}
	strcpy(K_NAMELIST(klp), k_namelist);

	K_COREFILE(klp) = (char *)malloc(strlen(k_corefile) + 1);
	if (K_COREFILE(klp) == NULL) {
		fprintf(KL_ERRORFP, "out of memory mallocing %ld bytes for "
			"corefile\n", (long)strlen(k_namelist) + 1);
		free_klib(klp);
		return((klib_t*)NULL);
	}
	strcpy(K_COREFILE(klp), k_corefile);

	K_PROGRAM(klp) = (char *)malloc(strlen(k_program) + 1);
	if (K_PROGRAM(klp) == NULL) {
		fprintf(KL_ERRORFP, "out of memory mallocing %ld bytes for name\n",
				(long)strlen(k_program) + 1);
		free_klib(klp);
		return((klib_t*)NULL);
	}
	strcpy(K_PROGRAM(klp), k_program);

	/* Only copy the icrashdef file name if the K_ICRASHDEF_FLAG is set
	 */
	if (K_FLAGS(klp) & K_ICRASHDEF_FLAG) {
		K_ICRASHDEF(klp) = (char *)malloc(strlen(k_icrashdef) + 1);
		if (K_ICRASHDEF(klp) == NULL) {
			fprintf(KL_ERRORFP, "out of memory mallocing %ld bytes for "
				"icrashdef\n", (long)strlen(k_icrashdef) + 1);
			free_klib(klp);
			return((klib_t*)NULL);
		}
		strcpy(K_ICRASHDEF(klp), k_icrashdef);
	}

	/* Check to see if k_corefile references a core file on disk, or,
	 * if it references /dev/mem or /dev/kmem. If we are trying to 
	 * open a core file, make sure the open rwflag is O_RDONLY. If 
	 * it's O_RDWR, don't fail. Just chnge it to O_RDONLY (and print 
	 * a warning message).
	 *
	 * XXX - Is there a reason why we would want to use /dev/kmem? It 
	 *       seems as if it makes the most sense to use /dev/mem instead 
	 *       (or is there even a difference).
	 */
	if ((strcmp(k_corefile, "/dev/mem") == 0) || 
	    (strcmp(k_corefile, "/dev/kmem") == 0)) {

		K_TYPE(klp) = dev_kmem; 
		K_RW_FLAG(klp) = rwflag; 
		if ((core_fd = open(k_corefile, rwflag)) == -1) {
			int myerr = errno;
			fprintf(KL_ERRORFP,
				"Cannot open corefile %s : %s\n", 
				k_corefile, strerror(myerr));
			free_klib(klp);
			return((klib_t*)NULL);
		}
		K_CORE_FD(klp) = core_fd;
	} 
	else {
		if (rwflag != O_RDONLY) {
			/*
			fprintf(KL_ERRORFP,"Can't open %s in read-only mode only\n", 
				k_corefile);
			*/
			rwflag = O_RDONLY;
		}

		K_RW_FLAG(klp) = rwflag;
		if ((core_fd = open(k_corefile, rwflag)) == -1) {
			int myerr = errno;
			fprintf(KL_ERRORFP,
				"Cannot open corefile %s : %s\n", 
				k_corefile, strerror(myerr));
			free_klib(klp);
			return((klib_t*)NULL);
		}

		/* Make a copy of the dump_hdr and link it into klp
		 */
		K_DUMP_HDR(klp) = (dump_hdr_t*)malloc(sizeof(dump_hdr_t));
		if (K_DUMP_HDR(klp) == NULL) {
			fprintf(KL_ERRORFP,"out of memory! malloc failed on %d bytes!\n",
				(int)sizeof(dump_hdr_t));
			free_klib(klp);
			return((klib_t*)NULL);
		}

		/* Attempt to read in a compressed dump header 
		 */
		if (read(core_fd, (void *)K_DUMP_HDR(klp), 
			 sizeof(dump_hdr_t)) < sizeof(dump_hdr_t)) 
		{
			fprintf(KL_ERRORFP,
				"error: Short core file (less than %d bytes long)\n",
				(int)sizeof(dump_hdr_t));
			free_klib(klp);
			return((klib_t*)NULL);
		}

		/* Save the file descriptor for the open corefile
		 */
		K_CORE_FD(klp) = core_fd;

		/* Check to see if DUMP_MAGIC is set. If it is, then this is
		 * a compressed dump. Otherwise, it is a regular (uncompressed) 
		 * dump.
		 */
		if (K_DUMP_HDR(klp)->dmp_magic == DUMP_MAGIC) {

			/* Now make sure we can read the dump header, check
			 * and see if the pointers are valid in size, and
			 * setup the compressed read sets.
			 */

			if (cmpinit(klp, K_CORE_FD(klp), (char *)NULL, 0) < 0) {
				fprintf(KL_ERRORFP,"unable to initialize compressed dump!\n");
				free_klib(klp);
				return((klib_t*)NULL);
			}

			K_TYPE(klp) = cmp_core;
		}
		else {
			K_TYPE(klp) = reg_core;
		}
	
	        /*
	         * After we have a reasonable confidence in the header,
		 * we have to check if we are dealing with the old dump
	         * (version < 4).
		 * If so, the header is one word shorter and it is neccessary
		 * to backspace the dump file by one word.
	         */

		fprintf(KL_ERRORFP, "corefile v%d",
			K_DUMP_HDR(klp)->dmp_version);
		if (K_DUMP_HDR(klp)->dmp_version < 4) 
		    	lseek(core_fd, -sizeof(int), SEEK_CUR);

		if (klib_check_dump_size(klp)) {
			return(klp);
		}

	}
	return (klp);
}

/*
 * klib_utsname_init() -- initialize utsname struct
 */
int
klib_utsname_init(klib_t *klp)
{
	int size;
	kaddr_t utsname;
	void *utsp;

    if (K_SYM_ADDR(klp)((void*)NULL, "utsname", &utsname) < 0) {
        return(1);
    }
    if (K_STRUCT_LEN(klp)((void*)NULL, "utsname", &size) < 0) {
        return(1);
    }
	if (!(utsp = malloc(size))) {
		return(1);
	}
    kl_get_block(klp, utsname, size, utsp, "utsname");
	K_UTSNAME(klp) = utsp;
	return(0);
}

/*
 * klib_set_machine_type() -- set machine type based on utsname info
 */
void
klib_set_machine_type(klib_t *klp)
{
    char ip[3];

	ip[0] = CHAR(klp, K_UTSNAME(klp), "utsname", "machine")[2];
	if (CHAR(klp, K_UTSNAME(klp), "utsname", "machine")[3]) {
		ip[1] = CHAR(klp, K_UTSNAME(klp), "utsname", "machine")[3];
		ip[2] = 0;
	}
	else {
		ip[1] = 0;
	}
	K_IP(klp) = atoi(ip);
}

/*
 * klib_set_os_revision()
 */
void
klib_set_os_revision(klib_t *klp)
{
	if (!strncmp(CHAR(klp, K_UTSNAME(klp), "utsname", "release"), "6.2", 3)) {
		K_IRIX_REV(klp) = IRIX6_2;
	}
	else if (!strncmp(CHAR(klp, K_UTSNAME(klp), 
			       "utsname", "release"), "6.3", 3)) {
		K_IRIX_REV(klp) = IRIX6_3;
	}
	else if (!strncmp(CHAR(klp, K_UTSNAME(klp), 
			       "utsname", "release"), "6.4", 3)) {
		K_IRIX_REV(klp) = IRIX6_4;
	}
	else if (!strncmp(CHAR(klp, K_UTSNAME(klp), 
			       "utsname", "release"), "6.5", 3)) {
		K_IRIX_REV(klp) = IRIX6_5;
	}
	else {
		K_IRIX_REV(klp) = IRIX_SPECIAL;
	}
}

/*
 * klib_set_struct_sizes() -- Set the struct sizes for key kernel structures
 */
void
klib_set_struct_sizes(klib_t *klp)
{
    AVLNODE_SIZE(klp) = kl_struct_len(klp, "avlnode");
    BHV_DESC_SIZE(klp) = kl_struct_len(klp, "bhv_desc");
    CFG_DESC_SIZE(klp) = kl_struct_len(klp, "cfg_desc");
    CRED_SIZE(klp) = kl_struct_len(klp, "cred");
    DOMAIN_SIZE(klp) = kl_struct_len(klp, "domain");
    EFRAME_S_SIZE(klp) = kl_struct_len(klp, "eframe_s");
    EXCEPTION_SIZE(klp) = kl_struct_len(klp, "exception");
    FDT_SIZE(klp) = kl_struct_len(klp, "fdt");
    GRAPH_S_SIZE(klp) = kl_struct_len(klp, "graph_s");
    GRAPH_VERTEX_GROUP_S_SIZE(klp) = kl_struct_len(klp, "graph_vertex_group_s");
    GRAPH_VERTEX_S_SIZE(klp) = kl_struct_len(klp, "graph_vertex_s");
    GRAPH_EDGE_S_SIZE(klp) = kl_struct_len(klp, "graph_edge_s");
    GRAPH_INFO_S_SIZE(klp) = kl_struct_len(klp, "graph_info_s");
    IN_ADDR_SIZE(klp) = kl_struct_len(klp, "inaddrpair");
    INVENT_MEMINFO_SIZE(klp) = kl_struct_len(klp, "invent_meminfo");
    INODE_SIZE(klp) = kl_struct_len(klp, "inode");
    INPCB_SIZE(klp) = kl_struct_len(klp, "inpcb");
#ifdef ANON_ITHREADS
    ITHREAD_S_SIZE(klp) = kl_struct_len(klp, "ithread_s");
#endif
    KNA_SIZE(klp) = kl_struct_len(klp, "kna");
    KNETVEC_SIZE(klp) = kl_struct_len(klp, "knetvec");
    KTHREAD_SIZE(klp) = kl_struct_len(klp, "kthread");
    LF_LISTVEC_SIZE(klp) = kl_struct_len(klp, "lf_listvec");
    MBUF_SIZE(klp) = kl_struct_len(klp, "mbuf");
    ML_INFO_SIZE(klp) = kl_struct_len(klp, "ml_info");
	MRLOCK_S_SIZE(klp) = kl_struct_len(klp, "mrlock_s");
    if (!(ML_SYM_SIZE(klp) = kl_struct_len(klp, "ml_sym"))) 
    {
	    /* XXX -- We have to do this because the ml_sym struct info
	     * has not been included in the kernel symbol table.
	     */
	    if (PTRSZ64(klp)) {
		    ML_SYM_SIZE(klp) = 16;
	    }
	    else {
		    ML_SYM_SIZE(klp) = 8;
	    }
    }
    MODULE_INFO_SIZE(klp) = kl_struct_len(klp, "module_info");
    NODEPDA_S_SIZE(klp) = kl_struct_len(klp, "nodepda_s");
    PDA_S_SIZE(klp) = kl_struct_len(klp, "pda_s");
    PFDAT_SIZE(klp) = kl_struct_len(klp, "pfdat");
    PID_ENTRY_SIZE(klp) = kl_struct_len(klp, "pid_entry");
    PID_SLOT_SIZE(klp) = kl_struct_len(klp, "pid_slot");
    PMAP_SIZE(klp) = kl_struct_len(klp, "pmap");
    PREGION_SIZE(klp) = kl_struct_len(klp, "pregion");
    PROC_SIZE(klp) = kl_struct_len(klp, "proc");
    PROC_PROXY_SIZE(klp) = kl_struct_len(klp, "proc_proxy");
    PROTOSW_SIZE(klp) = kl_struct_len(klp, "protosw");
    QINIT_SIZE(klp) = kl_struct_len(klp, "qinit");
    QUEUE_SIZE(klp) = kl_struct_len(klp, "queue");
    REGION_SIZE(klp) = kl_struct_len(klp, "region");
    RNODE_SIZE(klp) = kl_struct_len(klp, "rnode");
    SEMA_SIZE(klp) = kl_struct_len(klp, "sema_s");
    LSNODE_SIZE(klp) = kl_struct_len(klp, "lsnode");
    CSNODE_SIZE(klp) = kl_struct_len(klp, "csnode");
    SOCKET_SIZE(klp) = kl_struct_len(klp, "socket");
    STDATA_SIZE(klp) = kl_struct_len(klp, "stdata");
    STHREAD_S_SIZE(klp) = kl_struct_len(klp, "sthread_s");
    STRSTAT_SIZE(klp) = kl_struct_len(klp, "strstat");
    SWAPINFO_SIZE(klp) = kl_struct_len(klp, "swapinfo");
    TCPCB_SIZE(klp) = kl_struct_len(klp, "tcpcb");
    UFCHUNK_SIZE(klp) = kl_struct_len(klp, "ufchunk");
    UNPCB_SIZE(klp) = kl_struct_len(klp, "unpcb");
    UTHREAD_S_SIZE(klp) = kl_struct_len(klp, "uthread_s");
    VFS_SIZE(klp) = kl_struct_len(klp, "vfs");
    VFS_SIZE(klp) = kl_struct_len(klp, "vfs");
    VFILE_SIZE(klp) = kl_struct_len(klp, "vfile");
    VFSSW_SIZE(klp) = kl_struct_len(klp, "vfssw");
    VNODE_SIZE(klp) = kl_struct_len(klp, "vnode");
    VPRGP_SIZE(klp) = kl_struct_len(klp, "vprgp");
    VPROC_SIZE(klp) = kl_struct_len(klp, "vproc");
    VSOCKET_SIZE(klp) = kl_struct_len(klp, "vsocket");
    XFS_INODE_SIZE(klp) = kl_struct_len(klp, "xfs_inode");
    XTHREAD_S_SIZE(klp) = kl_struct_len(klp, "xthread_s");
    ZONE_SIZE(klp) = kl_struct_len(klp, "zone");
    PDE_SIZE(klp)  = kl_struct_len(klp, "pde");
}

/*
 * klib_init()
 */
int
klib_init(klib_t *klp)
{
	int namelist_fd, icrash_len, size, tlen, numgroups;
	kaddr_t kmp, kernel_magic, S_end, S__end;
	kaddr_t icrash_addr, taddr, paddr, naddr = 0;
	Elf32_Ehdr elf_header;
	Elf64_Ehdr elf_header64;
	void *icrashdef;

	/* Determine if this is a 32-bit or 64-bit kernel. We need to 
	 * do this here so that the compression code works properly for
	 * the calls that are made to kl_readmem() before we are completely 
	 * setup.
	 */
	if ((namelist_fd  = open(K_NAMELIST(klp), O_RDONLY)) == -1) {
		fprintf(KL_ERRORFP, "Cannot open kernel named %s\n", 
			K_NAMELIST(klp));
		free_klib(klp);
		return(1);
	}
	if (lseek(namelist_fd, 0, SEEK_SET) == -1) {
		fprintf(KL_ERRORFP, "Cannot seek to 0 for kernel named %s\n", 
			K_NAMELIST(klp));
		close(namelist_fd);
		free_klib(klp);
		return(1);
	}
	if (read(namelist_fd, &elf_header, 
		 sizeof(Elf32_Ehdr)) != sizeof(Elf32_Ehdr)) {
		fprintf(KL_ERRORFP, 
			"error: cannot read entire elf header of short kernel %s\n", 
			K_NAMELIST(klp));
		close(namelist_fd);
		free_klib(klp);
		return(1);
	}

	if (IS_ELF32(elf_header))  {
		K_PTRSZ(klp) = 32;
	} 
	else if (IS_ELF(elf_header)) {	
		K_PTRSZ(klp) = 64;
	}
	else {
		return(1);
	}
	close(namelist_fd);

	/* 
	 * Set the various struct sizes
	 */
	klib_set_struct_sizes(klp);

	/* Determine if there is a ram_offset (note that it doesn't matter 
	 * if the kl_sym_addr() function fails here, so no need to check for
	 * failure).
	 */
	K_SYM_ADDR(klp)((void*)NULL, "_physmem_start", &K_RAM_OFFSET(klp));

	/* Before we do anything else, we have to determine what the master 
	 * nasid is (with systems that contain more than one node). This 
	 * is necessary because, if the kernel is mapped, translation of K2 
	 * addresses will not be correct when the master nasid is non-zero. 
	 * If this is a core dump, we can just get the master nasid from the 
	 * dump header. Otherwise, we have to use the global variable 
	 * master_nasid.
	 */

	if (PTRSZ64(klp)) {
		if (K_DUMP_HDR(klp)) {
			K_MASTER_NASID(klp) = K_DUMP_HDR(klp)->dmp_master_nasid;
		}
		else if (K_SYM_ADDR(klp)((void*)NULL, "master_nasid", &taddr) >= 0) {

			/* Although the address of master_nasid is in K2 space,
			 * it is not actually a part of the mapped K2 segment.
			 * We can treat the address as we would a K0 address and
			 * go from there. Actually, since this is a live system,
			 * we let the /dev/mem driver do the translation work of
			 * this address for us.
			 */

			short nasid;

			if (ACTIVE(klp)) {
				kl_readmem(klp, taddr, 0, (char *)&nasid, 
					   (void *)NULL, 2, "master_nasid");
				K_MASTER_NASID(klp) = nasid;
			}
			else {
				fprintf(KL_ERRORFP, 
					"NOTICE: master_nasid not set!\n");
			}
		}
		else if (K_IP(klp) == 27) 
		{
			fprintf(KL_ERRORFP, 
				"NOTICE: master_nasid not set!\n");
		}
		else
		{
			/*
			 * Master Nasid is an invalid concept under other
			 * platforms. So clear it.
			 */
			K_MASTER_NASID(klp) = 0;
		}
	}

	/* Make sure we are using the right namelist (check to make sure 
	 * the kernel_magic, _end, and end match).
	 */
	S_end = kl_sym_addr(klp, "end");
	S__end = kl_sym_addr(klp, "_end");
	kmp = kl_sym_addr(klp, "kernel_magic");

	/* Because we haven't set the machine specific stuff yet, we need to
	 * manually calculate the physical address and make a call to kl_readmem()
	 * directly. We can't use kl_get_block() or kl_virtop() as they use 
	 * macros that rely on global variables that haven't been set yet!
	 */
	if (PTRSZ64(klp)) {
		if (ACTIVE(klp)) {
			paddr = kmp - ((!K_RAM_OFFSET(klp) || 
					ACTIVE(klp)) ? 0 : K_RAM_OFFSET(klp));
			
			/* If this isn't a mapped kernel, we have to convert the K0
			 * virtual address to a physical address.
			 */
			if (kmp < 0xc000000000000000) {
				paddr &= 0xffffffffff;
			}
		}
		else {
			naddr = ((k_uint_t)K_MASTER_NASID(klp)) << 32;
			paddr = (kmp & 0x0000000000ffffff) | naddr;
		}
		size = 8;
	}
	else {
		paddr = ((kmp & 0x1fffffff) -
			 ((!K_RAM_OFFSET(klp) || ACTIVE(klp)) ? 
			  0 : K_RAM_OFFSET(klp)));
		size = 4;
	}
	kl_readmem(klp, paddr, 0, (char*)&kernel_magic, 
		   (void *)NULL, size, "kernel_magic");
	if (KL_ERROR && !(K_FLAGS(klp) & K_IGNORE_FLAG)) {
		fprintf(KL_ERRORFP, 
			"\n%s: error reading the kernel_magic value ", K_PROGRAM(klp));
		if (COMPRESSED(klp)) {
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
	    !(K_FLAGS(klp) & K_IGNORE_FLAG)) {
		fprintf(KL_ERRORFP, 
			"\n%s: namelist and corefile do not match!\n", K_PROGRAM(klp));
		return(1);
	}

	/* If the icrashdef symbol name and struct definition are not in
	 * the symbol table, then bail.
	 */
	if (K_SYM_ADDR(klp)((void*)NULL, "icrashdef", &icrash_addr) < 0) {
		return(1);
	}
	if (K_STRUCT_LEN(klp)((void*)NULL, "icrashdef_s", &icrash_len) < 0) {
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
	if (K_FLAGS(klp) & K_ICRASHDEF_FLAG) {
		int ifp;

		if ((ifp = open(K_ICRASHDEF(klp), O_RDONLY)) == -1) {
			fprintf(KL_ERRORFP, "Unable to open icrashdef file %s.\n", 
				K_ICRASHDEF(klp));
			return(1);
		}
		if (read(ifp, icrashdef, icrash_len) != icrash_len) {
			fprintf(KL_ERRORFP, "\nError reading in icrashdef file \'%s\'.\n",
				K_ICRASHDEF(klp));
		}
	}
	else {

		/* Just like with kernel_magic, we have to do a manual calculation
		 * of the physical address. After we get the icrashdef struct, we 
		 * can read memory in a normal maner.
		 */
		if (PTRSZ64(klp)) {
			if (ACTIVE(klp)) {
				paddr = icrash_addr - ((!K_RAM_OFFSET(klp) || 
							ACTIVE(klp)) ? 0 : K_RAM_OFFSET(klp));

				/* If this isn't a mapped kernel, we have to convert the K0
				 * virtual address to a physical address.
				 */
				if (icrash_addr < 0xc000000000000000) {
					paddr &= 0xffffffffff;
				}
			}
			else {
				naddr = ((k_uint_t)K_MASTER_NASID(klp)) << 32;
				paddr = (icrash_addr & 0x0000000000ffffff) | naddr;
			}
		}
		else {
			paddr = ((icrash_addr & 0x1fffffff) -
				 ((!K_RAM_OFFSET(klp) || ACTIVE(klp)) ? 0 : K_RAM_OFFSET(klp)));
		}
		kl_readmem(klp, paddr, 0, icrashdef, 
			   (void *)NULL, icrash_len, "icrashdef_s");
	}

	/* Get the page size from the icrashdef struct
     */
	K_PAGESZ(klp) = KL_UINT(klp, icrashdef, "icrashdef_s", "i_pagesz");

	/* We need to make sure that the icrashdef struct actually contains
     * data. In some core dumps, there was a page of memory saved, but
     * it contained nothing but NULLS. We would die later anyway, but this
     * allows us to die with a more meaningful error message.
     */
	if (!K_PAGESZ(klp) || 
	    ((K_PAGESZ(klp) != 4096) && (K_PAGESZ(klp) != 16384))) {
		fprintf(KL_ERRORFP, "\npagesz (%d) is invalid!\n", K_PAGESZ(klp));
		return(1);
	}

	/* Now get the rest of the platform specific stuff from the icrashdef
     * struct
     */
	K_PNUMSHIFT(klp)    = KL_UINT(klp, icrashdef, "icrashdef_s", "i_pnumshift");
	K_USIZE(klp)        = KL_UINT(klp, icrashdef, "icrashdef_s", "i_usize");
	K_UPGIDX(klp)       = KL_UINT(klp, icrashdef, "icrashdef_s", "i_upgidx");
	K_KSTKIDX(klp)      = KL_UINT(klp, icrashdef, "icrashdef_s", "i_kstkidx");
	K_KUBASE(klp)       = KL_UINT(klp, icrashdef, "icrashdef_s", "i_kubase");
	K_KUSIZE(klp)       = KL_UINT(klp, icrashdef, "icrashdef_s", "i_kusize");
	K_K0BASE(klp)       = KL_UINT(klp, icrashdef, "icrashdef_s", "i_k0base");
	K_K0SIZE(klp)       = KL_UINT(klp, icrashdef, "icrashdef_s", "i_k0size");
	K_K1BASE(klp)       = KL_UINT(klp, icrashdef, "icrashdef_s", "i_k1base");
	K_K1SIZE(klp)       = KL_UINT(klp, icrashdef, "icrashdef_s", "i_k1size");
	K_K2BASE(klp)       = KL_UINT(klp, icrashdef, "icrashdef_s", "i_k2base");
	K_K2SIZE(klp)       = KL_UINT(klp, icrashdef, "icrashdef_s", "i_k2size");
	K_TO_PHYS_MASK(klp) = KL_UINT(klp, icrashdef, 
				      "icrashdef_s", "i_tophysmask");
	K_KPTEBASE(klp)     = KL_UINT(klp, icrashdef, "icrashdef_s", "i_kptebase");
	K_KERNSTACK(klp) 	= KL_UINT(klp, icrashdef, "icrashdef_s", "i_kernstack");
	if (PTRSZ32(klp)) {
		K_KPTEBASE(klp) = (K_KPTEBASE(klp) & 0xffffffff);
		K_KERNSTACK(klp) = (K_KERNSTACK(klp) & 0xffffffff);
	}
	K_PFN_MASK(klp)     = KL_UINT(klp, icrashdef, "icrashdef_s", "i_pfn_mask");
	K_PDE_PG_CC(klp)    = KL_UINT(klp, icrashdef, "icrashdef_s", "i_pde_pg_cc");
	K_PDE_PG_M(klp)     = KL_UINT(klp, icrashdef, "icrashdef_s", "i_pde_pg_m");
	K_PDE_PG_N(klp)     = KL_UINT(klp, icrashdef, "icrashdef_s", "i_pde_pg_n");
	K_PDE_PG_D(klp)     = KL_UINT(klp, icrashdef, "icrashdef_s", "i_pde_pg_d");
	K_PDE_PG_VR(klp)    = KL_UINT(klp, icrashdef, "icrashdef_s", "i_pde_pg_vr");
	K_PDE_PG_G(klp)     = KL_UINT(klp, icrashdef, "icrashdef_s", "i_pde_pg_g");
	K_PDE_PG_NR(klp)    = KL_UINT(klp, icrashdef, "icrashdef_s", "i_pde_pg_nr");
	K_PDE_PG_SV(klp)    = KL_UINT(klp, icrashdef, "icrashdef_s", "i_pde_pg_sv");
	K_PDE_PG_EOP(klp)   = KL_UINT(klp, icrashdef, 
				      "icrashdef_s", "i_pde_pg_eop");

	/* Code to handle mapped kernel addresses
	 */
	K_MAPPED_RO_BASE(klp) = KL_UINT(klp, icrashdef, "icrashdef_s",
					"i_mapped_kern_ro_base");
	K_MAPPED_RW_BASE(klp) = KL_UINT(klp, icrashdef, "icrashdef_s",
					"i_mapped_kern_rw_base");
	K_MAPPED_PAGE_SIZE(klp) = KL_UINT(klp, icrashdef, "icrashdef_s",
					  "i_mapped_kern_page_size");

	free(icrashdef);

	/* Set the maximum physical memory size
	 */
	K_MAXPHYS(klp) = K_TO_PHYS_MASK(klp);
	K_MAXPFN(klp) = K_MAXPHYS(klp) >> K_PNUMSHIFT(klp);

	/* Determine the shift values for the pte related stuff.
	 */
	K_PFN_SHIFT(klp) = kl_shift_value(K_PFN_MASK(klp));
	K_PG_CC_SHIFT(klp) = kl_shift_value(K_PDE_PG_CC(klp));
	K_PG_M_SHIFT(klp) = kl_shift_value(K_PDE_PG_M(klp));
	K_PG_VR_SHIFT(klp) = kl_shift_value(K_PDE_PG_VR(klp));
	K_PG_G_SHIFT(klp) = kl_shift_value(K_PDE_PG_G(klp));
	K_PG_NR_SHIFT(klp) = kl_shift_value(K_PDE_PG_NR(klp));
	K_PG_D_SHIFT(klp) = kl_shift_value(K_PDE_PG_D(klp));
	K_PG_SV_SHIFT(klp) = kl_shift_value(K_PDE_PG_SV(klp));
	K_PG_EOP_SHIFT(klp) = kl_shift_value(K_PDE_PG_EOP(klp));

	/* Check the size of the pde struct here
	 */
	if (PDE_SIZE(klp) <= 0) {
		fprintf(KL_ERRORFP, "ERROR: Could not figure out size of pde.!\n");
		return(1);
	}
    
	K_REGSZ(klp) = 8;		     /* We only support 64-bit cpus  */

	K_NBPW(klp) = (K_PTRSZ(klp) / 8);    /* number of bytes per word  */
	K_NBPC(klp) = K_PAGESZ(klp);	     /* number of bytes per 'click'  */
	K_NCPS(klp) = (K_NBPC(klp) / PDE_SIZE(klp)); /* # of clicks per segment */
	K_NBPS(klp) = (K_NCPS(klp) * K_NBPC(klp)); /* number of bytes per segment */

	K_KPTE_USIZE(klp) = (K_KUSIZE(klp) / K_NCPS(klp) * K_NBPC(klp));
	K_KPTE_SHDUBASE(klp) = (K_KPTEBASE(klp) - K_KPTE_USIZE(klp));

	/* kernstack is the top address of the kernel stack.  We need the
	 * start address in all the trace related routines.
	 */
	K_KERNELSTACK(klp) = K_KERNSTACK(klp) - K_PAGESZ(klp);

	/* Check to see if this kernel supports stack extensions (should be
	 * for 32 bit kernels only).
	 */
	if (K_SYM_ADDR(klp)((void*)NULL, "Stackext_freecnt", &taddr) >= 0) {
		K_EXTUSIZE(klp)++;
		K_EXTSTKIDX(klp) = K_KSTKIDX(klp) + 1;
		K_KEXTSTACK(klp) = K_KERNELSTACK(klp) - K_PAGESZ(klp);
	}

	/* Initialize key global values 
	 */
	if (K_SYM_ADDR(klp)((void*)NULL, "physmem", &taddr) >= 0) {
		kl_get_block(klp, taddr, 4, &K_PHYSMEM(klp), "physmem");
	}
	if (K_SYM_ADDR(klp)((void*)NULL, "numcpus", &taddr) >= 0) {
		kl_get_block(klp, taddr, 4, &K_NUMCPUS(klp), "numcpus");
	}

	/* If a platform supports NMIs, we have to check and see if the
	 * maxcpus value has been moved to nmi_maxcpus (a side effect of
	 * an NMI dump).
	 */
	if (K_SYM_ADDR(klp)((void*)NULL, "nmi_maxcpus", &taddr) >= 0) {
		kl_get_block(klp, taddr, 4, &K_MAXCPUS(klp), "nmi_maxcpus");
		if (K_MAXCPUS(klp) == 0) {

			/* This wasn't an NMI dump, so maxcpus is valid. 
			 */
			if (K_SYM_ADDR(klp)((void*)NULL, "maxcpus", &taddr) >= 0) {
				kl_get_block(klp, taddr, 4, &K_MAXCPUS(klp), "maxcpus");
				K_PANIC_TYPE(klp) = reg_panic;
			}
		}
		else {
			K_PANIC_TYPE(klp) = nmi_panic;
		}
	}
	else if (K_SYM_ADDR(klp)((void*)NULL, "maxcpus", &taddr) >= 0) {
		kl_get_block(klp, taddr, 4, &K_MAXCPUS(klp), "numcpus");
		K_PANIC_TYPE(klp) = reg_panic;
	}

	if (K_SYM_ADDR(klp)((void*)NULL, "numnodes", &taddr) >= 0) {
		kl_get_block(klp, taddr, 4, &K_NUMNODES(klp), "numnodes");
	}
	if (K_SYM_ADDR(klp)((void*)NULL, "maxnodes", &taddr) >= 0) {
		kl_get_block(klp, taddr, 4, &K_MAXNODES(klp), "maxnodes");
	}
	if (K_SYM_ADDR(klp)((void*)NULL, "nasid_shift", &taddr) >= 0) {
		kl_get_block(klp, taddr, 4, &K_NASID_SHIFT(klp), "nasid_shift");
	}
	if (K_SYM_ADDR(klp)((void*)NULL, "nasid_bitmask", &taddr) >= 0) {
		kl_get_block(klp, taddr, 8, &K_NASID_BITMASK(klp), "nasid_bitmask");
	}
	if (K_SYM_ADDR(klp)((void*)NULL, "slot_shift", &taddr) >= 0) {
		kl_get_block(klp, taddr, 4, &K_SLOT_SHIFT(klp), "slot_shift");
	}
	if (K_SYM_ADDR(klp)((void*)NULL, "slot_bitmask", &taddr) >= 0) {
		kl_get_block(klp, taddr, 8, &K_SLOT_BITMASK(klp), "slot_bitmask");
	}
	if (K_SYM_ADDR(klp)((void*)NULL, "parcel_shift", &taddr) >= 0) {
		kl_get_block(klp, taddr, 4, &K_PARCEL_SHIFT(klp), "parcel_shift");
	}
	if (K_SYM_ADDR(klp)((void*)NULL, "parcel_bitmask", &taddr) >= 0) {
		kl_get_block(klp, taddr, 4, &K_PARCEL_BITMASK(klp), "parcel_bitmask");
	}
	if (K_SYM_ADDR(klp)((void*)NULL, "slots_per_node", &taddr) >= 0) {
		kl_get_block(klp, taddr, 4, &K_SLOTS_PER_NODE(klp), "slots_per_node");
	}
	if (K_SYM_ADDR(klp)((void*)NULL, "parcels_per_slot", &taddr) >= 0) {
		kl_get_block(klp, taddr, 4, 
			     &K_PARCELS_PER_SLOT(klp), "parcels_per_slot");
	}
	if (K_NASID_SHIFT(klp)) {
		K_MEM_PER_NODE(klp) = (k_uint_t)1 << K_NASID_SHIFT(klp);
		K_MEM_PER_SLOT(klp) = 
			(k_uint_t)K_MEM_PER_NODE(klp) / K_SLOTS_PER_NODE(klp);
		K_MEM_PER_BANK(klp) = 
			(k_uint_t)K_MEM_PER_NODE(klp) / MD_MEM_BANKS;
	}

	/* Initialize the utsname struct and fill in the relevant information
	 * from there.
	 */
	if (klib_utsname_init(klp)) {
		return(-1);
	}
	else {
		char ip[3];

		klib_set_machine_type(klp);
		klib_set_os_revision(klp);

		/* Get all the useful stuff from the var struct
		 */
		if (K_SYM_ADDR(klp)((void*)NULL, "v", &taddr) >= 0) {
			void *varp;

			if (K_STRUCT_LEN(klp)((void*)NULL, "var", &tlen) < 0) {
				return(1);
			}
			if (!(varp = malloc(tlen))) {
				return(1);
			}
			kl_get_block(klp, taddr, tlen, varp, "var");
			K_NPROCS(klp) = KL_INT(klp, varp, "var", "v_proc");
			free(varp);
		}

		if (K_SYM_ADDR(klp)((void*)NULL, "syssegsz", &taddr) >= 0) {
			kl_get_block(klp, taddr, 4, &K_SYSSEGSZ(klp), "syssegsz");
		}

		/* These values represent the actual sym address
		 */
		K_ERROR_DUMPBUF(klp) = kl_sym_addr(klp, "error_dumpbuf");
#ifdef ANON_ITHREADS
		K_ITHREADLIST(klp) = kl_sym_addr(klp, "ithreadlist");
#endif
		K_LBOLT(klp) = kl_sym_addr(klp, "lbolt");
		K_NODEPDAINDR(klp) = kl_sym_addr(klp, "Nodepdaindr");
		K_PDAINDR(klp) = kl_sym_addr(klp, "pdaindr");
		K_PIDACTIVE(klp) = kl_sym_addr(klp, "pid_active_list");
		if (!K_PIDACTIVE(klp)) {
			/* XXX - So that things will work with older dumps 
			 */
			K_PIDACTIVE(klp) = kl_sym_addr(klp, "pidactive");
		}
		K_STHREADLIST(klp) = kl_sym_addr(klp, "sthreadlist");
		K_STRST(klp) = kl_sym_addr(klp, "strst");
		K_TIME(klp) = kl_sym_addr(klp, "time");
		K_XTHREADLIST(klp) = kl_sym_addr(klp, "xthreadlist");

		/* In these cases, the sym address is a pointer to the actual
		 * pointer value we want.
		 */
		K_KPTBL(klp) = kl_sym_pointer(klp, "kptbl");
		K_ACTIVEFILES(klp) = kl_sym_pointer(klp, "activefiles");
		K_MLINFOLIST(klp) = kl_sym_pointer(klp, "mlinfolist");
		K_PIDTAB(klp) = kl_sym_pointer(klp, "pidtab");
		K_PFDAT(klp) = kl_sym_pointer(klp, "pfdat");
		K_PUTBUF(klp) = kl_sym_pointer(klp, "putbuf");

		/* We have to read in the hwgraph struct, adjust graph_size by 
		 * the number of groups and then read it in again. Note that if
		 * the  hwgraph symbol doesn't exist, just skip this section.
		 */
		if (K_HWGRAPHP(klp) = kl_sym_pointer(klp, "hwgraph")) {
			init_hwgraph(klp);
			if (GRAPH_S_SIZE(klp)) {
				K_HWGRAPH(klp) = 
					klib_alloc_block(klp, GRAPH_S_SIZE(klp),
							 K_PERM);
				kl_get_struct(klp, K_HWGRAPHP(klp), 
					      GRAPH_S_SIZE(klp), K_HWGRAPH(klp),
					      "hwgraph");
				VERTEX_SIZE(klp) = 
					KL_UINT(klp, K_HWGRAPH(klp), "graph_s",
						"graph_vertex_size");
				numgroups = 
					KL_UINT(klp, K_HWGRAPH(klp), "graph_s",
						"graph_num_group");
				GRAPH_SIZE(klp) = GRAPH_S_SIZE(klp) + 
					((numgroups - 1) * 
					 GRAPH_VERTEX_GROUP_S_SIZE(klp));
				klib_free_block(klp, K_HWGRAPH(klp));
				K_HWGRAPH(klp) = 
					klib_alloc_block(klp, GRAPH_SIZE(klp), 
							 K_PERM);
				kl_get_struct(klp, K_HWGRAPHP(klp),
					      GRAPH_SIZE(klp), K_HWGRAPH(klp), 
					      "hwgraph");
			}
		}

		/* Determine the size of pidtab[]
		 */
		if (K_SYM_ADDR(klp)((void*)NULL, "pidtabsz", &taddr) >= 0) {
			kl_get_block(klp, taddr, 4, &K_PIDTABSZ(klp), "pidtabsz");
		}

		/* Get the base pid
		 */
		if (K_SYM_ADDR(klp)((void*)NULL, "pid_base", &taddr) >= 0) {
			kl_get_block(klp, taddr, 2, &K_PID_BASE(klp), "pid_base");
		}

		if (!ACTIVE(klp)) {
			int cpu;
			k_ptr_t pdap;

			/* Try and load the kernel page table (for faster lookups).
			 * Note that this does NOT make sense when analyzing a live
			 * system (the target keeps moving).
			 */
			K_KPTBLP(klp) = malloc(K_SYSSEGSZ(klp) * PDE_SIZE(klp));
			if (!K_KPTBLP(klp)) {
				fprintf(KL_ERRORFP, "error allocating kptbl\n");
				if (!(K_FLAGS(klp) & K_IGNORE_FLAG)) {
					return(1);
				}
			}
			kl_get_block(klp, K_KPTBL(klp), (K_SYSSEGSZ(klp) * PDE_SIZE(klp)), 
				     K_KPTBLP(klp), "pde in kptbl");
			if (KL_ERROR) {
				fprintf(KL_ERRORFP, "error initializing kptbl\n");
				if (!(K_FLAGS(klp) & K_IGNORE_FLAG)) {
					return(1);
				}
			}

			/* Get the pointer to the tlb dump area -- then grab the entire
			 * tlbdump. We have to do this here because we need to know which
			 * cpuboard this is.
			 */
			if ((K_IP(klp) == 19) || (K_IP(klp) == 22) ||
			    (K_IP(klp) == 20)) 
			{

				/* The R4000 chip has 48 TLB entries. Each TLB entry maps
				 * two consecutive virtual pages to physical pages.
				 */
				K_NTLBENTRIES(klp) = 48;
				K_TLBENTRYSZ(klp) = (K_REGSZ(klp) + (2 * PDE_SIZE(klp)));
			}
			else if (K_IP(klp) == 21 || K_IP(klp) == 26) {

				/* The TFP chip has 384 (128 * 3) TLB entries. There is a
				 * one to one mapping of virtual pages to physical pages.
				 */
				K_NTLBENTRIES(klp) = 128;
				K_TLBENTRYSZ(klp) = (2 * K_REGSZ(klp) * 3);
			}
			else if ((K_IP(klp) == 25) || (K_IP(klp) == 28) || 
				 (K_IP(klp) == 27) || (K_IP(klp) == 30) ||
				 (K_IP(klp) == 32) || (K_IP(klp) == 29)) 
			{

				/* 
				 * The R10000 chip has 64 TLB entries.
				 * Each TLB entry maps two consecutive
				 * virtual pages to physical pages.
				 */
				K_NTLBENTRIES(klp) = 64;
				if (K_IP(klp) == 32 &&
				    (K_SYM_ADDR(klp)((void*)NULL,
						     "is_r4600_flag",
						     &taddr) >= 0))
				{
					int is_flag;
					
					kl_get_block(klp, taddr, 4,
						     &is_flag,
						     "is_r4600_flag");
					if(is_flag)
					{
						K_NTLBENTRIES(klp) = 48;
					}
				}
				K_TLBENTRYSZ(klp) = (K_REGSZ(klp) + 
						     (2 * PDE_SIZE(klp)));
			}
			K_TLBDUMPSIZE(klp) = 
				(K_NTLBENTRIES(klp) * K_TLBENTRYSZ(klp)) + K_REGSZ(klp);

			/* Get a copy of dumpregs
			 */
			if (K_SYM_ADDR(klp)((void*)NULL, "dumpregs", &taddr) >= 0) {
				K_DUMPREGS(klp) = malloc(NJBREGS * K_REGSZ(klp));
				kl_get_block(klp, taddr, 
					     (NJBREGS * K_REGSZ(klp)), K_DUMPREGS(klp), "dumpregs");
			}

			/* See if there was a dumpproc. If there was, get the
			 * address of the proc.
			 */
			if (K_SYM_ADDR(klp)((void*)NULL, "dumpproc", &taddr) >= 0) {

				kl_get_kaddr(klp, taddr, (void*)&K_DUMPPROC(klp), "dumpproc");
				if (K_DUMPPROC(klp)) {
					k_ptr_t dproc;
					kaddr_t kthread;

					/* Perform a little sanity check to make sure that
					 * dumpproc is valid. Note that we shouldn't fail just
					 * because there is some problem with dumpproc. We are
					 * fairly late in the initialization stage and it just
					 * might be that the rest of the dump is OK. If not,
					 * we'll blow up anyway later on. :]
					 */
					if ((kaddr_t)K_DUMPPROC(klp) < K_K0BASE(klp)) {
						fprintf(KL_ERRORFP,
							"\nERROR: dumpproc = 0x%llx. This is not a "
							"valid kernel address!\n", K_DUMPPROC(klp));
						fprintf(KL_ERRORFP,
							"%s may be corrupted!\07\n", K_NAMELIST(klp));
						K_DUMPPROC(klp) = 0;
					}
				}
			}

			/* Determine which CPU initiated the system panic/dump. This
			 * is mostly done so that virtop() can properly translate 
			 * the kernelstack address for dumpproc when there are more
			 * than two kthreads associated with it (unlikely but possible).
			 */
			K_DUMPCPU(klp) = -1;
			pdap = klib_alloc_block(klp, PDA_S_SIZE(klp), K_TEMP);
			for (cpu = 0; cpu < K_MAXCPUS(klp); cpu++) {

				/* Get the pda_s for this cpu and see if the cpu was the
				 * one that initiated the panic.
				 */
				kl_get_pda_s(klp, (kaddr_t)cpu, pdap);
				if (KL_ERROR) {
					continue;
				}
				if (KL_UINT(klp, pdap, "pda_s", "p_panicking")) {

					/* This is a pda for a cpu that was panicking. We
					 * need to check and see if this is the cpu that
					 * actually initiated the dump. We check the
					 * p_va_panicspin field in the pda_s struct to
					 * make sure that this is the one (zero means it
					 * is).
					 */
					if (KL_INT(klp, pdap, "pda_s", "p_va_panicspin")) {
						continue;
					}

					K_DUMPCPU(klp) = cpu;
					K_DUMPKTHREAD(klp) = 
						kl_kaddr(klp, pdap, "pda_s", "p_curkthread");

					/* This SHOULD be the CPU we want. If there is a
					 * dumpproc, we need to make sure that curkthread
					 * for this CPU relates to it (just in case...).
					 */
					if (K_DUMPPROC(klp)) {
						int kcnt;
						k_ptr_t procp, utp;
						kaddr_t kthread;

						/* Just in case, we have to make sure that we were
						 * able to get both dumpproc and dumpkthread structs
						 * from the dump.
						 */
						procp = kl_get_proc(klp, K_DUMPPROC(klp), 2, 0);
						if (!KL_ERROR) {
							utp = kl_get_uthread_s(klp, 
									       K_DUMPKTHREAD(klp), 2, 0);
						}
						if (KL_ERROR) {
							K_DUMPPROC(klp) = 0;
							K_DUMPKTHREAD(klp) = 0;
						}
						else {
							if (KL_INT(klp, procp, "proc", "p_pid") !=
							    kl_uthread_to_pid(klp, utp)) {
								fprintf(KL_ERRORFP, "WARNING: curkthread "
									"(0x%llx) for panicing CPU and dumpproc "
									"(0x%llx) have different PIDs!\n", 
									K_DUMPKTHREAD(klp),
									K_DUMPPROC(klp));
							}
							klib_free_block(klp, procp);
							klib_free_block(klp, utp);
						}
					}
					break;
				}
			}
			klib_free_block(klp, pdap);

			/* At this point, we should have a valid dumpkthread
			 * (if there was one). We now need to set defkthread 
			 * equal to it.
			 */
			if (K_DUMPKTHREAD(klp)) {
				if (kl_set_defkthread(klp, K_DUMPKTHREAD(klp))) {
					fprintf(KL_ERRORFP, "\nERROR: Could not set defkthread\n");
				}
			}
		}
	}

#ifdef MEMCHECK
	if (K_IP(klp) == 27) {
		if (!map_node_memory(klp)) {

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

/*
 * klib_add_callbacks()
 */
int32
klib_add_callbacks(
	klib_t *klp, 
	k_ptr_t ptr, 
	klib_sym_addr_func sym_addr,
	klib_struct_len_func struct_len, 
	klib_member_offset_func member_offset,
	klib_member_size_func member_size,
	klib_member_bitlen_func member_bitlen,
	klib_member_base_value_func member_baseval,
	klib_block_alloc_func block_alloc,
	klib_block_free_func block_free,
	klib_print_error_func print_error)
{
	if (klp == NULL) {
		return(-1);
	}

	if (sym_addr == NULL || struct_len == NULL || 
					member_offset == NULL || member_size == NULL || 
					member_bitlen == NULL || member_baseval == NULL) {
		fprintf(KL_ERRORFP, "Error!  Callback functions absent, internal "
		"error in libklib\n");
		return(-1);
	}
	K_SYM_ADDR(klp) = sym_addr;
	K_STRUCT_LEN(klp) = struct_len;
	K_MEMBER_OFFSET(klp) = member_offset;
	K_MEMBER_SIZE(klp) = member_size;
	K_MEMBER_BITLEN(klp) = member_bitlen;
	K_MEMBER_BASEVAL(klp) = member_baseval;
	K_BLOCK_ALLOC(klp) = block_alloc;
	K_BLOCK_FREE(klp) = block_free;
	K_PRINT_ERROR(klp) = print_error;
	return(0);
}

/*
 * kl_sym_addr()
 */
kaddr_t
kl_sym_addr(klib_t *klp, char *s)
{
	kaddr_t addr = 0;

	K_SYM_ADDR(klp)((k_ptr_t)NULL, s, &addr);
	return(addr);
}

/*
 * kl_sym_pointer() - Return the pointer a symbol address points to
 */
kaddr_t
kl_sym_pointer(klib_t *klp, char *s)
{
	kaddr_t addr = 0;

	K_SYM_ADDR(klp)((k_ptr_t)NULL, s, &addr);
	kl_get_kaddr(klp, addr, &addr, s);
	return(addr);
}

/*
 * kl_struct_len()
 */
int
kl_struct_len(klib_t *klp, char *s)
{
	int size = 0;

	K_STRUCT_LEN(klp)((k_ptr_t)NULL, s, &size);
	return(size);
}

/*
 * kl_member_size()
 */
int
kl_member_size(klib_t *klp, char *s, char *m)
{
	int size = 0;

	K_MEMBER_SIZE(klp)((k_ptr_t)NULL, s, m, &size);
	return(size);
}

/*
 * kl_member_offset()
 */
int
kl_member_offset(klib_t *klp, char *s, char *m)
{
	int offset = 0;

	K_MEMBER_OFFSET(klp)((k_ptr_t)NULL, s, m, &offset);
	return(offset);
}
