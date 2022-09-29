#ident "$Header: "

#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <elf.h>
#include <errno.h>
#include <klib/klib.h>

#define IS_ELF32(hdr)   (IS_ELF(hdr) && (hdr).e_ident[EI_CLASS] == ELFCLASS32)

off_t lseek(), lseek64();

/*
 * kl_free_meminfo()
 */
void
kl_free_meminfo(void *p)
{
	meminfo_t *mip = (meminfo_t *)p;

	if (mip->namelist) {
		free(mip->namelist);
	}
	if (mip->corefile) {
		free(mip->corefile);
	}
	if (mip->dump_hdr) {
		free(mip->dump_hdr);
	}
	if (mip->core_fd) {
		close(mip->core_fd);
	}
	free(mip);
}

/*
 * kl_check_dump_size()
 *
 *   Check to make sure the size of the dump header is valid
 */
int
kl_check_dump_size(meminfo_t *mip)
{
	/*
	 * Here we see if we should ignore the OS, or if the
	 * page size is undetermined.
	 */
	if ((mip->core_type == cmp_core) && (mip->dump_hdr->dmp_pages == 1)) {
		KL_SET_ERROR(KLE_SHORT_DUMP);
		return(1);
	}
	return(0);
}

/* 
 * kl_setup_meminfo()
 */
int
kl_setup_meminfo(char *k_namelist, char *k_corefile, int32 rwflag)
{
	if (K_FLAGS & K_MEMINFO_ALLOCED) {
		return(1);
	}

    (KLP)->k_libmem.k_data = kl_open_core(k_namelist, k_corefile, rwflag);
	if (!(KLP)->k_libmem.k_data) {
		return(1);
	}
	(KLP)->k_libmem.k_free = kl_free_meminfo;

	K_FLAGS |= K_MEMINFO_ALLOCED;
	return(0);
}

/*
 * kl_open_core()
 */
meminfo_t *
kl_open_core(char *k_namelist, char *k_corefile, int32 rwflag)
{
	int namelist_fd, core_fd;
	Elf32_Ehdr elf_header;
	Elf64_Ehdr elf_header64;
	meminfo_t *mip;

	/* Make sure we were passed pointers to k_corefile and k_namelist.
	 */
	if (k_namelist == NULL) {
		fprintf(KL_ERRORFP,"\nNULL pointer provided as namelist name\n");
		return((meminfo_t*)NULL);
	}
	if (k_corefile == NULL) {
		fprintf(KL_ERRORFP,"\nNULL pointer provided as corefile name\n");
		return((meminfo_t*)NULL);
	}

	mip = (meminfo_t*)malloc(sizeof(meminfo_t));
	if (!mip) {
		fprintf(KL_ERRORFP,"\nOut of memory! malloc failed on %d bytes!",
			(int)sizeof(meminfo_t));
		return((meminfo_t*)NULL);
	}
	bzero(mip, sizeof(*mip));

	mip->namelist = (char *)malloc(strlen(k_namelist) + 1);
	if (mip->namelist == NULL) {
		fprintf(KL_ERRORFP, "\nOut of memory mallocing %ld bytes "
			"for namelist\n", (long)strlen(k_namelist) + 1);
		kl_free_meminfo(mip);
		return((meminfo_t*)NULL);
	}
	strcpy(mip->namelist, k_namelist);

	mip->corefile = (char *)malloc(strlen(k_corefile) + 1);
	if (mip->corefile == NULL) {
		fprintf(KL_ERRORFP, "\nOut of memory mallocing %ld bytes "
			"for corefile\n", (long)strlen(k_corefile) + 1);
		kl_free_meminfo(mip);
		return((meminfo_t*)NULL);
	}
	strcpy(mip->corefile, k_corefile);

	/* Check to see if k_corefile references a core file on disk, or,
	 * if it references /dev/mem. If we are trying to open a core file, 
	 * make sure the open rwflag is O_RDONLY. If it's O_RDWR, don't fail. 
	 * Just chnge it to O_RDONLY (and print a warning message).
	 */
	if (strcmp(k_corefile, "/dev/mem") == 0) {

		mip->core_type = dev_kmem;
		mip->rw_flag = rwflag;
		if ((core_fd = open(k_corefile, rwflag)) == -1) {
			int myerr = errno;
			fprintf(KL_ERRORFP, "\nCould not open corefile %s : %s\n", 
				k_corefile, strerror(myerr));
			kl_free_meminfo(mip);
			return((meminfo_t*)NULL);
		}
		mip->core_fd = core_fd;
	} 
	else {
		if (rwflag != O_RDONLY) {
			/*
			fprintf(KL_ERRORFP,"\nCan't open %s in read-only mode only\n", 
				k_corefile);
			*/
			rwflag = O_RDONLY;
		}

		mip->rw_flag = rwflag;
		if ((core_fd = open(k_corefile, rwflag)) == -1) {
			int myerr = errno;
			fprintf(KL_ERRORFP, "\nCould not open corefile %s : %s\n", 	
				k_corefile, strerror(myerr));
			kl_free_meminfo(mip);
			return((meminfo_t*)NULL);
		}

		/* Make a copy of the dump_hdr and link it into mip
		 */
		mip->dump_hdr = (dump_hdr_t*)malloc(sizeof(dump_hdr_t));
		if (mip->dump_hdr == NULL) {
			fprintf(KL_ERRORFP,"\nOut of memory! malloc failed on %d bytes!\n",
				(int)sizeof(dump_hdr_t));
			kl_free_meminfo(mip);
			return((meminfo_t*)NULL);
		}

		/* Attempt to read in a compressed dump header 
		 */
		if (read(core_fd, (void *)mip->dump_hdr, sizeof(dump_hdr_t)) < 
				sizeof(dump_hdr_t)) {
			fprintf(KL_ERRORFP, "\nShort core file (less than %d bytes "
				"long)\n", (int)sizeof(dump_hdr_t));
			kl_free_meminfo(mip);
			return((meminfo_t*)NULL);
		}

		/* Save the file descriptor for the open corefile
		 */
		mip->core_fd = core_fd;

		/* Check to see if DUMP_MAGIC is set. If it is, then this is
		 * a compressed dump. Otherwise, it is a regular (uncompressed) 
		 * dump.
		 */
		if (mip->dump_hdr->dmp_magic == DUMP_MAGIC) {

            /* Now make sure we can read the dump header, check
             * and see if the pointers are valid in size, and
             * setup the compressed read sets.
             */
            if (cmpinit(mip, mip->core_fd, (char *)NULL, 0) < 0) {
				fprintf(KL_ERRORFP,"\nUnable to initialize compressed dump!\n");
				kl_free_meminfo(mip);
				return((meminfo_t*)NULL);
            }
            mip->core_type = cmp_core;
		}
		else {
			mip->core_type = reg_core;
		}

		/*
		 * After we have a reasonable confidence in the header,
		 * we have to check if we are dealing with the old dump
		 * (version < 4).
		 * If so, the header is one word shorter and it is neccessary
		 * to backspace the dump file by one word.
		 */

#ifdef MEM_DEBUG
		fprintf(KL_ERRORFP, "corefile v%d", mip->dump_hdr->dmp_version);
#endif
		if (mip->dump_hdr->dmp_version < 4) {
			lseek(mip->core_fd, -sizeof(int), SEEK_CUR);
		}

		if (kl_check_dump_size(mip)) {
			/* If we have a short dump, we still need to return a mip
			 * pointer. That is because the dump_hdr is required in order
			 * for an availmon report to be generated. Because KLE_SHORT_DUMP
			 * is set in the kl_check_dump_size(), non-availmon applications
			 * will die appropriately.
			 */
			return(mip);
		}
	}

	/* Determine if this is a 32-bit or 64-bit kernel. We need to 
	 * do this here so that the compression code works properly for
	 * the calls that are made to kl_readmem() before we are completely 
	 * setup.
	 */
	if ((namelist_fd  = open(k_namelist, O_RDONLY)) == -1) {
		fprintf(KL_ERRORFP, "\nCould not open kernel named %s\n", k_namelist);
		kl_free_meminfo(mip);
		return((meminfo_t*)NULL);
	}
	if (lseek(namelist_fd, 0, SEEK_SET) == -1) {
		fprintf(KL_ERRORFP, "\nCould not seek to 0 for kernel named %s\n", 
			k_namelist);
		close(namelist_fd);
		kl_free_meminfo(mip);
		return((meminfo_t*)NULL);
	}
	if (read(namelist_fd, &elf_header, 
		 sizeof(Elf32_Ehdr)) != sizeof(Elf32_Ehdr)) {
		fprintf(KL_ERRORFP, "\nCould not read entire elf header of short "
			"kernel %s\n", k_namelist);
		close(namelist_fd);
		kl_free_meminfo(mip);
		return((meminfo_t*)NULL);
	}
	close(namelist_fd);

	if (IS_ELF32(elf_header))  {
		mip->ptrsz = 32;
	} 
	else if (IS_ELF(elf_header)) {	
		mip->ptrsz = 64;
	}
	else {
		kl_free_meminfo(mip);
		return((meminfo_t*)NULL);
	}
	return(mip);
}

/*
 * kl_seekmem()
 * 
 *   Seek through physical system memory or vmcore image to the offset
 *   of a particular physical address.
 */
k_uint_t
kl_seekmem(meminfo_t *mip, kaddr_t paddr)
{
	off_t off;
	off64_t off64;

	if (mip->ptrsz == 64) {
		if ((off64 = lseek64(mip->core_fd, paddr, 0)) == -1) {
			return(KLE_INVALID_LSEEK);
		}
	}
	else {
		if ((off = lseek(mip->core_fd, paddr, 0)) == -1) {
			return(KLE_INVALID_LSEEK);
		}
	}
	return(0);
}

/*
 * kl_readmem()
 *
 *   Read a size block from physical address addr in system memory or 
 *   vmcore image. For compressed cores, make a call to cmpreadmem(). 
 *   Otherwise, make a call to kl_seekmem() and then read in the block. 
 *
 *   Note that there is a special case (trying to read from mapped_kern 
 *   memory on a live system) when addr will be a virtual address and 
 *   not a physical address. That's OK, we treat that case the same as 
 *   we would any physical address. On anything but a live system, the 
 *   attempt to seek to addr will fail. On a live system, the /dev/mem 
 *   driver handles the translation for us.
 */
k_uint_t
kl_readmem(kaddr_t addr, unsigned size, k_ptr_t buffer)
{
	int tsize;
	k_uint_t ret;
	kaddr_t paddr;
	meminfo_t *mip;

	mip = LIBMEM_DATA;
	if (!mip) {
		return(KLE_NOT_INITIALIZED);
	}

	if (mip->core_type == cmp_core) {
		addr += mip->dump_hdr->dmp_physmem_start;
		tsize = cmpreadmem(mip, mip->core_fd,
				addr, buffer, size, CMP_VM_CACHED);
		if (tsize <= 0) {
			return(KLE_INVALID_CMPREAD);
		}
	}
	else {
		if (ret = kl_seekmem(mip, addr)) {
			return(ret);
		}
		if (read(mip->core_fd, buffer, size) != size) {
			return(KLE_INVALID_READ);
		}
	}
	return(0);
}

