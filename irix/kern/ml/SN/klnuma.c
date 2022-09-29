/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.14 $"

/*
 * Code for machine-dependent NUMA support
 */

#ident "$Revision: 1.14 $"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/SN/arch.h>
#include <sys/SN/addrs.h>
#include <sys/SN/klkernvars.h>
#include <sys/SN/gda.h>
#include "sn_private.h"

extern __elf_header();	/* Start of kernel text	*/
extern _etext();	/* End of kernel text	*/
extern _fdata();	/* Start of data	*/

#if defined (MAPPED_KERNEL)
extern int numa_kernel_replication_ratio;

#if defined(_COMPILER_VERSION) && (_COMPILER_VERSION>700)

extern _erodata();	/* End of rodata	*/
#define END_OF_REPL_TEXT	_erodata

#else

#define SLOP			(MAPPED_KERN_RW_TO_PHYS(_fdata) -    \
				 MAPPED_KERN_RO_TO_PHYS(_etext) - 1)
#define END_OF_REPL_TEXT	_etext + SLOP

#endif

static void copy_kernel(nasid_t dest_nasid);
static void set_ktext_source(nasid_t client_nasid, nasid_t server_nasid);
#else /* MAPPED_KERNEL */
#define END_OF_REPL_TEXT	_etext
#endif

static cpumask_t ktext_repmask;

/*
 * Return the address of the first free memory on the node.
 */
__psunsigned_t
get_freemem_start(cnodeid_t cnode)
{
	nasid_t nasid = COMPACT_TO_NASID_NODEID(cnode);
	extern _end();

	/* We should really be able to use FREEMEM here. */
#if defined (MAPPED_KERNEL)
	if (cnode == 0) {
		/* Only the master has text and read-write data */
		return MAPPED_KERN_RW_TO_PHYS(_end);
	} else if (CPUMASK_TSTB(ktext_repmask, cnode)) {
		/* has a copy of kernel text */
		return (TO_NODE(nasid, MAPPED_KERN_RO_TO_PHYS((__psunsigned_t)END_OF_REPL_TEXT)));
 	} else if (kdebug) {
		/* no kernel text but running the debugger */
		return KDM_TO_PHYS(SYMMON_STK_END(nasid));
	} else {
		/* no debugger and no kernel copy */
		return KDM_TO_PHYS(SYMMON_STK_ADDR(nasid, 0));
	}
#else /* MAPPED_KERNEL */
	return MAPPED_KERN_RW_TO_PHYS(_end);

#endif /* MAPPED_KERNEL */
}


#if defined (MAPPED_KERNEL)

/*
 * XXX - This needs to be much smarter about where it puts copies of the
 * kernel.  For example, we should never put a copy on a headless node,
 * and we should respect the topology of the machine.
 */
void
setup_replication_mask(int maxnodes)
{
	cnodeid_t	cnode;

	/* Set only the master cnode's bit.  The master cnode is always 0. */
	CPUMASK_CLRALL(ktext_repmask);
	CPUMASK_SETB(ktext_repmask, 0);

	for (cnode = 1; cnode < maxnodes; cnode++) {
		/* See if this node should get a copy of the kernel */
		if (numa_kernel_replication_ratio &&
		    !(cnode % numa_kernel_replication_ratio)) {

			/* Advertise that we have a copy of the kernel */
			CPUMASK_SETB(ktext_repmask, cnode);
		}
	}

	/* Set up a GDA pointer to the replication mask. */
	GDA->g_ktext_repmask = &ktext_repmask;
}


void
replicate_kernel_text(int maxnodes)
{
	cnodeid_t cnode;
	nasid_t client_nasid;
	nasid_t server_nasid;

	server_nasid = master_nasid;

	/* Text and data should never overlap. */
	if (MAPPED_KERN_RW_TO_K0(_fdata) < MAPPED_KERN_RO_TO_K0((__psunsigned_t)END_OF_REPL_TEXT)) {
		cmn_err(CE_PANIC,
			"Kernel text and data overlap (0x%x, 0x%x)."
			"  Please build a new kernel.",
			MAPPED_KERN_RO_TO_K0((__psunsigned_t)END_OF_REPL_TEXT),
			MAPPED_KERN_RW_TO_K0(_fdata));
	}

	/* Record where the master node should get its kernel text */
	set_ktext_source(master_nasid, master_nasid);

#ifdef DEBUG
	printf("Copying kernel from NASID %d to remaining NASIDs\n",
		master_nasid);
#endif
	for (cnode = 1; cnode < maxnodes; cnode++) {
		client_nasid = COMPACT_TO_NASID_NODEID(cnode);

		/* Check if this node should get a copy of the kernel */
		if (CPUMASK_TSTB(ktext_repmask, cnode)) {
			server_nasid = client_nasid;
			copy_kernel(server_nasid);
		}

		/* Record where this node should get its kernel text */
		set_ktext_source(client_nasid, server_nasid);
	}
}

/* XXX - When the BTE works, we should use it instead of this. */
static void
copy_kernel(nasid_t dest_nasid)
{
	__psunsigned_t dest_kern_start, kern_size, source_start, source_end;

#if 0
	printf("Copying kernel from NASID %d to NASID %d\n",
		master_nasid, dest_nasid);
#endif

	source_start = (__psunsigned_t)__elf_header;
	source_end = (__psunsigned_t)END_OF_REPL_TEXT;
	kern_size = source_end - source_start;

	dest_kern_start = CHANGE_ADDR_NASID(MAPPED_KERN_RO_TO_K0(source_start),
					    dest_nasid);
	bcopy((caddr_t)source_start, (caddr_t)dest_kern_start, kern_size);
}

#endif /* MAPPED_KERNEL */
static void
set_ktext_source(nasid_t client_nasid, nasid_t server_nasid)
{
	kern_vars_t *kvp;
	cnodeid_t client_cnode;

	client_cnode = NASID_TO_COMPACT_NODEID(client_nasid);
	
	kvp = &(NODEPDA(client_cnode)->kern_vars);

	KERN_VARS_ADDR(client_nasid) = (__psunsigned_t)kvp;

	kvp = (kern_vars_t *)KERN_VARS_ADDR(client_nasid);

	kvp->kv_magic = KV_MAGIC;

	kvp->kv_ro_nasid = server_nasid;
	kvp->kv_rw_nasid = master_nasid;
	kvp->kv_ro_baseaddr = NODE_CAC_BASE(server_nasid);
	kvp->kv_rw_baseaddr = NODE_CAC_BASE(master_nasid);
}

/* Check if a particular pfn corresponds to a kernel text
 * that need not be  dumped. Dump should save only the kernel text on
 * the master node
 */
int
page_dumpable(cnodeid_t	cnode,
	      pfn_t	pfn)
{
	nasid_t		curr_nasid = COMPACT_TO_NASID_NODEID(cnode);
	__psunsigned_t	local_kern_txt_start,local_kern_txt_end;
	__psunsigned_t	kern_txt_start,kern_txt_end;
	paddr_t		curr_page_start;
	paddr_t		kernel_pa_start,kernel_pa_end;
#if DEBUG
	static pfn_t	skip_start = 0;
	static pfn_t	skip_end = 0;
#endif
	
	/* Dump the kernel text on the master node */
	if (curr_nasid == master_nasid)
		return(1);

	/* If this  node does not have a copy of kernel text then
	 * then the page can be dumped
	 */
	if (!(CPUMASK_TSTB(ktext_repmask, cnode))) 
		return(1);
	
	/* Get the starting & ending  virtual addresses of the kernel text
	 * on the node corr. to "cnode".
	 */
	local_kern_txt_start = (__psunsigned_t)__elf_header;
	local_kern_txt_end = (__psunsigned_t)END_OF_REPL_TEXT;
	kern_txt_start =  
		CHANGE_ADDR_NASID(MAPPED_KERN_RO_TO_K0(local_kern_txt_start),
				  curr_nasid);

	kern_txt_end = kern_txt_start + 
		(local_kern_txt_end - local_kern_txt_start);

	/* Get the direct-mapped physcial address range corr. to the
	 * virtual addr range of the kernel text on node corr. to 
	 * "cnode".
	 */
	kernel_pa_start = kvtophys((void *)kern_txt_start);
	kernel_pa_end = kvtophys((void *)kern_txt_end);
	
	/* Get the starting physical address of the current
	 * pfn
	 */
	curr_page_start = ctob(pfn);

	/* Check if the current pfn's range of physical addresses fall 
	 * within the replicated kernel text which need not be dumped
	 */
	if ((kernel_pa_start <= curr_page_start) 	&&
	    (curr_page_start <= kernel_pa_end)) {

#if DEBUG
		if (!skip_start) 
			skip_start = pfn;
		else
			skip_end = pfn;
#endif
		return(0);
	}

#ifdef DEBUG
	if (skip_end && 
	    (skip_end == (pfn - 1))) {
		printf("\nSkipping pfns 0x%x thru 0x%x (replicated kernel text)"
		       " on nasid %d\n",
		       skip_start,skip_end,curr_nasid);
		skip_start = 0; 
		skip_end = 0;

	}
#endif

	return(1);
}
