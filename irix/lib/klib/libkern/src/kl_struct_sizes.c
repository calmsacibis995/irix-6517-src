#ident "$Header: "

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>

/*
 * klib_set_struct_sizes() -- Set the struct sizes for key kernel structures
 */
int
klib_set_struct_sizes()
{
	if (!KLP) {
		return(1);
	}
	if (K_FLAGS & K_STRUCT_SIZES_ALLOCED) {
		/* XXX - Set error value ?
		 */
		return(1);
	}
	LIBKERN_DATA->k_struct_sizes = 	
		(struct_sizes_t*)malloc(sizeof(struct_sizes_t));
	if (!LIBKERN_DATA->k_struct_sizes) {
		return(1);
	}
	K_FLAGS |= K_STRUCT_SIZES_ALLOCED;

	AVLNODE_SIZE = kl_struct_len("avlnode");
	BHV_DESC_SIZE = kl_struct_len("bhv_desc");
	CFG_DESC_SIZE = kl_struct_len("cfg_desc");
	CRED_SIZE = kl_struct_len("cred");
	CSNODE_SIZE = kl_struct_len("csnode");
	DOMAIN_SIZE = kl_struct_len("domain");
	EFRAME_S_SIZE = kl_struct_len("eframe_s");
	EXCEPTION_SIZE = kl_struct_len("exception");
	FDT_SIZE = kl_struct_len("fdt");
	GRAPH_S_SIZE = kl_struct_len("graph_s");
	GRAPH_VERTEX_GROUP_S_SIZE = kl_struct_len("graph_vertex_group_s");
	GRAPH_VERTEX_S_SIZE = kl_struct_len("graph_vertex_s");
	GRAPH_EDGE_S_SIZE = kl_struct_len("graph_edge_s");
	GRAPH_INFO_S_SIZE = kl_struct_len("graph_info_s");
	IN_ADDR_SIZE = kl_struct_len("inaddrpair");
	INVENT_MEMINFO_SIZE = kl_struct_len("invent_meminfo");
	INODE_SIZE = kl_struct_len("inode");
	INPCB_SIZE = kl_struct_len("inpcb");
	KNA_SIZE = kl_struct_len("kna");
	KNETVEC_SIZE = kl_struct_len("knetvec");
	KTHREAD_SIZE = kl_struct_len("kthread");
	LF_LISTVEC_SIZE = kl_struct_len("lf_listvec");
	LSNODE_SIZE = kl_struct_len("lsnode");
	MBUF_SIZE = kl_struct_len("mbuf");
	ML_INFO_SIZE = kl_struct_len("ml_info");
	if (!(ML_SYM_SIZE = kl_struct_len("ml_sym"))) {
		/* XXX -- We have to do this because the ml_sym struct info
		 * has not been included in the kernel symbol table.
		 */
		if (PTRSZ64) {
			ML_SYM_SIZE = 16;
		}
		else {
			ML_SYM_SIZE = 8;
		}
	}
	MRLOCK_S_SIZE = kl_struct_len("mrlock_s");
	MODULE_INFO_SIZE = kl_struct_len("module_info");
	NODEPDA_S_SIZE = kl_struct_len("nodepda_s");
	PDA_S_SIZE = kl_struct_len("pda_s");
	PFDAT_SIZE = kl_struct_len("pfdat");
	PID_ENTRY_SIZE = kl_struct_len("pid_entry");
	PID_SLOT_SIZE = kl_struct_len("pid_slot");
	PMAP_SIZE = kl_struct_len("pmap");
	PREGION_SIZE = kl_struct_len("pregion");
	PROC_SIZE = kl_struct_len("proc");
	PROC_PROXY_SIZE = kl_struct_len("proc_proxy_s");
	PROTOSW_SIZE = kl_struct_len("protosw");
	QINIT_SIZE = kl_struct_len("qinit");
	QUEUE_SIZE = kl_struct_len("queue");
	REGION_SIZE = kl_struct_len("region");
	RNODE_SIZE = kl_struct_len("rnode");
	SEMA_SIZE = kl_struct_len("sema_s");
	SOCKET_SIZE = kl_struct_len("socket");
	STDATA_SIZE = kl_struct_len("stdata");
	STHREAD_S_SIZE = kl_struct_len("sthread_s");
	STRSTAT_SIZE = kl_struct_len("strstat");
	SWAPINFO_SIZE = kl_struct_len("swapinfo");
	TCPCB_SIZE = kl_struct_len("tcpcb");
	UFCHUNK_SIZE = kl_struct_len("ufchunk");
	UNPCB_SIZE = kl_struct_len("unpcb");
	UTHREAD_S_SIZE = kl_struct_len("uthread_s");
	VFS_SIZE = kl_struct_len("vfs");
	VFS_SIZE = kl_struct_len("vfs");
	VFILE_SIZE = kl_struct_len("vfile");
	VFSSW_SIZE = kl_struct_len("vfssw");
	VNODE_SIZE = kl_struct_len("vnode");
	VPRGP_SIZE = kl_struct_len("vprgp");
	VPROC_SIZE = kl_struct_len("vproc");
	VSOCKET_SIZE = kl_struct_len("vsocket");
	XFS_INODE_SIZE = kl_struct_len("xfs_inode");
	XTHREAD_S_SIZE = kl_struct_len("xthread_s");
	ZONE_SIZE = kl_struct_len("zone");
	PDE_SIZE  = kl_struct_len("pde");

	LIBKERN_DATA->k_struct_sizes->flag = 1;
	return(0);
}
