/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1986-1995 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Id: sysget.c,v 1.32 1999/03/02 00:13:42 leedom Exp $"
/*
 * sysget - retrieval of system data across cells
 *
 * Format:  sysget(name, buf, buflen, flags, cookie)
 */
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/sysinfo.h>
#include <sys/atomic_ops.h>
#include <sys/sysmacros.h>
#include <sys/sysmp.h>
#include <sys/sysget.h>
#include <sys/systm.h>
#include <sys/runq.h>
#include <sys/shm.h>
#include <sys/map.h>
#include <sys/nodepda.h>
#include <sys/page.h>
#include <sys/lpage.h>
#include <sys/conf.h>
#include <sys/var.h>
#include <sys/splock.h>
#include <sys/vnode.h>
#include <sys/strstat.h>
#include <bsd/sys/tcpipstats.h>
#include <fs/nfs/nfs_stat.h>
#include <fs/cachefs/cachefs_fs.h>
#include <ksys/vshm.h>
#include <ksys/vhost.h>
#include <ksys/vm_pool.h>
#include <sys/miser_public.h>

static void addinfo(__uint32_t *, __uint32_t *, int);
static void addminfo(struct minfo *umi, struct minfo *kmi);
static caddr_t sysget_ksa_to_stats(int, struct ksa *);
static int sysget_pda(int, char *, int *, int, sgt_cookie_t *, sgt_info_t *);
static int sysget_gen(int, char *, int *, int, sgt_cookie_t *, sgt_info_t *);
static int sysget_node(int, char *, int *, int, sgt_cookie_t *, sgt_info_t *);
static int sysget_ksym(int, char *, int *, int, sgt_cookie_t *, sgt_info_t *);

extern caddr_t cachefs_getstat(int);
extern void m_allocstats(struct mbstat *);
extern void in_pcb_sockstats(struct sockstat *);
extern __uint32_t str_getpage_usage(void);

extern struct tunetable tunetable[];
extern struct tunename tunename[];
extern int bufmem, chunkpages, dchunkpages, pdcount;
extern unsigned int pmapmem;
extern int tuneentries;
extern int tunename_cnt;
extern char _physmem_start[];
extern daddr_t swplo;
extern __int32_t avenrun[3];
extern struct var v;
extern uint _ftext[]; 
extern int syssegsz;

extern hotLongCounter_t	vn_vnumber;
extern hotIntCounter_t	vn_nfree;
extern int	vn_epoch;
extern int	min_file_pages;
extern int	min_free_pages;

/*
 * Define jump-table for list of objects supported by sysget.  Each object
 * must have a function associated with it that can be called to return
 * the object.  The flags determine the type of operations supported for
 * each object.  It is up to the function to enforce these.  The functions
 * must work in conjunction with the sysget iterator in the VHOST service
 * which expects certain return values to control how iteration is done.
 * These are:
 *    1) If an error occurs a non-zero return value is to be returned.
 *    2) If no match for the data requested can be made on this cell then
 *       the buflen parameter returned is zero. The cookie is un-touched.
 *    3) A non-zero buflen parameter value indicates the number of bytes copied.
 *       The cookie status is re-set to SC_DONE if all the data could be
 *       copied or SC_CONTINUE if the buffer was not large enough.
 *    4) If SC_CONTINUE is to be returned the cookie must contain a value
 *       that will allow the call to be re-started where it left off.
 *    5) The first word of the cookie opaque field is always assumed to
 *       contain a cellid to allow the VHOST iterator select the correct cell.
 */
sgt_attr_t sysget_attr[SGT_MAX] = {
	sizeof(struct sysinfo),	          		/* SGT_SINFO */
		SA_PERCPU | SA_SUMMABLE, sysget_pda, 
	sizeof(struct minfo),	             		/* SGT_MINFO */
		SA_SUMMABLE, 		 sysget_pda, 
	sizeof(struct dinfo),                		/* SGT_DINFO */
		SA_PERCPU | SA_SUMMABLE, sysget_pda, 
	sizeof(struct syserr),               		/* SGT_SERR */
		SA_PERCPU | SA_SUMMABLE, sysget_pda, 
	sizeof(struct ncstats),	              		/* SGT_NCSTATS */
		SA_PERCPU | SA_SUMMABLE, sysget_pda, 
	sizeof(struct igetstats),            		/* SGT_EFS */
		SA_PERCPU | SA_SUMMABLE, sysget_pda,
	0, 		 	               		/* ...? */
		0, 0,
	sizeof(struct rminfo),                		/* SGT_RMINFO */
		SA_SUMMABLE | SA_GLOBAL, sysget_gen,
	sizeof(struct getblkstats),           		/* SGT_BUFINFO */
		SA_PERCPU | SA_SUMMABLE, sysget_pda, 
	0,			              		/* SGT_RUNQ obsolete */
		0, sysget_gen,
	0,			               		/* SGT_DISPQ obsolete*/
		0, 0,
	0,	              				/* SGT_PSET obsolete */
		0, 0,
	sizeof(struct vnodestats),           		/* SGT_VOPINFO */
		SA_PERCPU | SA_SUMMABLE, sysget_pda, 
	sizeof(struct kna),   	               		/* SGT_TCPIPSTATS */
		SA_PERCPU | SA_SUMMABLE | SA_GLOBAL, sysget_pda, 
	sizeof(struct rcstat),               	 	/* SGT_RCSTAT */
		SA_PERCPU | SA_SUMMABLE | SA_GLOBAL, sysget_pda, 
	sizeof(struct clstat),	              		/* SGT_CLSTAT */
		SA_PERCPU | SA_SUMMABLE | SA_GLOBAL, sysget_pda, 
	sizeof(struct rsstat),	              		/* SGT_RSSTAT */
		SA_PERCPU | SA_SUMMABLE | SA_GLOBAL, sysget_pda, 
	sizeof(struct svstat),                		/* SGT_SVSTAT */
		SA_PERCPU | SA_SUMMABLE | SA_GLOBAL, sysget_pda,
	0,			                	/* ... ? */
		0, 0,
	sizeof(struct xfsstats),             		/* SGT_XFSSTATS */
		SA_PERCPU | SA_SUMMABLE, sysget_pda, 
	sizeof(struct clstat),                		/* SGT_CLSTAT3 */
		SA_PERCPU | SA_SUMMABLE | SA_GLOBAL, sysget_pda, 
#ifdef TILES_TO_LPAGES
	sizeof(struct tileinfo),              		/* SGT_TILEINFO */
		0, sysget_gen,
#else
	0,			                
		0, 0,
#endif /* TILES_TO_LPAGES */
	sizeof(struct cachefs_stats),	 		/* SGT_CFSSTAT */
		SA_PERCPU | SA_SUMMABLE | SA_GLOBAL, sysget_pda, 
	sizeof(struct svstat),	               		/* SGT_SVSTAT3 */
		SA_PERCPU | SA_SUMMABLE | SA_GLOBAL, sysget_pda, 
	sizeof(struct nodeinfo),               		/* SGT_NODE_INFO */
		SA_PERNODE, sysget_node,
	sizeof(struct lpg_stat_info),          		/* SGT_LPGSTATS */
		0, sysget_gen,
	sizeof(struct shmstat),	               		/* SGT_SHMSTAT */   
		SA_LOCAL, 0,
	-1,						/* SGT_KSYM */
		SA_SUMMABLE, sysget_ksym,
	-1,	          				/* SGT_MSGQUEUE */   
		SA_LOCAL, 0,
	-1,	          				/* SGT_SEM */   
		SA_LOCAL, 0,
	sizeof(struct sockstat),   	     		/* SGT_SOCKSTATS */
		SA_PERCPU | SA_LOCAL, sysget_pda, 
	sizeof(struct sysinfo_cpu),	       		/* SGT_SINFO_CPU */
		SA_PERCPU | SA_SUMMABLE, sysget_pda, 
	sizeof(struct strstat),   	       		/* SGT_STREAMSTATS */
		SA_PERCPU | SA_SUMMABLE | SA_GLOBAL, sysget_pda, 
};

/* Define table structure for list of kernel symbols supported by 
 * SGT_KSYM.  This table is for simple kernel structures that don't
 * require pre-processing before returning the data to the user.
 */
struct sgt_ksym {
	void 	*addr;
	int	size;
	int	num;
	int	flags;
	char	*symname;
};

struct sgt_ksym sgt_ksym_static[] = {
 &v,		sizeof(v),         1,   SA_GLOBAL, 	KSYM_VAR,
 &swplo, 	sizeof(swplo),	   1,   SA_GLOBAL,  	KSYM_SWPLO,
 &defaultsemameter, sizeof(uint),  1,   SA_GLOBAL,  	KSYM_SEMAMETER,
 &time,		sizeof(time_t),	   1,   0,          	KSYM_TIME,
 &splockmeter,	sizeof(int),	   1,   SA_GLOBAL,  	KSYM_SPLOCKMETER,
 avenrun,	sizeof(avenrun),   1,   SA_GLOBAL,  	KSYM_AVENRUN,
 &physmem,	sizeof(pfn_t),	   1,   SA_GLOBAL,  	KSYM_PHYSMEM,
 &kpbase,	sizeof(pfn_t),	   1,	SA_GLOBAL,  	KSYM_KPBASE,
 &GLOBAL_FREEMEM_VAR, sizeof(pfn_t), 1, SA_GLOBAL,  	KSYM_FREEMEM,
 &GLOBAL_USERMEM_VAR,	sizeof(pgno_t),	   1,   SA_GLOBAL,  	KSYM_USERMEM,
 &dchunkpages,	sizeof(int),	   1,   SA_GLOBAL,	KSYM_PDWRIMEM,
 &bufmem,	sizeof(int),	   1,   SA_GLOBAL, 	KSYM_BUFMEM,
 &chunkpages,	sizeof(int),	   1,   SA_GLOBAL, 	KSYM_CHUNKMEM,
 &maxclick,	sizeof(int),	   1,   SA_GLOBAL,	KSYM_MAXCLICK,
 _physmem_start,sizeof(int),	   1,   SA_GLOBAL,	KSYM_PSTART,
 _ftext,	sizeof(uint),	   1,   SA_GLOBAL,	KSYM_TEXT,
 &syssegsz,     sizeof(int),	   1,   SA_GLOBAL,	KSYM_SYSSEGSZ,
 tunename,	sizeof(struct tunename), 0, SA_GLOBAL, 	KSYM_TUNENAME,
 tunetable,	sizeof(struct tunetable), 0, SA_GLOBAL, KSYM_TUNETABLE,
 (void*)&vn_vnumber.value, sizeof(u_long), 1,   0,	KSYM_VN_VNUMBER,
 &vn_epoch,	sizeof(int),	   1,   0,		KSYM_VN_EPOCH,
 (void*)&vn_nfree.value, sizeof(int), 1,   0,		KSYM_VN_NFREE,
 &min_file_pages, sizeof(int),	   1,   0,		KSYM_MIN_FILE_PAGES,
 &min_free_pages, sizeof(int),	   1,   0,		KSYM_MIN_FREE_PAGES,
};

/* Define dynamic ksym list */
struct sgt_ksym_list {
	struct sgt_ksym ksym;
	struct sgt_ksym_list *next;
};
struct sgt_ksym_list *sgt_ksym_list;
struct sgt_ksym_list *sgt_ksym_free;
#define KSYM_FREE_MAX	100
mutex_t sgt_ksym_sem;
	
/* Define cookie for use with SGT_KSYM requests */
struct sgt_cookie_ksym {
	cell_t	curcell;
	char    symname[KSYM_LENGTH];
};
	
void
ksym_add(char *symname, void *addr, int size, int num, int flags)
{
	int i;
	struct sgt_ksym_list *kp;
	static volatile int ksym_add_init;

	/* Since ksym_add can be called at any time during boot we have
	 * to make sure the ksym list mutex is initialized first.
	 */

	i = test_and_set_int((int *)&ksym_add_init, 2); /* Returns old value */
	if (i == 2) {
		/* someone else initializing - wait */

		while (ksym_add_init == 2);
	}
	else if (i == 1) {
		ksym_add_init = 1; /* Already done. Reset it back to 1 */
	}
	else {
		/* First time thru */

		mutex_init(&sgt_ksym_sem, MUTEX_DEFAULT, "sgt_ksym");
		ksym_add_init = 1;
	}

	mp_mutex_lock(&sgt_ksym_sem, PZERO);

	if (!sgt_ksym_free) {

		/* Free list is empty - allocate more */

		kp = (struct sgt_ksym_list *)kmem_zalloc(
			KSYM_FREE_MAX * sizeof(struct sgt_ksym_list), KM_SLEEP);
		for (i = 0; i < KSYM_FREE_MAX; i++) {
			kp->next = sgt_ksym_free;
			sgt_ksym_free = kp++;
		}
	}
	kp = sgt_ksym_free;
	sgt_ksym_free = sgt_ksym_free->next;

	kp->ksym.symname = symname;
	kp->ksym.addr = addr;
	kp->ksym.size = size;
	kp->ksym.num = num;
	kp->ksym.flags = flags;

	kp->next = sgt_ksym_list;
	sgt_ksym_list = kp;

	mp_mutex_unlock(&sgt_ksym_sem);
}

struct sgt_ksym *
ksym_find(char *symname, int required)
{
	struct sgt_ksym_list *klp = sgt_ksym_list;

	while (klp) {
		if (!strcmp(klp->ksym.symname, symname)) {
			break;
		}
		klp = klp->next;
	}
	if (!klp && required) {
		cmn_err(CE_PANIC, "ksym_find: %s not found\n", symname);
	}
	return(&klp->ksym);
}

void
ksym_init(void)
{
	int i;
	struct sgt_ksym *kap;
	static int ksym_initialized = 0;

	if (ksym_initialized) {
		return;
	}

	/* First time thru so we initialize as much as
	 * we can. 
	 */

	for (i = 0; i < sizeof(sgt_ksym_static) / sizeof(struct sgt_ksym);
			 i++) {
		kap = &sgt_ksym_static[i];
		ksym_add(kap->symname, kap->addr, kap->size, kap->num,
			 kap->flags);
	}
			
	kap = ksym_find(KSYM_TUNETABLE, 1);
	kap->num = tuneentries;
	kap = ksym_find(KSYM_TUNENAME, 1);
	kap->num = tunename_cnt;

	ksym_initialized++;
}

/* ARGSUSED */
int
phost_sysget(
	bhv_desc_t      *bdp,
	int		name,
	char		*buf,
	int		*buflen_p,
	int		flags,
	sgt_cookie_t	*cookie_p,
	sgt_info_t	*info_p)
{
	
	int error;

	error = (*sysget_attr[name - 1].func)(name, buf, buflen_p, flags,
		 cookie_p, info_p);
	return(error);
}

struct sysgeta {
	sysarg_t name;
	char *buf;
	sysarg_t buflen;
	sysarg_t flags;
	sgt_cookie_t *cookie_p;
};

sysget(struct sysgeta *uap, rval_t *rvp)
{
	int error;
	sgt_cookie_t cookie;
	sgt_info_t info;
	int tmp_buflen = 0;
	int buflen = uap->buflen;
	sgt_attr_t *sap;
	struct sgt_ksym *kap = (struct sgt_ksym *)0;
	char *buffer = uap->buf;

	/* Check for out-of-range index */

	if (uap->name < 1 || uap->name > SGT_MAX) 
		return(EINVAL);

	sap = &sysget_attr[uap->name - 1];
	if (!sap->func || !sap->size) {

		/* Function not supported yet */

		return(ENOENT);
	}

	if (copyin(uap->cookie_p, &cookie, sizeof(cookie))) 
		return(EFAULT);

	if (cookie.sc_status == SC_DONE) {

		/* Cookie is not set correctly */

		return(EINVAL);
	}

#ifndef CELL
	if (cookie.sc_type == SC_CELLID && 
			!(cookie.sc_id.cellid == SC_CELL_ALL ||
			cookie.sc_id.cellid == SC_MYCELL)) {

		/* Cell-specific actions are not supported */

		return(ENOTSUP);
	}
#endif

	if (!uap->flags) {

		/* Must specify a flag */

		return(EINVAL);
	}
	uap->flags |= SGT_UADDR;	/* To identify buffer type */

	if (!(sap->flags & SA_PERCPU) &&
			 (uap->flags & (SGT_CPUS | SGT_NODES))) {

		/* These aren't per-cpu stats */

		return(EINVAL);
	}

	if (!(sap->flags & (SA_PERNODE | SA_PERCPU)) &&
			 cookie.sc_type == SC_CNODEID) {

		/* These aren't per-node stats */

		return(EINVAL);
	}

	if (uap->name == SGT_KSYM) {

		ksym_init();

		/* The index to the actual object is in the cookie */

		kap = ksym_find(
		     ((struct sgt_cookie_ksym *)cookie.sc_opaque)->symname, 0);
		if (!kap) {
			return(ENOENT);
		}
	}

	if (uap->flags & SGT_SUM) {
		 if (!(sap->flags & SA_SUMMABLE)) {

			/* Summing not supported for this object */

			return(ENOTSUP);
		}
#if CELL
		/* Check that the caller wants to check all cells.  
		 */
		else if (cookie.sc_type != SC_CELLID || 
				cookie.sc_id.cellid != SC_CELL_ALL) {

			/* Summing is only allowed across all cells */

			return(EINVAL);
		}

		if (uap->flags & SGT_READ) {

			/* Allocate a buffer to sum in to. We initialize it to
			 * zero so the other cells can sum into it and clear
			 * the UADDR flag to indicate it is a kernel buffer.
			 * We assume that the buffer size is the same for all
			 * cells.  We have to convert the address to a physical
			 * one so it can be used on other cells.
			 */

			if (uap->name == SGT_KSYM) {
				tmp_buflen = kap->size * kap->num;
			}
			else if (uap->flags & SGT_CPUS) {
				tmp_buflen = sap->size * maxcpus;
			}
			else if (uap->flags & SGT_NODES) {
				tmp_buflen = sap->size * numnodes;
			}
			else {
				tmp_buflen = sap->size;
			}
			buffer = (char *)kmem_zalloc(tmp_buflen, KM_SLEEP);
			buflen = tmp_buflen;
			uap->flags &= ~SGT_UADDR;
		}
		
#else
		/* For NON cell systems the SUM flag is meaningless so we
		 * have to disable it since the data functions require that
		 * summing is done to a kernel buffer allocated by the vhost
		 * iterator.
                 */

		uap->flags &= ~SGT_SUM;
#endif
	}

	if (uap->flags & SGT_WRITE && !(sap->flags & SA_RDWR)) {

		/* write access not supported for this object */

		return(ENOTSUP);
	}

	if (sap->flags & SA_LOCAL || 
				(cookie.sc_type == SC_CELLID &&
				(cookie.sc_id.cellid == cellid() ||
			 	 cookie.sc_id.cellid == SC_MYCELL))) {

		/* Local cell only */

		error = (*sap->func)(uap->name, buffer, &buflen, uap->flags,
			&cookie, &info);
	}
	else {

		/* Go to server to aggregate across cells */

		bzero(&info, sizeof(info));
		VHOST_SYSGET(uap->name, buffer, &buflen, uap->flags,
			 &cookie, &info, error);
	}

	if (error) {
		goto done;
	}

	if (!buflen && uap->buflen && cookie.sc_status != SC_DONE) {

		/* cookie must have not have resulted in a valid match */

		error = ESRCH;
		goto done;
	}

	if (uap->flags & SGT_INFO) {

		/* Need to copyout info result */

		buflen = MIN(uap->buflen, sizeof(info));
		if (copyout(&info, uap->buf, buflen)) {
			error = EFAULT;
			goto done;
		}
	}

	if (tmp_buflen) {
		buflen = MIN(uap->buflen, tmp_buflen);
		buffer = (char *)PHYS_TO_K0(buffer);
		if (copyout(buffer, uap->buf, buflen)) {
			error = EFAULT;
			goto done;
		}
	}

	/* Copyout the cookie */

	if (copyout(&cookie, uap->cookie_p, sizeof(cookie))) {
		error = EFAULT;
		goto done;
	}

done:
	if (tmp_buflen) {
		kmem_free(buffer, tmp_buflen);
	}
	rvp->r_val1 = buflen;
	return(error);
}

struct sgt_cookie_pda {
	cell_t	curcell;
	uint	index;
};

/*
 * Generic routine used by pda entries in sysget_attr table. It has to be
 * able to sum up objects across all nodes and cpus or be able to return
 * the list of objects for all nodes or cpus depending on the flags specified.
 * If the caller's buffer is not big enough it has to return a cookie that
 * that allows a subsequent call to continue.  The cookie has a field which
 * is used as the index into the pdaindr array.
 */
static int
sysget_pda(	int name,
		char *buf,
		int *buflen_p,
		int flags,
		sgt_cookie_t *cookie_p,
		sgt_info_t *info_p)
{
	int i, start = 0;
	int copied, error = 0;
	int node_cpu_cnt = 0;
	int end = maxcpus;
	sgt_attr_t *ap = &sysget_attr[name - 1];
	caddr_t ubuf, src = NULL;
	caddr_t sarg, targ = NULL;
	int klen, ulen;
	struct ksa *ksa;
	struct kna *kna;
	cnodeid_t prev_id = -1;
	static int nacpus = 0;
	struct sockstat sockstat;
	struct strstat *kstr_statsp;
	sgt_stat_t stat;

	struct sgt_cookie_pda *ckop =
		(struct sgt_cookie_pda *)cookie_p->sc_opaque;
	
	/* process request for this cell */
	if (!nacpus) {
		for (i = 0, klen = 0; i < maxcpus; i++) {
			if ((pdaindr[i].CpuId == -1) ||
				!pdaindr[i].pda->ksaptr) {
				continue;
			}
			klen++;
		}
		nacpus = klen;
	}

	if (cookie_p->sc_type == SC_CNODEID) {

		/* Check if the specified node is part of this cell */

		for (i = 0; i < maxcpus; i++) {
			if (pdaindr[i].pda->p_nodeid ==
					 cookie_p->sc_id.cnodeid) {
				node_cpu_cnt++;
			}
		}
		if (!node_cpu_cnt) {

			/* No match found */

			*buflen_p = 0;
			return(0);
		}
	}
	else if (cookie_p->sc_type == SC_CPUID) {
		if (cookie_p->sc_id.cpuid < 0 || cookie_p->sc_id.cpuid >=
				maxcpus) {

			/* No match found */

			*buflen_p = 0;
			return(0);
		}
		start = cookie_p->sc_id.cpuid;
		end = start + 1;
	}

	switch (flags & (SGT_READ | SGT_WRITE | SGT_INFO | SGT_STAT)) {
	case SGT_INFO:
	case SGT_STAT:

		/* accumulate info for this cell into the info_p struct */

		info_p->si_size = ap->size;
		if (flags & SGT_NODES) {
			info_p->si_num = numnodes;
		}
		else if (flags & SGT_CPUS) {
			if (node_cpu_cnt) {
				info_p->si_num = node_cpu_cnt;
			}
			else {
				info_p->si_num = nacpus;
			}
		}
		else {
			info_p->si_num = 1;
		}
		info_p->si_hiwater = info_p->si_num;
		cookie_p->sc_status = SC_DONE;
		if (flags & SGT_INFO) {
			copied = sizeof(sgt_info_t);
		}
		else {
			stat.info = *info_p;
			stat.cellid = cellid();
			ulen = MIN(*buflen_p, sizeof(stat));
			if (copyout(&stat, buf, ulen)) {
				copied = 0;
				error = EFAULT;
				break;
			}
			copied = ulen;
		}
		break;
	case SGT_READ:

		if (cookie_p->sc_status == SC_CONTINUE) {

			/* the user's cookie tells us where it left off */

			start = ckop->index + 1;
		}
		else if (cookie_p->sc_status == SC_SEEK) {

			/* Cookie defines a cpu as a starting point */

			start = ckop->index;
			if (start < 0 || start >= maxcpus || 
				pdaindr[start].CpuId == -1) {

				/* specified cpu is invalid */

				return(ESRCH);
			}
		}

		/* Initialize buffer arguments */

		if (!(flags & SGT_UADDR)) {

			/* The buffer was already set up by the kernel */

			klen = *buflen_p;
			targ = buf;
		}
		else {

			/* Allocate a temporary work buffer */

			klen = ap->size;
			if (flags & SGT_CPUS) {
				klen *= nacpus;
			}
			else if (flags & SGT_NODES) {
				klen *= numnodes;
			}
			targ = kmem_zalloc(klen, KM_CACHEALIGN);
		}
		sarg = targ;	/* Save current position within targ */

		if (flags & SGT_SUM) {
			ASSERT(!(flags & SGT_UADDR));
		}

		/* Aggregate across cpus/nodes. If the user's buffer is not
		 * large enough the cookie will point to the next cpu/node
		 * where we left off.
		 */

		copied = 0;
		ubuf = buf;
		ulen = *buflen_p;

		for (i = start; i >= 0 && i < end; i++) {
			if (pdaindr[i].CpuId == -1) {
				continue;
			}
			ksa = pdaindr[i].pda->ksaptr;
			if (!ksa || (node_cpu_cnt && pdaindr[i].pda->p_nodeid !=
					cookie_p->sc_id.cnodeid)) {
				continue;
			}

			switch (name) {
			case SGT_MINFO:
				addminfo((struct minfo *)sarg, &ksa->mi); 
				break;
			case SGT_TCPIPSTATS:
				kna = (struct kna *)pdaindr[i].pda->knaptr;
				if (kna) {
					/*
					 * For first cpu return global
					 * mbuf allocation counts
					 * for entire system
					 */
					src = (caddr_t)&kna->ipstat;
					if (i == 0) {
					  m_allocstats(&kna->mbstat);
					}
				} else {
					src = NULL;
				}
				break;
			case SGT_RCSTAT:
				src = nfs_getrcstat(i);
				break;
			case SGT_CLSTAT:
				src = nfs_getclstat(i);
				break;
			case SGT_CLSTAT3:
				src = nfs_getclstat3(i);
				break;
			case SGT_RSSTAT:
				src = nfs_getrsstat(i);
				break;
			case SGT_SVSTAT:
				src = nfs_getsvstat(i);
				break;
			case SGT_SVSTAT3:
				src = nfs_getsvstat3(i);
				break;
			case SGT_CFSSTAT:
				src = cachefs_getstat(i);
				break;
			case SGT_SOCKSTATS:
				src = (caddr_t)&sockstat;
				in_pcb_sockstats(&sockstat);
				break;
			case SGT_STREAMSTATS:
				kstr_statsp = (struct strstat *)
					pdaindr[i].pda->kstr_statsp;
				src = (kstr_statsp)
					? (caddr_t)kstr_statsp : NULL;
				break;
			default:
				src = sysget_ksa_to_stats(name, ksa);
				break;
			}

			/*
			 * This part is ugly.  We have to copy the data back
			 * to the user's buffer.  If SGT_CPUS is set we
			 * can copy each cpu's data seperately without having
			 * to add it together unless SGT_SUM is specified
			 * in which case we have to sum them with the cpu data
			 * from the user's buffer first. sarg is the current
			 * position within the temp buffer pointed to by targ.
			 */

			if (src && (!(flags & SGT_CPUS) || flags & SGT_SUM)) {

				/* Add in this cpu's data */
				addinfo((__uint32_t *)sarg, (__uint32_t *)src,
					ap->size / sizeof(__uint32_t));
			}
			else if (!src && (flags & (SGT_CPUS | SGT_NODES))) {

				/* Per-cpu data currently not available */

				error = EINVAL;
				break;
			}
			
			if (flags & SGT_CPUS) {
				if (!(flags & SGT_SUM)) {
					bcopy(src, sarg, ap->size);
				}
				sarg += ap->size;
			}
			else if ((flags & SGT_NODES) &&
					pdaindr[i].pda->p_nodeid != prev_id) {
				if (!(flags & SGT_SUM)) {
					bcopy(src, sarg, ap->size);
				}
				sarg += ap->size;
				prev_id = pdaindr[i].pda->p_nodeid;
			}
		} /* end of for loop */

		if (error) {
			break;
		}

		/* Reset the cookie */

		if (ulen < klen && ulen >= ap->size) {

			/* User's buffer wasn't big enough so return a
			 * cookie that will allow the call to re-start
			 * where it left off.
			 */

			cookie_p->sc_status = SC_CONTINUE;
			ckop->curcell = cellid();
			ckop->index = ulen / ap->size - 1;
		} else {
			/* Set cookie to indicate this cell is done.
			 * We re-initialize the index for the next cell
			 * in case the iterator decides to give up.
			 */
			cookie_p->sc_status = SC_DONE;
			ckop->index = -1;
		}
				
		if (!(flags & SGT_UADDR)) {
			copied = klen;
		}
		else {

			/* Copy out the data for this cell to the user buffer */

			ulen = MIN(ulen, klen);
			if (copyout(targ, ubuf, ulen)) {
				error = EFAULT;
				break;
			}
			copied = ulen;
		}
		break;
	default:
		error = ENOTSUP;
	}

	if (targ && (flags & SGT_UADDR)) {
		kmem_free(targ, klen);
	}
	*buflen_p = copied;
	return(error);
}

static void
addinfo(__uint32_t *accum, __uint32_t *from, int n)
{
	while (n--)
		*accum++ += *from++;
}

static void
addminfo(struct minfo *umi, struct minfo *kmi)
{
	extern ulong freeswap;
	u_long oldmem;
	int	zonemem;

	/*
	 * First time around compute memory in zones. Add it to the
	 * heap.
	 */
	if (umi->heapmem == 0) {
		extern void kmem_zone_mem(int *, int *);
		extern int bmappedcnt;
		extern int ptpool_vm;

		kmem_zone_mem(&zonemem, &umi->sptzone);

		umi->heapmem = ctob(zonemem);
		sptgetsizes(&sptbitmap, &umi->sptclean, &umi->sptdirty,
			&umi->sptaged, &umi->sptintrans);

		umi->sptbp = bmappedcnt;
		umi->sptheap = kmi->sptheap;
		umi->sptpt = ptpool_vm;
	}

	oldmem = umi->freemem[0];
	umi->freemem[0] += kmi->freemem[0];
	umi->freemem[1] += kmi->freemem[1];
	if (umi->freemem[0] < oldmem)
		umi->freemem[1]++;
	umi->freeswap = freeswap;	/* recalced by 1 processor */
	umi->vfault += kmi->vfault;
	umi->demand += kmi->demand;
	umi->swap += kmi->swap;
	umi->cache += kmi->cache;
	umi->file += kmi->file;
	umi->pfault += kmi->pfault;
	umi->cw += kmi->cw;
	umi->steal += kmi->steal;
	umi->freedpgs += kmi->freedpgs;
	umi->unmodsw += kmi->unmodsw;
	umi->unmodfl += kmi->unmodfl;
	umi->tlbpids += kmi->tlbpids;
	umi->tfault += kmi->tfault;
	umi->rfault += kmi->rfault;
	umi->tlbflush += kmi->tlbflush;
	umi->tlbsync += kmi->tlbsync;
	umi->tvirt += kmi->tvirt;
	umi->tphys += kmi->tphys;
	umi->twrap += kmi->twrap;
	umi->tdirt += kmi->tdirt;
	umi->heapmem += kmi->heapmem;
	umi->hovhd += kmi->hovhd;
	umi->hunused += kmi->hunused;
	umi->halloc += kmi->halloc;
	umi->hfree += kmi->hfree;
	umi->iclean += kmi->iclean;
	umi->sfault += kmi->sfault;
	umi->bsdnet += kmi->bsdnet;
	umi->palloc += kmi->palloc;
	umi->sptalloc += kmi->sptalloc;
	umi->sptfree += kmi->sptfree;

}

static caddr_t
sysget_ksa_to_stats(int code, struct ksa *ksa)
{

	switch (code) {
	default:
		return NULL;

	case SGT_SINFO:
	case SGT_SINFO_CPU:
		return (caddr_t) &ksa->si;
	case SGT_MINFO:
		return (caddr_t) &ksa->mi;
	case SGT_DINFO:
		return (caddr_t) &ksa->di;
	case SGT_SERR:
		return (caddr_t) &ksa->se;
	case SGT_NCSTATS:
		return (caddr_t) &ksa->ncstats;
	case SGT_EFS:
		return (caddr_t) &ksa->p_igetstats;
	case SGT_BUFINFO:
		return (caddr_t) &ksa->p_getblkstats;
	case SGT_VOPINFO:
		return (caddr_t) &ksa->p_vnodestats;
	case SGT_XFSSTATS:
		return (caddr_t) &ksa->p_xfsstats;
	}

	/* NOTREACHED */
}

/*
 * Handler for old-style MPSA_SAGET requests. This only returns data for
 * the local cell to remain compatible with the old behavior of sysmp.
 */
sysget_mpsa1(int name, char *buf, int buflen, int index)
{
	sgt_cookie_t cookie;
	sgt_info_t info;
	int flags = SGT_READ | SGT_UADDR;
	int error;
	
	if (!sysget_attr[name - 1].func || !sysget_attr[name - 1].size) {
		return(ENOENT);
	}

	/* Set up a cookie to return data for the cpu specified by "index" */

	if (!(sysget_attr[name - 1].flags & SA_PERCPU)) {

		/* These aren't per-cpu stats */

		return(EINVAL);
	}

	if (index < 0 || index >= maxcpus || pdaindr[index].CpuId == -1) {
		return(EINVAL);
	}

	SGT_COOKIE_SET_CPU(&cookie, index);
	error = phost_sysget((bhv_desc_t *)NULL, name, buf, &buflen, flags,
		 &cookie, &info);

	return((error == ESRCH ? EINVAL : error));
}

/*
 * Handler for old-style MPSA_SAGET requests. These only return data for
 * the local cell to remain compatible with the old behavior of sysmp.
 */
sysget_mpsa(int name, char *buf, int buflen)
{
	sgt_cookie_t cookie;
	sgt_info_t info;
	int flags = SGT_READ | SGT_UADDR;
	int error;
	
	if (!sysget_attr[name - 1].func || !sysget_attr[name - 1].size) {
		return(ENOENT);
	}

	/* Set up a cookie to return mpsa data */

	SGT_COOKIE_INIT(&cookie);
	error = phost_sysget((bhv_desc_t *)NULL, name, buf, &buflen, flags,
		 &cookie, &info);

	return(error);
}

struct rminfo rminfo;

static void
update_rminfo(void)
{
	static time_t last_update;
	extern int bufmem, chunkpages, dchunkpages, pdcount;
	extern unsigned int pmapmem;
	extern long Strcount;
	extern time_t lbolt;

	/* To try and keep an accurate picture of memory resources in-use
	 * for user programs that request it, we update an rminfo structure
	 * here.  This should make it less likely that some of the data
	 * is stale since we get that data immediately after a call to
	 * GLOBAL_FREEMEM_SNAP.  Programs that ask for rminfo now
	 * get a once-per-second snapshot as they request it.
	 */

	if (lbolt - last_update < HZ) {
		return;
	}

	rminfo.freemem = GLOBAL_FREEMEM_SNAP();
	rminfo.availsmem = GLOBAL_AVAILSMEM() +
		(miser_pool ? GET_AVAILSMEM(miser_pool) : 0);
	rminfo.availrmem = GLOBAL_USERMEM() +
		(miser_pool ? GET_USERMEM(miser_pool) : 0);
	rminfo.bufmem = bufmem;
	rminfo.physmem = physmem;
	rminfo.dchunkpages = dchunkpages;
	rminfo.pmapmem = pmapmem;
	rminfo.strmem = str_getpage_usage();
	rminfo.chunkpages = chunkpages;
	rminfo.dpages = pdcount;
	rminfo.emptymem = GLOBAL_EMPTYMEM_GET();
	rminfo.ravailrmem = GLOBAL_AVAILRMEM() +
		 (miser_pool ? GET_AVAILRMEM(miser_pool) : 0);

	last_update = lbolt;
}

/* Generic routine to return data on single structures that exist per cell.
 */
static int
sysget_gen(	int name,
		char *buf,
		int *buflen_p,
		int flags,
		sgt_cookie_t *cookie_p,
		sgt_info_t *info_p)
{
	int error = 0;
	sgt_attr_t *ap = &sysget_attr[name - 1];
	caddr_t ubuf, src = NULL;
	caddr_t targ = NULL;
	int klen, ulen;

	/* process request for this cell */
	switch (flags & (SGT_READ | SGT_WRITE | SGT_INFO | SGT_STAT)) {
	case SGT_INFO:

		/* accumulate info for this cell into the info_p struct */

		info_p->si_size = ap->size;
		info_p->si_num = 1;
		info_p->si_hiwater = info_p->si_num;
		ulen = sizeof(sgt_info_t);
		break;
	case SGT_READ:

		if (flags & SGT_UADDR) {

			/* Initialize temp buffer arguments */

			klen = ap->size;
			targ = kmem_zalloc(klen, KM_CACHEALIGN);
		} 
		else {

			/* The buffer was already set up by the kernel */

			klen = *buflen_p;
			targ = buf;
			ASSERT(klen == ap->size);
		}
		src = targ;

		switch (name) {
		case SGT_RMINFO:
			/* Read the global once-a-second snapshot of rminfo */

			mp_mutex_lock(&sgt_ksym_sem, PZERO);
			update_rminfo();
			if (flags & SGT_SUM) {
				addinfo((__uint32_t *)targ,
					 (__uint32_t *)&rminfo,
					 sizeof(rminfo) / sizeof(__uint32_t));
			}
			else {
				bcopy(&rminfo, targ, sizeof(rminfo));
			}
			mp_mutex_unlock(&sgt_ksym_sem);
			break;
		case SGT_LPGSTATS:
			collect_lpg_stats((lpg_stat_info_t *)targ);
			break;
#ifdef TILES_TO_LPAGES
		case SGT_TILEINFO:
			src = (caddr_t)&tileinfo;
			break;
#endif /* TILES_TO_LPAGES */
		default:
			if (targ && (flags & SGT_UADDR)) {
				kmem_free(targ, klen);
			}
			return(ENOENT);
		}

		ubuf = buf;
		ulen = MIN(*buflen_p, klen);

		if (flags & SGT_UADDR) {

			/* Copy out the data for this cell. */

			if (copyout(src, ubuf, ulen)) {
				error = EFAULT;
				break;
			}
		}
		break;
	default:
		error = EINVAL;
	}

	if (targ && (flags & SGT_UADDR)) {
		kmem_free(targ, klen);
	}
	cookie_p->sc_status = SC_DONE;
	*buflen_p = ulen;
	return(error);
}

/* Routine to return data for SGT_KSYM requests.
 */
/* ARGSUSED */
static int
sysget_ksym(	int name,
		char *buf,
		int *buflen_p,
		int flags,
		sgt_cookie_t *cookie_p,
		sgt_info_t *info_p)
{
	int error = 0;
	struct sgt_ksym *kap;
	caddr_t ubuf, src = NULL;
	caddr_t targ = NULL;
	int klen, ulen;

	/* process request for this cell */

	ksym_init();

	if (!(kap = ksym_find(
	     ((struct sgt_cookie_ksym *)cookie_p->sc_opaque)->symname, 0))) {
		return(ENOENT);
	}

	/* Get the address for the symbol.  
	 */

	if (!kap->addr || !kap->size) {
		return(ENOENT);
	}

	switch (flags & (SGT_READ | SGT_WRITE | SGT_INFO | SGT_STAT)) {
	case SGT_INFO:

		/* accumulate info for this cell into the info_p struct */

		info_p->si_size = kap->size;
		info_p->si_num = kap->num;
		info_p->si_hiwater = info_p->si_num;
		ulen = sizeof(sgt_info_t);
		break;
	case SGT_READ:

		/* Initialize buffer arguments */

		klen = kap->size * kap->num;
		src = kap->addr;

		if (flags & SGT_SUM) {

			/* Copy the remote kernel buffer that we will sum
			 * into. It should be the same size as on this kernel.
			 * For R2 we will need real remote copies.
			 */

			ASSERT(!(flags & SGT_UADDR));
			ASSERT(*buflen_p == klen);
			targ = kmem_zalloc(klen, KM_CACHEALIGN);
			sysget_bcopy(buf, targ, klen);
		}

		ubuf = buf;
		ulen = MIN(*buflen_p, klen);

		if (flags & SGT_SUM) {

			/* Sum up data and copy it back to the remote kernel */

			addinfo((__uint32_t *)targ, (__uint32_t *)src,
				 klen / sizeof(__uint32_t));
			sysget_bcopy(targ, buf, klen);
		}
		else {

			/* Copy out the data for this cell. */

			if (copyout(src, ubuf, ulen)) {
				error = EFAULT;
				break;
			}
		}
		break;
	default:
		error = EINVAL;
	}

	if (targ) {
		kmem_free(targ, klen);
	}
	cookie_p->sc_status = SC_DONE;
	*buflen_p = ulen;
	return(error);
}

/* Define cookie for use with sysget_node requests */
struct sgt_cookie_node {
	cell_t	curcell;
	uint	index;
};

/* Routine to return data on structures that exist per node.
 */
static int
sysget_node(	int name,
		char *buf,
		int *buflen_p,
		int flags,
		sgt_cookie_t *cookie_p,
		sgt_info_t *info_p)
{
	int i, start, max, psi, copied, error = 0;
	sgt_attr_t *ap = &sysget_attr[name - 1];
	caddr_t ubuf, targ = NULL;
	nodeinfo_t *ni;
	int klen, ulen;
	struct sgt_cookie_node *ckop =
		 (struct sgt_cookie_node *)cookie_p->sc_opaque;

	/* process node request for this cell */

	if (cookie_p->sc_type == SC_CNODEID) {

		/* Check if the specified node is part of this cell */

		for (i = 0; i < maxcpus; i++) {
			if (pdaindr[i].pda->p_nodeid ==
					 cookie_p->sc_id.cnodeid) {
				break;
			}
		}
		if (i == maxcpus) {

			/* No match found */

			*buflen_p = 0;
			return(0);
		}
		
#ifdef TODO
		/* We probably can't assume in future that a cnodeid will map
		 * directly to a NODEPDA index but for now it does.
		 */
#endif

		start = cookie_p->sc_id.cnodeid;
		max = start + 1;
		ASSERT(max <= numnodes);
	}
	else {
		start = 0;
		max = numnodes;
	}

	switch (flags & (SGT_READ | SGT_WRITE | SGT_INFO | SGT_STAT)) {
	case SGT_INFO:

		/* save node info for this cell into the info_p struct */

		info_p->si_size = ap->size;
		info_p->si_num = max - start;
		info_p->si_hiwater = info_p->si_num;
		copied = sizeof(sgt_info_t);
		break;
	case SGT_READ:

		/* Initialize buffer arguments */

		klen = ap->size;
		targ = kmem_zalloc(klen, KM_CACHEALIGN);

		if (flags & SGT_SUM) {

			/* Copy the remote kernel buffer that we will sum
			 * into. It should be the same size as on this kernel.
			 * For R2 we will need real remote copies.
			 */

			ASSERT(!(flags & SGT_UADDR));
			ASSERT(*buflen_p == klen);
			sysget_bcopy(buf, targ, klen);
		}

		/* Aggregate across nodes. We require that the user's
		 * buffer be large enough to hold at least one data object.
		 */

		copied = 0;
		ubuf = buf;
		ulen = MIN(*buflen_p, klen);

		if (cookie_p->sc_status == SC_CONTINUE) {

			/* the user's cookie tells us where to start again */

			start = ckop->index + 1;
			max = numnodes;
		}

		for (i = start; i < max; i++) {
			if (!ulen) {

				/* No buffer left */

				break;
			}
			switch (name) {
			case SGT_NODE_INFO:
				ni = (nodeinfo_t *)targ;
				ni->totalmem = ctob(NODE_TOTALMEM(i));
				ni->freemem = ctob(NODE_FREEMEM(i));
				ni->node_device = cnodeid_to_vertex(i);
				psi = MAX_PGSZ_INDEX;
				ni->num16mpages = GET_NUM_FREE_PAGES(i, psi);
				psi--;
				ni->num4mpages = GET_NUM_FREE_PAGES(i, psi);
				psi--;
				ni->num1mpages = GET_NUM_FREE_PAGES(i, psi);
				psi--;
				ni->num256kpages = GET_NUM_FREE_PAGES(i, psi);
				psi--;
				ni->num64kpages = GET_NUM_FREE_PAGES(i, psi);
				break;
			default:
				if (targ) {
					kmem_free(targ, klen);
				}
				return(ENOENT);
			}

			if (flags & SGT_SUM) {
				continue;
			}
			if (copyout(targ, ubuf, ulen)) {
				error = EFAULT;
				break;
			}
			copied += ulen;
			ubuf = &buf[copied];
			ulen = MIN((*buflen_p - copied), klen);
		}

		if (i < max) {

			/* User's buffer wasn't big enough so return a
			 * cookie that will allow the call to start up again.
			 */
 
			cookie_p->sc_status = SC_CONTINUE;
			ckop->curcell = cellid();
			ckop->index = i;
		}
		else {

			/* Set cookie to indicate this cell is done.
			 * We re-initialize the index for the next cell
			 * in case the iterator decides to give up.
			 */

			cookie_p->sc_status = SC_DONE;
			ckop->index = -1;
		}
			
		if (flags & SGT_SUM) {

			/* Copy the summed-up data back to the remote kernel */

			sysget_bcopy(targ, buf, klen);
		}
		break;
	default:
		error = EINVAL;
	}

	if (targ) {
		kmem_free(targ, klen);
	}
	*buflen_p = copied;
	return(error);
}
