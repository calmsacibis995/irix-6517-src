/**************************************************************************
 *									  *
 *		 Copyright (C) 1992-1996, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs	 contain  *
 *  unpublished	 proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may	not be disclosed  *
 *  to	third  parties	or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#if defined (SN0)  && defined (FORCE_ERRORS)
/*
 * File: error_force.c
 */
#include <sys/types.h>
#include <sys/systm.h>
#include "sn_private.h"
#include "error_private.h"
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>

/*
 * Function   : error_force_set_dir_state
 *
 * Purpose    : To set the memory state pointed to in uap to whate ever legal value 
 *              the user desires.

 * Returns    : 0 - success, -1 - failure
 */ 

int 
error_force_set_dir_state(md_error_params_t *uap,rval_t *rvp) 
{

  if (uap->mem_addr == NULL || !(uap->flags & ERRF_USER)) {
    rvp->r_val1 = -1;
    return -1;
  }
  /* States can only be three bits */
  if (uap->usr_def > 0x7) {
    rvp->r_val1 = -1;
    return -1;
  }
  set_dir_state(kvtophys((void*)uap->mem_addr),uap->usr_def & 0x7);
  
  return 0;
}	

/* 
 * Function       : errora_force_protoerr
 * Parameters     : addr -> memory to inject error onto
 *                  flag -> ERRF_WRITE, ERRF_READ or ERRF_SHOW, CORRUPT_STATE
 *                  rvp  -> return val
 * Purpose        : To force a memory page error with the memory protocol.
 * Returned       : 0 for success, -1 for failure
 */

int 
error_force_protoerr(md_error_params_t *uap, rval_t *rvp)
{       
	volatile __psunsigned_t addr = uap->mem_addr;
	paddr_t paddr;
	cacheop_t cop;
	__psunsigned_t alloc_addr = 0;
	int mem_alloc = 0;
	
	if (addr == NULL) {
	  if ((alloc_addr = (__psunsigned_t)error_alloc(4096)) == NULL)
	    return ENOMEM;

	  addr = alloc_addr;
	  mem_alloc = 1;
	}
	
	paddr = kvtophys((void *)addr);
	
	cmn_err(CE_NOTE, "!error_force_protoerr: addr vaddr 0x%lx (paddr 0x%lx)\n",
		addr, paddr);
	
	error_force_daccess((__uint64_t *)addr, uap->read_write, uap->access_val,
			    uap->flags, rvp);
	
	/* here is where we corrupt the page */
	if (uap->corrupt_target & CORRUPT_STATE)
		set_dir_state(paddr, 0);
	else
		set_dir_owner(paddr, -1);
	
	/* Here is where we empty the cache, so we must access the page */
	ecc_cacheop_get(CACH_SD + C_HWBINV, addr, &cop);
	
	error_force_daccess((__uint64_t *)addr, uap->read_write, uap->access_val,
			    uap->flags, rvp);
	
	cmn_err(CE_NOTE, "!error_force_protoerror: done, addr 0x%x dir 0x%lx\n",
		addr, *(__uint64_t *)BDDIR_ENTRY_LO(paddr));
	
	if (mem_alloc) {
		kmem_free((void *)addr, 4096);
	}
	return 0;
}


/*
 * Function	: error_force_poisonerr
 * Parameters	: addr -> memory of page to poison
 *                type -> flag for ERRF_WRITE, ERRF_READ or ERRF_SHOW
 *                rvp  -> return val
 * Purpose	: to poison a page and then access it
 * Returns	: Returns 0 on success, -1 on failure.
 */
int 
error_force_poisonerr(md_error_params_t *uap, rval_t *rvp)
{		
	volatile __psunsigned_t addr = uap->mem_addr;
	paddr_t paddr;
	cacheop_t cop;
	__psunsigned_t alloc_addr;	
	extern void poison_page(paddr_t);
	
	if ((alloc_addr = (__psunsigned_t)error_alloc(4096)) == NULL)
		return ENOMEM;
	
	if (addr == NULL) addr = alloc_addr;
	
	paddr = kvtophys((void *)addr);
	
	printf("error_force_poisonerr: vaddr 0x%lx (paddr 0x%lx)\n", addr, paddr);
	
	poison_page(paddr);
	
	ecc_cacheop_get(CACH_SD + C_HWBINV, addr, &cop);
	
	error_force_daccess((__uint64_t *)addr, uap->read_write, uap->access_val,
			    uap->flags, rvp);
	
	printf("error_force_poisonerror: done, addr 0x%x dir 0x%lx\n",
	       addr, *(__uint64_t *)BDDIR_ENTRY_LO(paddr));
	
	kmem_free((void *)alloc_addr, 4096);
	return 0;
}


/* UNknown if this really works */
/*
 * Function	: error_force_nackerr
 * Parameters	: addr -> the memory to access as a double word
 *                flag -> flag for ERRF_WRITE, ERRF_READ or ERRF_SHOW
 *                rvp  -> return value
 * Purpose	: to lock a page, and then request it so many times it times out.
 * Returns	: Returns 0 on success, -1 on failure.
 */

int 
error_force_nackerr(md_error_params_t* uap, rval_t *rvp)
{
	volatile __psunsigned_t addr=uap->mem_addr;
	paddr_t paddr;
	__psunsigned_t alloc_addr;	
	
	if ((alloc_addr = (__psunsigned_t)error_alloc(4096)) == NULL)
		return ENOMEM;
	
	if (addr == NULL) addr = alloc_addr;
	
	paddr = kvtophys((void *)addr);
	
	cmn_err(CE_NOTE,"!error_force_nackerror: vaddr 0x%lx (paddr 0x%lx)\n",
		addr, paddr);
	
	set_dir_state(paddr, (uap->usr_def >> 16) & 0xf);
	set_dir_owner(paddr, (uap->usr_def >> 20) & 0xff);
	
	error_force_daccess((__uint64_t *)addr, uap->read_write,uap->access_val,
			    uap->flags, rvp);
	
	kmem_free((void *)alloc_addr, 4096);
	return 0;
}


/*
 * Function	: error_force_direrror
 * Parameters	: addr -> the memory to force the directory error on
 *                type -> flag for ERRF_WRITE, ERRF_READ or ERRF_SHOW, ERRF_SBE
 *                rvp  -> return val
 * Purpose	: to corrupt the ECC on the memory directory.
 * Assumptions	: None.
 * Returns	: Returns 0 on success
 */
int
error_force_direrror(md_error_params_t *uap, rval_t *rvp)
{
	volatile __psunsigned_t addr = uap->mem_addr;
	paddr_t paddr;
	__uint64_t *dir_addr;
	__psunsigned_t alloc_addr;	
	int premium;
	md_sdir_low_t mdslow;
	md_pdir_low_t mdplow;	
	cacheop_t cop;
	
	if ((alloc_addr = (__psunsigned_t)error_alloc(4096)) == NULL)
		return ENOMEM;
	
	if (addr == NULL) addr = alloc_addr;
	
	paddr = kvtophys((void *)addr);
	ecc_cacheop_get(CACH_SD + C_HWBINV, addr, &cop);	
	printf("error_force_direrror: vaddr 0x%lx (paddr 0x%lx)\n",
	       addr, paddr);
	
	dir_addr = (__uint64_t *)BDDIR_ENTRY_LO(paddr);
	premium = (LOCAL_HUB_L(MD_MEMORY_CONFIG) & MMC_DIR_PREMIUM);
	
	if (premium) {
		mdplow.pd_lo_val = *(dir_addr);
		mdplow.pde_lo_fmt.pde_lo_ecc ^= ((uap->flags & ERRF_SBE) ? 0x1 : 0x11);
		*(dir_addr) = mdplow.pd_lo_val | MD_DIR_FORCE_ECC;
	} else {
		mdslow.sd_lo_val = *(dir_addr);
		mdslow.sde_lo_fmt.sde_lo_ecc ^= ((uap->flags & ERRF_SBE) ? 0x1 : 0x11);
		*(dir_addr) = (mdslow.sd_lo_val | MD_DIR_FORCE_ECC);
	}
	ecc_cacheop_get(CACH_SD + C_HINV, addr, &cop);	
	error_force_daccess((__uint64_t *)addr, uap->read_write, uap->access_val, uap->flags,
			    rvp);
	
	printf("error_force_direrror: done, addr 0x%x\n", addr, *dir_addr);
	kmem_free((void *)alloc_addr, 4096);
	return 0;
}
/* 
   Function      : error_force_memerror 
   Parameter     : addr -> memory to force error onto
                   flag -> PERF_SBE, ERF_READ, ERF_WRITE, ERF_SHOW, CORRUPT_DATA, 
                           CORRUPT_TAG 
                   rvp  -> return value
   Purpose       : to simulate a variety of memory errors, on both secondary
                   mem and the caches
   Returns       : 0 on success and -1 on error
   
*/

int 
error_force_memerror(md_error_params_t *uap,  rval_t *rvp)
{	
	__psunsigned_t addr = uap->mem_addr;
	paddr_t paddr;
	int mem_alloc = 0;

	/* If there is no memory supplied to inject an error we need to
	   allocate our own */
	if (addr == NULL) {
	  
		addr = (__psunsigned_t)error_alloc(4096);
		if (addr == NULL) {
			rvp->r_val1 = -1;
			return ENOMEM;
		}
		mem_alloc = 1; 
	}
	
	/* In any case get the pysical address */
	paddr = kvtophys((void *)addr);

	printf("error_force_memerror: corruption 0x%lx (paddr 0x%lx)\n",
	       addr, paddr);
	switch (uap->sub_code) {
		/* ERF_SBE, ERF_READ, ERF_WRITE & ERF_SHOW */
	case ETYPE_MEMERR:
		printf("forcing mem error: addr 0x%lx paddr 0x%lx\n",
		       addr, paddr);
		kl_force_memecc(paddr, uap, rvp);
		printf("forcing mem error, done\n");
		break;
		
		/* ERF_SBE, ERF_READ, ERF_WRITE & ERF_SHOW */
		/* CORRUPT_DATA, CORRUPT_TAG */
	case ETYPE_SCACHERR:
		printf("forcing scache error: addr 0x%lx paddr 0x%lx\n",
		       addr, paddr);
		kl_force_scecc(paddr, uap, rvp);
		printf("forcing scache error, done\n");
		break;
		
		/* ERF_SBE, ERF_READ, ERF_WRITE & ERF_SHOW */
		/* CORRUPT_DATA, CORRUPT_TAG */
	case ETYPE_PDCACHERR:
		printf("forcing dcache error: addr 0x%lx paddr 0x%lx\n",
		       addr, paddr);
		kl_force_pdecc(paddr, uap, rvp);
		printf("forcing dcache error, done\n");
		break;
		
		/* ??? */
	case ETYPE_PICACHERR:
		printf("forcing icache error: addr 0x%lx paddr 0x%lx\n",
		       addr, paddr);
		kl_force_piecc(paddr, uap->usr_def);
		printf("forcing dcache error, done\n");
		break;
		
	default:
		printf("Unsupported mem test\n");
		break;
		
	}
	
	/* Clean up the mem we allocated above. (If we did) */
	if (mem_alloc)
		kmem_free((void *)addr, 4096);
	return 0;
}

/*
 * Function	: error_force_prot
 * Parameters	: addr -> the memory to protect and then access
 *                type -> flag for ERRF_WRITE, ERRF_READ or ERRF_SHOW, ERRF_PROT_RO,
 *                        ERRF_ALL_NODES
 *                rvp  -> return val
 * Purpose	: to protect a page then access it
 * Assumptions	: None.
 * Returns	: Returns 0 on success
 */
error_force_prot(md_error_params_t *uap, rval_t *rvp)
{	
	volatile __psunsigned_t addr = uap->mem_addr;
	paddr_t paddr;
	int mem_alloc = 0;
	cacheop_t cop;
	
	if (addr == NULL) {
		addr = (__psunsigned_t)error_alloc(4096);
		if (addr == NULL) {
			rvp->r_val1 = -1;
			return ENOMEM;
		}
		mem_alloc = 1;
	}
	paddr = kvtophys((void *)addr);
	ecc_cacheop_get(CACH_SD + C_HWBINV, addr, &cop);
	printf("error_force_prot: addr 0x%lx (paddr 0x%lx) noaccess\n",
	       addr, paddr);
	
	/* Set page to read only for all or just some nodes */
	if (uap->flags & ERRF_ALL_NODES) {
		int i;	
		
		for (i = 0; i < cnodeid(); i++) {
			memory_set_access(paddr, get_region(i),
					  (uap->flags & ERRF_PROT_RO) ? 
					  MD_PROT_RO : MD_PROT_NO);
		}
	}
	else
		memory_set_access(paddr, get_region(cnodeid()),
				  (uap->flags & ERRF_PROT_RO) ? 
				  MD_PROT_RO : MD_PROT_NO);
	
	/* access the memory */
	error_force_daccess((__uint64_t *)addr, uap->read_write, uap->access_val,uap->flags, 
			    rvp);	
	
	printf("error_force_prot: done with error injection\n");
	
	if (mem_alloc)
		kmem_free((void *)addr, 4096);
	return 0;
	
}


/*
 * Function	: error_force_flush
 * Parameters	: addr -> the address to perform the flush on
 * Purpose	: to force a pio error
 * Assumptions	: None.
 * Returns	: Returns 0 on success.
 */

error_force_flush(__psunsigned_t addr)
{
	cacheop_t cop;
	/* invalidate the secondary cache */
	ecc_cacheop_get(CACH_SD + C_HWBINV, addr, &cop);
	return 0;
}	    


/*
 * Function   : eror_force_aerr_bte_pull
 * 
 * Purpose    : To have the bte issue a pull from non-populated memory
 * Returns    : 0 - success, -1 - failure
 */ 
int 
error_force_aerr_bte_pull(md_error_params_t *params, rval_t *rvp)
{
	void *nonpop_addr;
	void *dst;
	bte_handle_t *bte;
	int rc;

	EF_PRINT(("Reached ef_aerr_bte_pull\n"));

	if(!error_force_nodeValid(params->usr_def)) {
		printf("EF::Invalid node: 0x%lld\n",params->usr_def);
		return NULL;	
	}	

	/* first get the nonpopulated memory */
	nonpop_addr = (void*)EF_MEMBANK_ADDR(params->usr_def,7);

	dst = error_alloc(128);

	EF_PRINT(("BTE Pull for non-populated mem\n"
		     "NonPopulated Addr: 0x%llx (0x%llx)\n"
		     "Dst 0x%llx (0x%llx)\n"
		     "Len 0x%llx \n",
		     nonpop_addr, (paddr_t)nonpop_addr,
		     dst, kvtophys(dst),128));

	ASSERT(BTE_VALID_SRC((__psunsigned_t)nonpop_addr));
	ASSERT(BTE_VALID_DEST((__psunsigned_t)dst));
	ASSERT(BTE_VALID_LEN(128));
	
	/* Ask for bte and wait for it */
	bte = bte_acquire(1);
	
	/* DO the BTE pull */
	rc = bte_copy(bte,(paddr_t)nonpop_addr,kvtophys(dst),128,BTE_NORMAL);
	if (rc != BTE_SUCCESS) {
		printf("bte_copy failed and returned %d\n",rc);
		bte_release(bte);
		kmem_free(dst,128);
		rvp->r_val1 = -1;
		return -1;
		
	}
	
	bte_release(bte);
	kmem_free(dst,128);
	EF_PRINT(("Error handled\n"));
	return 0;

}

/*
 * Function   : error_force_bte_absent_node
 *
 * Purpose    : To issue a bte pull from or push to a memory address 
 *              of an absent node
 * Notes      : This function leaks 2 cache lines of memory each time 
 *              it is called successfully.
 * Returns    : 0 - success, -1 - failure
 */
 
int
error_force_bte_absent_node(int direction, rval_t *rvp)
{

	void *absent_mem;
	void *safe_mem;
	void *src;
	void *dst;
	int rc;
	bte_handle_t *bte;
	int retVal = -1;
	EF_PRINT(("Reached ef_bte_absent_node\n"));

	/* first get the nonpopulated memory */
	absent_mem = (void*)error_force_absent_node_mem();
	safe_mem = error_alloc(128);
	if (!absent_mem || !safe_mem) {
		printf("EF::Unable to allocate memory\n");
		goto absent_bte_done;
	}	
	
	EF_PRINT(("BTE Access for Absent Node\n"
		     "Absent_Mem 0x%llx (0x%llx)\n"
		     "Safe_Mem 0x%llx (0x%llx)\n"
		     "Len 0x%llx \n",
		     absent_mem, kvtophys(absent_mem),
		     safe_mem, kvtophys(safe_mem),128));

	/* If it is a pull then we want the src to be corrupt */
	if (direction == EF_BTE_PULL) {
		EF_PRINT(("EF::BTE Pull\n"));
		src = absent_mem;
		dst = safe_mem;
	} else {
		/* Otherwise we want the dst to be corrupt */
		ASSERT(direction == EF_BTE_PUSH);
		EF_PRINT(("EF::BTE Push\n"));
		src = safe_mem;
		dst = absent_mem;
	}

	/* Ask for bte and wait for it */
	bte = bte_acquire(1);

	/* DO the BTE pull */
	if(bte == NULL) {
		printf("EF::Could not acquire BTE\n");
		goto absent_bte_done;
	}
	/* Now execute the first bte_copy to poison the line */
	rc = bte_copy(bte,kvtophys(src),kvtophys(dst),128,BTE_NORMAL);
	if (rc != BTE_SUCCESS) {
	  printf("bte_copy failed and returned %d\n",rc);
	}

	retVal = 0;
	EF_PRINT(("Error handled\n"));

absent_bte_done:

	if (bte) bte_release(bte);
#if 0
	if (absent_mem) kmem_free(absent_mem,128);
	if (safe_mem) kmem_free(safe_mem,128);
#endif
	rvp->r_val1 = retVal;
	return retVal;
	

}

/*
 * Function   : error_force_perr_bte
 *
 * Purpose    : To issue a bte pull from or push to a memory address 
 *              that has been poisoned
 * Notes      : This function leaks 2 cache lines of memory each time 
 *              it is called successfully.
 * Returns    : 0 - success, -1 - failure
 */ 
int
error_force_perr_bte(int direction,rval_t* rvp)
{
	void *poison_mem;
	void *safe_mem;
	void *src;
	void *dst;
	int rc;
	bte_handle_t *bte;
	int retVal = -1;
	md_error_params_t params;
	EF_PRINT(("Reached ef_perr_bte_pull\n"));

	/* first get the nonpopulated memory */
	poison_mem = error_alloc(128);
	safe_mem = error_alloc(128);
	if (!poison_mem || !safe_mem) {
		printf("EF::Unable to allocate memory\n");
		goto perr_bte_done;
	}	
	
	EF_PRINT(("BTE access of perr\n"
		     "Poison_Mem 0x%llx (0x%llx)\n"
		     "Safe_Mem 0x%llx (0x%llx)\n"
		     "Len 0x%llx \n",
		     poison_mem, kvtophys(poison_mem),
		     safe_mem, kvtophys(safe_mem),128));

	/* Set up corruption of the directory of the cacheline */
	bzero(&params,sizeof(md_error_params_t));
	params.mem_addr = (__uint64_t)poison_mem;
	params.read_write = ERRF_NO_ACTION;
	
	/* Corrupt Memory */
	if (error_force_poisonerr(&params,rvp)) {
		printf("EF::Corruption failed.\n");
		goto perr_bte_done;
	}	

	/* If it is a pull then we want the src to be corrupt */
	if (direction == EF_BTE_PULL) {
		EF_PRINT(("EF::BTE Pull\n"));
		src = poison_mem;
		dst = safe_mem;
	} else {
		/* Otherwise we want the dst to be corrupt */
		ASSERT(direction == EF_BTE_PUSH);
		EF_PRINT(("EF::BTE Push\n"));
		src = safe_mem;
		dst = poison_mem;
	}

	/* Ask for bte and wait for it */
	bte = bte_acquire(1);

	/* DO the BTE pull */
	if(bte == NULL) {
		printf("EF::Could not acquire BTE\n");
		goto perr_bte_done;
	}
	/* Now execute the first bte_copy to poison the line */
	rc = bte_copy(bte,kvtophys(src),kvtophys(dst),128,BTE_NORMAL);
	if (rc != BTE_SUCCESS) {
	  printf("bte_copy failed and returned %d\n",rc);
	}

	retVal = 0;
	EF_PRINT(("Error handled\n"));

perr_bte_done:

	if (bte) bte_release(bte);
#if 0
	if (poison_mem) kmem_free(poison_mem,128);
	if (safe_mem) kmem_free(safe_mem,128);
#endif
	rvp->r_val1 = retVal;
	return retVal;
	

}

/*
 * Function   : error_force_bddir_bte
 *
 * Purpose    : To issue a bte pull from or push to a memory address 
 *              which has had its directory corrupted in HSPEC.
 * Notes      : This function leaks 2 cache lines of memory each time 
 *              it is called successfully.
 * Returns    : 0 - success, -1 - failure
 */ 
int
error_force_bddir_bte(md_error_params_t* params,int direction, rval_t* rvp)
{
	void *corrupt_mem;
	void *safe_mem;
	void *src;
	void *dst;
	int rc;
	bte_handle_t *bte;
	int retVal = -1;
	__uint64_t config_val;
	cnodeid_t node = params->usr_def;

	EF_PRINT(("Reached ef_bddir_bte\n"));

	/* First grab some memory */
	corrupt_mem = kmem_alloc_node(128,VM_DIRECT|VM_CACHEALIGN|KM_NOSLEEP,node);
	safe_mem = kmem_alloc_node(128,VM_DIRECT|VM_CACHEALIGN|KM_NOSLEEP,node);
	if (!corrupt_mem || !safe_mem) {
		printf("EF::Unable to allocate memory\n");
		goto bddir_bte_done;
	}	
	
	/* We have to force the hub to ignore the ecc error we are
           about to create. So we need MD_MEMORY_CONFIG */
	config_val = REMOTE_HUB_L(node,MD_MEMORY_CONFIG);
	EF_PRINT(("Loaded  Node:%d MD_MEMORY_CONFIG Addr:0x%X Val:0x%X\n",
		     node,MD_MEMORY_CONFIG,config_val));

	EF_PRINT(("BTE Pull for derr\n"
		     "Corrupt_Mem 0x%llx (0x%llx)\n"
		     "Safe_Mem 0x%llx (0x%llx)\n"
		     "Len 0x%llx \n",
		     corrupt_mem, kvtophys(corrupt_mem),
		     safe_mem, kvtophys(safe_mem),128));

	/* Set up corruption of the directory of the cacheline */
	bzero(params,sizeof(md_error_params_t));
	params->mem_addr = (__uint64_t)corrupt_mem;
	params->read_write = ERRF_NO_ACTION;
	
	/* Corrupt Memory */
	if (error_force_direrror(params,rvp)) {
		printf("EF::Corruption failed.\n");
		goto bddir_bte_done;
	}	

	/* Clear the ignore ecc bit */
	config_val &= ~MMC_IGNORE_ECC_MASK;

	EF_PRINT(("Storing Node:%d MD_MEMORY_CONFIG Addr:0x%X Val:0x%X\n",
		     node,MD_MEMORY_CONFIG,config_val));
	REMOTE_HUB_S(node,MD_MEMORY_CONFIG,config_val);

	/* If it is a pull then we want the src to be corrupt */
	if (direction == EF_BTE_PULL) {
		EF_PRINT(("EF::BTE Pull\n"));
		src = corrupt_mem;
		dst = safe_mem;
	} else {
		/* Otherwise we want the dst to be corrupt */
		ASSERT(direction == EF_BTE_PUSH);
		EF_PRINT(("EF::BTE Push\n"));
		src = safe_mem;
		dst = corrupt_mem;
	}
	
	/* Ask for bte and wait for it */
	bte = bte_acquire(1);
	if(bte == NULL) {
		printf("EF::Could not acquire BTE\n");
		goto bddir_bte_done;
	}

	/* Now execute the bte_copy */
	rc = bte_copy(bte,kvtophys(src),kvtophys(dst),128,BTE_NORMAL);
	if (rc != BTE_SUCCESS) {
		printf("bte_copy failed and returned %d\n",rc);
	}

	retVal = 0;
	EF_PRINT(("Error handled\n"));

bddir_bte_done:

	if (bte) bte_release(bte);
#if 0
	if (corrupt_mem) kmem_free(corrupt_mem,128);
	if (safe_mem) kmem_free(safe_mem,128);
#endif
	rvp->r_val1 = retVal;
	return retVal;
	

}

/*
 * Function   : error_force_corrupt_bte_pull
 *
 * Purpose    : To issue a bte pull from to a memory address 
 *              which has had its ECC corrupted.
 * Notes      : This function leaks 2 cache lines of memory each time 
 *              it is called successfully.
 * Returns    : 0 - success, -1 - failure
 */ 
int
error_force_corrupt_bte_pull(md_error_params_t *params, rval_t *rvp)
{
	
	void *src;
	void *dst;
	int rc;
	bte_handle_t *bte;
	int retVal = -1;
	cnodeid_t node = params->usr_def;

	EF_PRINT(("Reached ef_corrupt_bte_pull\n"));

	/* first get the nonpopulated memory */
	src = kmem_alloc_node(128,VM_DIRECT|VM_CACHEALIGN|KM_NOSLEEP,node);
	dst = kmem_alloc_node(128,VM_DIRECT|VM_CACHEALIGN|KM_NOSLEEP,node);
	if (!src || !dst) {
		printf("EF::Unable to allocate memory\n");
		goto _bte_pull_done;
	}	
	
	/* now get the stack memory that is valid for BTE */
	EF_PRINT(("BTE pull for corrupted Mem\n"
		     "Src 0x%llx (0x%llx)\n"
		     "Dst 0x%llx (0x%llx)\n"
		     "Len 0x%llx \n",
		     src, kvtophys(src),
		     dst, kvtophys(dst),128));

	/* Set up corruption of the directory of the cacheline */
	bzero(params,sizeof(md_error_params_t));
	params->sub_code = ETYPE_MEMERR;
	params->mem_addr = (__uint64_t)src;
	params->read_write = ERRF_NO_ACTION;
	
	/* Corrupt Memory */
	if (error_force_memerror(params,rvp)) {
		printf("EF::Corruption failed.\n");
		goto _bte_pull_done;
	}	

	/* Ask for bte and wait for it */
	bte = bte_acquire(1);
	if(bte == NULL) {
		printf("EF::Could not acquire BTE\n");
		goto _bte_pull_done;
	}
	
	/* Now execute the bte_copy */
	rc = bte_copy(bte,kvtophys(src),kvtophys(dst),128,BTE_NORMAL);
	if (rc != BTE_SUCCESS) {
		printf("bte_copy failed and returned %d\n",rc);
	}

	retVal = 0;
	EF_PRINT(("Error handled\n"));

_bte_pull_done:

	if (bte) bte_release(bte);
#if 0
	if (src) kmem_free(src,128);
	if (dst) kmem_free(dst,128);
#endif

	rvp->r_val1 = retVal;
	return retVal;

}
#endif /* SN && FORCE_ERRORS */
