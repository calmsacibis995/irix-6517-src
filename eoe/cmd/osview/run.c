# include	<sys/types.h>
# include	<sys/param.h>
# include	<sys/sysinfo.h>
# include	<sys/ksa.h>
# include	<sys/sysmacros.h>
# include	<sys/immu.h>
# include	<sys/buf.h>
# include	<sys/ioctl.h>
# include	<sys/socket.h>
# include	<sys/sysmp.h>
# include	<sys/pda.h>
# include	<sys/tcpipstats.h>
# include	<sys/syssgi.h>
#ifdef TILES_TO_LPAGES
# include	<sys/tile.h>
#endif /* TILE_TO_LPAGES */

# include	<netinet/in_systm.h>
# include	<netinet/in.h>
# include	<netinet/ip.h>
# include	<netinet/ip_var.h>
# include	<netinet/udp.h>
# include	<netinet/udp_var.h>
# include	<net/if.h>

# include	<invent.h>
# include	<stdio.h>
# include	<time.h>
# include	<signal.h>
# include	<string.h>
# include	<unistd.h>

# include	"osview.h"

static void	preupdate(void);
static void	postupdate(void);
static char	*konvert(long long);
static char	*fkonvert(float);

static void	fill_avenrun(Reg_t *);
static void	fill_cpu(Reg_t *);
static void	fill_rmem(Reg_t *);
static void	fill_vmem(Reg_t *);
static void	fill_vfaults(Reg_t *);
static void	fill_tlb(Reg_t *);
static void	fill_intr(Reg_t *);
static void	fill_gfx(Reg_t *);
static void	fill_video(Reg_t *);
static void	fill_sys(Reg_t *);
#ifdef TILES_TO_LPAGES
static void	fill_tiles(Reg_t *);
#endif
static void	fill_sched(Reg_t *);
static void	fill_ifsock(Reg_t *);
static void	fill_tcp(Reg_t *);
static void	fill_udp(Reg_t *);
static void	fill_ip(Reg_t *);
static void	fill_swap(Reg_t *);
static void	fill_wait(Reg_t *);
static void	fill_buf(Reg_t *);
static void	fill_nc(Reg_t *);
static void	fill_iget(Reg_t *);
static void	fill_xfs(Reg_t *);
static void	fill_getblk(Reg_t *);
static void	fill_heap(Reg_t *);
static void	fill_vnode(Reg_t *);
static void	fill_node_data(Reg_t *);
static void	fill_lpage_stats(Reg_t *);
static void	fill_cell(Reg_t *);

static Reg_t	*expand_avenrun(Reg_t *);
static Reg_t	*expand_cpu(Reg_t *);
static Reg_t	*expand_rmem(Reg_t *);
static Reg_t	*expand_vfaults(Reg_t *);
static Reg_t	*expand_vmem(Reg_t *);
static Reg_t	*expand_tlb(Reg_t *);
static Reg_t	*expand_intr(Reg_t *);
static Reg_t	*expand_gfx(Reg_t *);
static Reg_t	*expand_video(Reg_t *);
static Reg_t	*expand_sys(Reg_t *);
#ifdef TILES_TO_LPAGES
static Reg_t	*expand_tiles(Reg_t *);
#endif
static Reg_t	*expand_sched(Reg_t *);
static Reg_t	*expand_ifsock(Reg_t *);
static Reg_t	*expand_tcp(Reg_t *);
static Reg_t	*expand_udp(Reg_t *);
static Reg_t	*expand_ip(Reg_t *);
static Reg_t	*expand_swap(Reg_t *);
static Reg_t	*expand_wait(Reg_t *);
static Reg_t	*expand_buf(Reg_t *);
static Reg_t	*expand_nc(Reg_t *);
static Reg_t	*expand_iget(Reg_t *);
static Reg_t	*expand_xfs(Reg_t *);
static Reg_t	*expand_getblk(Reg_t *);
static Reg_t	*expand_heap(Reg_t *);
static Reg_t	*expand_vnode(Reg_t *);
static Reg_t	*expand_node_data(Reg_t *);
static Reg_t	*expand_cell(Reg_t *);
static Reg_t	*expand_lpage_stats(Reg_t *);

static initReg_t	sinit[] = {
/*  0 */{ RF_HDRSYS|RF_ON, "Load Average", fill_avenrun, 0, expand_avenrun },
	{ RF_HDRSYS|RF_HDRCPU|RF_ON, "CPU Usage", fill_cpu, 0, expand_cpu },
	{ RF_HDRSYS|RF_ON, "System Activity", fill_sys, 0, expand_sys },
	{ RF_HDRSYS|RF_HDRMEM|RF_ON, "System Memory",fill_rmem,0,expand_rmem},
	{ RF_HDRSYS|RF_ON, "Scheduler", fill_sched, 0, expand_sched },
	{ RF_HDRSYS|RF_ON, "Wait Ratio", fill_wait, 0, expand_wait },
	{ RF_HDRMEM|RF_ON, "Memory Faults", fill_vfaults, 0, expand_vfaults },
	{ RF_HDRMEM|RF_ON|RF_SUPRESS, "Swap", fill_swap, 0, expand_swap },
	{ RF_HDRMEM|RF_ON|RF_SUPRESS, "System VM", fill_vmem, 0, expand_vmem },
	{ RF_HDRMEM|RF_ON|RF_SUPRESS, "Heap", fill_heap, 0, expand_heap },
/* 10 */{ RF_HDRMEM|RF_ON|RF_SUPRESS, "TLB Actions", fill_tlb, 0, expand_tlb },
	{ RF_HDRMEM|RF_ON|RF_SUPRESS, "Large page stats", fill_lpage_stats, 0, 
						expand_lpage_stats },
	{ RF_HDRMEM|RF_ON, "Nodes", fill_node_data, 0, expand_node_data },
# define	nodeoff()	sinit[12].flags &= ~RF_ON
	{ RF_HDRNET|RF_ON, "TCP", fill_tcp, 0, expand_tcp },
	{ RF_HDRNET|RF_ON, "UDP", fill_udp, 0, expand_udp },
	{ RF_HDRNET|RF_ON, "IP", fill_ip, 0, expand_ip },
	{ RF_HDRNET|RF_ON, "Net Interface", fill_ifsock, 0, expand_ifsock },
	{ RF_HDRMSC|RF_ON, "Block Devices", fill_buf, 0, expand_buf },
	{ RF_HDRMSC|RF_ON, "Cell Activity", fill_cell, 0, expand_cell },
# define	celloff() 	sinit[18].flags &= ~RF_ON
	{ RF_HDRMSC|RF_ON, "Graphics", fill_gfx, 0, expand_gfx },
# define	gfxoff()	sinit[19].flags &= ~RF_ON
/* 20 */{ RF_HDRMSC|RF_ON, "Video", fill_video, 0, expand_video },
# define	vidoff()	sinit[20].flags &= ~RF_ON
	{ RF_HDRMSC|RF_ON|RF_SUPRESS, "Interrupts", fill_intr, 0, expand_intr },
	{ RF_HDRMSC|RF_ON|RF_SUPRESS, "PathName Cache", fill_nc, 0, expand_nc },
	{ RF_HDRMSC|RF_ON|RF_SUPRESS, "EfsAct", fill_iget, 0, expand_iget },
	{ RF_HDRMSC|RF_ON|RF_SUPRESS, "XfsAct", fill_xfs, 0, expand_xfs },
	{ RF_HDRMSC|RF_ON|RF_SUPRESS, "Getblk", fill_getblk, 0, expand_getblk },
	{ RF_HDRMSC|RF_ON|RF_SUPRESS, "Vnodes", fill_vnode, 0, expand_vnode },
#ifdef TILES_TO_LPAGES
	{ RF_HDRMSC|RF_ON|RF_SUPRESS, "Tiles", fill_tiles, 0, expand_tiles},
# define	tilesoff()	sinit[27].flags &= ~RF_ON
#else
	{ RF_SUPRESS, "Tiles OFF", 0, 0, 0},
#endif
/* 28 */{ 0, 0, 0, 0, 0 }
};

static struct pageconst pageconst;

/*
 * These defines have been added based on the use of
 * the following pagesize-dependent macros:
 *
 *	ctob
 *	dtop
 */
# define	_PAGESZ		pageconst.p_pagesz
# define	PNUMSHFT	pageconst.p_pnumshft


# define	topersec(d)	((long)(((d)/(float)ticks)*HZ + 0.5))
# define	ctobl(x)	((long long)(x) * (long long)(_PAGESZ))
# define	sgrelval(x)	(persec ?  \
				   topersec(nGp->x-lGp->x) : \
				   (nGp->x-lGp->x))
# define	ssrelval(x)	(persec ?  \
				   topersec(nSp->x-lSp->x) : \
				   (nSp->x-lSp->x))

# define	srelval(x)	(nSp->x-lSp->x)
# define	relval(m,x)	(nsp[m]->x-lsp[m]->x)
# define	frelval(x)	((float)(nSp->x-lSp->x))

# define	mval(x)	(nmp->x)
# define	mrelval(x)	(persec ?  \
		   topersec(nmp->x-lmp->x):\
		   (nmp->x-lmp->x))

# define	ncrelval(x)	(persec ?  \
	   	   topersec(nncstat->x-lncstat->x):\
		   (nncstat->x-lncstat->x))

# define	igetrelval(x)	(persec ? \
		   topersec(nigetstat->x-ligetstat->x):\
		   (nigetstat->x-ligetstat->x))

# define	xfsrelval(x)	(persec ? \
		   topersec(nxfsstat->x-lxfsstat->x):\
		   (nxfsstat->x-lxfsstat->x))

# define	getblkrelval(x)	(persec ? \
		   topersec(ngetblkstat->x-lgetblkstat->x):\
		   (ngetblkstat->x-lgetblkstat->x))

# define	vnoderelval(x)	(persec ? \
		   topersec(nvnodestat->x-lvnodestat->x):\
		   (nvnodestat->x-lvnodestat->x))

# define	lpgstatrelval(x)	(persec ? \
		   topersec(nlpg_stat->x-llpg_stat->x):\
		   (nlpg_stat->x-llpg_stat->x))

# define	spercent(x,y,m) \
		   ((int)((((nsp[m]->x-lsp[m]->x)/(float)(y))*100)+0.5))

# define	fpercent(x,y,m) \
		   (((nsp[m]->x-lsp[m]->x)/(float)(y))*100)

# define	sfpercent(x,y) \
		   (((nSp->x-lSp->x)/(float)(y))*100)

# define	cell_fpercent(x,y,m) \
		   ((nCSp[m].x - lCSp[m].x) ? \
		    (((nCSp[m].x - lCSp[m].x)/(float)(y))*100) : 0.0)

# define	mkformat(n,f,v)	\
	case n:sprintf(rp->format, f, v);break;
# define	mkformat2(n,f,v1,v2)	\
	case n:sprintf(rp->format, f, v1, v2);break;
# define	kmkformat(n,v)	\
	case n:strcpy(rp->format, konvert(v));break;
# define	nckmkformat(n,v,c)	\
	case n:if(!(rp->flags&(c))) {strcpy(rp->format, konvert(v));}break;
# define	fkmkformat(n,v)	\
	case n:strcpy(rp->format, fkonvert(v));break;
# define	setnformat(zf)		\
	static void zf(Reg_t *rp) {int n,which; \
	if (rp->flags & RF_HEADER) return;\
	n = (rp->fillarg & 0xfff00000) >> 20; which = rp->fillarg & 0xff; \
	switch (which)
# define	setformat(zf)		\
	static void zf(Reg_t *rp) {int which; \
	if (rp->flags & RF_HEADER) return;\
	which = rp->fillarg & 0xff; \
	switch (which)
# define	fsetnformat(zf,fu)		\
	static void zf(Reg_t *rp) {int n,which; \
	if (rp->flags & RF_HEADER) return;\
	n = (rp->fillarg & 0xfff00000) >> 20; which = rp->fillarg & 0xff; \
	fu; \
	switch (which)
# define	fsetformat(zf,fu)		\
	static void zf(Reg_t *rp) {int which; \
	if (rp->flags & RF_HEADER) return;\
	which = rp->fillarg & 0xff; \
	fu; \
	switch (which)

#ifdef TILES_TO_LPAGES
#define trelval(x)		(persec ? \
				 topersec(ntileinfo->x-ltileinfo->x) : \
				 (ntileinfo->x-ltileinfo->x))
static int istiles(void);
#endif

static int isgfx(void);
static int isvideo(void);

int have_gfx = 1;
static int have_vid = 1;

/*
 * Set it all up and get it going.
 */
void
runit(void)
{
	if (syssgi(SGI_CONST, SGICONST_PAGESZ, &pageconst,
						sizeof(pageconst), 0) == -1) {
		perror("syssgi: pageconst");
		exit(1);
	}

	have_gfx = isgfx();
	if (!have_gfx)
		gfxoff();

	have_vid = isvideo();
	if (!have_vid)
		vidoff();

#ifdef TILES_TO_LPAGES
	if (!istiles())
		tilesoff();
#endif

	initinfo();
	setinfo();
	if (!persec) {
		if (counts)
			pbegin(&sinit[0], preupdate, postupdate,
				"Osview 2.1 : Counts", interval);
		else
			pbegin(&sinit[0], preupdate, postupdate,
				"Osview 2.1 : No Average", interval);
	} else
		pbegin(&sinit[0], preupdate, postupdate,
			"Osview 2.1 : One Second Average", interval);
}

/*
 * Expand a list in-line.
 */
static Reg_t *
expand_any(Reg_t *rp, char **np, int n, long extra)
{
   int		i;
   Reg_t	*nrp;

	for (i = 0; i < n; i++, np++) {
		nrp = pfollow(rp);
		strcpy(nrp->name, "  ");
		strncat(nrp->name, *np, TEXTSIZE - 2);
		nrp->name[TEXTSIZE-1] = '\0';
		strcpy(nrp->format, "");
		nrp->fillarg = i|extra;
		rp = nrp;
	}
	return(rp);
}

/*
 * Pre and post update routines - read data from kernel.
 */
static void
preupdate(void)
{
	getinfo();
}

static void
postupdate(void)
{
}

/*
 * Fill out the CPU bar.
 */

static char	*cpu_header[] = {
	"%user",
	"%sys",
	"%intr",
	"%gfxc",
	"%gfxf",
	"%sxbrk",
	"%idle",
};
static int	cpu_header_cnt = sizeof(cpu_header)/sizeof(char *);

static void
suppress_gfx(Reg_t *rp, Reg_t *erp)
{
	if (!have_gfx) {
		while (rp != erp) {
			if (strcmp(rp->name, "  %gfxf") == 0)
				rp->flags &= ~RF_ON;
			else if (strcmp(rp->name, "  %gfxc") == 0)
				rp->flags &= ~RF_ON;
			rp = rp->next;
		}
	}
}

void
suppress_node_info(void)
{
	nodeoff();
}

void
suppress_cell_info(void)
{
	celloff();
}

static Reg_t	*
expand_cpu(Reg_t *rp)
{
   	long		i;
   	Reg_t		*nrp;
   	Reg_t		*trp;
	struct pda_stat *p = pdastat;

	sysmp(MP_STAT, pdastat);

	nrp = expand_any(rp, cpu_header, cpu_header_cnt, 0xfff << 20);
	suppress_gfx(rp, nrp);
	if (nscpus > 1) {
		for (i = 0; i < nscpus; i++, p++) {
			trp = pfollow(nrp);
			trp->flags |= RF_HDRCPU;
			sprintf(trp->name, "CPU %d Usage", p->p_cpuid);
			nrp = expand_any(trp, cpu_header, cpu_header_cnt, i<<20);
			suppress_gfx(trp, nrp);
		}
	}
	return(nrp);
}

static float	cpuval;

/* ARGSUSED */
static void
calccpu(Reg_t *rp, int n, int which)
{
   	float	total;

	if (n == 0xfff) {
		total = srelval(cpu[CPU_USER]) +
			srelval(cpu[CPU_KERNEL]) +
			srelval(cpu[CPU_INTR]) +
			srelval(wait[W_GFXC]) +
			srelval(wait[W_GFXF]) +
			srelval(cpu[CPU_SXBRK]) +
			srelval(cpu[CPU_IDLE]) +
			srelval(cpu[CPU_WAIT]);

		if (total == 0.0) {
			cpuval = 0.0;
			return;
		}

		switch (which) {
		case 0:
			cpuval = sfpercent(cpu[CPU_USER], total);
			break;
		case 1:
			cpuval = sfpercent(cpu[CPU_KERNEL], total);
			break;
		case 2:
			cpuval = sfpercent(cpu[CPU_INTR], total);
			break;
		case 3:
			cpuval = sfpercent(wait[W_GFXC], total);
			break;
		case 4:
			cpuval = sfpercent(wait[W_GFXF], total);
			break;
		case 5:
			cpuval = sfpercent(cpu[CPU_SXBRK], total);
			break;
		case 6:
			cpuval = sfpercent(cpu[CPU_IDLE], total) +
				 sfpercent(cpu[CPU_WAIT], total);
			break;
		}
	}
	else {
		total = relval(n, cpu[CPU_USER]) +
			relval(n, cpu[CPU_KERNEL]) +
			relval(n, cpu[CPU_INTR]) +
			relval(n, wait[W_GFXC]) +
			relval(n, wait[W_GFXF]) +
			relval(n, cpu[CPU_SXBRK]) +
			relval(n, cpu[CPU_IDLE]) +
			relval(n, cpu[CPU_WAIT]);

		if (total == 0.0) {
			cpuval = 0.0;
			return;
		}

		switch (which) {
		case 0:
			cpuval = fpercent(cpu[CPU_USER], total, n);
			break;
		case 1:
			cpuval = fpercent(cpu[CPU_KERNEL], total, n);
			break;
		case 2:
			cpuval = fpercent(cpu[CPU_INTR], total, n);
			break;
		case 3:
			cpuval = fpercent(wait[W_GFXC], total, n);
			break;
		case 4:
			cpuval = fpercent(wait[W_GFXF], total, n);
			break;
		case 5:
			cpuval = fpercent(cpu[CPU_SXBRK], total, n);
			break;
		case 6:
			cpuval = fpercent(cpu[CPU_IDLE], total, n) +
				 fpercent(cpu[CPU_WAIT], total, n);
			break;
		}
	}
}

fsetnformat(fill_cpu, calccpu(rp, n, which))
{
	mkformat(0, "%6.2f", cpuval);
	mkformat(1, "%6.2f", cpuval);
	mkformat(2, "%6.2f", cpuval);
	mkformat(3, "%6.2f", cpuval);
	mkformat(4, "%6.2f", cpuval);
	mkformat(5, "%6.2f", cpuval);
	mkformat(6, "%6.2f", cpuval);
}}

/*
 * Wait time fiddling.
 */
static char	*wait_header[] = {
	"%IO",
	"%Swap",
	"%Physio",
};
static float 	wait_io;
static float 	wait_swap;
static float 	wait_pio;
static int	wait_header_cnt = sizeof(wait_header)/sizeof(char *);

static Reg_t *
expand_wait(Reg_t *rp)
{
	return(expand_any(rp, &wait_header[0], wait_header_cnt, 0L));
}

static void
calcwait(void)
{
   int			i;
   int			nenabled = 0;
   float		ratio;
   float		fwait;

	wait_io = 0;
	wait_swap = 0;
	wait_pio = 0;
	for (i = 0; i < nscpus; i++) {
		if (relval(i, cpu[CPU_WAIT]) > 0) {
			wait_io += relval(i, wait[W_IO]);
			wait_swap += relval(i, wait[W_SWAP]);
			wait_pio += relval(i, wait[W_PIO]);
		}
		if (pdastat[i].p_flags & PDAF_ENABLED)
			nenabled++;
	}
	if (srelval(cpu[CPU_WAIT]) > 0) {
		fwait = frelval(cpu[CPU_WAIT]);
		wait_io /= fwait; wait_swap /= fwait; wait_pio /= fwait;
		ratio = fwait / (ticks * nenabled);
		wait_io *= ratio;
		wait_swap *= ratio;
		wait_pio *= ratio;
	}
}

fsetformat(fill_wait, calcwait())
{
	mkformat(0, "%4.1f", wait_io * 100.0);
	mkformat(1, "%4.1f", wait_swap * 100.0);
	mkformat(2, "%4.1f", wait_pio * 100.0);
}}

/*
 * Real Memory functions.
 */
static char	*rmem_header[] = {
	"Phys",
	" kernel",
	"  heap",
	"   mbufs",	/* uses heap */
	"   stream",	/* uses heap */
	"  ptbl",
	"  fs ctl",
	" fs data",
	"  delwri",
	" free",
	"  data",
	"  empty",
	" userdata",
	" reserved",
	" pgallocs",
};
static int	rmem_header_cnt = sizeof(rmem_header)/sizeof(char *);

static Reg_t *
expand_rmem(Reg_t *rp)
{
	return(expand_any(rp, &rmem_header[0], rmem_header_cnt, 0L));
}

static int
get_userdata(int reserved)
{
	int userdata;

	/* There is a time lag in the kernel from when memory is reserved
	 * from the global pool and the pages are taken from the free list.
	 * This means that the calculation for userdata is just an estimate.
	 * Sometimes a negative value will be calculated for userdata until
         * the free list is updated. A zero value will be returned in this
	 * case since there is no way to know the true value.   If this
	 * situation persists the 'reserved' value is an indication of
	 * memory reserved in the kernel but never allocated for some reason.
	 */

	userdata = rminfo.availrmem -
		(rminfo.chunkpages + rminfo.dpages + rminfo.freemem);
	if (reserved) {
		if (userdata < 0) {
			return(-userdata);
		}
		else {
			return(0);
		}
	} 
	else if (userdata < 0) {
		return(0);
	}
	return(userdata);
}

setformat(fill_rmem)
{
	kmkformat(0, ctobl(rminfo.physmem));
	kmkformat(1, ctobl(rminfo.physmem-(rminfo.availrmem+rminfo.bufmem)));
	kmkformat(2, mval(heapmem));
	kmkformat(3, (nmbstat->m_clusters*ps));
	kmkformat(4, ctobl(rminfo.strmem));
	kmkformat(5, ctobl(rminfo.pmapmem));
	kmkformat(6, ctobl(rminfo.bufmem));
	kmkformat(7, ctobl(rminfo.chunkpages+rminfo.dpages));
	kmkformat(8, ctobl(rminfo.dchunkpages+rminfo.dpages));
	kmkformat(9, ctobl(rminfo.freemem));
	kmkformat(10, ctobl(rminfo.freemem-rminfo.emptymem));
	kmkformat(11, ctobl(rminfo.emptymem));
	kmkformat(12, ctobl(get_userdata(0)));
	kmkformat(13, ctobl(get_userdata(1)));
	kmkformat(14, mrelval(palloc));
}}

/*
 * Kernel Virtual Memory functions.
 */
static char	*vmem_header[] = {
	"VirtMem",
	" avail",
	"  clean",
	"  intrans",
	"  ageing",
	"  dirty",
	" in use",
	"  files",
	"  heap",
	"  zones",
	"  ptbl",
	" allocd",
	" freed",
};
static int	vmem_header_cnt = sizeof(vmem_header)/sizeof(char *);

static Reg_t *
expand_vmem(Reg_t *rp)
{
	return(expand_any(rp, &vmem_header[0], vmem_header_cnt, 0L));
}

setformat(fill_vmem)
{
	kmkformat(0, ctobl(syssegsz));
	kmkformat(1, ctobl(mval(sptclean)+mval(sptdirty)+mval(sptintrans)+mval(sptaged)));
	kmkformat(2, ctobl(mval(sptclean)));
	kmkformat(3, ctobl(mval(sptintrans)));
	kmkformat(4, ctobl(mval(sptaged)));
	kmkformat(5, ctobl(mval(sptdirty)));
	kmkformat(6, ctobl(syssegsz) - ctobl(mval(sptclean)+mval(sptdirty)+
					   mval(sptintrans)+mval(sptaged)));
	kmkformat(7, ctobl(mval(sptbp)));
	kmkformat(8, ctobl(mval(sptheap)));
	kmkformat(9, ctobl(mval(sptzone)));
	kmkformat(10, ctobl(mval(sptpt)));
	kmkformat(11, ctobl(mrelval(sptalloc)));
	kmkformat(12, ctobl(mrelval(sptfree)));
}}

/*
 * Load average.
 */
static char	*avenrun_header[] = {
	"1 Min",
	"5 Min",
	"15 Min",
};
static int	avenrun_header_cnt = sizeof(avenrun_header)/sizeof(char *);

static Reg_t *
expand_avenrun(Reg_t *rp)
{
	return(expand_any(rp, &avenrun_header[0], avenrun_header_cnt, 0L));
}

setformat(fill_avenrun)
{
	mkformat(0, "%5.3f", avenrun[0]/1024.0);
	mkformat(1, "%5.3f", avenrun[1]/1024.0);
	mkformat(2, "%5.3f", avenrun[2]/1024.0);
}}

/*
 * memory management data
 */
static char	*vfault_header[] = {
	"vfault",
	"protection",
	"demand",
	"cw",
	"steal",
	"onswap",
	"oncache",
	"onfile",
	"freed",
	"unmodswap",
	"unmodfile",
	"iclean",
};
static int	vfault_header_cnt = sizeof(vfault_header)/sizeof(char *);

static Reg_t *
expand_vfaults(Reg_t *rp)
{
	return(expand_any(rp, &vfault_header[0], vfault_header_cnt, 0L));
}

setformat(fill_vfaults)
{
	kmkformat(0, mrelval(vfault));
	kmkformat(1, mrelval(pfault));
	kmkformat(2, mrelval(demand));
	kmkformat(3, mrelval(cw));
	kmkformat(4, mrelval(steal));
	kmkformat(5, mrelval(swap));
	kmkformat(6, mrelval(cache));
	kmkformat(7, mrelval(file));
	kmkformat(8, mrelval(freedpgs));
	kmkformat(9, mrelval(unmodsw));
	kmkformat(10, mrelval(unmodfl));
	kmkformat(11, mrelval(iclean));
}}

/*
 * Large page statistics 
 */
static char	*lpage_stats_header[] = {
	"Coalesce atts",
	"Coalesce succ",
	"lpgfaults",
	"lpgallocs",
	"lpgdowngrades",
	"lpgsplits",
};

static int	lpage_stats_header_cnt = 
			sizeof(lpage_stats_header)/sizeof(char *);

static Reg_t *
expand_lpage_stats(Reg_t *rp)
{
	return(expand_any(rp, &lpage_stats_header[0], 
					lpage_stats_header_cnt, 0L));
}

setformat(fill_lpage_stats)
{
	kmkformat(0, lpgstatrelval(coalesce_atts));
	kmkformat(1, lpgstatrelval(coalesce_succ));
	kmkformat(2, lpgstatrelval(num_lpg_faults));
	kmkformat(3, lpgstatrelval(num_lpg_allocs));
	kmkformat(4, lpgstatrelval(num_lpg_downgrade));
	kmkformat(5, lpgstatrelval(num_page_splits));
}}

/*
 * Per node data
 */
static char	*node_data_header[] = {
	"  Totalmem",
	"  freemem",
	"  64k pages",
	"  256k pages",
	"  1MB pages",
	"  4MB pages",
	"  16MB pages"	
};
static int	node_data_header_cnt = sizeof(node_data_header)/sizeof(char *);

static Reg_t	*
expand_node_data(Reg_t *rp)
{
   long		i;
   Reg_t	*nrp;
   Reg_t	*trp;

	if (numnodes == -1) {
		sprintf(rp->name, "Non-NUMA System");
		rp->flags &= ~RF_ON;
	} else {
		sprintf(rp->name, "Node[0]");
		rp->fillfunc = fill_node_data;
		nrp = expand_any(rp, node_data_header,
				node_data_header_cnt, 0);
		for (i = 1; i < numnodes; i++) {
			trp = pfollow(nrp);
			trp->flags |= RF_HDRMEM;
			sprintf(trp->name, "Node[%d]", i);
			nrp = expand_any(trp, node_data_header,
					node_data_header_cnt,
					i<<20);
		}
		return(nrp);
	}
	return NULL;
}

setnformat(fill_node_data)
{
	kmkformat(0, (node_info[n].totalmem));
	kmkformat(1, (node_info[n].freemem));
	kmkformat(2, (node_info[n].num64kpages));
	kmkformat(3, (node_info[n].num256kpages));
	kmkformat(4, (node_info[n].num1mpages));
	kmkformat(5, (node_info[n].num4mpages));
	kmkformat(6, (node_info[n].num16mpages));
}}
	
/*
 * Per cell data
 */
static char	*cell_data_header[] = {
	"% of user",
	"% of sys ",
};
static int	cell_data_header_cnt = sizeof(cell_data_header)/sizeof(char *);

static Reg_t	*
expand_cell(Reg_t *rp)
{
   long		i;
   Reg_t	*nrp;
   Reg_t	*trp;

	if (numcells == 1 || cellid >= 0) {
		sprintf(rp->name, "Non-Cell System");
		rp->flags &= ~RF_ON;
	} else {
		sprintf(rp->name, "Cell[0]");
		rp->fillfunc = fill_cell;
		nrp = expand_any(rp, cell_data_header,
				cell_data_header_cnt, 0);
		for (i = 1; i < numcells; i++) {
			trp = pfollow(nrp);
			trp->flags |= RF_HDRMSC;
			sprintf(trp->name, "Cell[%d]", i);
			nrp = expand_any(trp, cell_data_header,
					cell_data_header_cnt,
					i<<20);
		}
		return(nrp);
	}
	return NULL;
}

/* ARGSUSED */
static void
calc_cell_cpu(Reg_t *rp, int n, int which)
{

	/* Display percentage of total cpu time used by cell */

	switch (which) {
	case 0:
		cpuval = cell_fpercent(cpu[CPU_USER], 
			srelval(cpu[CPU_USER]), n);
		break;
	case 1:
		cpuval = cell_fpercent(cpu[CPU_KERNEL], 
			srelval(cpu[CPU_KERNEL]), n);
		break;
	}
}

fsetnformat(fill_cell, calc_cell_cpu(rp, n, which))
{
	mkformat(0, "%6.2f", cpuval);
	mkformat(1, "%6.2f", cpuval);
}}

/*
 * translation lookaside buffer
 */
static char	*tlb_header[] = {
	"newpid",
	"tfault",
	"rfault",
	"flush",
	"sync",
};

static int	tlb_header_cnt = sizeof(tlb_header)/sizeof(char *);

static Reg_t *
expand_tlb(Reg_t *rp)
{
	return(expand_any(rp, &tlb_header[0], tlb_header_cnt, 0L));
}

setformat(fill_tlb)
{
	kmkformat(0, mrelval(tlbpids));
	kmkformat(1, mrelval(tfault));
	kmkformat(2, mrelval(rfault));
	kmkformat(3, mrelval(tlbflush));
	kmkformat(4, mrelval(tlbsync));
}}

/*
 * interrupts
 */
static char	*intr_header[] = {
	"all",
	"vme",
};
static int	intr_header_cnt = sizeof(intr_header)/sizeof(char *);

static Reg_t *
expand_intr(Reg_t *rp)
{
	return(expand_any(rp, &intr_header[0], intr_header_cnt, 0L));
}

setformat(fill_intr)
{
	kmkformat(0, ssrelval(intr_svcd));
	kmkformat(1, ssrelval(vmeintr_svcd));
}}

/*
 * graphics activity
 */
static char	*gfx_header[] = {
	"griioctl",
	"gintr",
	"swapbuf",
	"switch",
	"fifowait",
	"fifonwait",
};
static int	gfx_header_cnt = sizeof(gfx_header)/sizeof(char *);

static Reg_t *
expand_gfx(Reg_t *rp)
{
	return(expand_any(rp, &gfx_header[0], gfx_header_cnt, 0L));
}

setformat(fill_gfx)
{
	kmkformat(0, sgrelval(griioctl));
	kmkformat(1, sgrelval(gintr));
	kmkformat(2, sgrelval(gswapbuf));
	kmkformat(3, sgrelval(gswitch));
	kmkformat(4, sgrelval(fifowait));
	kmkformat(5, sgrelval(fifonowait));
}}

/*
 * Video activity
 */
static char	*video_header[] = {
	"vidioctl",
	"vidintr",
	"drop_add",
};
static int	video_header_cnt = sizeof(video_header)/sizeof(char *);

static Reg_t *
expand_video(Reg_t *rp)
{
	return(expand_any(rp, &video_header[0], video_header_cnt, 0L));
}

setformat(fill_video)
{
	kmkformat(0, ssrelval(vidioctl));
	kmkformat(1, ssrelval(vidintr));
	kmkformat(2, ssrelval(viddrop_add));
}}

/*
 * syscall activity
 */
static char	*sys_header[] = {
	"syscall",
	"read",
	"write",
	"fork",
	"exec",
	"readch",
	"writech",
	"iget",
};
static int	sys_header_cnt = sizeof(sys_header)/sizeof(char *);

static Reg_t *
expand_sys(Reg_t *rp)
{
	return(expand_any(rp, &sys_header[0], sys_header_cnt, 0L));
}

setformat(fill_sys)
{
	kmkformat(0, ssrelval(syscall));
	kmkformat(1, ssrelval(sysread));
	kmkformat(2, ssrelval(syswrite));
	kmkformat(3, ssrelval(sysfork));
	kmkformat(4, ssrelval(sysexec));
	kmkformat(5, ssrelval(readch));
	kmkformat(6, ssrelval(writech));
	kmkformat(7, ssrelval(iget));
}}

#ifdef TILES_TO_LPAGES
/* tiles to bytes */
#define ttobl(t)	ctobl((t) * TILE_PAGES)

/*
 * Tile activity
 */
static char	*tiles_header[] = {
	"tavail",
	" avfree",
	"tfrag",
	" fraglock",
	" fragfree",
	"tfull",
	" ttile",
	"pglocks",
	"tallocmv",
	"tiledmv",
};
static int	tiles_header_cnt = sizeof(tiles_header)/sizeof(char *);

static Reg_t *
expand_tiles(Reg_t *rp)
{
	return(expand_any(rp, &tiles_header[0], tiles_header_cnt, 0L));
}

/*
 * Can not use the regular setformat macro because if
 * tiles are not present, then ltileinfo and ntileinfo are NULL.
 */
static void
fill_tiles(Reg_t *rp)
{
	int which;

	if (rp->flags & RF_HEADER)
		return;

	if (!(rp->flags & RF_ON))
		return;

	which = rp->fillarg & 0xff;
	switch (which) {
		kmkformat(0, ctobl(ntileinfo->tavail));
		kmkformat(1, ctobl(ntileinfo->avfree));
		kmkformat(2, ctobl(ntileinfo->tfrag));
		kmkformat(3, ctobl(ntileinfo->fraglock));
		kmkformat(4, ctobl(ntileinfo->fragfree));
		kmkformat(5, ctobl(ntileinfo->tfull));
		kmkformat(6, ctobl(ntileinfo->ttile));
		mkformat(7, "%d", trelval(pglocks));
		kmkformat(8, ctobl(trelval(tallocmv)));
		kmkformat(9, ctobl(trelval(tiledmv)));
}}
#endif /* TILES_TO_LPAGES */

/*
 * scheduler activity
 */
static char	*sched_header[] = {
	"runq",
	"swapq",
	"switch",
	"kswitch",
	"preempt",
};
static int	sched_header_cnt = sizeof(sched_header)/sizeof(char *);

static Reg_t *
expand_sched(Reg_t *rp)
{
	return(expand_any(rp, &sched_header[0], sched_header_cnt, 0L));
}

setformat(fill_sched)
{
	kmkformat(0, srelval(runocc) == 0 ? 0 :
		(int)((float)srelval(runque)/srelval(runocc)));
	kmkformat(1, srelval(swpocc) == 0 ? 0 :
		(int)((float)srelval(swpque)/srelval(swpocc)));
	kmkformat(2, ssrelval(pswitch));
	kmkformat(3, ssrelval(kswitch));
	kmkformat(4, ssrelval(kpreempt));
}}

/*
 * Buffer cache activity
 */
static char	*buf_header[] = {
	"lread",
	"bread",
	"%rcache",
	"lwrite",
	"bwrite",
	"wcancel",
	"%wcache",
	"phread",
	"phwrite",
};
static int	buf_header_cnt = sizeof(buf_header)/sizeof(char *);

static Reg_t *
expand_buf(Reg_t *rp)
{
	return(expand_any(rp, &buf_header[0], buf_header_cnt, 0L));
}

setformat(fill_buf)
{
	fkmkformat(0, (persec ? (BBTOB(srelval(lread))/(float)ticks)*HZ :
		BBTOB(srelval(lread))));
	fkmkformat(1, (persec ? (BBTOB(srelval(bread))/(float)ticks)*HZ :
		BBTOB(srelval(bread))));
	mkformat(2, "%4.1f", (srelval(lread) == 0 ? 0.0 :
		((frelval(lread)-frelval(bread))/frelval(lread))*100.0));
	fkmkformat(3, (persec ? (BBTOB(srelval(lwrite))/(float)ticks)*HZ :
		BBTOB(srelval(lwrite))));
	fkmkformat(4, (persec ? (BBTOB(srelval(bwrite))/(float)ticks)*HZ :
		BBTOB(srelval(bwrite))));
	fkmkformat(5, (persec ? (BBTOB(srelval(wcancel))/(float)ticks)*HZ :
		BBTOB(srelval(wcancel))));
	mkformat(6, "%4.1f", (srelval(lwrite) == 0 ? 0.0 :
	     (srelval(bwrite) > srelval(lwrite) ? 
	     (-(frelval(bwrite)-frelval(lwrite))/frelval(bwrite))*100.0 :
	     ((frelval(lwrite)-frelval(bwrite))/frelval(lwrite))*100.0)));
	fkmkformat(7, (persec ? (BBTOB(srelval(phread))/(float)ticks)*HZ :
		BBTOB(srelval(phread))));
	fkmkformat(8, (persec ? (BBTOB(srelval(phwrite))/(float)ticks)*HZ :
		BBTOB(srelval(phwrite))));
}}

/*
 * See what's up in socket-land.
 */

# define SOCK_LO	RF_USERF1	/* if local interface */

static char	*ifsock_header[] = {
	"Ipackets",
	"Opackets",
	"Ierrors",
	"Oerrors",
	"collisions",
};

static char	*iflosock_header[] = {
	"Ipackets",
	"Opackets",
};

static Reg_t	*
expand_ifsock(Reg_t *rp)
{
   long		i;
   Reg_t	*nrp;
   Reg_t	*trp;

	if (nif == 0) {
		sprintf(rp->name, "No Net Interfaces");
		rp->flags &= ~RF_ON;
	}
	else {
		sprintf(rp->name, "NetIF[%s]", 
			nifreq[0].ifr_name);
		rp->fillfunc = fill_ifsock;
		if (strncmp(nifreq[0].ifr_name, "lo", 2) == 0) {
			rp->flags |= SOCK_LO;
			nrp = expand_any(rp, iflosock_header,
				sizeof(iflosock_header)/sizeof(char *), 0);
		}
		else {
			rp->flags &= ~SOCK_LO;
			nrp = expand_any(rp, ifsock_header,
				sizeof(ifsock_header)/sizeof(char *), 0);
		}
		for (i = 1; i < nif; i++) {
			trp = pfollow(nrp);
			trp->flags |= (RF_HDRNET | RF_ON);
			sprintf(trp->name, "NetIF[%s]",
				nifreq[i].ifr_name);
			if (strncmp(nifreq[i].ifr_name, "lo", 2) == 0) {
				trp->flags |= SOCK_LO;
				trp->flags |= RF_SUPRESS;
				nrp = expand_any(trp, iflosock_header,
					sizeof(iflosock_header)/sizeof(char *),
					i<<20);
			}
			else {
				trp->flags &= ~SOCK_LO;
				nrp = expand_any(trp, ifsock_header,
					sizeof(ifsock_header)/sizeof(char *),
					i<<20);
			}
		}
		return(nrp);
	}
	return NULL;
}

# define	ifcalcnet(x,n)	(persec ? \
			((int)(((nifreq[n].ifr_stats.x-lifreq[n].ifr_stats.x)/\
				(float)ticks)*HZ)):\
				(nifreq[n].ifr_stats.x-lifreq[n].ifr_stats.x))

setnformat(fill_ifsock)
{
	kmkformat(0, ifcalcnet(ifs_ipackets, n));
	kmkformat(1, ifcalcnet(ifs_opackets, n));
	nckmkformat(2, ifcalcnet(ifs_ierrors, n), SOCK_LO);
	nckmkformat(3, ifcalcnet(ifs_oerrors, n), SOCK_LO);
	nckmkformat(4, ifcalcnet(ifs_collisions, n), SOCK_LO);
}}

/*
 * See what's up in tcp-land.
 */
static char	*tcp_header[] = {
	"acc. conns",
	"sndtotal",
	"rcvtotal",
	"sndbyte",
	"rexmtbyte",
	"rcvbyte",
};
static int	tcp_header_cnt = sizeof(tcp_header)/sizeof(char *);

static Reg_t *
expand_tcp(Reg_t *rp)
{
	return(expand_any(rp, &tcp_header[0], tcp_header_cnt, 0L));
}

# define	tcalcnet(x)	(persec ? \
			((int)(((ntcpstat->x - ltcpstat->x)/(float)ticks)*HZ)):\
				(ntcpstat->x - ltcpstat->x))

setformat(fill_tcp)
{
	kmkformat(0, tcalcnet(tcps_accepts));
	kmkformat(1, tcalcnet(tcps_sndtotal));
	kmkformat(2, tcalcnet(tcps_rcvtotal));
	kmkformat(3, tcalcnet(tcps_sndbyte));
	kmkformat(4, tcalcnet(tcps_sndrexmitbyte));
	kmkformat(5, tcalcnet(tcps_rcvbyte) + tcalcnet(tcps_rcvoobyte));
}}

/*
 * See what's up in udp-land.
 */
static char	*udp_header[] = {
	"ipackets",
	"opackets",
	"dropped",
	"errors",
};
static int	udp_header_cnt = sizeof(udp_header)/sizeof(char *);

static Reg_t *
expand_udp(Reg_t *rp)
{
	return(expand_any(rp, &udp_header[0], udp_header_cnt, 0L));
}

# define	ucalcnet(x)	(persec ? \
			((int)(((nudpstat->x - ludpstat->x)/(float)ticks)*HZ)):\
				(nudpstat->x - ludpstat->x))

int	udpval;

static void
calcudp(int which)
{
	switch (which) {
	case 0:
		udpval = ucalcnet(udps_ipackets);
		break;
	case 1:
		udpval = ucalcnet(udps_opackets);
		break;
	case 2:
		udpval = ucalcnet(udps_hdrops) +
			 ucalcnet(udps_fullsock) +
			 ucalcnet(udps_noport) +
			 ucalcnet(udps_noportbcast);
		break;
	case 3:
		udpval = ucalcnet(udps_badsum) +
			 ucalcnet(udps_badlen);
		break;
	}
}

fsetformat(fill_udp, calcudp(which))
{
	kmkformat(0, udpval);
	kmkformat(1, udpval);
	kmkformat(2, udpval);
	kmkformat(3, udpval);
}}

/*
 * See what's up in ip-land.
 */
static char	*ip_header[] = {
	"ipackets",
	"opackets",
	"forward",
	"dropped",
	"errors",
};
static int	ip_header_cnt = sizeof(ip_header)/sizeof(char *);

static Reg_t *
expand_ip(Reg_t *rp)
{
	return(expand_any(rp, &ip_header[0], ip_header_cnt, 0L));
}

# define	icalcnet(x)	(persec ? \
			((int)(((nipstat->x - lipstat->x)/(float)ticks)*HZ)):\
				(nipstat->x - lipstat->x))

int	ipval;

static void
calcip(int which)
{
	switch (which) {
	case 0:
		ipval = icalcnet(ips_total);
		break;
	case 1:
		ipval = icalcnet(ips_localout);
		break;
	case 2:
		ipval = icalcnet(ips_forward);
		break;
	case 3:
		ipval = icalcnet(ips_odropped) +
			icalcnet(ips_noroute) +
			icalcnet(ips_cantforward);
		break;
	case 4:
		ipval = icalcnet(ips_badsum) +
			icalcnet(ips_tooshort) +
			icalcnet(ips_toosmall) +
			icalcnet(ips_badhlen) +
			icalcnet(ips_badlen) +
			icalcnet(ips_noproto) +
			icalcnet(ips_badoptions);
		break;
	}
}

fsetformat(fill_ip, calcip(which))
{
	kmkformat(0, ipval);
	kmkformat(1, ipval);
	kmkformat(2, ipval);
	kmkformat(3, ipval);
	kmkformat(4, ipval);
}}

/*
 * Page swapping information.
 */
static char	*swap_header[] = {
	"freeswap",
	"vswap",
	"swapin",
	"swapout",
	"bswapin",
	"bswapout",
};
static int	swap_header_cnt = sizeof(swap_header)/sizeof(char *);

static Reg_t *
expand_swap(Reg_t *rp)
{
	return(expand_any(rp, &swap_header[0], swap_header_cnt, 0L));
}

setformat(fill_swap)
{
	fkmkformat(0, (float) ctobl(dtop(nmp->freeswap)));
	kmkformat(1, ctobl(rminfo.availsmem - min_file_pages));
	kmkformat(2, ssrelval(swapin));
	kmkformat(3, ssrelval(swapout));
	kmkformat(4, ctob(ssrelval(bswapin)));
	kmkformat(5, ctob(ssrelval(bswapout)));
}}

/*
 * Pathname cache information
 */
static char	*nc_header[] = {
	"hits",
	"misses",
	"long_look",
	"enters",
	"dbl_enters",
	"long_enter",
	"purges",
	"vfs_purges",
	"removes",
	"searches",
	"stale_hits",
	"steps",
};
static int	nc_header_cnt = sizeof(nc_header)/sizeof(char *);

static Reg_t *
expand_nc(Reg_t *rp)
{
	return(expand_any(rp, &nc_header[0], nc_header_cnt, 0L));
}

setformat(fill_nc)
{
	kmkformat(0, ncrelval(hits));
	kmkformat(1, ncrelval(misses));
	kmkformat(2, ncrelval(long_look));
	kmkformat(3, ncrelval(enters));
	kmkformat(4, ncrelval(dbl_enters));
	kmkformat(5, ncrelval(long_enter));
	kmkformat(6, ncrelval(purges));
	kmkformat(7, ncrelval(vfs_purges));
	kmkformat(8, ncrelval(removes));
	kmkformat(9, ncrelval(searches));
	kmkformat(10, ncrelval(stale_hits));
	kmkformat(11, ncrelval(steps));
}}

/*
 * getblk information
 */
static char	*getblk_header[] = {
	"getblks",
	"getfree",
	" b-lockmiss",
	" b-found",
	" b-chg",
	" b-loops",
	" empty",
	" hmiss",
	" hmisx",
	" alllck",
	" delwri",
	" refcnt",
	" relse",
	" overlaps",
	"clusters",
	" clustered",
	"getfrags",
	"patched",
	"trimmed",
	"flush",
	" flushloops",
	"decomms",
	" flush decomms",
	"delrsv",
	" delrsvfree",
	" delrsvclean",
	" delrsvdirty",
	" delrsvwait",
	"sync commits",
	"commits",
	"getfreecommit",
	"deactivate",
	"activate",
	"force pinned",
};
static int	getblk_header_cnt = sizeof(getblk_header)/sizeof(char *);

static Reg_t *
expand_getblk(Reg_t *rp)
{
	return(expand_any(rp, &getblk_header[0], getblk_header_cnt, 0L));
}

setformat(fill_getblk)
{
	kmkformat(0, getblkrelval(getblks));
	kmkformat(1, getblkrelval(getfree));
	kmkformat(2, getblkrelval(getblockmiss));
	kmkformat(3, getblkrelval(getfound));
	kmkformat(4, getblkrelval(getbchg));
	kmkformat(5, getblkrelval(getloops));
	kmkformat(6, getblkrelval(getfreeempty));
	kmkformat(7, getblkrelval(getfreehmiss));
	kmkformat(8, getblkrelval(getfreehmissx));
	kmkformat(9, getblkrelval(getfreealllck));
	kmkformat(10, getblkrelval(getfreedelwri));
	kmkformat(11, getblkrelval(getfreeref));
	kmkformat(12, getblkrelval(getfreerelse));
	kmkformat(13, getblkrelval(getoverlap));
	kmkformat(14, getblkrelval(clusters));
	kmkformat(15, getblkrelval(clustered));
	kmkformat(16, getblkrelval(getfrag));
	kmkformat(17, getblkrelval(getpatch));
	kmkformat(18, getblkrelval(trimmed));
	kmkformat(19, getblkrelval(flush));
	kmkformat(20, getblkrelval(flushloops));
	kmkformat(21, getblkrelval(decomms));
	kmkformat(22, getblkrelval(flush_decomms));
	kmkformat(23, getblkrelval(delrsv));
	kmkformat(24, getblkrelval(delrsvfree));
	kmkformat(25, getblkrelval(delrsvclean));
	kmkformat(26, getblkrelval(delrsvdirty));
	kmkformat(27, getblkrelval(delrsvwait));
	kmkformat(28, getblkrelval(sync_commits));
	kmkformat(29, getblkrelval(commits));
	kmkformat(30, getblkrelval(getfreecommit));
	kmkformat(31, getblkrelval(inactive));
	kmkformat(32, getblkrelval(active));
	kmkformat(33, getblkrelval(force));
}}

/*
 * heap information
 */
static char	*heap_header[] = {
	"heapmem",
	"  overhd",
	"  unused",
	" allocs",
	" frees",
};
static int	heap_header_cnt = sizeof(heap_header)/sizeof(char *);

static Reg_t *
expand_heap(Reg_t *rp)
{
	return(expand_any(rp, &heap_header[0], heap_header_cnt, 0L));
}

setformat(fill_heap)
{
	kmkformat(0, mval(heapmem));
	kmkformat(1, mval(hovhd));
	kmkformat(2, mval(hunused));
	kmkformat(3, mrelval(halloc));
	kmkformat(4, mrelval(hfree));
}}

/*
 * vnode information
 */
static char	*vnode_header[] = {
	"vnodes",
	"active",
	"destroyed",
	"vn_alloc",
	"  freelist",
	"  freeloops",
	"  freemiss",
	"  heap",
	"vn_get",
	"  changed",
	"  freelist",
	"vn_rele",
	"vn_reclaim",
};
static int	vnode_header_cnt = sizeof(vnode_header)/sizeof(char *);

static Reg_t *
expand_vnode(Reg_t *rp)
{
	return(expand_any(rp, &vnode_header[0], vnode_header_cnt, 0L));
}

setformat(fill_vnode)
{
	kmkformat(0, vn_vnodes - vn_epoch);
	kmkformat(1, vn_vnodes - vn_epoch - vn_nfree);
	kmkformat(2, vnoderelval(vn_destroy));
	kmkformat(3, vnoderelval(vn_alloc));
	kmkformat(4, vnoderelval(vn_afree));
	kmkformat(5, vnoderelval(vn_afreeloops));
	kmkformat(6, vnoderelval(vn_afreemiss));
	kmkformat(7, vnoderelval(vn_aheap));
	kmkformat(8, vnoderelval(vn_get));
	kmkformat(9, vnoderelval(vn_gchg));
	kmkformat(10, vnoderelval(vn_gfree));
	kmkformat(11, vnoderelval(vn_rele));
	kmkformat(12, vnoderelval(vn_reclaim));
}}

/*
 * Efs (iget/iupdat) information
 */
static char	*iget_header[] = {
	"attempts",
	" found",
	" frecycle",
	" missed",
	" dup",
	"reclaims",
	"itobp",
	" hit-bc",
	"iupdat",
	" acc",
	" upd",
	" chg",
	" mod",
	" unk",
	"iallocrd",
	" hit-bc",
	" coll",
	"bit-rd",
	" rd-hit-bm",
	" rd-hit-bc",
	"dirblks",
	"dirupd",
	"truncs",
	"creats",
	"attrchg",
};
static int	iget_header_cnt = sizeof(iget_header)/sizeof(char *);

static Reg_t *
expand_iget(Reg_t *rp)
{
	return(expand_any(rp, &iget_header[0], iget_header_cnt, 0L));
}

setformat(fill_iget)
{
	kmkformat(0, igetrelval(ig_attempts));
	kmkformat(1, igetrelval(ig_found));
	kmkformat(2, igetrelval(ig_frecycle));
	kmkformat(3, igetrelval(ig_missed));
	kmkformat(4, igetrelval(ig_dup));
	kmkformat(5, igetrelval(ig_reclaims));
	kmkformat(6, igetrelval(ig_itobp));
	kmkformat(7, igetrelval(ig_itobpf));
	kmkformat(8, igetrelval(ig_iupdat));
	kmkformat(9, igetrelval(ig_iupacc));
	kmkformat(10, igetrelval(ig_iupupd));
	kmkformat(11, igetrelval(ig_iupchg));
	kmkformat(12, igetrelval(ig_iupmod));
	kmkformat(13, igetrelval(ig_iupunk));
	kmkformat(14, igetrelval(ig_iallocrd));
	kmkformat(15, igetrelval(ig_iallocrdf));
	kmkformat(16, igetrelval(ig_ialloccoll));
	kmkformat(17, igetrelval(ig_bmaprd));
	kmkformat(18, igetrelval(ig_bmapfbm));
	kmkformat(19, igetrelval(ig_bmapfbc));
	kmkformat(20, ssrelval(dirblk));
	kmkformat(21, igetrelval(ig_dirupd));
	kmkformat(22, igetrelval(ig_truncs));
	kmkformat(23, igetrelval(ig_icreat));
	kmkformat(24, igetrelval(ig_attrchg));
}}

/*
 * Xfs information
 */
static char	*xfs_header[] = {
	"allocext",
	" blk",
	"freeext",
	" blk",
	"abtlook",
	" cmp",
	" insr",
	" delr",
	"blkmapr",
	" mapw",
	" unmap",
	"exlstadd",
	" del",
	" srch",
	" cmp",
	"bmbtlook",
	" cmp",
	" insr",
	" delr",
	"dirlookup",
	" create",
	" remove",
	" getd",
	"attr_list",
	" attr_get",
	" attr_set",
	" attr_remove",
	"iattempts",
	" found",
	" frecycle",
	" missed",
	" dup",
	" reclaims",
	"attrchg",
	"xactsync",
	" async",
	" empty",
	"logwrites",
	" blocks",
	" noiclogs",
	"log forces",
	" sleep",
	"logspace",
	" sleep",
	"ail pushes",
	" success",
	" pushbuf",
	" pinned",
	" locked",
	" flushing",
	" restarts",
	" log force",
	"write calls",
	"write bytes",
	"write bufs",
	"read calls",
	"read bytes",
	"read bufs",
	"xfsd bufs",
	"xstrat bytes",
	"xstrat quick",
	"xstrat split",
        "quota recl",
        " recl mis",
        " dq dups",
        " cache mis",
        " cache hit",
        " want hit",
        " shake recl",
        " inact recl",
	"xfs_iflush",
	" flushzero",
};
static int	xfs_header_cnt = sizeof(xfs_header)/sizeof(char *);

static Reg_t *
expand_xfs(Reg_t *rp)
{
	return(expand_any(rp, &xfs_header[0], xfs_header_cnt, 0L));
}

setformat(fill_xfs)
{
	kmkformat(0, xfsrelval(xs_allocx));
	kmkformat(1, xfsrelval(xs_allocb));
	kmkformat(2, xfsrelval(xs_freex));
	kmkformat(3, xfsrelval(xs_freeb));
	kmkformat(4, xfsrelval(xs_abt_lookup));
	kmkformat(5, xfsrelval(xs_abt_compare));
	kmkformat(6, xfsrelval(xs_abt_insrec));
	kmkformat(7, xfsrelval(xs_abt_delrec));
	kmkformat(8, xfsrelval(xs_blk_mapr));
	kmkformat(9, xfsrelval(xs_blk_mapw));
	kmkformat(10, xfsrelval(xs_blk_unmap));
	kmkformat(11, xfsrelval(xs_add_exlist));
	kmkformat(12, xfsrelval(xs_del_exlist));
	kmkformat(13, xfsrelval(xs_look_exlist));
	kmkformat(14, xfsrelval(xs_cmp_exlist));
	kmkformat(15, xfsrelval(xs_bmbt_lookup));
	kmkformat(16, xfsrelval(xs_bmbt_compare));
	kmkformat(17, xfsrelval(xs_bmbt_insrec));
	kmkformat(18, xfsrelval(xs_bmbt_delrec));
	kmkformat(19, xfsrelval(xs_dir_lookup));
	kmkformat(20, xfsrelval(xs_dir_create));
	kmkformat(21, xfsrelval(xs_dir_remove));
	kmkformat(22, xfsrelval(xs_dir_getdents));
	kmkformat(23, xfsrelval(xs_attr_list));
	kmkformat(24, xfsrelval(xs_attr_get));
	kmkformat(25, xfsrelval(xs_attr_set));
	kmkformat(26, xfsrelval(xs_attr_remove));
	kmkformat(27, xfsrelval(xs_ig_attempts));
	kmkformat(28, xfsrelval(xs_ig_found));
	kmkformat(29, xfsrelval(xs_ig_frecycle));
	kmkformat(30, xfsrelval(xs_ig_missed));
	kmkformat(31, xfsrelval(xs_ig_dup));
	kmkformat(32, xfsrelval(xs_ig_reclaims));
	kmkformat(33, xfsrelval(xs_ig_attrchg));
	kmkformat(34, xfsrelval(xs_trans_sync));
	kmkformat(35, xfsrelval(xs_trans_async));
	kmkformat(36, xfsrelval(xs_trans_empty));
	kmkformat(37, xfsrelval(xs_log_writes));
	kmkformat(38, xfsrelval(xs_log_blocks));
	kmkformat(39, xfsrelval(xs_log_noiclogs));
	kmkformat(40, xfsrelval(xs_log_force));
	kmkformat(41, xfsrelval(xs_log_force_sleep));
	kmkformat(42, xfsrelval(xs_try_logspace));
	kmkformat(43, xfsrelval(xs_sleep_logspace));
	kmkformat(44, xfsrelval(xs_push_ail));
	kmkformat(45, xfsrelval(xs_push_ail_success));
	kmkformat(46, xfsrelval(xs_push_ail_pushbuf));
	kmkformat(47, xfsrelval(xs_push_ail_pinned));
	kmkformat(48, xfsrelval(xs_push_ail_locked));
	kmkformat(49, xfsrelval(xs_push_ail_flushing));
	kmkformat(50, xfsrelval(xs_push_ail_restarts));
	kmkformat(51, xfsrelval(xs_push_ail_flush));
	kmkformat(52, xfsrelval(xs_write_calls));
	kmkformat(53, xfsrelval(xs_write_bytes));
	kmkformat(54, xfsrelval(xs_write_bufs));
	kmkformat(55, xfsrelval(xs_read_calls));
	kmkformat(56, xfsrelval(xs_read_bytes));
	kmkformat(57, xfsrelval(xs_read_bufs));
	kmkformat(58, xfsrelval(xs_xfsd_bufs));
	kmkformat(59, xfsrelval(xs_xstrat_bytes));
	kmkformat(60, xfsrelval(xs_xstrat_quick));
	kmkformat(61, xfsrelval(xs_xstrat_split));

	kmkformat(62, xfsrelval(xs_qm_dqreclaims));
	kmkformat(63, xfsrelval(xs_qm_dqreclaim_misses));
	kmkformat(64, xfsrelval(xs_qm_dquot_dups));
	kmkformat(65, xfsrelval(xs_qm_dqcachemisses));
	kmkformat(66, xfsrelval(xs_qm_dqcachehits));
	kmkformat(67, xfsrelval(xs_qm_dqwants));
	kmkformat(68, xfsrelval(xs_qm_dqshake_reclaims));
	kmkformat(69, xfsrelval(xs_qm_dqinact_reclaims));

	kmkformat(70, xfsrelval(xs_iflush_count));
	kmkformat(71, xfsrelval(xs_icluster_flushzero));
}}

/*
 * Figure out if this machine has a graphics head.  No need to display
 * graphics data if it doesn't!
 */
static int
isgfx(void)
{
   inventory_t	*ip;

	while ((ip = getinvent()) != NULL) {
		if (ip->inv_class == INV_GRAPHICS) {
			endinvent();
			return(1);
		}
	}
	endinvent();
	return(0);
}

static int
isvideo(void)
{
   inventory_t	*ip;

	while ((ip = getinvent()) != NULL) {
		if (ip->inv_class == INV_VIDEO) {
			endinvent();
			return(1);
		}
	}
	endinvent();
	return(0);
}

#ifdef TILES_TO_LPAGES
static int
istiles(void)
{
	if (sysmp(MP_SASZ, MPSA_TILEINFO) == -1)
		return 0;
	else
		return 1;
}
#endif /* TILES_TO_LPAGES */

# define	MAXKBUF		20
# define	kilo		1024
# define	mega		(kilo*kilo)
# define	giga		(kilo*kilo*kilo)

static char *
konvert(long long val)
{
   static char	kbuf[MAXKBUF];

	if (val >= giga)
		sprintf(kbuf, "%5.1fG", ((float)val) / giga);
	else if (val >= mega)
		sprintf(kbuf, "%5.1fM", ((float)val) / mega);
	else if (val >= kilo)
		sprintf(kbuf, "%5.1fK", ((float)val) / kilo);
	else
		sprintf(kbuf, "%lld", val);
	return(kbuf);
}

static char *
fkonvert(float val)
{
   static char	kbuf[MAXKBUF];

	if (val >= giga)
		sprintf(kbuf, "%5.1fG", ((float)val) / giga);
	else if (val >= mega)
		sprintf(kbuf, "%5.1fM", ((float)val) / mega);
	else if (val >= kilo)
		sprintf(kbuf, "%5.1fK", ((float)val) / kilo);
	else
		sprintf(kbuf, "%d", (int) val);
	return(kbuf);
}
