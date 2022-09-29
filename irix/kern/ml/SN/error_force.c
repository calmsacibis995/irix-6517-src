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
#include <sys/PCI/bridge.h>
#include <sys/SN/klconfig.h>

#define MD_DIR_UNDEF 	0x8
#ifdef EF_DEBUG
int ef_print = 1;
#endif
void
error_force_init(void)
{
	
	
}

/*
 * Function : error_force_nodeValid
 *
 * Purpose : to tell the caller if the node is legitimate (which means
 * 	     existent and not disabled) 
 * Returns : 0 - if node does not exist or has at least one disabled cpu
 *	     1 - If node has two enabled cpu's
 */

int 
error_force_nodeValid(cnodeid_t node) 
{
	char buff[1024];    
	
	/* Accessing the hw graph */
	sprintf(buff,"/hw/nodenum/%d",node);
	if(hwgraph_path_to_vertex(buff) == GRAPH_VERTEX_NONE) {
		return 0;
	}

	if (is_headless_node(node)) {
		printf("Error: Node %d is disabled\n",node);
		return 0;
	}

	return 1;
	
}

/* 
 * Function       : error_alloc
 * Parameters     : <none>
 * Purpose        : To allocate a page of memory
 * Returned       : Memory that has just been allocated
 */

void *
error_alloc(int size)
{
	return kmem_alloc_node(size, VM_DIRECT | VM_CACHEALIGN|KM_NOSLEEP,
			       cnodeid());
	
}


/*
 * Function	: error_force_daccess
 * Parameters	: addr -> the memory to access as a double word
 *                type -> flag for ERRF_WRITE, ERRF_READ or ERRF_SHOW
 *                rvp  -> return val
 * Purpose	: to access the memory for read, write or both.
 * Assumptions	: addr points to valid data
 * Returns	: Returns 0 on success
 */

int
error_force_daccess(volatile __uint64_t *addr, int rdrw, int value,
		    int flag, rval_t *rvp)
{
	__uint64_t x;
	
	/* There are some cases where no action is desried */
	if (rdrw & ERRF_NO_ACTION) {
	  return 0;
	}

	if (rdrw & ERRF_READ) {
		x = *addr;
		if (flag & ERRF_SHOW)
			printf("Read 0x%x \n", x);
	}
	
	if (rdrw & ERRF_WRITE) {
		x = value;
		*addr = value;
		if (flag & ERRF_SHOW)
			printf("Wrote 0x%x \n", x);
	}
	if (rvp) rvp->r_val1 = x;
	return 0;
}


/*
 * Function	: error_force_waccess
 * Parameters	: addr -> the memory to access as a word
 *                type -> flag for ERRF_WRITE, ERRF_READ or ERRF_SHOW
 *                rvp  -> return val
 * Purpose	: to access the memory for read, write or both.
 * Assumptions	: addr point to valid data
 * Returns	: Returns 0 on success
 */

int
error_force_waccess(volatile __uint32_t *addr, int rdrw, int value,
		    int flag, rval_t *rvp)
{
	__uint64_t x;

	/* There are some cases where no action is desried */
	if (rdrw & ERRF_NO_ACTION) {
	  return 0;
	}


	if (rdrw & ERRF_READ) {
		x = *addr;
		if (flag & ERRF_SHOW)
			printf("Read 0x%x \n", x);
	}
	
	if (rdrw & ERRF_WRITE) {
		x = value;
		*addr = value;
		if (flag & ERRF_SHOW)
			printf("Wrote 0x%x \n", x);
	}
	if (rvp) rvp->r_val1 = x;
	return 0;
}

/*
 * Function   : error_force_absent_node_mem
 *
 * Purpose :   to determine an address for a node that is not present
 * Returns :   NULL if all nodes are present, otherwise the address.
 */

__int64_t
error_force_absent_node_mem(void)
{

  cnodeid_t node = error_force_get_absent_node();
  if (node == CNODEID_NONE) {
	  printf("All nodes are full\n");
	  return NULL;			
  }
	
  return NODE_CAC_BASE(node);

}

/*
 * Function : error_force_get_absent_node
 *
 * Purpose    : to return a nodeid that is not enabled.
 * Returns    : node that is absent or CNODEID_NONE.
 */ 

cnodeid_t
error_force_get_absent_node(void)
{
	vertex_hdl_t deviceVertex;
	char buff[1024];
	int node;

	/* We are looking for an absent widget. A place without a
	   device, a place inside the twilight zone */ 
	for (node = 0; node < MAX_COMPACT_NODES; node++) { 

		/* Accessing the hw graph */
		sprintf(buff,"/hw/nodenum/%d",node);
		deviceVertex = hwgraph_path_to_vertex(buff);
		/* We found a blank spot! */
		if (deviceVertex == GRAPH_VERTEX_NONE) {
			/* Masking out the higher bits, to prevent
                           memory scribbles */
			return node;
		}
	}

	return CNODEID_NONE;
}

/*
 * Function : print_error_params
 *
 * Purpose  : To show the contnents of error_params_t based on its
 * 	      sub-code (since it is a union)
 * Returns  : <none>
 */

void
print_error_params(error_params_t *uap) 
{
  ni_error_params_t *net_params = (ni_error_params_t*)uap;
  md_error_params_t *mem_params = (md_error_params_t*)uap;
  io_error_params_t *io_params  = (io_error_params_t*)uap;

  if (IS_IO_ERROR(io_params->sub_code)) {
    
    EF_PRINT(("Kernel Parameters:\n"
	      "------------------\n"		
	      "SubCode: 0x%llx\n"
	      "Node:    %lld\n"
	      "Widget:  %lld\n"
	      "Module:  %lld\n"
	      "Slot:    %lld\n"
	      "Usr1:    %lld\n"
	      "Usr2:    %lld\n",
	      io_params->sub_code, 
	      io_params->node,
	      io_params->widget,
	      io_params->module,
	      io_params->slot, 
	      io_params->usr_def1,
	      io_params->usr_def2));
    
  }
  
#if 0
  if (IS_MD_ERROR(io_params->sub_code)) {
    EF_PRINT(("Kernel Parameters:\n"
	      "------------------\n"		
	      "SubCode: 		0x%llx\n"
	      "Mem Addr:		0x%llx\n"
	      "Read_Write:   	0x%llx\n"
	      "Access_val:   	0x%lld\n"	
	      "Flags:        	0x%llx\n"
	      "Corrupt:		0x%llx\n"
	      "Usr_Def:		0x%llx\n",
	      mem_params->sub_code, mem_params->mem_addr,
	      mem_params->read_write, mem_params->access_val,mem_params->flags,
	      mem_params->corrupt_target, mem_params->usr_def));
  }	
#endif
  
  if (IS_NI_ERROR(io_params->sub_code)) {
    EF_PRINT(("Kernel Parameters:\n"
	      "------------------\n"		
	      "SubCode: 		0x%llx\n"
	      "Node:			0x%lld\n"
	      "Net_vec:	   		0x%llx\n"
	      "Port:	   		0x%llx\n"	
	      "Usr_Def1:        	0x%llx\n"
	      "Usr_Def2:        	0x%llx\n"
	      "Usr_Def3:        	0x%llx\n",
	      net_params->sub_code, net_params->node,
	      net_params->net_vec, net_params->port,net_params->usr_def1,
	      net_params->usr_def2, net_params->usr_def3));
  }	
  
}

/*
 * Function	: error_induce
 * Parameters	: uap -> The struct holding all of the arguements
 *                rvp  -> return value
 * Purpose	: to multiplex all the possible errors from one syscall
 * Returns	: Returns 0 on success, -1 on failure.
 */
error_induce(error_params_t *uap, rval_t *rvp)
{
	ni_error_params_t *net_params = (ni_error_params_t*)uap;
	md_error_params_t *mem_params = (md_error_params_t*)uap;
	io_error_params_t *io_params  = (io_error_params_t*)uap;

	int code = net_params->sub_code;
	int rc = 0;
	int state = MD_DIR_UNDEF;
	__uint64_t vec_ptr;
	hubreg_t elo;

	EF_PRINT_PARAMS(uap);
	
	/* If the user supplies the system call with a memory address,
	   it must be translated to an address the kernel can use. */
	
	/* If there is a memory address and the user accessed the memory,
	   translate it. */
	if ((code & MD_ERROR_BASE) && mem_params->mem_addr && 
	    (mem_params->flags & ERRF_USER)) {
	  
		paddr_t paddr;
		extern paddr_t error_force_get_user_paddr(__psunsigned_t);
		EF_PRINT(("EF::Converting memory 0x%llx\n",mem_params->mem_addr));
		paddr = error_force_get_user_paddr(mem_params->mem_addr);
		if (paddr == -1) {
			return EINVAL;
		}
		/* Record the memory state */
		get_dir_ent(paddr, &state, &vec_ptr, &elo);

		mem_params->mem_addr = (PHYS_TO_K0(paddr));
		EF_PRINT(("EF::New address is 0x%llx\n",mem_params->mem_addr));
	}

	switch (code) {
	case ETYPE_SET_DIR_STATE:
		rc = error_force_set_dir_state(mem_params,rvp);
		break;
	case ETYPE_SET_READ_ACCESS:	
		rc = error_force_set_read_access(io_params,1,rvp);
		break;	
	case ETYPE_CLR_READ_ACCESS:
		rc = error_force_set_read_access(io_params,0,rvp);
		break;	
	case ETYPE_XTALK_HDR_REQ:
		rc = error_force_pio_write_absent_reg(io_params,0,0,rvp);
		break;
	case ETYPE_XTALK_HDR_RSP:
		rc = error_force_pio_write_absent_reg(io_params,1,0,rvp);
		break;
	case ETYPE_ABSENT_WIDGET:
		rc = error_force_absent_widget(rvp);
		break;
	case ETYPE_FORGE_ID_WRITE:
		rc = error_force_forge_widgetId(io_params,EF_FORGE_WRITE,rvp);
		break;
	case ETYPE_FORGE_ID_READ:
		rc = error_force_forge_widgetId(io_params,EF_FORGE_READ,rvp);
		break;
	case ETYPE_LINK_RESET:
		rc = error_force_link_reset(net_params,rvp);
		break;
	case ETYPE_XTALK_SIDEBAND:
		rc = error_force_xtalk_sideband(io_params,rvp);
		break;	
	case ETYPE_AERR_BTE_PULL:
		rc = error_force_aerr_bte_pull(mem_params,rvp);
		break;
	case ETYPE_DERR_BTE_PULL:
		rc = error_force_bddir_bte(mem_params,EF_BTE_PULL,rvp);
		break;
	case ETYPE_WERR_BTE_PUSH:
		rc = error_force_bddir_bte(mem_params,EF_BTE_PUSH,rvp);
		break;
	case ETYPE_PERR_BTE_PULL:
		rc = error_force_perr_bte(EF_BTE_PULL,rvp);
		break;
	case ETYPE_PERR_BTE_PUSH:
		rc = error_force_perr_bte(EF_BTE_PUSH,rvp);
		break;
	case ETYPE_CORRUPT_BTE_PULL:
		rc = error_force_corrupt_bte_pull(mem_params,rvp);
		break;
	case ETYPE_CLR_OUTBND_ACCESS:
		rc = error_force_clr_outbound_access(io_params,rvp);
		break;
	case ETYPE_PIO_N_PRB_ERR:
		rc = error_force_pio_write_absent_reg(io_params,0,1,rvp);
		break;
	case ETYPE_CLR_LOCAL_ACCESS:
		rc = error_force_clr_local_access(io_params,rvp);
		break;
	case ETYPE_XTALK_CREDIT_TIMEOUT:
		rc = error_force_xtalk_credit_timeout(io_params,rvp);
		break;
	case ETYPE_SPURIOUS_MSG:
		rc = error_force_forge_widgetId(io_params,EF_FORGE_SPURIOUS,rvp);
		break;
	case ETYPE_SET_CRB_TIMEOUT:
		rc = error_force_set_CRB_timeout(io_params,0,rvp);
		break;
	case ETYPE_XTALK_USES_NON_POP_MEM:
		rc = error_force_xtalk_force_addr(io_params,EF_ABSENT_MEM_BANK,rvp);
		break;
	case ETYPE_XTALK_USES_ABSENT_NODE:
		rc = error_force_xtalk_force_addr(io_params,EF_ABSENT_NODE,rvp);
		break;
	case ETYPE_NO_NODE_BTE_PUSH:
		rc = error_force_bte_absent_node(EF_BTE_PUSH,rvp);
		break;
		/* Disable a router link */
	case ETYPE_RTR_LINK_DISABLE:
		error_force_rtr_link_down((cnodeid_t)net_params->node,
					  (net_vec_t)net_params->net_vec,
					  (int)net_params->port);
		break;
		/* Take a link down then panic */	
	case ETYPE_LINK_DN_N_PANIC:
		error_force_rtr_link_down((cnodeid_t)net_params->node,
					  (net_vec_t)net_params->net_vec,
					  (int)net_params->port);
		/* Now the link is down we force the mem error with a write */
		mem_params->sub_code = ETYPE_MEMERR;
		mem_params->mem_addr = 0;
		mem_params->read_write = ERRF_WRITE;
		error_force_memerror(mem_params,rvp);
		break;
		
		/* Error DWord Access */		
	case ETYPE_DACCESS:
		rc = error_force_daccess((__uint64_t *)mem_params->mem_addr, 
					 mem_params->read_write, mem_params->access_val,
					 mem_params->flags,rvp);
		break;
		
		/* Error Word Access */			
	case ETYPE_WACCESS:
		rc = error_force_waccess((__uint32_t *)mem_params->mem_addr,
					 mem_params->read_write,mem_params->access_val,
					 mem_params->flags,rvp);
		break;
		
		/* Error Directory Entry */		
	case ETYPE_DIRERR:
		rc = error_force_direrror(mem_params, rvp);
		break;
		
	case ETYPE_MEMERR:
	case ETYPE_PDCACHERR: /* Primary Data Cache Error */	      
	case ETYPE_SCACHERR:  /* Secondary Cache Entry */	      
	case ETYPE_PICACHERR: /* Primary Instr. Cache Error */	      
		rc = error_force_memerror(mem_params, rvp);
		break;
		
		/* Memory Protection Error */		
	case ETYPE_PROT:
		rc = error_force_prot(mem_params,rvp);
		break;
		
		/* Mem protocol error */		
	case ETYPE_PROTOCOL:
		rc = error_force_protoerr(mem_params, rvp);
		break;
		
		/* Poison page then access it error */	
	case ETYPE_POISON:
		rc = error_force_poisonerr(mem_params, rvp);
		break;
		/* NACK Timeout error */		
	case ETYPE_NACK:
		rc = error_force_nackerr(mem_params, rvp);
		break;
		/* Flush the cache */			
	case ETYPE_FLUSH:
		rc = error_force_flush(mem_params->mem_addr);
		break;
#ifdef FRU_DEBUG
		
		/* Node Region Error */			
	case ETYPE_RERR: 
		/* access non existent node */
		LOCAL_HUB_S(PI_REGION_PRESENT,0xfffffffffffffff);
		REMOTE_HUB_L(20,PI_REGION_PRESENT);
		break;		
		
		/* Bridge Error */			
	case ETYPE_BRIDGEDEV:
		{
			int	*pci_addr;
			int	val;
			pci_addr = (int *) (BRIDGE_DEVIO(1) + IO_BASE);
			val = *pci_addr;
			*pci_addr = 0xdeadbeef;
			break;
		}
#endif
	default:	
		printf("error_induce: %d test not supported\n", code);
		break;
	}
	
	/* If the user has passed in a memory address they may want
           the state of the memory before it was messed with, for later cleanup. */
	if (state != MD_DIR_UNDEF && rvp->r_val1 != -1) {
	  rvp->r_val1 = state;
	} 

	return rc;
}



#endif /* SN0 && FORCE_ERRORS */














