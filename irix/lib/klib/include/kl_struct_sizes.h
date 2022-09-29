#ident "$Header: "

/* Kernel struct sizes
 */
typedef struct struct_sizes_s {
	int	flag;						/* Struct sizes initialized?             */
	int avlnode_size;               /* avlnode struct                        */
	int bhv_desc_size;              /* bhv_desc struct                       */
	int cfg_desc_size;              /* cfg_desc struct                       */
	int cred_size;                  /* cred struct                           */
	int csnode_size;                /* csnode struct                         */
	int domain_size;                /* domain struct                         */
	int eframe_s_size;              /* eframe_s struct                       */
	int exception_size;             /* exception struct                      */
	int fdt_size;                   /* fdt struct                            */
	int graph_s_size;               /* graph_s struct                        */
	int graph_vertex_group_s_size;  /* graph_vertex_group_s struct           */
	int graph_vertex_s_size;        /* graph_vertex_s struct                 */
	int graph_edge_s_size;          /* graph_edge_s struct                   */
	int graph_info_s_size;          /* graph_info_s struct                   */
	int in_addr_size;               /* in_addr struct                        */
	int invent_meminfo_size;        /* invent_meminfo struct                 */
	int inode_size;                 /* inode struct                          */
	int inpcb_size;                 /* inpcb struct                          */
	int kna_size;                   /* kan struct                            */
	int knetvec_size;               /* knetvec struct                        */
	int kthread_size;               /* kthread struct                        */
	int lf_listvec_size;            /* lf_listvec struct                     */
	int lsnode_size;                /* lsnode struct                         */
	int mbuf_size;                  /* mbuf struct                           */
	int ml_info_size;               /* ml_info struct                        */
	int ml_sym_size;				/* ml_sym struct                         */
	int mrlock_s_size;				/* mrlock_s struct                       */
	int module_info_size;           /* module_info struct                    */
	int mntinfo_size;               /* mntinfo struct                        */
	int nodepda_s_size;             /* nodepda_s struct                      */
	int pda_s_size;                 /* pda_s struct                          */
	int pde_size;                   /* pde struct                            */
	int pfdat_size;                 /* pfdat struct                          */
	int pmap_size;                  /* pmap struct                           */
	int pid_entry_size;             /* pid_entry struct                      */
	int pid_slot_size;              /* pid_slot struct                       */
	int pregion_size;               /* pregion struct                        */
	int proc_size;                  /* proc struct                           */
	int proc_proxy_size;            /* proc_proxy struct                     */
	int protosw_size;               /* protosw struct                        */
	int qinit_size;                 /* qinit struct                          */
	int queue_size;                 /* queue struct                          */
	int region_size;                /* pregion struct                        */
	int rnode_size;                 /* rnode struct                          */
	int sema_size;                  /* sema struct                           */
	int socket_size;                /* socket struct                         */
	int stdata_size;                /* stdata struct                         */
	int sthread_s_size;             /* sthread_s struct                      */
	int strstat_size;               /* strstat struct                        */
	int swapinfo_size;              /* swapinfo struct                       */
	int tcpcb_size;                 /* tcpcb struct                          */
	int ufchunk_size;               /* ufchunk struct                        */
	int unpcb_size;                 /* unpcb struct                          */
	int uthread_s_size;             /* uthread_s struct                      */
	int vfile_size;                 /* vfile struct                          */
	int vfs_size;                   /* vfs struct                            */
	int vfssw_size;                 /* vfssw struct                          */
	int vnode_size;                 /* vnode struct                          */
	int vprgp_size;                 /* vprgp struct                          */
	int vproc_size;                 /* vproc struct                          */
	int vsocket_size;               /* vsocket struct                        */
	int xfs_inode_size;             /* xfs_inode struct                      */
	int xthread_s_size;             /* xthread_s struct                      */
	int zone_size;                  /* zone struct                           */
} struct_sizes_t;

/* From the struct_sizes_s struct
 */
#define AVLNODE_SIZE		(LIBKERN_DATA->k_struct_sizes->avlnode_size)
#define BHV_DESC_SIZE		(LIBKERN_DATA->k_struct_sizes->bhv_desc_size)
#define CFG_DESC_SIZE		(LIBKERN_DATA->k_struct_sizes->cfg_desc_size)
#define CRED_SIZE			(LIBKERN_DATA->k_struct_sizes->cred_size)
#define CSNODE_SIZE			(LIBKERN_DATA->k_struct_sizes->csnode_size)
#define DOMAIN_SIZE			(LIBKERN_DATA->k_struct_sizes->domain_size)
#define EFRAME_S_SIZE		(LIBKERN_DATA->k_struct_sizes->eframe_s_size)
#define EXCEPTION_SIZE		(LIBKERN_DATA->k_struct_sizes->exception_size)
#define FDT_SIZE			(LIBKERN_DATA->k_struct_sizes->fdt_size)
#define GRAPH_S_SIZE		(LIBKERN_DATA->k_struct_sizes->graph_s_size)
#define GRAPH_VERTEX_GROUP_S_SIZE \
					(LIBKERN_DATA->k_struct_sizes->graph_vertex_group_s_size)
#define GRAPH_VERTEX_S_SIZE	(LIBKERN_DATA->k_struct_sizes->graph_vertex_s_size)
#define GRAPH_EDGE_S_SIZE	(LIBKERN_DATA->k_struct_sizes->graph_edge_s_size)
#define GRAPH_INFO_S_SIZE	(LIBKERN_DATA->k_struct_sizes->graph_info_s_size)
#define IN_ADDR_SIZE		(LIBKERN_DATA->k_struct_sizes->in_addr_size)
#define INVENT_MEMINFO_SIZE	(LIBKERN_DATA->k_struct_sizes->invent_meminfo_size)
#define INODE_SIZE			(LIBKERN_DATA->k_struct_sizes->inode_size)
#define INPCB_SIZE			(LIBKERN_DATA->k_struct_sizes->inpcb_size)
#define KNA_SIZE			(LIBKERN_DATA->k_struct_sizes->kna_size)
#define KNETVEC_SIZE		(LIBKERN_DATA->k_struct_sizes->knetvec_size)
#define KTHREAD_SIZE		(LIBKERN_DATA->k_struct_sizes->kthread_size)
#define LF_LISTVEC_SIZE		(LIBKERN_DATA->k_struct_sizes->lf_listvec_size)
#define LSNODE_SIZE			(LIBKERN_DATA->k_struct_sizes->lsnode_size)
#define MBUF_SIZE			(LIBKERN_DATA->k_struct_sizes->mbuf_size)
#define ML_INFO_SIZE		(LIBKERN_DATA->k_struct_sizes->ml_info_size)
#define ML_SYM_SIZE			(LIBKERN_DATA->k_struct_sizes->ml_sym_size)
#define MRLOCK_S_SIZE		(LIBKERN_DATA->k_struct_sizes->mrlock_s_size)
#define MODULE_INFO_SIZE	(LIBKERN_DATA->k_struct_sizes->module_info_size)
#define MNTINFO_SIZE    	(LIBKERN_DATA->k_struct_sizes->mntinfo_size)
#define NODEPDA_S_SIZE		(LIBKERN_DATA->k_struct_sizes->nodepda_s_size)
#define PDA_S_SIZE			(LIBKERN_DATA->k_struct_sizes->pda_s_size)
#define PDE_SIZE			(LIBKERN_DATA->k_struct_sizes->pde_size)
#define PFDAT_SIZE			(LIBKERN_DATA->k_struct_sizes->pfdat_size)
#define PMAP_SIZE			(LIBKERN_DATA->k_struct_sizes->pmap_size)
#define PID_ENTRY_SIZE		(LIBKERN_DATA->k_struct_sizes->pid_entry_size)
#define PID_SLOT_SIZE		(LIBKERN_DATA->k_struct_sizes->pid_slot_size)
#define PREGION_SIZE		(LIBKERN_DATA->k_struct_sizes->pregion_size)
#define PROC_SIZE			(LIBKERN_DATA->k_struct_sizes->proc_size)
#define PROC_PROXY_SIZE		(LIBKERN_DATA->k_struct_sizes->proc_proxy_size)
#define PROTOSW_SIZE		(LIBKERN_DATA->k_struct_sizes->protosw_size)
#define QINIT_SIZE			(LIBKERN_DATA->k_struct_sizes->qinit_size)
#define QUEUE_SIZE			(LIBKERN_DATA->k_struct_sizes->queue_size)
#define REGION_SIZE			(LIBKERN_DATA->k_struct_sizes->region_size)
#define RNODE_SIZE			(LIBKERN_DATA->k_struct_sizes->rnode_size)
#define SEMA_SIZE			(LIBKERN_DATA->k_struct_sizes->sema_size)
#define SOCKET_SIZE			(LIBKERN_DATA->k_struct_sizes->socket_size)
#define STDATA_SIZE			(LIBKERN_DATA->k_struct_sizes->stdata_size)
#define STHREAD_S_SIZE		(LIBKERN_DATA->k_struct_sizes->sthread_s_size)
#define STRSTAT_SIZE		(LIBKERN_DATA->k_struct_sizes->strstat_size)
#define SWAPINFO_SIZE		(LIBKERN_DATA->k_struct_sizes->swapinfo_size)
#define TCPCB_SIZE			(LIBKERN_DATA->k_struct_sizes->tcpcb_size)
#define UFCHUNK_SIZE		(LIBKERN_DATA->k_struct_sizes->ufchunk_size)
#define UNPCB_SIZE			(LIBKERN_DATA->k_struct_sizes->unpcb_size)
#define UTHREAD_S_SIZE		(LIBKERN_DATA->k_struct_sizes->uthread_s_size)
#define VFILE_SIZE			(LIBKERN_DATA->k_struct_sizes->vfile_size)
#define VFS_SIZE			(LIBKERN_DATA->k_struct_sizes->vfs_size)
#define VFSSW_SIZE			(LIBKERN_DATA->k_struct_sizes->vfssw_size)
#define VNODE_SIZE			(LIBKERN_DATA->k_struct_sizes->vnode_size)
#define VPRGP_SIZE			(LIBKERN_DATA->k_struct_sizes->vprgp_size)
#define VPROC_SIZE			(LIBKERN_DATA->k_struct_sizes->vproc_size)
#define VSOCKET_SIZE		(LIBKERN_DATA->k_struct_sizes->vsocket_size)
#define XFS_INODE_SIZE		(LIBKERN_DATA->k_struct_sizes->xfs_inode_size)
#define XTHREAD_S_SIZE		(LIBKERN_DATA->k_struct_sizes->xthread_s_size)
#define ZONE_SIZE			(LIBKERN_DATA->k_struct_sizes->zone_size)

/**
 ** Function prototype
 **/
int klib_set_struct_sizes();
