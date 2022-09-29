#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libklib/RCS/klib_mem.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/invent.h>
#include "klib.h"
#include "klib_extern.h"

extern int mem_validity_check;

/* The node memory array is sized dynamically at startup based on the
 * maximum number of nodes that could be installed in a system. Note
 * that the nasid for each node, not the nodeid, is used to index into 
 * the array. This is because we want to map each node to the physical 
 * address space it controls.
 */
node_memory_t **node_memory;

/* 
 * This module contains all the functions necessary for accessing
 * data in a system dump (or on a live system). The functions defined 
 * in this module fall into a number of categories:
 *
 * o Translating kernel virtual addresses to physical addresses 
 *
 * o Seeking and reading from the dump 
 *
 * o Reading blocks of memory from the dump
 *
 * o Adjusting kernel data read from the dump based on differences 
 *   between 32-bit and 64-bit systems and specific differences between 
 *   platforms 
 *
 */

/* Macro that returns the proper physical address. This depends on weather 
 * or not this is a live system or coredump. Also, weather or not there is 
 * a ram_offset.
 */ 
#define RETURN_PADDR(paddr) return((!K_RAM_OFFSET(klp) || \
	ACTIVE(klp)) ? paddr : (paddr - K_RAM_OFFSET(klp)))

/*
 * kl_is_valid_kaddr()
 */
int
kl_is_valid_kaddr(klib_t *klp, kaddr_t addr, k_ptr_t ktp, int flags)
{
	if (DEBUG(KLDC_FUNCTRACE, 5)) {
		fprintf(KL_ERRORFP, "addr=0x%llx, ktp=0x%x\n", addr, ktp);
	}

	kl_reset_error();

	kl_virtop(klp, addr, ktp);
	if (KL_ERROR) {
		return(0);
	}
	if ((flags & WORD_ALIGN_FLAG) && (addr % K_NBPW(klp))) {
		KL_SET_ERROR_NVAL(KLE_INVALID_VADDR_ALIGN, addr, 2);
		return(0);
	}
	return(1);
}

/*
 * is_mapped_kern_ro() -- Determine if a given address is a mapped kernel
 *                        address in a given read-only range.
 */
int
is_mapped_kern_ro(klib_t *klp, kaddr_t addr)
{
	if ((addr >= K_MAPPED_RO_BASE(klp)) &&
		(addr < K_MAPPED_RO_BASE(klp) + K_MAPPED_PAGE_SIZE(klp))) {
			return 1;
	}
	return 0;
}

/*
 * is_mapped_kern_rw() -- Determine if a given address is a mapped kernel
 *                        address in a given read-write range.
 */
int
is_mapped_kern_rw(klib_t *klp, kaddr_t addr)
{
	if ((addr >= K_MAPPED_RW_BASE(klp)) &&
		(addr < K_MAPPED_RW_BASE(klp) + K_MAPPED_PAGE_SIZE(klp))) {
			return 1;
	}
	return 0;
}

/*
 * kl_virtop() -- Virtual to physical offset address translation
 */
kaddr_t
kl_virtop(klib_t *klp, kaddr_t vaddr, k_ptr_t ktp)
{
	int s, e, pfn, ktp_alloced = 0;
	kaddr_t kthread, paddr, kptbl_addr; 
	k_uint_t pde, pgi;
	k_ptr_t pte = 0;

	kl_reset_error();

	if (DEBUG(KLDC_FUNCTRACE, 5)) {
		fprintf(KL_ERRORFP, "kl_virtop: vaddr=0x%llx, ktp=0x%x\n", 
			vaddr, ktp);
	}

	if (IS_HUBSPACE(klp, vaddr))
		return(vaddr);

	if (is_kseg0(klp, vaddr)) {
		paddr = k0_to_phys(klp, vaddr);
		if (e = kl_valid_physmem(klp, paddr)) {
			KL_SET_ERROR_NVAL(e, paddr, 2);
			return(paddr);
		}
		RETURN_PADDR(paddr);
	}

	if (is_kseg1(klp, vaddr)) {
		paddr = k1_to_phys(klp, vaddr);
		if (e = kl_valid_physmem(klp, paddr)) {
			KL_SET_ERROR_NVAL(e, paddr, 2);
			return(paddr);
		}
		RETURN_PADDR(paddr);
	}

	if (is_kseg2(klp, vaddr)) {

		/* handle mapped kernel addresses here (to k0 base!) 
		 */
		if ((is_mapped_kern_ro(klp, vaddr)) || 
		    (is_mapped_kern_rw(klp, vaddr))) {
			
			/* clear out text/data field 
			 */
			vaddr &= 0xffffffff00ffffff;

			/* correct address if master_nodeid is non-zero.
			 */
			if (K_MASTER_NASID(klp)) {
				vaddr += ((k_uint_t)K_MASTER_NASID(klp)) << 32;
			}

			/* get physical address based on k0 offset 
			 */
			paddr = k0_to_phys(klp, vaddr);
			if (e = kl_valid_physmem(klp, paddr)) {
				KL_SET_ERROR_NVAL(e, paddr, 2);
				return(paddr);
			}
			RETURN_PADDR(paddr);
		}

		kptbl_addr = (K_KPTBL(klp) + (((vaddr - K_K2BASE(klp)) >> 
				K_PNUMSHIFT(klp)) * PDE_SIZE(klp)));
		if (DEBUG(KLDC_MEM, 1)) {
			fprintf(KL_ERRORFP, "kl_virtop: kptbl_addr=0x%llx\n", kptbl_addr);
		}
		pte = kl_get_pde(klp, kptbl_addr, &pde);
		if (KL_ERROR) {
			return(vaddr);
		}
		pgi = kl_get_pgi(klp, pte);
		if (DEBUG(KLDC_MEM, 1)) {
			fprintf(KL_ERRORFP, "kl_virtop: pte=0x%x, pde=0x%llx, "
				"pgi=0x%llx\n", pte, pde, pgi);
		}
		if (((k_uint_t)pgi & (k_uint_t)K_PDE_PG_VR(klp)) == 0) {
			if (DEBUG(KLDC_MEM, 1)) {
				fprintf(KL_ERRORFP, "kl_virtop: KSEG2 address %llx "
					"invalid\n", vaddr);
			}
			KL_SET_ERROR_NVAL(KLE_INVALID_K2_ADDR, vaddr, 2);
			return(vaddr);
		}
		paddr = (Ctob(klp, kl_pte_to_pfn(klp, pte)) + 
				(vaddr & (K_PAGESZ(klp)-1)));
		if (DEBUG(KLDC_MEM, 1)) {
			fprintf(KL_ERRORFP, "kl_virtop: paddr=0x%llx\n", paddr);
			fprintf(KL_ERRORFP, "kl_virtop: pfn_mask=0x%llx "
				"pfn_shift=%d\n", K_PFN_MASK(klp), K_PFN_SHIFT(klp));
		}
		RETURN_PADDR(paddr);
	}

	/* The remaining translations depends upon us having a proc 
	 * structure. If the kthread pointer passed in references a process
	 * kthread, then we can use ktp as a proc pointer. If no kthread
	 * was passed in, then we will have to use either K_DEFKTHREAD() or
	 * K_DUMPPROC(). 
	 */
	if (!ktp) {
		if (K_DEFKTHREAD(klp)) {
			kthread = K_DEFKTHREAD(klp);
			ktp = kl_get_kthread(klp, kthread, 0);
			if (KL_ERROR) {
				goto merror;
			}

			/* Check to make sure the defkthread is a uthread. If it 
			 * isn't, then we have to return an error (there is no way
			 * we can translate the kernelstack address.
			 */
			if (!IS_UTHREAD(klp, ktp)) {
				KL_SET_ERROR_NVAL(KLE_WRONG_KTHREAD_TYPE|KLE_BAD_DEFKTHREAD, 
					kthread, 2);
				klib_free_block(klp, ktp);
				goto merror;
			}
			ktp_alloced++;
		}
		else if (K_DUMPPROC(klp)) {
			kthread = K_DUMPKTHREAD(klp);
			ktp = kl_get_kthread(klp, kthread, 0);
			if (KL_ERROR) {
				goto merror;
			}
			ktp_alloced++;
		}
		else {
			KL_SET_ERROR(KLE_NO_DEFKTHREAD);
			goto merror;
		}
	} 

	/* As a last ditch sanity check, make sure that the kthread is 
	 * of type KT_UTHREAD (the only type of kthread that contains 
	 * translation information for a kernelstack).
	 */
	if (!IS_UTHREAD(klp, ktp)) {
		KL_SET_ERROR_NVAL(KLE_WRONG_KTHREAD_TYPE, kthread, 2);
		if (ktp_alloced) {
			klib_free_block(klp, ktp);
		}
		goto merror;
	}

	/* Now we need to check and see if vaddr is from the kernel 
	 * stack page or kernel stack extension page.
	 */
	if ((vaddr >= K_KERNELSTACK(klp)) && (vaddr < K_KERNSTACK(klp))) {
		pte = (k_ptr_t)(ADDR(klp, ktp, "uthread_s", "ut_kstkpgs"));
		pfn = kl_pte_to_pfn(klp, pte);
	}
	else if (K_EXTUSIZE(klp) && 
				(vaddr >= K_KEXTSTACK(klp)) && (vaddr < K_KERNELSTACK(klp))) {
		pte = (k_ptr_t)(ADDR(klp, ktp, "uthread_s", "ut_kstkpgs") + 
				(PDE_SIZE(klp) * K_EXTSTKIDX(klp)));
		pfn = kl_pte_to_pfn(klp, pte);
		if (KL_ERROR) {
			/* Check and see if the current kthread is running on a
			 * cpu. If it is, then get the mapping for the kextstack
			 * page from the pda_s struct.
			 */
			int cpuid;

			cpuid = KL_INT(klp, ktp, "kthread", "k_sonproc");
			if (cpuid != -1) {
				kaddr_t pda;
				k_ptr_t pdap;

				pdap = klib_alloc_block(klp, PDA_S_SIZE(klp), K_TEMP);
				kl_get_pda_s(klp, (kaddr_t)cpuid, pdap);
				if (KL_ERROR) {
				}
				else {
					pte = (k_ptr_t)ADDR(klp, pdap, "pda_s", "p_stackext");
					pfn = kl_pte_to_pfn(klp, pte);
				}
			}
			else {
				KL_SET_ERROR_NVAL(KL_ERROR|KLE_INVALID_KERNELSTACK, vaddr, 2);
			}
		}

	} 
	else {
		/* We don't have a valid kernel address...
		 *
		 * XXX - Add translation for proc virtual addresses?
		 */
		KL_SET_ERROR_NVAL(KL_ERROR|KLE_INVALID_VADDR, vaddr, 2);
	}

	/* Free the kthread buffer that was allocated by kl_get_kthread() above.
	 */
	if (ktp_alloced) {
		klib_free_block(klp, ktp);
	}

	if (!KL_ERROR) {
		paddr = Ctob(klp, pfn) | (vaddr & (K_PAGESZ(klp) - 1));
		RETURN_PADDR(paddr);
	}

merror:
	if (!KL_ERROR) {
		KL_SET_ERROR_NVAL(KLE_INVALID_VADDR, vaddr, 2);
	}
	return(vaddr);
}

/*
 * kl_pfn_to_K2() -- Convert a Page Frame Number (pfn) to a K2 address
 *
 *   Search through the kernel page table for a pfn that matches.
 */
kaddr_t
kl_pfn_to_K2(klib_t *klp, uint pfn)
{
	int i, kptbl_alloced = 0;
	kaddr_t k2addr, kptbl_addr;
	k_ptr_t pte;
	k_uint_t pde, pgi;

	if (DEBUG(KLDC_FUNCTRACE, 6)) {
		fprintf(KL_ERRORFP, "kl_pfn_to_K2: pfn=%d\n", pfn);
	}

	/* The in-memory kernel page talble (if one exists) is used 
	 * when analyzing a core dump (for performance reasons).
	 */
	if (ACTIVE(klp) || !K_KPTBLP(klp)) {
		K_KPTBLP(klp) = 
			K_BLOCK_ALLOC(klp)(0, K_SYSSEGSZ(klp) * PDE_SIZE(klp), K_TEMP);
		kl_get_block(klp, K_KPTBL(klp), 
			(K_SYSSEGSZ(klp) * PDE_SIZE(klp)), K_KPTBLP(klp), "pde in kptbl");
		if (KL_ERROR) {
			K_BLOCK_FREE(klp)(0, K_KPTBLP(klp));
			K_KPTBLP(klp) = 0;
			return((kaddr_t)0);
		}
		kptbl_alloced++;
	}
	for (i = 0; i < K_SYSSEGSZ(klp); i++) {
		pte = (k_ptr_t)((uint)K_KPTBLP(klp) + (i * PDE_SIZE(klp)));
	    pgi = kl_get_pgi(klp, pte);

	    /* Use the Pde_pg_vr mask instead of pte bit_field because
		 * of performance issues (we don't want to go down the dwarf
		 * path.
		 */
	    if ((pgi & K_PDE_PG_VR(klp)) == 0) {
			continue;
	    }
		if (pfn == kl_pte_to_pfn(klp, pte)) {
			k2addr = Ctob(klp, i) | K_K2BASE(klp);
			if (kptbl_alloced) {
				K_BLOCK_FREE(klp)(0, K_KPTBLP(klp));
				K_KPTBLP(klp) = 0;
			}
			return((kaddr_t)k2addr);
		}
	} 
	if (kptbl_alloced) {
		K_BLOCK_FREE(klp)(0, K_KPTBLP(klp));
		K_KPTBLP(klp) = 0;
	}
	return((kaddr_t)0);
}

/*
 * kl_get_addr_mode()
 */
int
kl_get_addr_mode(klib_t *klp, kaddr_t addr)
{
	int mode = -1;

	if (DEBUG(KLDC_FUNCTRACE, 6)) {
		fprintf(KL_ERRORFP, "kl_get_addr_mode: addr=0x%llx\n", addr);
	}

	if (is_kseg0(klp, addr)) {
		mode = 0;
	} 
	else if (is_kseg1(klp, addr)) {
		mode = 1;
	} 
	else if (is_kseg2(klp, addr)) {
		mode = 2;
	} 
	else if (is_kpteseg(klp, addr)) {

		/* For now, just return an error...
		 */
		if (DEBUG(KLDC_MEM, 1)) {
			fprintf(KL_ERRORFP, "Can't print KPTESEG yet\n");
		}
		KL_SET_ERROR_NVAL(KLE_INVALID_VADDR, addr, 2);
		return(-1);
	} 
	else if ((addr >= K_KERNELSTACK(klp)) && (addr < K_KERNSTACK(klp))) {
		mode = 3;
	}
	else {
		int e;

		if (e = kl_valid_physmem(klp, addr)) {
		 	KL_SET_ERROR_NVAL(e, addr, 2);
			if (e != KLE_AFTER_PHYSMEM) {
				return(-1);
			}
		}
		mode = 4; /* physical address mode */
	}

	if ((mode >= 0) && (mode <= 3)) {
		kl_virtop(klp, addr, (k_ptr_t)NULL);
		if (KL_ERROR) {
			return(-1);
		}
	}
	return(mode);
}

/*
 * kl_seekmem()
 * 
 *   Seek through memory (normally from an uncompressed core file.) If
 *   mode == 1 then convert 'addr' from virtual to physical else treat
 *   'addr' as a physical address.
 */
int
kl_seekmem(klib_t *klp, kaddr_t addr, int mode, k_ptr_t ktp)
{
	kaddr_t paddr;
	off_t off, lseek(), lseek64();
	off64_t off64;

	if (DEBUG(KLDC_FUNCTRACE, 5)) {
		fprintf(KL_ERRORFP, "kl_seekmem: addr=0x%llx, mode=%d, ktp=0x%x\n", 
			addr, mode, ktp);
	}

	if (mode) {
		paddr = kl_virtop(klp, addr, ktp);
		if (DEBUG(KLDC_MEM, 5)) {
			fprintf(KL_ERRORFP, "kl_seekmem: addr=0x%llx, paddr=0x%llx\n", 
				addr, paddr);
		}
		KL_CHECK_ERROR_RETURN;
	} 
	else {
		paddr = addr;
	}

	if (paddr == -1) {
		if (DEBUG(KLDC_MEM, 1)) {
			fprintf(KL_ERRORFP, "kl_seekmem: %llx is an invalid address\n", 
				addr);
		}
		KL_SET_ERROR_NVAL(KLE_INVALID_VADDR, addr, 2);
		KL_RETURN_ERROR;
	}

	if (PTRSZ64(klp)) {
		if ((off64 = lseek64(K_CORE_FD(klp), paddr, 0)) == -1) {
			if (DEBUG(KLDC_MEM, 1)) {
				fprintf(KL_ERRORFP, "kl_seekmem: lseek64 error on address "
					"%llx\n", addr);
			}
			KL_SET_ERROR_NVAL(KLE_INVALID_LSEEK, addr, 2);
			KL_RETURN_ERROR;
		}
	}
	else {
		if ((off = lseek(K_CORE_FD(klp), paddr, 0)) == -1) {
			if (DEBUG(KLDC_MEM, 1)) {
				fprintf(KL_ERRORFP, "kl_seekmem: lseek error on "
					"address %llx\n", addr);
			}
			KL_SET_ERROR_NVAL(KLE_INVALID_LSEEK, paddr, 2);
			KL_RETURN_ERROR;
		}
	}
	return(0);
}

/*
 * kl_readmem()
 *
 *   Read through memory, depending on the corefile that is specified.
 *   For compressed cores, call cmpreadmem, otherwise, if running on a
 *   live system, just read the location, or read directly from the
 *   uncompressed core file.
 */
int
kl_readmem(klib_t *klp, kaddr_t addr, int mode, char *buffer, k_ptr_t ktp,
		unsigned size, char *name)
{
	int e = 0, s, offset, tsize;
	kaddr_t p, a, paddr;
	char *b;

	if (DEBUG(KLDC_FUNCTRACE, 5)) {
		fprintf(KL_ERRORFP,
			"kl_readmem: addr=0x%llx, mode=%d, buffer=0x%x, ktp=0x%x, size=%d, "
			"name=\"%s\"\n", addr, mode, buffer, ktp, size, name);
	}

	if (mode) {
		if (ACTIVE(klp) && (is_mapped_kern_ro(klp, addr) || 
										is_mapped_kern_rw(klp, addr))) {
			paddr = addr;
		}
		else {
			paddr = kl_virtop(klp, addr, ktp);
		}
		KL_CHECK_ERROR_RETURN;
	} 
	else {
		paddr = addr;
	}
	if (DEBUG(KLDC_MEM, 5)) {
		fprintf(KL_ERRORFP, "kl_readmem: addr=0x%llx, paddr=0x%llx\n", 
			addr, paddr);
	}

#ifdef I_TEMP
	if (DEBUG(KLDC_MEM, 2)) {
		fprintf(KL_ERRORFP, "kl_readmem: addr=0x%llx, KSTACK=%d, "
			"size=%d, oversize=%d\n", addr, IS_KERNELSTACK(klp, addr), size, 
				(size > (K_PAGESZ(klp) - (addr % K_PAGESZ(klp)))));
	}
	if ((is_kseg2(klp, addr) || IS_KERNELSTACK(klp, addr)) && 
				(size > (K_PAGESZ(klp) - (addr % K_PAGESZ(klp))))) {
#else
	if ((is_kseg2(klp, addr) || is_kpteseg(klp, addr)) && 
			(size > (K_PAGESZ(klp) - (addr % K_PAGESZ(klp))))) {
#endif /* I_TEMP */
		offset = (K_PAGESZ(klp) - (addr % K_PAGESZ(klp)));
		if (COMPRESSED(klp)) {
			paddr += K_DUMP_HDR(klp)->dmp_physmem_start;
			tsize = cmpreadmem(klp, 
					K_CORE_FD(klp), paddr, buffer, offset, CMP_VM_CACHED);
			if (tsize <= 0) {
				if (DEBUG(KLDC_MEM, 1)) {
					fprintf(KL_ERRORFP, "kl_readmem: read error on %s\n", 
						name);
				}
				KL_SET_ERROR_NVAL(KLE_INVALID_CMPREAD, paddr, 2);
				KL_RETURN_ERROR;
			}
		}
		else {
			kl_seekmem(klp, addr, mode, ktp);
			KL_CHECK_ERROR_RETURN;

			if (read(K_CORE_FD(klp), buffer, offset) != offset) {
				if (DEBUG(KLDC_MEM, 1)) {
					fprintf(KL_ERRORFP, "kl_readmem: read error on %s\n",
						name);
				}
				KL_SET_ERROR_NVAL(KLE_INVALID_READ, offset, 2);
				KL_RETURN_ERROR;
			}
		}
		s = size - offset;
		a = addr + offset;
		b = buffer + offset;

		p = kl_virtop(klp, a, ktp);
		KL_CHECK_ERROR_RETURN;

		while (s) {
			if (COMPRESSED(klp)) {
				p += K_DUMP_HDR(klp)->dmp_physmem_start;
				tsize = cmpreadmem(klp, K_CORE_FD(klp), p, b, s, CMP_VM_CACHED);
				if (tsize <= 0) {
					if (DEBUG(KLDC_CMP, 3)) {
						fprintf(KL_ERRORFP, "cmpreadmem: read error on %s\n",
							name);
					}
					KL_SET_ERROR_NVAL(KLE_INVALID_CMPREAD, p, 2);
					KL_RETURN_ERROR;
				}
			}
			else {
				kl_seekmem(klp, a, mode, ktp);
				KL_CHECK_ERROR_RETURN;

				if (read(K_CORE_FD(klp), b, s) != s) {
					if (DEBUG(KLDC_MEM, 1)) {
						fprintf(KL_ERRORFP, "kl_readmem: read error on %s\n", 
							name);
					}
					KL_SET_ERROR_NVAL(KLE_INVALID_READ, a, 2);
					KL_RETURN_ERROR;
				}
			}
			if (s > K_PAGESZ(klp)) {
				s -= K_PAGESZ(klp);
				b += K_PAGESZ(klp);
				a += K_PAGESZ(klp);
				offset += K_PAGESZ(klp);
				p = kl_virtop(klp, a, ktp);
				KL_CHECK_ERROR_RETURN;
			}
			else {
				s = 0;
			}
		}
	}
	else {
		if (COMPRESSED(klp)) {
			paddr += K_DUMP_HDR(klp)->dmp_physmem_start;
			tsize = cmpreadmem(klp, 	
					K_CORE_FD(klp), paddr, buffer, size, CMP_VM_CACHED);
			if (tsize <= 0) {
				if (DEBUG(KLDC_CMP, 3)) {
					fprintf(KL_ERRORFP, "cmpreadmem: read error on %s\n", 
						name);
				}
				KL_SET_ERROR_NVAL(KLE_INVALID_CMPREAD, paddr, 2);
				KL_RETURN_ERROR;
			}
		}
		else {
			kl_seekmem(klp, paddr, 0, ktp);
			KL_CHECK_ERROR_RETURN;

			if (read(K_CORE_FD(klp), buffer, size) != size) {
				if (DEBUG(KLDC_MEM, 1)) {
					fprintf(KL_ERRORFP, "kl_readmem: read error on %s\n", 
						name);
				}
				KL_SET_ERROR_NVAL(KLE_INVALID_READ, paddr, 2);
				KL_RETURN_ERROR;
			}
		}
	}
	return(0);
}

/*
 * kl_get_block() -- Get an object of size bytes from the dump
 *
 *   The object can be a kernel data structure, a block of memory,
 *   a stack, or whatever. If a pointer to a buffer has been passed, 
 *   put the data there. Otherwise, get the space using klib_alloc_block(). 
 *   In either case, return a pointer to the location of the data (or 
 *   NULL if a failure occurred). Note that only temporary (K_TEMP) 
 *   blocks of memory can be allocated via kl_get_block(). An explicit 
 *   call to klib_alloc_block() must be made prior to the kl_get_block() 
 *   call if a perminant block is desired.
 */
k_ptr_t
kl_get_block(klib_t *klp, kaddr_t addr, unsigned size, 
	k_ptr_t buffp, char *name)
{
	int buffp_alloced = 0;

	if (DEBUG(KLDC_FUNCTRACE, 5)) {
		fprintf(KL_ERRORFP, "kl_get_block: klp=0x%x, addr=0x%llx, size=%d, "
			"buffp=0x%x, name=\"%s\"\n", klp, addr, size, buffp, name);
	}

	/* If a buffer pointer wasn't passed in, get one now
	 */
	if (!buffp) {
		if (!size || !(buffp = (k_ptr_t)klib_alloc_block(klp, size, K_TEMP))) {
			if (DEBUG(KLDC_MEM, 1)) {
				if (size) {
					fprintf(KL_ERRORFP,
						"kl_get_block: not enough memory to allocate %s "
						"at 0x%llx\n", name, addr);
				}
				else {
					fprintf(KL_ERRORFP, "kl_get_block: trying to allocate "
						"a zero size block!\n");
				}
			}
			return((k_ptr_t)NULL);
		}
		buffp_alloced = 1;
	}

	/* If addr is less a physical address, set mode appropriately.
	 */
	kl_readmem(klp, addr, 
		(IS_PHYSICAL(klp, addr) ? 0 : 1), buffp, (k_ptr_t)0, size, name);
	if (KL_ERROR) {
		if (DEBUG(KLDC_MEM, 1)) {
			fprintf(KL_ERRORFP, "kl_get_block: error reading %s at 0x%llx\n", 
					name, addr);
		}
		if (buffp_alloced) {
			klib_free_block(klp, buffp);
			buffp = (k_ptr_t*)NULL;
		}
		KL_SET_ERROR_NVAL(KL_ERROR, addr, 2);
		return((k_ptr_t)NULL);
	}
	return(buffp);
}

/*
 * kl_get_kaddr() -- Get the kernel address pointed to by 'addr'
 *
 *   If this is a 64-bit kernel, get an eight byte address. Otherwise, 
 *   get a four byte address and right shift them to the low-order word. 
 *   If a buffer pointer wasn't passed in, allocate a K_TEMP buffer (big 
 *   enough to hold an 8 byte value). If a block was allocated here, then 
 *   it has to be freed up here if some sort of failure occurs. Note
 *   that it is not necessary to call kl_reset_error() since that will be
 *   done in the kl_get_block() function (as will the setting of the
 *   global error information).
 */
k_ptr_t
kl_get_kaddr(klib_t *klp, kaddr_t addr, k_ptr_t buffp, char *label)
{
	int buffp_alloced = 0;

	if (DEBUG(KLDC_FUNCTRACE, 4)) {
		fprintf(KL_ERRORFP, "kl_get_kaddr: addr=0x%llx, buffp=0x%x, "
			"label=\"%s\\n", addr, buffp, label);
	}

	if (!buffp) {
		buffp = klib_alloc_block(klp, sizeof(kaddr_t), K_TEMP);
		buffp_alloced = 1;
	}
	if (PTRSZ64(klp)) {
		kl_get_block(klp, addr, sizeof(kaddr_t), buffp, label);
		if (KL_ERROR) {
			if (buffp_alloced) {
				klib_free_block(klp, buffp);
			}
			return((k_ptr_t)NULL);
		}
	}
	else {
		kl_get_block(klp, addr, 4, buffp, label);
		if (KL_ERROR) {
			if (buffp_alloced) {
				klib_free_block(klp, buffp);
			}
			return((k_ptr_t)NULL);
		}
		*(kaddr_t*)buffp = (*(kaddr_t*)buffp >> 32);
	}
	return(buffp);
}

/*
 * kl_uint() -- Return an unsigned intiger value stored in a structure
 *
 *    Pointer 'p' points to a buffer that contains a kernel structure 
 *    of type 's.' If the size of member 'm' is less than eight bytes
 *    then right shift the value the appropriate number of bytes so that 
 *    it lines up properly in an k_uint_t space. Return the resulting 
 *    value. 
 */
k_uint_t
kl_uint(klib_t *klp, k_ptr_t p, char *s, char *m, unsigned offset)
{
	int nbytes, shft;
	k_uint_t v;
	k_ptr_t source;

	if (DEBUG(KLDC_FUNCTRACE, 6)) {
		fprintf(KL_ERRORFP, "k_uint: p=0x%x, s=\"%s\", m=\"%s\", offset=%u\n", 
			p, s, m, offset);
	}

	if ((K_MEMBER_SIZE(klp)((k_ptr_t)NULL, s, m, &nbytes) < 0) || !nbytes) {
		KL_SET_ERROR_CVAL(KLE_BAD_FIELD, m);
		return((k_uint_t)0);
	}
	source = (k_ptr_t)(ADDR(klp, p, s, m) + offset);
	bcopy(source, &v, nbytes);
	if (shft = (8 - nbytes)) {
		v = v >> (shft * 8);
	}
	return(v);
}

/*
 * kl_int() -- Return a signed intiger value stored in a structure
 *
 *    Pointer 'p' points to a buffer that contains a kernel structure 
 *    of type 's.' If the size of member 'm' is less than eight bytes
 *    then right shift the value the appropriate number of bytes so that 
 *    it lines up properly in an k_uint_t space. Return the resulting 
 *    value. 
 */
k_int_t
kl_int(klib_t *klp, k_ptr_t p, char *s, char *m, unsigned offset)
{
	int nbytes, shft;
	k_int_t v;
	k_ptr_t source;

	if (DEBUG(KLDC_FUNCTRACE, 6)) {
		fprintf(KL_ERRORFP, "kl_int: klp=0x%x, p=0x%x, s=\"%s\", m=\"%s\", "
			"offset=%u\n", klp, p, s, m, offset);
	}

    if ((K_MEMBER_SIZE(klp)((k_ptr_t)NULL, s, m, &nbytes) < 0) || !nbytes) {
		KL_SET_ERROR_CVAL(KLE_BAD_FIELD, m);
		return((k_uint_t)0);
	}
	source = (k_ptr_t)(ADDR(klp, p, s, m) + offset);
	bcopy(source, &v, nbytes);
	if (shft = (8 - nbytes)) {
		v = v >> (shft * 8);
	}
	return(v);
}

/*
 * kl_kaddr() -- Return a kernel virtual address stored in a structure
 *
 *   Pointer 'p' points to a buffer that contains a kernel structure 
 *   of type 's.' Get the kernel address located in member 'm.' If the 
 *   system is running a 32-bit kernel, then right shift the address 
 *   four bytes. If the system is running a 64-bit kernel, then the 
 *   address is OK just the way it is.
 */
kaddr_t
kl_kaddr(klib_t *klp, k_ptr_t p, char *s, char *m)
{
	kaddr_t k;

	if (DEBUG(KLDC_FUNCTRACE, 6)) {
		fprintf(KL_ERRORFP, "kaddr: p=0x%x, s=\"%s\", m=\"%s\"\n", p, s, m);
	}

	bcopy(K_PTR(klp, p, s, m), &k, 8);

	if (PTRSZ64(klp)) {
		return(k);
	}
	else {
		return(k >> 32);
	}
}

/*
 * kl_reg() -- Return a register value
 *
 *   Pointer 'p' points to a buffer that contains a kernel structure 
 *   of type 's.' Get the register value located in member 'm.' Unlike
 *   with the kl_kaddr() function, the full 64-bit value is returned
 *   even if the system is running a 32-bit kernel (all CPU registers
 *   are 64-bit).
 */
kaddr_t
kl_reg(klib_t *klp, k_ptr_t p, char *s, char *m)
{
	kaddr_t k;

	if (DEBUG(KLDC_FUNCTRACE, 6)) {
		fprintf(KL_ERRORFP, "kl_reg: p=0x%x, s=\"%s\", m=\"%s\"\n", p, s, m);
	}

	bcopy(K_PTR(klp, p, s, m), &k, 8);
	return(k);
}

/*
 * kl_kaddr_val() -- Return the kernel virtual address located at pointer 'p'
 *
 *   If the system is running a 32-bit kernel, then right shift the 
 *   kernel address four bytes. If the system is running a 64-bit 
 *   kernel, then the address is OK just the way it is.
 */
kaddr_t
kl_kaddr_val(klib_t *klp, k_ptr_t p)
{
	kaddr_t k;

	if (DEBUG(KLDC_FUNCTRACE, 6)) {
		fprintf(KL_ERRORFP, "kl_kaddr_val: p=0x%x\n", p);
	}
	bcopy(p, &k, 8);

	if (PTRSZ64(klp)) {
		return(k);
	}
	else {
		return(k >> 32);
	}
}

/*
 * kl_kaddr_to_ptr() -- Return the pointer stored at kernel address 'k'
 *
 */
kaddr_t
kl_kaddr_to_ptr(klib_t *klp, kaddr_t k)
{
	kaddr_t k1;

	if (DEBUG(KLDC_FUNCTRACE, 6)) {
		fprintf(KL_ERRORFP, "kl_kaddr_to_ptr: k=0x%llx\n", k);
	}

	kl_get_kaddr(klp, k, &k1, "kaddr");
	if (KL_ERROR) {
		return((kaddr_t)0);
	}
	else {
		return(k1);
	}
}

/*
 * kl_addr_convert()
 *
 *   Takes value and, based on the contents of flag, determins the
 *   physical address, pfn, and all other possible kernel virtual 
 *   addresses. The value is passed in without any pre-testing
 *   (other than to make sure it is numeric). It is necessary to 
 *   check to make sure it is valid. The paramaters for the various
 *   return values MAY be NULL (at least one should be a valid 
 *   pointer or it doesn't make sense to call this routine). Unlike
 *   most base routines, this function does not set the global error
 *   information (in almost all cases). It does, however, return 
 *   whatever error code is relevant.
 */
int
kl_addr_convert(klib_t *klp, kaddr_t value, int flag, kaddr_t *paddr, 
	uint *pfn, kaddr_t *k0, kaddr_t *k1, kaddr_t *k2) 
{
	int e;

	switch (flag) {

		case PFN_FLAG :
			*pfn = (uint)value;
			if (*pfn > K_MAXPFN(klp)) {
				return(KLE_AFTER_MAXPFN);
			}
			*paddr = PFN_TO_PHYS(klp, *pfn);
			break;

		case PADDR_FLAG :
			*paddr = value;
			if (*paddr >= ((k_uint_t)K_MAXPFN(klp) * K_PAGESZ(klp))) {
				return(KLE_AFTER_MAXMEM);
			}
			*pfn = Pnum(klp, *paddr);
			break;

		case VADDR_FLAG :
			*paddr = kl_virtop(klp, value, (k_ptr_t)NULL);
			if (e = KL_ERROR) { 
				if (!((KL_ERROR == KLE_AFTER_PHYSMEM) || 
							(KL_ERROR == KLE_PHYSMEM_NOT_INSTALLED))) {
					KL_RETURN_ERROR;
				}
			}
			else if (K_RAM_OFFSET(klp)) {
				/* kl_virtop() returns the offset to physical address 
				 * (starting from memory address zero). So, we have to 
				 * add the ram_offset back in order to get the proper 
				 * physical address from the machines point of view.
				 */
				*paddr |= K_RAM_OFFSET(klp);
			}
			*pfn = Pnum(klp, *paddr);
			break;
	}

	*k0 = PFN_TO_K0(klp, *pfn) | (*paddr & (K_PAGESZ(klp) - 1));
	*k1 = PFN_TO_K1(klp, *pfn) | (*paddr & (K_PAGESZ(klp) - 1));

	/* Get the k2 address if one is mapped. If we already saw an error
	 * (physical memory not installed), skip this step (obviously).
	 */
	if (!e && (*k2 = (kaddr_t)kl_pfn_to_K2(klp, *pfn))) {
		*k2 |= (*paddr & (K_PAGESZ(klp) - 1));
	}
	
	if (e = kl_valid_physmem(klp, *paddr)) {
		return(e);
	}
	return(0);
}

/*
 * check_node_memory()
 *
 *   Check to see if the node_memory table has been initialized. If
 *   it hasn't, or if the system doesn't contain node memory, then
 *   return 1. Otherwise, return 0.
 */
int
check_node_memory(klib_t *klp)
{
	if (node_memory) {
		return(0);
	}
	fprintf(KL_ERRORFP, "Initializeing node memory table...\n");
	return(map_system_memory(klp));
}

/*
 * map_system_memory()
 */
int
map_system_memory(klib_t *klp)
{
	if (K_IP(klp) == 27) {
		return(map_node_memory(klp));
	}

	node_memory = (node_memory_t**)klib_alloc_block(klp, 1, K_PERM);

	/* Allocate space for the node_memory struct for our single node. 
	 * With non-IP27 systems, there will only ever be one node.
	 *
	 * XXX - For now, we will not be allocating space for any memory 
	 *       bank information (like we do with IP27 node_memory). 
	 */
	node_memory[0] = 
		(node_memory_t*)klib_alloc_block(klp, sizeof(node_memory_t), K_PERM);

	/* Fill in the structure. All we need to do is set size as all the
	 * other values are zero...
	 */
	node_memory[0]->n_slot = strdup("na");
	node_memory[0]->n_flag = 1; /* Always enabled? */
	node_memory[0]->n_memsize = (K_PHYSMEM(klp) * K_NBPC(klp)) / 0x100000;
	return(0);
}

/*
 * map_node_memory()
 */
int
map_node_memory(klib_t *klp)
{
	int i, j, s, len, module, flag, size, banks, bank_size;
	int vhndl, node_vhndl, slot_vhndl;
	int pages, nasid;
	k_uint_t nodeid;
	path_t *path;
	path_chunk_t *pcp;
	kaddr_t value, addr, nodepda, meminfo, membank;
	k_ptr_t gvp, gep, gip, meminfop, membankp, mbp, npdap;
	char label[256];

	/* Allocate space for node_memory array
	 */
	node_memory = 
		(node_memory_t**)klib_alloc_block(klp, (K_NASID_BITMASK(klp) + 1) * 
			K_NBPW(klp), K_PERM);

	/* Allocate some buffers...
	 */
	gvp = klib_alloc_block(klp, VERTEX_SIZE(klp), K_TEMP);
	gip = klib_alloc_block(klp, GRAPH_INFO_S_SIZE(klp), K_TEMP);
	gep = klib_alloc_block(klp, GRAPH_EDGE_S_SIZE(klp), K_TEMP);
	meminfop = klib_alloc_block(klp, INVENT_MEMINFO_SIZE(klp), K_TEMP);
	npdap = klib_alloc_block(klp, NODEPDA_S_SIZE(klp), K_TEMP);

	for (i = 0; i < K_NUMNODES(klp); i++) {

		/* Get the address of the nodepda_s struct for this node. We 
		 * can get the nasid and vertex handl from there.
		 */
		value = (K_NODEPDAINDR(klp) + (i * K_NBPW(klp)));
		kl_get_kaddr(klp, value, &nodepda, "nodepda_s");
		if (KL_ERROR) {
			continue;
		}
		kl_get_struct(klp, nodepda, NODEPDA_S_SIZE(klp), npdap, "nodepda_s");
		if (KL_ERROR) {
			continue;
		}
		nasid = kl_addr_to_nasid(klp, nodepda);
		module = KL_UINT(klp, npdap, "nodepda_s", "module_id");

		/* With certain extremely short memory dumps, the page of memory
		 * that contains the hwgraph is not included in the dump. In this
		 * case, we will not be able to tell the size of a vertex nor will
		 * we be able to use any of the hwgraph related information. We
		 * can use the VERTEX_SIZE() macro to test for this. Note that the
		 * information we get for memory may be incomplete and/or incorrect.
		 */
		if (VERTEX_SIZE(klp) == 0) {
			k_ptr_t node_pg_data;

			node_pg_data = (k_ptr_t)((k_uint_t)npdap +
							FIELD("nodepda_s", "node_pg_data"));
			size = kl_int(klp, node_pg_data, "pg_data", "node_total_mem", 0);

			/* Convert size to MB
			 */
			size = (size * K_NBPC(klp)) / 0x100000;

			node_memory[nasid] = (node_memory_t*)klib_alloc_block(klp, 
				sizeof(node_memory_t), K_PERM);

			node_memory[nasid]->n_module = module;
			node_memory[nasid]->n_slot = strdup("na");
			node_memory[nasid]->n_nodeid = i; /* XXX this may not be correct */
			node_memory[nasid]->n_nasid = nasid;
			node_memory[nasid]->n_memsize = size; 
			node_memory[nasid]->n_numbanks = 0;
			node_memory[nasid]->n_flag = 1;
			continue;
		}

		/* Get the node vertex handle from the nodepda_s sturct
		 */
		node_vhndl = KL_UINT(klp, npdap, "nodepda_s", "node_vertex");

		/* We have to walk back a couple of elements in order to get
		 * the slot name
		 */
		if (vhndl = connect_point(klp, node_vhndl)) {
			if (slot_vhndl = connect_point(klp, vhndl)) {
				if (!kl_get_graph_vertex_s(klp, slot_vhndl, 
							0, gvp, 0) || KL_ERROR) {
					continue;
				}
				if (!vertex_edge_vhndl(klp, gvp, vhndl, gep) || KL_ERROR) {
					continue;
				}
				if (get_edge_name(klp, gep, label)) {
					continue;
				}
			}
		}
		else {
			continue;
		}

		if (!kl_get_graph_vertex_s(klp, node_vhndl, 0, gvp, 0) || KL_ERROR) {
			continue;
		}
		if (vertex_info_name(klp, gvp, "_cnodeid", gip)) {
			nodeid = KL_UINT(klp, gip, "graph_info_s", "i_info");
		}
		else {
			nodeid = -1;
		}

		/* Get the memory vertex info entry
		 */
		if (!vertex_edge_name(klp, gvp, "memory", gep) || KL_ERROR) {
			continue;
		}
		vhndl = KL_UINT(klp, gep, "graph_edge_s", "e_vertex");
		if (!kl_get_graph_vertex_s(klp, vhndl, 0, gvp, 0) || KL_ERROR) {
			continue;
		}

		/* Get the _detail_invent info entry
		 */
		if (!vertex_info_name(klp, gvp, "_detail_invent", gip) || KL_ERROR) {
			continue;
		}

		meminfo = kl_kaddr(klp, gip, "graph_info_s", "i_info");
		kl_get_struct(klp, meminfo, INVENT_MEMINFO_SIZE(klp), meminfop, 
				"invent_meminfo");
		if (KL_ERROR) {
			continue;
		}
		module = KL_UINT(klp, meminfop, "invent_generic_s", "ig_module");
		flag = KL_UINT(klp, meminfop, "invent_generic_s", "ig_flag");

		size = KL_UINT(klp, meminfop, "invent_meminfo", "im_size");
		banks = KL_UINT(klp, meminfop, "invent_meminfo", "im_banks");

		node_memory[nasid] = (node_memory_t*)klib_alloc_block(klp, 
			sizeof(node_memory_t) + ((banks - 1) * sizeof(banks_t)), K_PERM);

		node_memory[nasid]->n_module = module;

		node_memory[nasid]->n_slot = strdup(label);
		node_memory[nasid]->n_nodeid = nodeid;
		node_memory[nasid]->n_nasid = nasid;
		node_memory[nasid]->n_memsize = size; 
		node_memory[nasid]->n_numbanks = banks;
		node_memory[nasid]->n_flag = flag;
		bank_size = (banks * kl_struct_len(klp, "invent_membnkinfo"));
		membankp = klib_alloc_block(klp, bank_size, K_TEMP);
		membank = meminfo + 
			kl_member_offset(klp, "invent_meminfo", "im_bank_info");

		kl_get_block(klp, membank, bank_size, membankp, "invent_membnkinfo");
		mbp = membankp;
		
		for (j = 0; j < banks; j++) {
			s = KL_UINT(klp, mbp, "invent_membnkinfo", "imb_size");
			node_memory[nasid]->n_bank[j].b_paddr = 
				((k_uint_t)nasid << K_NASID_SHIFT(klp)) + 
				((k_uint_t)j * K_MEM_PER_BANK(klp));
			if (s) {
				node_memory[nasid]->n_bank[j].b_size = s;
			}
			len = kl_struct_len(klp, "invent_membnkinfo");
			mbp = (k_ptr_t) ((uint)mbp + len);
		}
		klib_free_block(klp, membankp);
	}

	/* We have to check to see if gvp contains a pointer to a
	 * block that needs to be freed. The size of a vertex is 
	 * derived (it's not obtained from the symbol table). If 
	 * we have problems determining the vertex size (normally 
	 * because key memory pages are missing from the dump), gvp 
	 * will be NULL. The rest of the blocks should never be
	 * NULL.
	 */
	if (gvp) {
		klib_free_block(klp, gvp);
	}
	klib_free_block(klp, gip);
	klib_free_block(klp, gep);
	klib_free_block(klp, npdap);
	klib_free_block(klp, meminfop);
	return(0);
}

/* 
 * kl_valid_nasid()
 */
int
kl_valid_nasid(klib_t *klp, int nasid)
{
    if (check_node_memory(klp)) {
		return(0);
	}
	if ((K_IP(klp) == 27) && node_memory[nasid]) {
		return(1);
	}
	else if (nasid == 0) {
		return(1);
	}
	return(0);
}

/*
 * kl_addr_to_nasid()
 */
int
kl_addr_to_nasid(klib_t *klp, kaddr_t addr) 
{
	return((addr >> K_NASID_SHIFT(klp)) & K_NASID_BITMASK(klp));
}

/*
 * kl_addr_to_slot()
 */
int
kl_addr_to_slot(klib_t *klp, kaddr_t addr) 
{
	return((addr >> K_SLOT_SHIFT(klp)) & K_SLOT_BITMASK(klp));
}

/*
 * kl_valid_physmem()
 */
int
kl_valid_physmem(klib_t *klp, kaddr_t paddr)
{
	/* If this routine is called before all the proper global
	 * variables are set, we have to just return with success.
	 */
	if (!mem_validity_check || (K_FLAGS(klp) & K_IGNORE_MEMCHECK)) {
		return(0);
	}

	if (K_RAM_OFFSET(klp)) {
		if (paddr < K_RAM_OFFSET(klp)) {
			return(KLE_BEFORE_RAM_OFFSET);
		}
	}

	/* Now check for valid physmem based on the type of system 
	 * architecture
	 */
	if (K_NASID_SHIFT(klp)) {
		int nasid, slot;
		kaddr_t slot_start, slot_size;

		nasid = kl_addr_to_nasid(klp, paddr);
		if (!node_memory[nasid]) {
			return(KLE_PHYSMEM_NOT_INSTALLED);
		}
		slot = kl_addr_to_slot(klp, paddr);
		slot_start = node_memory[nasid]->n_bank[slot].b_paddr;
		slot_size = 
			node_memory[nasid]->n_bank[slot].b_size * (1024*1024);

		if (!slot_size || !((paddr >= slot_start) && 
				    (paddr < (slot_start + slot_size)))) 
		{
			return(KLE_PHYSMEM_NOT_INSTALLED);
		}
	}
	else {
		int pfn;

		pfn = paddr >> K_PNUMSHIFT(klp);
		if (K_RAM_OFFSET(klp)) {
			if ((pfn - (K_RAM_OFFSET(klp) >> 
				    K_PNUMSHIFT(klp))) >= K_PHYSMEM(klp)) {
				return(KLE_AFTER_PHYSMEM);
			}
		}
		else if (pfn >= K_PHYSMEM(klp)) {
			return(KLE_AFTER_PHYSMEM);
		}
	}
	return(0);
}
