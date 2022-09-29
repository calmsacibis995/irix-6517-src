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

#if defined (SN) && defined (FORCE_ERRORS)
/*
 * File: error_force.c
 */
#include <sys/types.h>
#include <sys/systm.h>
#include "sn_private.h"
#include "error_private.h"
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/SN/module.h>
#include <sys/SN/SN0/slotnum.h>
#include <sys/SN/addrs.h>
#include <sys/debug.h>
#include <sys/PCI/bridge.h>
#include <sys/SN/SN1/snacioregs.h>
#include <sys/SN/SN0/ip27config.h>
#include <sys/runq.h>
#include <sys/SN/io.h>

#include <sys/PCI/pciio.h>
#define A64_BIT_OPERATION
#include <sys/ql.h>

typedef struct ef_device_s {
	void * mem_base;
	xwidgetnum_t widget;
	cnodeid_t master_node;
	vertex_hdl_t vertex;
} ef_device_t;

/* This is a global used to insert arbitrary addresses we select into
   the q-logic interface. */
__int64_t ef_dma_addr = NULL;

extern vertex_hdl_t	base_io_scsi_ctlr_vhdl[];	

#define IS_ROOT_DEVICE(_dev) \
	((_dev) == base_io_scsi_ctlr_vhdl[0] || (_dev) == base_io_scsi_ctlr_vhdl[1])


/*
 * Function   : error_force_node_to_dev
 *
 * Purpose    : To retrieve the needed information on a device and insure
 *              the device exists, based on node and widget.
 * Returns    :  0 for success (found device)
 *              -1 for failure (no device)
 * 		retVal will hold all information needed on the device.
 */

int 
error_force_node_to_dev(cnodeid_t node, xwidgetnum_t widget, ef_device_t *retVal)
{
	char buff[1024];
	

	sprintf(buff,"/hw/nodenum/%d/xtalk/%d",node,widget);

	retVal->vertex = hwgraph_path_to_vertex(buff);

	if (retVal->vertex == GRAPH_VERTEX_NONE) {
		printf("Error: Did not find device on Node %d, with widgetNum %d\n",
		       node, widget);
		return -1;
	}

	if (is_headless_node(node)) {
		printf("Error: Node %d is disabled\n",node);
		return -1;
	}

	retVal->master_node = node;
	retVal->widget = widget;

	/* Masking out the higher bits, since widgetnum_t stores extra
           info there */
	retVal->mem_base = (void*)(NODE_SWIN_BASE(retVal->master_node,
						  retVal->widget & 0xf));
	
	return 0;
}


/*
 * Function   : error_force_modNslot_to_dev
 *
 * Purpose    : To retrieve the needed information on a device and insure
 *              the device exists, based on module and slot
 * Returns    :  0 for success (found device)
 *              -1 for failure (no device)
 * 		retVal will hold all information needed on the device.
 */

int
error_force_modNslot_to_dev(moduleid_t module,slotid_t slot, ef_device_t *retVal)
{
	char buff[1024];

	/* check our parameters */
	if ( module > MODULE_MAX+1 ||  slot > MAX_IO_SLOT_NUM+1) {
		printf("ef_get_device: Invalid input. Module %d Slot %d\n",
		       module, slot);
		return -1;
	}
	if (SN00) {
		sprintf(buff,"/hw/module/%d/slot/MotherBoard/node/xtalk/8/pci/%d",
			module,slot);
	} else {
		sprintf(buff,"/hw/module/%d/slot/io%d",module,slot);
	}
	EF_PRINT(("Passing this path to hwgraph %s\n",buff));
	retVal->vertex = hwgraph_path_to_vertex(buff);

	/* Is the device even present? */
	if(retVal->vertex == GRAPH_VERTEX_NONE){
		printf("ef_get_device:did not find Module %d Slot %d\n",
		       module, slot);
		return -2;
	}
	
	/* Does it have a master? */
	retVal->master_node = master_node_get(retVal->vertex);
	if (is_headless_node(retVal->master_node)) {
		printf("ef_get_device:did not find NODE from vertex, or node disabled."
		       " Module %d Slot %d\n", module, slot);	
		return -3;
	}
	
	retVal->widget = slot_to_widget((slot & 0xf) -1);
	EF_PRINT(("ef_get_device: WidgetNum:0x%x\n",retVal->widget));
	/* Masking out the higher bits, since widgetnum_t stores extra
           info there */
	retVal->mem_base = (void*)(NODE_SWIN_BASE(retVal->master_node,
						  retVal->widget & 0xf));
	
	
	return 0;
}

/*
 * Function   : error_force_get_device
 *
 * Purpose    : To retrieve the needed information on a device and insure
 *              the device exists, based on a error_params_t, which could 
 *              have specifications for module & slot, or widget & node
 * Returns    :  0 for success (found device)
 *              -1 for failure (no device)
 * 		retVal will hold all information needed on the device.
 */

int 
error_force_get_device(io_error_params_t *params, ef_device_t *retVal)
{
	/* IF  the user is specifying by module and slot */
	if (params->module && params->slot) {
		return error_force_modNslot_to_dev(params->module, params->slot,
						   retVal);
	}

	/* If the user is specifying by node and widget num */
	if (params->widget && params->node != CNODEID_NONE) {
		return error_force_node_to_dev(params->node, params->widget, 
						      retVal);
	} 

	return -1;
	
	
}

/*
 * Function   : error_force_set_read_access
 *
 * Purpose    : Sets or clears the IIWA register for the specified device.
 * Returns    : 0 - success, -1 failure
 */ 

int
error_force_set_read_access(io_error_params_t *params,int val, rval_t *rvp)
{
	__int64_t access = 0;
	int rv;
	ef_device_t ef_dev;

	EF_PRINT(("Made it to function illegal_read_access\n"));


	rv = error_force_get_device(params,&ef_dev);

	if (rv) {
		EF_PRINT(("EF::Cannot find device Error %d\n",rv));
		rvp->r_val1 = rv;
		return -1;
	}	

	/* Retrieve the current access state */
	access = REMOTE_HUB_L(ef_dev.master_node,II_IIWA);
	EF_PRINT(("Device is widget %d\n", ef_dev.widget));
	EF_PRINT(("Read IIWA bits on node %d for value of 0x%llx\n",
		     ef_dev.master_node,access));

	if (val) {
		/* Set the access bit */
		access = access | (1 << ef_dev.widget);
	} else {
		/* Clear the access bit for this widget */
		access = access & ~(1 << ef_dev.widget);
	}

	EF_PRINT(("About to write back 0x%llx\n",access));
	
	REMOTE_HUB_S(ef_dev.master_node,II_IIWA,access);

	return 0;
}

/*
 * Function   : error_force_pio_write_absent_reg
 *
 * Purpose    : To issue a PIO write to an absent register on a device.
 * Returns    : 0 - success, -1 - failure
 */ 

int
error_force_pio_write_absent_reg(io_error_params_t *params, int response,
				 int twice,rval_t *rvp) 
{
	
	ef_device_t ef_dev;
	char *addr;
	int rv;
	__uint64_t iprb_val;

	EF_PRINT(("Made it to function pio_write_absent_reg\n"));

	rv = error_force_get_device(params,&ef_dev);
	if (rv) {
		EF_PRINT(("EF::Cannot find device. Error %d\n",rv));
		rvp->r_val1 = rv;
		return -1;
	}	

	/* Find a non-exsitent register on the device. */
	addr = (char*)ef_dev.mem_base;
	
	/* needed to get to the pci bus */
	addr += 0x27000;

	/* If we need a response we must tell the hub to use a NAK
           protocol on the LLP level */
	if (response) {

	  iprb_val = REMOTE_HUB_L(ef_dev.master_node,IIO_IOPRB(ef_dev.widget));
	  EF_PRINT(("Read IPRB%d (0x%llX) bits on node %d for value of 0x%llx\n",
		       ef_dev.widget,IIO_IOPRB(ef_dev.widget),
		       ef_dev.master_node,iprb_val));

	  /* Set the access bit */	
	  iprb_val &=  ~(UINT64_CAST (1) << 42);

	  /* Now store the register value */
	  EF_PRINT(("About to write back 0x%llx\n",iprb_val));
	  REMOTE_HUB_S(ef_dev.master_node,IIO_IOPRB(ef_dev.widget),iprb_val);
	  
	}
	
	/* DO the PIO write to the non-existent register */
	EF_PRINT(("PIO write to address 0x%llX\n", addr));
	*addr= 3;

	/* Certain errors need more than one access */
	if (twice) {
	  EF_PRINT(("Second PIO write to address 0x%llX\n", addr));
	  *addr= 3;
	}
	
	EF_PRINT(("Handled the error\n"));

	return 0;

}

/*
 * Function   : error_force_absent_widget
 *
 * Purpose    : Issues a PIO read from an absent widget
 *		Searches the hwgraph for an absent widget and then reads from it.
 * Returns    : 0 - success, -1 - failure
 */ 

int
error_force_absent_widget(rval_t *rvp)
{
	vertex_hdl_t deviceVertex;
	char buff[1024];
	int *mem_base;
	int widget,testVal,node;

	EF_PRINT(("Made it to function ef_absent_widget\n"));
	/* We are looking for an absent widget. A place without a
	   device, a place inside the twilight zone */ 
	for (node = 0; node < MAX_COMPACT_NODES; node++) { 

		for (widget=8; widget < 0x10; widget++) {

			/* Accessing the hw graph */
			sprintf(buff,"/hw/nodenum/%d/xtalk/%d",node,widget);
			deviceVertex = hwgraph_path_to_vertex(buff);
			/* We found a blank spot! */
			if (deviceVertex == GRAPH_VERTEX_NONE) {
				EF_PRINT(("Found absence on node %d widget 0%x\n",
					     node, widget));

				/* Masking out the higher bits, to
                                   prevent memory scribbles */
				mem_base = (int*)(NODE_SWIN_BASE(node, widget & 0xf));
				EF_PRINT(("ABout to read from Addr 0x%llx\n",
					     mem_base));
				/* Now to do the read, from an absent widget */
				testVal = *mem_base;
				testVal ++;
				EF_PRINT(("Handled the error\n"));
				return 0;
			}
		}
	}
	
	printf("No absent widgets on this machine\n");
	rvp->r_val1 = -1;
	return -1;
}

/*
 * Function   : error_force_forge_widgetId
 *

 * Purpose    : To give a widget an incorrect widget ID and then perform
 *              some access to the widget.
 * Parameters : params -> device to select
 *  		action -> Whether the widget access is read/write or spurious.
 * 			  where spurious is an attempt to generate a
 * 			  spurious error, which appears to not work.
 * Returns : 0 - success, -1 - failure
 * 
 */

int
error_force_forge_widgetId(io_error_params_t *params, int action, rval_t *rvp)
{
	ef_device_t ef_dev;
	widgetreg_t ctrl_reg, test_read;
	int rv,ii;
	cpu_cookie_t was_running;
	volatile widgetreg_t *addr;

	EF_PRINT(("Made it to ef_forge_widgetId\n"));
	rv = error_force_get_device(params,&ef_dev);
	if (rv) {
		EF_PRINT(("EF::Cannot find device. Error %d\n",rv));
		rvp->r_val1 = rv;
		return -1;
	}	

	
	/* First we find the first device's widget id. */
	ctrl_reg = ((bridge_t*)ef_dev.mem_base)->b_wid_control;
	EF_PRINT(("EF::Original Widget Control 0x%x\n", ctrl_reg));
	/* Clear the bottom four bits */
	ctrl_reg = ctrl_reg & ~(0xf);
	
	/* If we are attempting a spurious message error set it to 0x3 */
	if (action == EF_FORGE_SPURIOUS) {
	  /* If it was 0xf then we should make it something else */
	  ctrl_reg = ctrl_reg | 0x3;
	} else {
		if (ef_dev.widget != 0xf) {
			/* If the widget was not widget 0xf then make it think
			   it is! */
			ctrl_reg = ctrl_reg | 0xf;
		} else {
			/* If it was 0xf then we should make it something else */
			ctrl_reg = ctrl_reg | 0x8;
		}
	}

	EF_PRINT(("EF::Widget Control changed to 0x%x\n", ctrl_reg));
	
	/* Now we make the widget believe it is another witdget. THis
           is so sneaky. */
	((bridge_t*)ef_dev.mem_base)->b_wid_control = ctrl_reg;

	/* Make sure we are running on Node B */
	EF_PRINT(("Setting process to run on node %d\n",ef_dev.master_node));
	was_running = setmustrun(NODEPDA(ef_dev.master_node)->node_first_cpu);
	
	addr = &(((bridge_t*)ef_dev.mem_base)->b_wid_control);
	EF_PRINT(("About to acccess Addr 0x%llX\n",addr));
	/* Now we do either a PIO read or write to see what happens. */
	switch(action) {
	case EF_FORGE_SPURIOUS:
		for (ii=0; ii < 10; ii++) {
			*addr = 5;
		}
		break;
	case EF_FORGE_WRITE:
		*addr = 5;
		break;
	case EF_FORGE_READ:
		test_read = *(addr);
		break;
	default:
		printf("EF::Unknown action code %d in ef_forge_widgetId\n",action);
	}
	
	/* Go back to our former processor */
	EF_PRINT(("Returing back to old processor\n"));
	restoremustrun(was_running);
	
	test_read++;
	EF_PRINT(("Handled error\n"));
	return 0;
}

/*
 * Function   : error_xtalk_hdr_sideband.
 *
 * Purpose    : This one is complicated. The goal is to allocate memory on nodeA,
 * 		corrupt the memory and then access it through nodeB via the Xbow.
 * 		This is done in a few steps:
 * 
 * 		1) Allocate the memory from a specific node.
 * 		2) Corrupt the memory.
 * 		3) Open a big window on nodeB to access the memory in nodeA. 
 * 		   We have to do this since the memory offset may be larger than a 
 * 		   small window 
 * 		4) Do a PIO read of the memory from a cpu on nodeB
 * 		   through the big window.  
 * 
 * Returns    : 0 - success, -1 - failure
 * 
 * Note       : This function does not recognize the devices supplied to it
 * 		by the paramters. Everything is hardcoded, due to the
 * 		fact I have been unable to figure out how to
 * 		generalize the fourth arguement to ITTE_PUT.
 */

error_force_xtalk_sideband(io_error_params_t *params, rval_t *rvp)
{
	
  paddr_t nodeA_mem,phys_addr, nodeB_access;
  
  md_error_params_t md_params;
  cpu_cookie_t was_running;
  int test_read = 0;
  cnodeid_t nodeA, nodeB;

  test_read = params->node;
  nodeA = 2;
  nodeB = 0;

  if (SN00) {
	  printf("Cannot run test on Speedo\n");
	  rvp->r_val1 = -1;
	  return -1;
  }

  /* insuring we are ntop the same node or on the same xbow */
  if (nodeA == nodeB) {
	  printf("Error:: Nodes cannot be equal\n");
	  rvp->r_val1 = -1;
	  return -1;
  }

  /* If the nodes are on the same xbow this test will not work */
  if (NODEPDA(nodeA)->xbow_peer != nodeB) {
	  printf("Error:: Nodes must share a xbow.\n");
	  rvp->r_val1 = -1;
	  return -1;
  }

  /* Is node B valid? */
  if (!error_force_nodeValid(nodeA) || !error_force_nodeValid(nodeB)) {
	  printf("Node %d or %d is not valid\n",nodeA, nodeB);
	  rvp->r_val1 = -1;
	  return -1;
  }	
  
  
  /* Grab memory from Node A */
  nodeA_mem = (paddr_t)kmem_alloc_node(4096, VM_DIRECT | VM_CACHEALIGN | KM_NOSLEEP, 
				       nodeA);

  if (nodeA_mem == NULL) {
	  printf("Cannot allocate enough memory for test\n");
	  rvp->r_val1 = -1;
	  return -1;
  }
  /* translate it to a physical address */
  phys_addr = kvtophys((void*)nodeA_mem);

  /* Set up the ecc error */
  md_params.sub_code = ETYPE_MEMERR;
  md_params.read_write = ERRF_NO_ACTION;

  /* Corrupt ecc on node A */
  
  EF_PRINT(("EF::Corrupting Addr 0x%llx , Phys 0x%llx\n",nodeA_mem,phys_addr));
  if (kl_force_memecc(phys_addr,&md_params, rvp)) {
	  
	  printf("Error: Cannot corrupt Node 0 memory 0x:llx\n",nodeA_mem);
	  kmem_free((void*)nodeA_mem,4096);
	  rvp->r_val1 = -1;
	  return -1;

  }

  EF_PRINT(("ITTE1 State: 0x%llx\n",*(IIO_ITTE_GET(nodeB,0)) ));
  /* Now prepare the big window, this will allow us to use the big
     window form me on xbow slot 0xa (or N3) at our corrupted physical
     address */
  
  IIO_ITTE_PUT(nodeB,0,HUB_PIO_MAP_TO_IO, 0xa, TO_NODE_ADDRSPACE(phys_addr));

  EF_PRINT(("ITTE1 State(after meddling): 0x%llx\n",*(IIO_ITTE_GET(nodeB,0))));

  
  /* construct an xtalk addres that will reference that page */
  nodeB_access = NODE_BWIN_BASE(nodeB,0); 
  EF_PRINT(("nodeB:NODE_BWIN_BASE 0x%llx\n",nodeB_access));

  nodeB_access += (0xFFFFFFFF & phys_addr); 
  EF_PRINT(("nodeB:With phys_addr 0x%llx\n",nodeB_access));

  /* Make sure we are running on Node B */
  EF_PRINT(("About to run on Node: %d CPU %d\n",nodeB,
	       NODEPDA(nodeB)->node_first_cpu));

  was_running = setmustrun(NODEPDA(nodeB)->node_first_cpu);

  /* A PIO READ from node B */
  EF_PRINT(("About to read from 0x%llx\n",nodeB_access));
  test_read = *((int*)nodeB_access);

  /* Go back to our former processor */
  restoremustrun (was_running);

  EF_PRINT(("Handled Error. Read %d\n", test_read));

  kmem_free((void*)nodeA_mem, 4096);

  return 0;
  
}


/*
 * Function   : error_force_clr_outbound_access
 *
 * Purpose    : To clear the IOWA register field for the specified device and 
 * 		then execute a PIO write
 * Returns    : 0 - for success, -1 for failure
 */ 
int
error_force_clr_outbound_access(io_error_params_t *params, rval_t *rvp)
{
	__int64_t access = 0;
	int rv;
	ef_device_t ef_dev;

	EF_PRINT(("Made it to function illegal_read_access\n"));

	rv = error_force_get_device(params,&ef_dev);

	if (rv) {
		EF_PRINT(("EF::Cannot find device Error %d\n",rv));
		rvp->r_val1 = rv;
		return -1;
	}	

	/* Retrieve the current access state */
	access = REMOTE_HUB_L(ef_dev.master_node,II_IOWA);
	EF_PRINT(("Device is widget 0x%X\n", ef_dev.widget));
	EF_PRINT(("Read IOWA(0x%llX) bits on node %d for value of 0x%llx\n",
		     II_IOWA,ef_dev.master_node,access));

	/* Set the access bit */
	access = access & ~(1 << ef_dev.widget);

	/* Now store the register value */
	EF_PRINT(("About to write back 0x%llx\n",access));
	REMOTE_HUB_S(ef_dev.master_node,II_IOWA,access);

	/* Now we execute a PIO write to the widget */
	ef_dev.mem_base = ((char*)ef_dev.mem_base) + WIDGET_CONTROL;
	EF_PRINT(("PIO write to Addr:0x%llX\n",ef_dev.mem_base));
	*((int*)ef_dev.mem_base) = 0xff;

	EF_PRINT(("Error Handled\n"));

	return 0;
}

/*
 * Function   : error_force_clr_local_access
 *
 * Purpose    : To clear the access bit of the ILAPR register then issue a PIO write
 * Returns    : 0 - success, -1 - failure
 */ 

error_force_clr_local_access(io_error_params_t *params, rval_t *rvp) 
{
	
	ef_device_t ef_dev;
	int *addr;
	int rv, test_val;
	__uint64_t ilapr_val;

	EF_PRINT(("Made it to function ef_clr_local_access\n"));

	rv = error_force_get_device(params,&ef_dev);
	if (rv) {
		EF_PRINT(("EF::Cannot find device Error %d\n",rv));
		rvp->r_val1 = rv;
		return -1;
	}	

	addr = ((int*)ef_dev.mem_base) + 1;

	EF_PRINT(("First PIO read from address 0x%llX\n", addr));
	test_val = *addr;

	ilapr_val = REMOTE_HUB_L(params->node,IIO_ILAPR);
	EF_PRINT(("Read ILAPR (0x%llX) on node %d for value of 0x%llx\n",
		     IIO_ILAPR,params->node,ilapr_val));

	/* Set the access bit */	
	ilapr_val &=  ~(UINT64_CAST (1) << params->node);

	/* Now store the register value */
	EF_PRINT(("About to write back 0x%llx\n",ilapr_val));
	REMOTE_HUB_S(params->node,IIO_ILAPR,ilapr_val);
	
	/* DO the PIO write to the non-existent register */
	EF_PRINT(("Second PIO read from address 0x%llX\n", addr));
	test_val = *addr;
	test_val++;

	EF_PRINT(("Handled the error\n"));

	return 0;

}

/*
 * Function   : error_force_xtalk_credit_timeout
 *
 * Purpose    : To Set the IXCC timeout value to 1, then issue a tone of
 *              PIO reads to force a harware error
 * Returns    : 0 - success, -1 - failure 
 */

int
error_force_xtalk_credit_timeout(io_error_params_t *params, rval_t *rvp) 
{

	ef_device_t ef_dev;
	int *addr;
	int rv,ii,test_val;
	__uint64_t ixcc_val;
	cpu_cookie_t was_running;

	EF_PRINT(("Made it to function ef_xtalk_credit_timeout\n"));

	rv = error_force_get_device(params,&ef_dev);
	if (rv) {
		EF_PRINT(("EF::Cannot find device Error %d\n",rv));
		rvp->r_val1 = rv;
		return -1;
	}	

	ixcc_val = REMOTE_HUB_L(params->node,IIO_IXCC);
	EF_PRINT(("Read IXCC (0x%llX) on node %d for value of 0x%llx\n",
		     IIO_IXCC,params->node,ixcc_val));

	/* Set the access bit */	
	ixcc_val &= 0xFFFFFFFFFA000000;
	ixcc_val += 0x1;

	/* Now store the register value */
	EF_PRINT(("About to write back 0x%llx\n",ixcc_val));
	REMOTE_HUB_S(params->node,IIO_IXCC,ixcc_val);
	
	addr = ((int*)ef_dev.mem_base) + 1;
	
	/* Make sure we are running on Node B */
	was_running = setmustrun(NODEPDA(params->node)->node_first_cpu);
	
	/* Do the PIO read in a loop to insure we create the error */
	EF_PRINT(("PIO reading from address 0x%llX\n", addr));
	for (ii=0; ii < 50; ii++) {
		test_val = *addr;
		test_val++;
	}
	
	/* Go back to our former processor */
	restoremustrun(was_running);

	EF_PRINT(("Handled the error\n"));

	return 0;

}

/*
 * Function   : error_force_set_CRB_timeout
 *
 * Purpose    : To set the ICTO timeout to whatever the caller passes in as <val>
 * Returns    : 0 - success, -1 - failure
 */ 
error_force_set_CRB_timeout(io_error_params_t *params, int count,rval_t *rvp) 
{

	__uint64_t icto_val;
	
	EF_PRINT(("Made it to function ef_set_CRB_timeout\n"));

	if (!error_force_nodeValid(params->node)) {
		printf("Node %d or %d is not valid\n",params->node);
		rvp->r_val1 = -1;
		return -1;
	}	
  
	icto_val = REMOTE_HUB_L(params->node,IIO_ICTO);
	EF_PRINT(("Read ICTO (0x%llX) on node %d for value of 0x%llx\n",
		     IIO_ICTO,params->node,icto_val));

	/* Set the access bit */	
	icto_val &= ~(0xFF);

	icto_val += (count & 0xFF);
	
	/* Now store the register value */
	EF_PRINT(("About to write back 0x%llx\n",icto_val));
	REMOTE_HUB_S(params->node,IIO_ICTO,icto_val);

	return 0;

}

/*
 * Function   : error_force_redirect_dma
 *

 * Purpose    : If #FORCE_ERRORS is defined than the drivers in io/ql.c

 *              will operate slightly differently. Instead of passing
 *              in the values needed for a DMA_READ or DMA_WRITE
 *              directly, they will check through this function first
 *              to see if the error system has addresses it should use
 *              instead. 
 * Returns    : <none>
 */

void
error_force_redirect_dma(pHA_INFO ha,data_seg *dsp, command_entry *q_ptr, int i)
{
	
	/* If this device is the root device than we do not want to
           mess with it. */
	if (IS_ROOT_DEVICE(ha->ctlr_vhdl) || ef_dma_addr == NULL) {
		q_ptr->dseg[i].base_high_32 = dsp->base >>32;
		q_ptr->dseg[i].base_low_32  = dsp->base;
		
	} else {
		q_ptr->dseg[i].base_high_32  = ef_dma_addr >> 32;	
		q_ptr->dseg[i].base_low_32 = ef_dma_addr;

		ef_dma_addr = NULL;
	} 
}	

/*
 * Function   : error_force_xtallk_force_addr
 *
 * Purpose    : To inject invalid addresses into the q-logic driver,
 *		using the global, ef_dma_addr, referenced in the
 *		function error_force_redirect_dma. Currently we can
 *		inject addresses from an absent node, and from an
 *		absent widget (based on the type parameter).
 * Returns    : 0 - success, -1 - failure
 */

int 
error_force_xtalk_force_addr(io_error_params_t *params, int type,rval_t *rvp)
{

	ef_device_t ef_dev;
	int rv;
	
	EF_PRINT(("Made it to function ef_xtalk_force_addr\n"));

	rv = error_force_get_device(params,&ef_dev);
	if (rv) {
		EF_PRINT(("EF::Cannot find device Error %d\n",rv));
		rvp->r_val1 = rv;
		return -1;
	}	

	if(IS_ROOT_DEVICE(ef_dev.vertex)) {
		printf("Cannot perform tests on root device\n");
		rvp->r_val1 = -1;
		return -1;
	}
	
	switch(type) {
	case EF_ABSENT_MEM_BANK:
		ef_dma_addr = EF_MEMBANK_ADDR(ef_dev.master_node,7);
		break;
	case EF_ABSENT_NODE:
		ef_dma_addr = error_force_absent_node_mem();
		if(ef_dma_addr == NULL) {
			rvp->r_val1 = -1;
		}
		break;
	default:
		printf("Illegal ef_xtalk_force_addr %d\n",type);
		rvp->r_val1 = -1;
		return -1;
	}

	EF_PRINT(("DMA will access:0x%llX\n",ef_dma_addr));

	return 0;
}
	
#endif /* SN0 && FORCE_ERRORS */

