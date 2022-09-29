#ident "$Header: "

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>

/* Macro that returns the proper physical address. This depends on weather
 * or not this is a live system or coredump. Also, weather or not there is
 * a ram_offset.
 */
#define RETURN_PADDR(paddr) return((!K_RAM_OFFSET || \
	   ACTIVE) ? paddr : (paddr - K_RAM_OFFSET))

/*
 * kl_virtop() -- Virtual to physical address translation
 *
 */
kaddr_t
kl_virtop(kaddr_t vaddr, k_ptr_t ktp)
{
	int s, e, pfn, ktp_alloced = 0;
	kaddr_t kthread, addr, paddr, kptbl_addr; 
	k_uint_t pde, pgi;
	k_ptr_t pte = 0;

	if (DEBUG(KLDC_FUNCTRACE, 5)) {
		fprintf(KL_ERRORFP, "kl_virtop: vaddr=0x%llx, ktp=0x%x\n", vaddr, ktp);
	}

	kl_reset_error();

	if (IS_HUBSPACE(vaddr)) {
		return(vaddr);
	}

	if (is_kseg0(vaddr)) {
		paddr = k0_to_phys(vaddr);
		if (e = kl_valid_physmem(paddr)) {
			KL_SET_ERROR_NVAL(e, paddr, 2);
			return(paddr);
		}
		RETURN_PADDR(paddr);
	}

	if (is_kseg1(vaddr)) {
		paddr = k1_to_phys(vaddr);
		if (e = kl_valid_physmem(paddr)) {
			KL_SET_ERROR_NVAL(e, paddr, 2);
			return(paddr);
		}
		RETURN_PADDR(paddr);
	}

	if (is_kseg2(vaddr)) {

		/* handle mapped kernel addresses here (to k0 base!) 
		 */
		if ((is_mapped_kern_ro(vaddr)) || (is_mapped_kern_rw(vaddr))) {

			/* clear out text/data field 
			 */
			vaddr &= 0xffffffff00ffffff;

			/* correct address if master_nodeid is non-zero.
			 */
			if (K_MASTER_NASID) {
				vaddr += ((k_uint_t)K_MASTER_NASID) << 32;
			}

			/* get physical address based on k0 offset 
			 */
			paddr = k0_to_phys(vaddr);
			if (e = kl_valid_physmem(paddr)) {
				KL_SET_ERROR_NVAL(e, paddr, 2);
			}
			RETURN_PADDR(paddr);
		}

		kptbl_addr = (K_KPTBL + (((vaddr - K_K2BASE) >> 
				K_PNUMSHIFT) * PDE_SIZE));
		if (DEBUG(KLDC_MEM, 1)) {
			fprintf(KL_ERRORFP, "kl_virtop: kptbl_addr=0x%llx\n", kptbl_addr);
		}
		pte = kl_get_pde(kptbl_addr, &pde);
		if (KL_ERROR) {
			return(vaddr);
		}
		pgi = kl_get_pgi(pte);
		if (DEBUG(KLDC_MEM, 1)) {
			fprintf(KL_ERRORFP, "kl_virtop: pte=0x%x, pde=0x%llx, "
				"pgi=0x%llx\n", pte, pde, pgi);
		}
		if (((k_uint_t)pgi & (k_uint_t)K_PDE_PG_VR) == 0) {
			if (DEBUG(KLDC_MEM, 1)) {
				fprintf(KL_ERRORFP, "kl_virtop: KSEG2 address %llx "
					"invalid\n", vaddr);
			}
			KL_SET_ERROR_NVAL(KLE_INVALID_K2_ADDR, vaddr, 2);
			return(vaddr);
		}
		paddr = (Ctob(kl_pte_to_pfn(pte)) + (vaddr & (K_PAGESZ-1)));
		if (DEBUG(KLDC_MEM, 1)) {
			fprintf(KL_ERRORFP, "kl_virtop: paddr=0x%llx\n", paddr);
			fprintf(KL_ERRORFP, "kl_virtop: pfn_mask=0x%llx "
				"pfn_shift=%d\n", K_PFN_MASK, K_PFN_SHIFT);
		}
		RETURN_PADDR(paddr);
	}

	/* The remaining translations depends upon us having a pointer
	 * to a uthread_s struct. Either the kthread parameter (if one is
	 * passed in) or K_DEFKTHREAD() will be used. If both kthread
	 * parameter and K_DEFKTHREAD() are NULL, then use K_DUMPKTHREAD()
	 * (the kthread for dumpproc). If no uthread pointer is available
	 * or the kthread is not of type KT_UTHREAD, then return an error.
	 */
	if (!ktp) {
		if (K_DEFKTHREAD) {
			kthread = K_DEFKTHREAD;
		}
		else if (K_DUMPKTHREAD) {
			kthread = K_DUMPKTHREAD;
		}
		else {
			KL_SET_ERROR(KLE_NO_DEFKTHREAD);
			goto merror;
		}
		ktp = kl_get_kthread(kthread, K_TEMP);
		if (KL_ERROR) {
			goto merror;
		}
		ktp_alloced = 1;
	}

	/* Check to make sure the kthread we have is of type KT_UTHREAD. If
	 * it isn't, then we have to return an error (there is no way we can 
	 * translate the kernelstack address).
	 */
	if (!IS_UTHREAD(ktp)) {
		KL_SET_ERROR_NVAL(KLE_WRONG_KTHREAD_TYPE, kthread, 2);
		if (ktp_alloced) {
			kl_free_block(ktp);
		}
		goto merror;
	}

	/* Now we need to check and see if vaddr is from the kernel 
	 * stack page or kernel stack extension page.
	 */
	if ((vaddr >= K_KERNELSTACK) && (vaddr < K_KERNSTACK)) {
		pte = (k_ptr_t)(ADDR(ktp, "uthread_s", "ut_kstkpgs"));
		pfn = kl_pte_to_pfn(pte);
	}
	else if (K_EXTUSIZE && (vaddr >= K_KEXTSTACK) && (vaddr < K_KERNELSTACK)) {
		pte = (k_ptr_t)(ADDR(ktp, "uthread_s", "ut_kstkpgs") + 
				(PDE_SIZE * K_EXTSTKIDX));
		pfn = kl_pte_to_pfn(pte);
		if (KL_ERROR) {
			/* Check and see if the current kthread is running on a
			 * cpu. If it is, then get the mapping for the kextstack
			 * page from the pda_s struct.
			 */
			int cpuid;

			cpuid = KL_INT(ktp, "kthread", "k_sonproc");
			if (cpuid != -1) {
				kaddr_t pda;
				k_ptr_t pdap;

				pdap = kl_alloc_block(PDA_S_SIZE, K_TEMP);
				kl_get_pda_s((kaddr_t)cpuid, pdap);
				if (KL_ERROR) {
					if (ktp_alloced) {
						kl_free_block(ktp);
					}
					goto merror;
				}
				else {
					pte = (k_ptr_t)ADDR(pdap, "pda_s", "p_stackext");
					pfn = kl_pte_to_pfn(pte);
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

	/* Free the kthread buffer, if one was allocated within this routine
	 */
	if (ktp_alloced) {
		kl_free_block(ktp);
	}

	if (!KL_ERROR) {
		paddr = Ctob(pfn) | (vaddr & (K_PAGESZ - 1));
		RETURN_PADDR(paddr);
	}

merror:

	if (!KL_ERROR) {
		KL_SET_ERROR_NVAL(KLE_INVALID_VADDR, vaddr, 2);
	}
	return(vaddr);
}

/*
 * kl_is_valid_kaddr()
 */
int
kl_is_valid_kaddr(kaddr_t addr, k_ptr_t ktp, int flags)
{
	if (DEBUG(KLDC_FUNCTRACE, 5)) {
		fprintf(KL_ERRORFP, "addr=0x%llx, ktp=0x%x\n", addr, ktp);
	}

	kl_reset_error();

	kl_virtop(addr, ktp);
	if (KL_ERROR) {
		return(0);
	}
	if ((flags & WORD_ALIGN_FLAG) && (addr % K_NBPW)) {
		KL_SET_ERROR_NVAL(KLE_INVALID_VADDR_ALIGN, addr, 2);
		return(0);
	}
	return(1);
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
kl_addr_convert(kaddr_t value, int flag, kaddr_t *paddr, 
	uint *pfn, kaddr_t *k0, kaddr_t *k1, kaddr_t *k2) 
{
	int e;

	switch (flag) {

		case PFN_FLAG :
			*pfn = (uint)value;
			if (*pfn > K_MAXPFN) {
				return(KLE_AFTER_MAXPFN);
			}
			*paddr = PFN_TO_PHYS(*pfn);
			break;

		case PADDR_FLAG :
			*paddr = value;
			if (*paddr >= ((k_uint_t)K_MAXPFN * K_PAGESZ)) {
				return(KLE_AFTER_MAXMEM);
			}
			*pfn = Pnum(*paddr);
			break;

		case VADDR_FLAG :
			*paddr = kl_virtop(value, (k_ptr_t)NULL);
			if (e = KL_ERROR) { 
				if (!((KL_ERROR == KLE_AFTER_PHYSMEM) || 
							(KL_ERROR == KLE_PHYSMEM_NOT_INSTALLED))) {
					KL_RETURN_ERROR;
				}
			}
			else if (K_RAM_OFFSET) {
				/* kl_virtop() returns the offset to physical address 
				 * (starting from memory address zero). So, we have to 
				 * add the ram_offset back in order to get the proper 
				 * physical address from the machines point of view.
				 */
				*paddr |= K_RAM_OFFSET;
			}
			*pfn = Pnum(*paddr);
			break;
	}

	*k0 = PFN_TO_K0(*pfn) | (*paddr & (K_PAGESZ - 1));
	*k1 = PFN_TO_K1(*pfn) | (*paddr & (K_PAGESZ - 1));

	/* Get the k2 address if one is mapped. If we already saw an error
	 * (physical memory not installed), skip this step (obviously).
	 */
	if (!e && (*k2 = (kaddr_t)kl_pfn_to_K2(*pfn))) {
		*k2 |= (*paddr & (K_PAGESZ - 1));
	}
	
	if (e = kl_valid_physmem(*paddr)) {
		return(e);
	}
	return(0);
}

/*
 * is_mapped_kern_ro() -- Determine if a given address is a mapped kernel
 *                        address in a given read-only range.
 */
int
is_mapped_kern_ro(kaddr_t addr)
{
	if ((addr >= K_MAPPED_RO_BASE) &&
			(addr < K_MAPPED_RO_BASE + K_MAPPED_PAGE_SIZE)) {
		return 1;
	}
	return 0;
}

/*
 * is_mapped_kern_rw() -- Determine if a given address is a mapped kernel
 *                        address in a given read-write range.
 */
int
is_mapped_kern_rw(kaddr_t addr)
{
	if ((addr >= K_MAPPED_RW_BASE) &&
			(addr < K_MAPPED_RW_BASE + K_MAPPED_PAGE_SIZE)) {
		return 1;
	}
	return 0;
}

/*
 * kl_pfn_to_K2() -- Convert a Page Frame Number (pfn) to a K2 address
 *
 *   Search through the kernel page table for a pfn that matches.
 */
kaddr_t
kl_pfn_to_K2(uint pfn)
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
	if (ACTIVE || !K_KPTBLP) {
		K_KPTBLP = kl_alloc_block(K_SYSSEGSZ * PDE_SIZE, K_TEMP);
		kl_get_block(K_KPTBL, 
			(K_SYSSEGSZ * PDE_SIZE), K_KPTBLP, "pde in kptbl");
		if (KL_ERROR) {
			kl_free_block(K_KPTBLP);
			K_KPTBLP = 0;
			return((kaddr_t)0);
		}
		kptbl_alloced++;
	}
	for (i = 0; i < K_SYSSEGSZ; i++) {
		pte = (k_ptr_t)((uint)K_KPTBLP + (i * PDE_SIZE));
		pgi = kl_get_pgi(pte);

		/* Use the Pde_pg_vr mask instead of pte bit_field because
		 * of performance issues (we don't want to go down the dwarf
		 * path.
		 */
		if ((pgi & K_PDE_PG_VR) == 0) {
			continue;
		}
		if (pfn == kl_pte_to_pfn(pte)) {
			k2addr = Ctob(i) | K_K2BASE;
			if (kptbl_alloced) {
				kl_free_block(K_KPTBLP);
				K_KPTBLP = 0;
			}
			return((kaddr_t)k2addr);
		}
	} 
	if (kptbl_alloced) {
		kl_free_block(K_KPTBLP);
		K_KPTBLP = 0;
	}
	return((kaddr_t)0);
}

/*
 * kl_get_addr_mode()
 */
int
kl_get_addr_mode(kaddr_t addr)
{
	int mode = -1;

	if (DEBUG(KLDC_FUNCTRACE, 6)) {
		fprintf(KL_ERRORFP, "kl_get_addr_mode: addr=0x%llx\n", addr);
	}

	if (is_kseg0(addr)) {
		mode = 0;
	} 
	else if (is_kseg1(addr)) {
		mode = 1;
	} 
	else if (is_kseg2(addr)) {
		mode = 2;
	} 
	else if (is_kpteseg(addr)) {

		/* For now, just return an error...
		 */
		if (DEBUG(KLDC_MEM, 1)) {
			fprintf(KL_ERRORFP, "Can't print KPTESEG yet\n");
		}
		KL_SET_ERROR_NVAL(KLE_INVALID_VADDR, addr, 2);
		return(-1);
	} 
	else if ((addr >= K_KERNELSTACK) && (addr < K_KERNSTACK)) {
		mode = 3;
	}
	else {
		int e;

		if (e = kl_valid_physmem(addr)) {
			KL_SET_ERROR_NVAL(e, addr, 2);
			if (e != KLE_AFTER_PHYSMEM) {
				return(-1);
			}
		}
		mode = 4; /* physical address mode */
	}

	if ((mode >= 0) && (mode <= 3)) {
		kl_virtop(addr, (k_ptr_t)NULL);
		if (KL_ERROR) {
			return(-1);
		}
	}
	return(mode);
}

/*
 * kl_addr_to_nasid()
 */
int
kl_addr_to_nasid(kaddr_t addr) 
{
	return((addr >> K_NASID_SHIFT) & K_NASID_BITMASK);
}

/*
 * kl_addr_to_slot()
 */
int
kl_addr_to_slot(kaddr_t addr) 
{
	return((addr >> K_SLOT_SHIFT) & K_SLOT_BITMASK);
}
