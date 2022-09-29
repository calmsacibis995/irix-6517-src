/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 3.108 $"

/*
 * DO NOT PUT ANY ROUTINE DEFINITIONS IN THIS FILE.  IT IS ONLY FOR VARIABLE
 * DECLARATIONS.
 */

#include <sys/types.h>
#include <sys/buf.h>
#include <sys/callo.h>
#include <sys/conf.h>
#include <sys/dvh.h>
#include <sys/edt.h>
#include <sys/elog.h>
#include <sys/fcntl.h>
#include <ksys/vfile.h>
#include <sys/flock.h>
#include <sys/immu.h>
#include <sys/ipc.h>
#include <sys/log.h>
#include <sys/map.h>
#include <sys/msg.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/pfdat.h>
#include <sys/page.h>
#include <sys/proc.h>
#include <sys/reg.h>
#include <os/as/region.h>
#include <sys/resource.h>
#include <sys/sbd.h>
#include <sys/schedctl.h>
#include <sys/sem.h>
#include <sys/sema.h>
#include <sys/shm.h>
#include <sys/signal.h>
#include <sys/splock.h>
#include <sys/stream.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/termio.h>
#include <sys/time.h>
#include <sys/tuneable.h>
#include <sys/utsname.h>
#include <sys/var.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/mload.h>
#include <sys/uuid.h>
#include <elf.h>
#include <sym.h>
#include <sys/idbg.h>
#include <sys/mloadpvt.h>
#include <sys/tcpipstats.h>
#include <sys/mbuf.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <os/as/pmap.h>
#include <fs/nfs/types.h>
#include <fs/nfs/nfs.h>
#include <fs/nfs/auth.h>
#include <fs/nfs/xdr.h>
#include <fs/nfs/svc.h>
#include <fs/nfs/nfs_clnt.h>
#include <fs/nfs/nfs_impl.h>
#include <fs/specfs/spec_atnode.h>
#include <fs/specfs/spec_csnode.h>
#include <fs/specfs/spec_lsnode.h>
#include <sys/fs/rnode.h>
#include <sys/strstat.h>
#include <sys/stty_ld.h>
#include <sys/protosw.h>
#include <sys/swap.h>
#include <sys/unpcb.h>
#include <sys/fs/efs_inode.h>
#include <sys/socketvar.h>
#include <ksys/as.h>
#include <ksys/kern_heap.h>
#include <ksys/exception.h>
#include <ksys/pid.h>
#include <ksys/vproc.h>
#include <ksys/sthread.h>
#include <ksys/xthread.h>
#include <ksys/fdt_private.h>
#include <sys/hashing.h>
#include <net/if.h>
#include <net/radix.h>
#include <net/route.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#include <netinet/ip_mroute.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <sys/strtbl.h>
#include <sys/invent.h>
#include <sys/strsubr.h>
#include <io/graph/graph_private.h>
#include <sys/dnlc.h>
#include <sys/rtmon.h>
#include "os/tstamp.h"

/*
 * SCSI header files
 */
#include <sys/scsi.h>
#include <sys/iobuf.h>
#include <sys/dksc.h>

#if SN || IP30
#include "sys/PCI/pciio.h"
#include <sys/ql.h>
#define SH_SG_DESCRIPTOR void
#include <sys/fcadp.h>
#endif


#include "os/proc/pproc_private.h"	/* XXX bogus */
#include "os/proc/pid_private.h"

#include "os/scheduler/space.h"

/*
 * Include nodepda to allow the nodepda_s to be printed by dbx 
 */
#include <sys/nodepda.h>
/*
 * Include dump.h so as to get to the dump header structure.
 * Useful for icrash.
 */
#include <sys/dump.h>

/*
 * A number of source files have been added (in fs/xfs, bsd/misc,
 * etc.) that contain declarations of pointers to key kernel structs.
 * By compiling these modules using '-g' the type related information
 * for the structs gets stored in the kernel's symbol table. Debugging
 * tools such as icrash and dbx rely heavily on this stored information.
 * In order to ensure that this information gets included in the symbol
 * table, a dummy function was added to each of the modules. The
 * icrash_init() function makes a call to each of those dummy functions,
 * which forces the type information in each of the modules to be sucked 
 * in to the symbol table.
 * 
 * It should be noted that the icrash_init() function is NEVER called
 * by the kernel. It exists ONLY as a means of forcing type definitions 
 * to be included in the kernel's symbol table. 
 */
extern void bsd_icrash(void);
extern void xfs_icrash(void);
extern void xlv_icrash(void);
extern void as_icrash(void);
extern void vm_icrash(void);
extern void nfs_icrash(void);
extern void autofs_icrash(void);
extern void lofs_icrash(void);
extern void cachefs_icrash(void);

void
icrash_init()
{
	bsd_icrash();
	xfs_icrash();
	xlv_icrash();
	as_icrash();
	vm_icrash();
	nfs_icrash();
	autofs_icrash();
	lofs_icrash();
	cachefs_icrash();
}

/* 
 * The following structure contains fields that are pointers to 
 * various kernel structures. This forces the type information for 
 * those structures to be sucked into the kernel symbol table for 
 * use by dbx and icrash.
 */
typedef struct icrash_types {
	cfg_desc_t *icrash1;
	cfg_driver_t *icrash2;
	cfg_enet_t *icrash3;
	cfg_fsys_t *icrash4;
	cfg_idbg_t *icrash5;
	cfg_streams_t *icrash6;
	struct mbstat *icrash7;
	struct inpcb *icrash8;
	struct ml_info *icrash9;
	pdaindr_t *icrash10;
	struct pmap *icrash11;
	struct protosw *icrash12;
	struct lsnode *icrash13;
	struct strstat *icrash14;
	struct stty_ld *icrash15;
	struct swapinfo *icrash16;
	struct termio *icrash18;
	struct unpcb *icrash19;
	struct rnode *icrash20;
	struct utsname *icrash21;
	struct inode *icrash22;
	struct socket *icrash23;
	struct mbuf *icrash24;
	struct tcpcb *icrash25;
	struct radix_node *icrash26;
	struct rtentry *icrash27;
	struct in_ifaddr *icrash28;
	struct ifaddr *icrash29;
	struct ifnet *icrash30;
	struct vif *icrash31;
	struct mfc *icrash32;
	struct tbf *icrash33;
	struct pkt_queue *icrash34;
	struct in_multi *icrash35;
	struct in_bcast *icrash36;
	struct hashbucket *icrash37;
	vproc_t *icrash38;
	vproc_ops_t *icrash39;
	pid_entry_t *icrash40;
	struct string_table *icrash41;
	struct string_table_item *icrash42;
	sthread_t *icrash44;
	xthread_t *icrash45;
	ufchunk_t *icrash46;
	fdt_t *icrash47;
	nodepda_t *icrash48;
	cpu_inv_t *icrash49;
	invent_generic_t *icrash50;
	invent_membnkinfo_t *icrash51;
	invent_meminfo_t *icrash52;
	invent_cpuinfo_t *icrash53;
	preg_set_t *icrash54;
	pid_slot_t *icrash55;
	vasid_t *icrash56;
	as_ohandle_t *icrash57;
	as_mohandle_t *icrash58;
	vas_t *icrash59;
	as_faultop_t *icrash60;
	as_fault_t *icrash61;
	as_faultres_t *icrash62;
	as_addspaceop_t *icrash63;
	as_addspace_t *icrash64;
	as_addspaceres_t *icrash65;
	as_deletespaceop_t *icrash66;
	as_deletespace_t *icrash67;
	as_deletespaceres_t *icrash68;
	as_newop_t *icrash69;
	as_new_t *icrash70;
	as_newres_t *icrash71;
	as_getasattr_t *icrash72;
	as_getvaddrattr_t *icrash73;
	as_getattrop_t *icrash74;
	as_getattr_t *icrash75;
	kvmap_t *icrash76;
	as_getattrin_t *icrash77;
	as_setattrop_t *icrash78;
	as_setattr_t *icrash79;
	as_getrangeattrop_t *icrash80;
	as_getrangeattrin_t *icrash81;
	as_getrangeattr_t *icrash82;
	as_setrangeattrop_t *icrash83;
	as_setrangeattr_t *icrash84;
	as_setrangeattrres_t *icrash85;
	as_shakeop_t *icrash86;
	as_shake_t *icrash87;
	as_watchop_t *icrash88;
	as_watch_t *icrash89;
	as_shareop_t *icrash90;
	as_share_t *icrash91;
	as_shareres_t *icrash92;
	as_growop_t *icrash93;
	as_lockop_t *icrash94;
	asvo_ops_t *icrash95;
	as_exec_state_t *icrash96;
	as_scanop_t *icrash97;
	as_scan_t *icrash98;
	dump_hdr_t *icrash99;	
	vnode_t *icrash100;
	bhv_desc_t *icrash101;
	vfile_t *icrash102;
	inventory_t *icrash103;
	gzone_t *icrash104;
	struct pernfsq *icrash105;
	struct dupreq *icrash107;
	struct atnode *icrash108;
	struct csnode *icrash109;

	/* ADD ENTRIES FOR NEW TYPES ABOVE ^^^^^
	 */

	/* so icrash can parse the utrace/rtmon buffers and xlation table 
	 */
	tstamp_obj_t *icrash111;
	tstamp_shared_state_t *icrash112;
	tstamp_event_entry_t *icrash113;
	utrace_trtbl_t *icrash114;

	/*
	 * scsi structures.
	 */
	struct dksoftc *icrash115;
	struct dksc_local_info *icrash116;

	struct scsi_request *icrash117;
	struct scsi_target_info *icrash118;
	struct scsi_part_info *icrash119;
	struct scsi_disk_info *icrash120;

#if SN || IP30
	struct ha_information *icrash121;

	struct fcadpctlrinfo *icrash122;
	struct fcadpluninfo *icrash123;
	struct fcadptargetinfo *icrash124;
#endif


	/* Hardware Graph (hwgraph) related structures ... To be included
	 * so that we get the right graph fields in the symbol table so 
	 * that icrash can create the 'hinv' output from a static system 
	 * crash dump.
	 */
	arb_info_desc_t *graph_icrash1;
	arbitrary_info_t *graph_icrash2;
	graph_info_t *graph_icrash3;
	vertex_hdl_t *graph_icrash4;
	graph_edge_t *graph_icrash5;
	graph_vertex_t *graph_icrash6;
	graph_t *graph_icrash7;
	graph_vertex_group_t *graph_icrash8;
	graph_vertex_place_t *graph_icrash9;
	graph_hdl_t *graph_icrash10;
	vertex_hdl_t *graph_icrash11;
} icrash_types_t;

icrash_types_t *icrash_types;

/* The following data structure abstracts out product and kernel
 * specific information for icrash to configure itself.
 */

struct icrashdef_s {
	__uint32_t	i_pagesz;
	__uint32_t	i_pnumshift;
	__uint32_t	i_usize;
	__uint32_t	i_upgidx;
	__uint32_t	i_kstkidx;
	__uint64_t	i_uaddr;
	__uint64_t	i_kubase;
	__uint64_t	i_kusize;
	__uint64_t	i_k0base;
	__uint64_t	i_k0size;
	__uint64_t	i_k1base;
	__uint64_t	i_k1size;
	__uint64_t	i_k2base;
	__uint64_t	i_k2size;
	__uint64_t	i_tophysmask;
	__uint64_t	i_kptebase;
	__uint64_t	i_kernstack;
	__uint64_t	i_pfn_mask;
	__uint64_t	i_pde_pg_cc;
	__uint64_t	i_pde_pg_m;
	__uint64_t	i_pde_pg_vr;
	__uint64_t	i_pde_pg_g;
	__uint64_t	i_pde_pg_eop;
	__uint64_t	i_pde_pg_nr;
	__uint64_t	i_pde_pg_d;
	__uint64_t	i_pde_pg_sv;
	__uint64_t	i_pde_pg_n;
	__uint64_t	i_mapped_kern_ro_base;
	__uint64_t	i_mapped_kern_rw_base;
	__uint64_t	i_mapped_kern_page_size;
};

struct icrashdef_s icrashdef = {
	(__uint32_t) _PAGESZ,
	(__uint32_t) PNUMSHFT,
	(__uint32_t) KSTKSIZE,
	(__uint32_t) -1,	/* Was EXPGIDX - use -1, an invalid index */
	(__uint32_t) KSTKIDX,
	(__uint64_t) 0,		/* Was UADDR */
	(__uint64_t) KUBASE,
	(__uint64_t) KUSIZE,
	(__uint64_t) K0BASE,
	(__uint64_t) K0SIZE,
	(__uint64_t) K1BASE,
	(__uint64_t) K1SIZE,
	(__uint64_t) K2BASE,
	(__uint64_t) K2SIZE,
	(__uint64_t) TO_PHYS_MASK,
	(__uint64_t) KPTEBASE,
	(__uint64_t) KERNELSTACK,
#if defined(PG_PFNUM)
	(__uint64_t) PG_PFNUM,
#else 
	(__uint64_t) PG_ADDR,
#endif /* PG_PFNUM */
	(__uint64_t) PG_CACHMASK,
	(__uint64_t) PG_M,
	(__uint64_t) PG_VR,
	(__uint64_t) PG_G,
#if defined(PG_EOP)
	(__uint64_t) PG_EOP,
#else
	0,
#endif /* PG_EOP */
	(__uint64_t) PG_NDREF,
	(__uint64_t) PG_D,
	(__uint64_t) PG_SV,
	(__uint64_t) PG_N,
#ifdef MAPPED_KERNEL
	(__uint64_t) MAPPED_KERN_RO_BASE,
	(__uint64_t) MAPPED_KERN_RW_BASE,
	(__uint64_t) MAPPED_KERN_PAGE_SIZE
#else
	(__uint64_t) 0,
	(__uint64_t) 0,
	(__uint64_t) 0
#endif /* MAPPED_KERNEL */
};

unsigned  prfstat;		/* state of profiler, locore.s needs this  */
char *conbuf;
char *putbuf;
char *errbuf;
struct tune tune;
struct var v;

/*
 * File system cache tuneable structures.
 */
struct	buf *global_buf_table;
struct	hbuf *global_buf_hash;
struct  dhbuf *global_dev_hash;

/*
 * dquot - start of the in core quota structures
 */
struct dquot *dquot;

/*
 * This symbol is defined for kernel namelist searching tools.  Using
 * this symbol, the program can lookup up "kernel_magic" and "end", and
 * make sure that the value stored at "kernel_magic" equals the namelist
 * value of "end".  If they don't agree then the namelist being scanned
 * doesn't match /dev/kmem.
 */
extern	long	end;
long	*kernel_magic = &end;

/*
 * since AFS is loadable it is compiled -G0 - these variables are referenced
 * in idbg.c/page.c so must be compiled in at the appropriate G level.
 * In addition we can't have any stubs and be loadable, so we set
 * up pointers to the appropriate functions here.
 */
struct inode;
struct efs_dinode;
void *afs_vnodeopsp;
void *afs_vfsopsp;
void (*afsidestroyp)(struct inode *);
void (*afsdptoipp)(struct efs_dinode *, struct inode *);
void (*afsiptodpp)(struct inode *, struct efs_dinode *);
int (*idbg_prafsnodep)(vnode_t *);
int (*idbg_afsvfslistp)(void);

/* For debugging to force struct decls into symtab */
exception_t *d_exception_t;
shaddr_t *d_shaddr_t;
proc_t *d_proc_t;
reg_t *d_reg_t;
preg_t *d_preg_t;
pfd_t *d_pfd_t;
plist_t *d_plist_t;
phead_t *d_phead_t;
pde_t *d_pde_t;
struct eframe_s *d_eframe_t;
mblk_t d_mblk_t;
dblk_t *d_dblk_t;
queue_t *d_queue_t;
struct module_info *d_module_info_t;
struct qinit *d_qinit_t;
struct streamtab *d_streamtab_t;
struct stdata *d_stdata_t;
struct linkblk *d_linkblk_t;
struct strevent *d_strevent_t;
struct iocblk *d_iocblk_t;
struct stroptions *d_stroptions_t;
struct pathname *d_pathname_t;
job_t *d_job_t;
cpuset_t *d_cpuset_t;
ncache_t *d_ncache_t;
batchq_t *d_batchq_t;

preg_t	syspreg;
reg_t	sysreg;

struct plist	*pdhash;	/* Page delayed-write hash list. */
sv_t	pfdwtsv[NPWAIT];	/* Page wait semaphores. */
int		pdhashmask;	/* Page delayed-write hash mask. */


struct	syswait	syswait;

/*	The following describe the physical memory configuration.
**
**		maxclick - The largest physical click number.
**			   ctob(maxclick+1) is the largest physical
**			   address configured plus 1.
**
**		kpbase	 - The physical address of the start of
**			   the allocatable memory area.  That is,
**			   after all kernel tables are allocated.
**			   Pfdat table entries are allocated for
**			   physical memory starting at this address.
**			   It is always on a page boundary.
**
**		low_maxclick - maxclick before memory hole (not set
**			   on all machines supporting memory holes).
*/

pfn_t	maxclick;
pfn_t	kpbase;
pfn_t	low_maxclick;


/*	The following are concerned with the kernel virtual
**	address space.
**
**		kptbl	- The address of the kernel page table.
**			  It is dynamically allocated in startup.c
*/

pde_t	*kptbl;
