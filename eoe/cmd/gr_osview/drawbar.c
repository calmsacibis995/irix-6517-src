/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1993 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * drawbar.c - routines to draw the bars all the time.  Once program is
 * set up, all work is concentrated here with calls only to the bar
 * drawing module.
 */

# include	"grosview.h"

# include	<sys/param.h>
# include	<sys/sysmacros.h>
# include	<sys/sysinfo.h>
# include	<sys/buf.h>
# include	<sys/time.h>
# include	<sys/sysmp.h>
# include	<sys/sbd.h>
# include	<sys/immu.h>
# include	<sys/swap.h>
# include	<sys/fs/efs_fs.h>
# include	<sys/fs/efs_ino.h>
# include	<sys/tcpipstats.h>
# include	<netinet/in_systm.h>
# include	<netinet/in.h>
# include	<netinet/ip.h>
# include	<sys/ioctl.h>
# include	<sys/socket.h>
# include	<sys/var.h>
# include	<net/if.h>
# include	<gl.h>
# include	<gl/device.h>
# include	<bstring.h>
# include	<memory.h>
# include	<stdlib.h>
# include	<errno.h>
# include	<signal.h>
# include	<stdarg.h>
# include	<string.h>
# include	<setjmp.h>
# include	<unistd.h>

/*
 * This table gives the functions that can be called from outside the
 * moving bar module.
 */
struct cbfunc_s	cbfunc = {
	cbinit,
	cbredraw,
	cbdrawlegend,
	cbdraw,
	cbdrawabs,
	cbmessage,
	cblayout,
	cbtakedown,
};

/*
 * Since the basic algorithms for most bars require finding the (usually small)
 * difference between2 (potentially large) numbers, we require a lot of
 * precision. a float has 23 bits - clearly not enough for a 32 bit integer.
 * Doubles have 52 bits which should last use even when the kernel
 * counters go to longlong
 */
# define	WAITSLOTS	3
# define	scalcavg(y,x)	((double)(nsp[y]->x-barinfo->x)/\
					(nticks-barinfo->nticks))
# define	mrelval(x)	(nmp->x - barinfo->x)
# define	calcavg(x)	(((x - barinfo->x)/\
					(nticks-barinfo->nticks))*HZ)
# define	mcalcavg(x)	(((nmp->x-barinfo->x)/\
					(nticks-barinfo->nticks))*HZ)
# define	tcalcavg(x)	((double)(x - barinfo->x)/\
					(nticks-barinfo->nticks))
# define	relval(x)	(x - barinfo->x)
# define	tosec(x)	(((double)(x)/(nticks-barinfo->nticks))*HZ)
# define	dbsetup()	{\
	rembar = cbp;	\
	if (cbp->cb_info == 0)\
		cbp->cb_info = calloc(1, sizeof(struct bar_s));\
	barinfo = (struct bar_s *) cbp->cb_info; \
	cbp->cb_infolen = sizeof(*barinfo);}

static void		startpoker(int);
static void		locmodtlimit(colorblock_t *, double);
static void		kicked(void);
static void		remotedraw(void);

static void		(*modtlimit)(colorblock_t *, double) = locmodtlimit;
static colorblock_t	*rembar;

void			(*drawproc)(colorblock_t *, ...);

void
drawbar_text_start(void) {}	/* beginning of text to lock down */

void
drawcpubar(colorblock_t *cbp)
{
   struct bar_s {
# ifdef FIXGFX
	time_t	cpu[6];
	time_t	gfxf;
	time_t	gfxc;
# else
	time_t	cpu[8];
# endif
	double	idlesum;
	long	nticks;
   } *barinfo;
   int		i;
   double	idlesum;
# ifdef FIXGFX
   time_t	gfxf;
   time_t	gfxc;
# endif

	dbsetup();
	if (cbp->cb_subtype == CPU_SUM) {
	   long		cpu[6];
	   long		stick;

		cpu[CPU_IDLE] = nsp[0]->cpu[CPU_IDLE];
		cpu[CPU_WAIT] = nsp[0]->cpu[CPU_WAIT];
		cpu[CPU_USER] = nsp[0]->cpu[CPU_USER];
		cpu[CPU_KERNEL] = nsp[0]->cpu[CPU_KERNEL];
		cpu[CPU_INTR] = nsp[0]->cpu[CPU_INTR];
		cpu[CPU_SXBRK] = nsp[0]->cpu[CPU_SXBRK];
# ifdef FIXGFX
		gfxf = nsp[0]->wait[W_GFXF];
		gfxc = nsp[0]->wait[W_GFXC];
# endif
		for (i = 1; i < ncpu; i++) {
			cpu[CPU_IDLE] += nsp[i]->cpu[CPU_IDLE];
			cpu[CPU_WAIT] += nsp[i]->cpu[CPU_WAIT];
			cpu[CPU_USER] += nsp[i]->cpu[CPU_USER];
			cpu[CPU_KERNEL] += nsp[i]->cpu[CPU_KERNEL];
			cpu[CPU_INTR] += nsp[i]->cpu[CPU_INTR];
			cpu[CPU_SXBRK] += nsp[i]->cpu[CPU_SXBRK];
# ifdef FIXGFX
			gfxf += nsp[i]->wait[W_GFXF];
			gfxc += nsp[i]->wait[W_GFXC];
# endif
		}
		idlesum = cpu[CPU_IDLE] + cpu[CPU_WAIT] + cpu[CPU_SXBRK];
		stick = nticks;
		nticks *= ncpu;
		(*drawproc)(cbp,
			tcalcavg(cpu[CPU_USER]),
			tcalcavg(cpu[CPU_KERNEL]),
			tcalcavg(cpu[CPU_INTR]),
# ifdef FIXGFX
			tcalcavg(gfxf),
			tcalcavg(gfxc),
# else
			tcalcavg(cpu[CPU_GFXF]),
			tcalcavg(cpu[CPU_GFXC]),
# endif
			tcalcavg(idlesum));
		memcpy(barinfo->cpu, cpu, sizeof(barinfo->cpu));
# ifdef FIXGFX
		barinfo->gfxf = gfxf;
		barinfo->gfxc = gfxc;
# endif
		barinfo->idlesum = idlesum;
		barinfo->nticks = nticks;
		nticks = stick;
	}
	else {
		idlesum = nsp[cbp->cb_subtype]->cpu[CPU_IDLE] +
			nsp[cbp->cb_subtype]->cpu[CPU_WAIT] +
			nsp[cbp->cb_subtype]->cpu[CPU_SXBRK];
# ifdef FIXGFX
		gfxf = nsp[cbp->cb_subtype]->wait[W_GFXF];
		gfxc = nsp[cbp->cb_subtype]->wait[W_GFXC];
# endif
		(*drawproc)(cbp,
			(double)scalcavg(cbp->cb_subtype, cpu[CPU_USER]),
			(double)scalcavg(cbp->cb_subtype, cpu[CPU_KERNEL]),
			(double)scalcavg(cbp->cb_subtype, cpu[CPU_INTR]),
# ifdef FIXGFX
			(double)tcalcavg(gfxf),
			(double)tcalcavg(gfxc),
# else
			(double)scalcavg(0, cpu[CPU_GFXF]),
			(double)scalcavg(0, cpu[CPU_GFXC]),
# endif
			(double)tcalcavg(idlesum));
		memcpy(barinfo->cpu, nsp[cbp->cb_subtype]->cpu,
			sizeof(barinfo->cpu));
# ifdef FIXGFX
		barinfo->gfxf = gfxf;
		barinfo->gfxc = gfxc;
# endif
		barinfo->idlesum = idlesum;
		barinfo->nticks = nticks;
	}
}

void
drawwaitbar(colorblock_t *cbp)
{
   struct bar_s {
	double	wait[MAXCPU][WAITSLOTS];
	double	cpuwait[MAXCPU];
	double	totidle;
	long	nticks;
   } *barinfo;
   int		i;
   int		j;
   double	wait[WAITSLOTS];
   double	totwait;
   double	totidle, totidleperiod;
   double	tsum;
   double	div;
   double	last;

	dbsetup();
	for (i = 0; i < WAITSLOTS; i++)
		wait[i] = 0;
	totwait = 0;
	totidle = 0.0;
	tsum = 0;
	for (i = 0; i < ncpu; i++) {
		totidle += nsp[i]->cpu[CPU_IDLE] +
			nsp[i]->cpu[CPU_WAIT] +
			nsp[i]->cpu[CPU_SXBRK];
		for (j = 0; j < WAITSLOTS; j++) {
			wait[j] += nsp[i]->wait[j]-barinfo->wait[i][j];
			barinfo->wait[i][j] = nsp[i]->wait[j];
		}
		totwait += nsp[i]->cpu[CPU_WAIT] - barinfo->cpuwait[i];
		barinfo->cpuwait[i] = nsp[i]->cpu[CPU_WAIT];
	}
	totidleperiod = totidle - barinfo->totidle;
	barinfo->totidle = totidle;
	if (totwait > 0) {
		for (i = 0; i < WAITSLOTS; i++) tsum += wait[i];
		if (tsum > totidleperiod) {
			div = totidle;
			last = 0;
		}
		else {
			div = totidleperiod;
			last = (totidleperiod - tsum)/div;
		}
		for (i = 0; i < WAITSLOTS; i++) {
			wait[i] /= div;
		}
	}
	if (totwait > 0)
		(*drawproc)(cbp, wait[W_IO], wait[W_SWAP],
			wait[W_PIO], last);
	else
		(*drawproc)(cbp, 0.0, 0.0, 0.0, 1.0);
	barinfo->nticks = nticks;
}

void
drawmembar(colorblock_t *cbp)
{
	struct bar_s {
		int	first;
		long	nticks;
	} *barinfo;

	dbsetup();
	if (barinfo->first == 1) {
		(*modtlimit)(cbp, (double) physmem);
		CBDRAWLEGEND(cbp);
	}

	/*
	printf("phys %d buf %d chunk %d pdwri %d usr %d free %d dpages %d\n",
			nrm->physmem, nrm->bufmem, nrm->chunkpages,
			nrm->dchunkpages, nrm->availrmem, nrm->freemem,
			nrm->dpages);
	*/

	/*
	 * Usermem(availrmem) includes ALL pages in use by processes
	 *	as well as ALL
	 *	main chunk pages (dirty or not, mmap'ed or not) and ALL
	 *		free pages.
	 *	dpages are all dirty mmap pages that have been pushed
	 *		to the vnode list. They may or may not still
	 *		be valid in the process. (they are part of Usermem,
	 *		but are NOT part of chunkpages).
	 *	a) chunkpages and dpages are disjoint
	 *	b) dpages may still be validly mapped to a user process
	 *	   OR may be free except for needing to be written.
	 *	c) dchunkpages is a subset of chunkpages
	 *	d) bufmem uses kvpalloc so comes out of the kernel (availrmem)
	 *		pool, so we subtract that from kernel memory
	 *
	 * Fsclean are all data pages owned by the chunk/buff cache
	 *	that aren't writable. Note taht these may not be
	 *	'free-able' since they could easily be attached to
	 *	just faulted in data.
	 * Fsdirty is ALL dirty pages - note that the dpages part
	 *	may not be 'freeable' after writing since they
	 *	could simply be part of an msync() and still be
	 *	mapped to a user's address space
	 *
	 * NOTE: rm->availrmem is really Usermem.
	 */
	(*drawproc)(cbp, 
		(double)(physmem - (nrm->availrmem + nrm->bufmem)),
		(double)(nrm->bufmem),
		(double)(nrm->dchunkpages + nrm->dpages),
		(double)(nrm->chunkpages - nrm->dchunkpages),
		(double)(nrm->freemem),
		(double)(nrm->availrmem -
			(nrm->freemem + nrm->chunkpages + nrm->dpages)));

	barinfo->first++;
	barinfo->nticks = nticks;
}

void
drawmemcbar(colorblock_t *cbp)
{
	struct bar_s {
		int	first;
		long	nticks;
	} *barinfo;

	dbsetup();
	if (barinfo->first == 1) {
		(*modtlimit)(cbp, (double) physmem);
		CBDRAWLEGEND(cbp);
	}

	(*drawproc)(cbp, 
		(double)(physmem - (nrm->availrmem + nrm->bufmem)),
		(double)(nrm->bufmem),
		(double)(nrm->dchunkpages + nrm->dpages),
		(double)(nrm->chunkpages - nrm->dchunkpages),
		(double)(nrm->freemem - nrm->emptymem),
		(double)(nrm->emptymem),
		(double)(nrm->availrmem -
			(nrm->freemem + nrm->chunkpages + nrm->dpages)));

	barinfo->first++;
	barinfo->nticks = nticks;
}

void
drawgfxbar(colorblock_t *cbp)
{
   struct bar_s {
	double	gintr;
	double	gswitch;
	double	griioctl;
	double	gswapbuf;
	double	fifowait;
	double	fifonowait;
	long	nticks;
   } *barinfo;
   double	gintr = 0;
   double	gswitch = 0;
   double	griioctl = 0;
   double	gswapbuf = 0;
   double	fifowait = 0;
   double	fifonowait = 0;
   int		i;

	dbsetup();
	for (i = 0; i < ncpu; i++) {
		gintr += nGp[i].gintr;
		gswitch += nGp[i].gswitch;
		griioctl += nGp[i].griioctl;
		gswapbuf += nGp[i].gswapbuf;
		fifowait += nGp[i].fifowait;
		fifonowait += nGp[i].fifonowait;
	}
	if (cbp->cb_type == T_NUM)
		(*drawproc)(cbp, relval(gintr), relval(gswitch),
			relval(griioctl), relval(gswapbuf), relval(fifowait),
			relval(fifonowait), 0.0);
	else
		(*drawproc)(cbp, calcavg(gintr), calcavg(gswitch),
			calcavg(griioctl), calcavg(gswapbuf), calcavg(fifowait),
			calcavg(fifonowait), 0.0);
	barinfo->gintr = gintr;
	barinfo->gswitch = gswitch;
	barinfo->griioctl = griioctl;
	barinfo->gswapbuf = gswapbuf;
	barinfo->fifowait = fifowait;
	barinfo->fifonowait = fifonowait;
	barinfo->nticks = nticks;
}

void
drawsyscallbar(colorblock_t *cbp)
{
   struct bar_s {
	double	syscall;
	double	pswitch;
	double	sysfork;
	double	sysexec;
	double	iget;
	long	nticks;
   } *barinfo;
   double	syscall = 0;
   double	pswitch = 0;
   double	sysfork = 0;
   double	sysexec = 0;
   double	iget = 0;
   int		i;

	dbsetup();
	for (i = 0; i < ncpu; i++) {
		syscall += nsp[i]->syscall;
		pswitch += nsp[i]->pswitch;
		sysfork += nsp[i]->sysfork;
		sysexec += nsp[i]->sysexec;
		iget += nsp[i]->iget;
	}
	if (cbp->cb_type == T_NUM)
		(*drawproc)(cbp, relval(syscall), relval(pswitch),
			relval(sysfork), relval(sysexec), relval(iget), 0.0);
	else
		(*drawproc)(cbp, calcavg(syscall),
			calcavg(pswitch), calcavg(sysfork), calcavg(sysexec),
			calcavg(iget), 0.0);
	barinfo->syscall = syscall;
	barinfo->pswitch = pswitch;
	barinfo->sysfork = sysfork;
	barinfo->sysexec = sysexec;
	barinfo->iget = iget;
	barinfo->nticks = nticks;
}

void
drawiothrubar(colorblock_t *cbp)
{
   struct bar_s {
	double	rbtot;
	double	wbtot;
	long	nticks;
   } *barinfo;
   double	rbtot;
   double	wbtot;
   int		i;

	dbsetup();
	rbtot = 0;
	wbtot = 0;
	for (i = 0; i < ncpu; i++) {
		rbtot += nsp[i]->readch;
		wbtot += nsp[i]->writech;
	}
	if (cbp->cb_type == T_NUM)
		(*drawproc)(cbp, relval(rbtot)/1024.0,
			relval(wbtot)/1024.0,0.0);
	else
		(*drawproc)(cbp, calcavg(rbtot)/1024.0,
			calcavg(wbtot)/1024.0,0.0);
	barinfo->rbtot = rbtot;
	barinfo->wbtot = wbtot;
	barinfo->nticks = nticks;
}

void
drawdiskbar(colorblock_t *cbp)
{
   struct bar_s {
	long	nticks;
	daddr_t	dsize;
   } *barinfo;
   struct statfs	*fp = &dstat[cbp->cb_disk].d_stat;
   double		inoblks;
   double		inofree;
   double		inuse;
   double		unuse;

	dbsetup();
	if (barinfo->dsize != fp->f_blocks) {
		(*modtlimit)(cbp, blktomb(fp->f_blocks, fp->f_bsize));
		CBDRAWLEGEND(cbp);
		barinfo->dsize = fp->f_blocks;
	}
	inuse = blktomb(((double)(fp->f_blocks-fp->f_bfree)), fp->f_bsize);
	unuse = blktomb(((double)fp->f_bfree), fp->f_bsize);
	if (dstat[cbp->cb_disk].d_flags & DF_EFS) {
		inoblks = blktomb(((double)(fp->f_files-fp->f_ffree)/EFS_INOPBB),
			fp->f_bsize);
		inofree = blktomb(((double)fp->f_ffree/EFS_INOPBB),
			fp->f_bsize);
		(*drawproc)(cbp,
			inoblks,
			inofree,
			inuse,
			unuse,
			0.0);
	}
	else {
		(*drawproc)(cbp,
			inuse,
			unuse,
			0.0);
	}
	barinfo->nticks = nticks;
}

void
drawbdevbar(colorblock_t *cbp)
{
   struct bar_s {
	double	rbtot;
	double	wbtot;
	double	lrbtot;
	double	lwbtot;
	double	wcncltot;
	long	nticks;
   } *barinfo;
   double	rbtot;
   double	wbtot;
   double	lrbtot;
   double	lwbtot;
   double	wcncltot;
   int		i;

	dbsetup();
	rbtot = 0;
	wbtot = 0;
	lrbtot = 0;
	lwbtot = 0;
	wcncltot = 0;
	for (i = 0; i < ncpu; i++) {
		rbtot += nsp[i]->bread;
		wbtot += nsp[i]->bwrite;
		lrbtot += nsp[i]->lread;
		lwbtot += nsp[i]->lwrite;
		wcncltot += nsp[i]->wcancel;
	}
	if (cbp->cb_type == T_NUM) {
	   double	whit;
	   double	rhit;
	   double	arbtot = relval(rbtot);
	   double	alrbtot = relval(lrbtot);
	   double	awbtot = relval(wbtot);
	   double	alwbtot = relval(lwbtot);
	   double	awcncltot = relval(wcncltot);

		if (alwbtot == 0)
			whit = 0;
		else if (awbtot == 0)
			whit = 100;
		else {
			whit = ((alwbtot - awbtot)/alwbtot) * 100;
			if (whit < 0)
				whit = 0;
		}
		if (alrbtot == 0)
			rhit = 0;
		else if (arbtot == 0)
			rhit = 100;
		else {
			rhit = ((alrbtot - arbtot)/alrbtot) * 100;
			if (rhit < 0)
				rhit = 0;
		}
		(*drawproc)(cbp, alrbtot, arbtot, rhit, alwbtot,
			awbtot, awcncltot, whit, 0.0);
	}
	else
		(*drawproc)(cbp,  calcavg(rbtot), calcavg(wbtot),
			0.0);
	barinfo->rbtot = rbtot;
	barinfo->wbtot = wbtot;
	barinfo->lrbtot = lrbtot;
	barinfo->lwbtot = lwbtot;
	barinfo->wcncltot = wcncltot;
	barinfo->nticks = nticks;
}

void
drawswpsbar(colorblock_t *cbp) {
   struct bar_s {
	off_t	vblocks;
	long	nticks;
   } *barinfo;

   off_t resswap;	/* reserved swap */
   off_t pmem;		/* physical memory for processes */
   off_t pmemr;		/* phys mem reserved */
   off_t fswp;		/* amount of free physical swap */
   off_t fswpr;		/* amount of free physical swap - reserved */
   off_t uswp;		/* amount of used physical swap */
   off_t uswpr;		/* amount of used physical swap - reserved */
   off_t vswp;		/* amount of virtual swap */
   off_t vswpr;		/* amount of virtual swap - reserved */
	dbsetup();
	if (barinfo->vblocks != nlswap) {
		(*modtlimit)(cbp, (double) nlswap * 512);
		CBDRAWLEGEND(cbp);
	}
	/*
	 * we are trying to show both reserved swap and partitioning
	 * of where logical swap comes from - thus we disperse the
	 * reserved info amoung the other bars
	 */
	resswap = rlswap;
	pmem = nlswap - vswap - mswap;
	pmemr = MIN(resswap, pmem);
	resswap -= pmemr;
	pmem -= pmemr;

	fswp = fswap;
	fswpr = MIN(resswap, fswp);
	resswap -= fswpr;
	fswp -= fswpr;

	uswp = tswap - fswap;
	uswpr = MIN(resswap, uswp);
	resswap -= uswpr;
	uswp -= uswpr;

	vswp = vswap;
	vswpr = MIN(resswap, vswp);
	resswap -= vswpr;
	vswp -= vswpr;

	(*drawproc)(cbp, (double)pmemr * 512, (double)pmem * 512,
			(double)fswpr * 512, (double)fswp * 512,
			(double)uswpr * 512, (double)uswp * 512,
			(double)vswpr * 512, (double)vswp * 512,
			0.0);
	barinfo->vblocks = nlswap;
	barinfo->nticks = nticks;
}

void
drawnetudpbar(colorblock_t *cbp) {
   struct bar_s {
	double	udps_inp;
	double	udps_outp;
	double	udps_drop;
	long	nticks;
   } *barinfo;
   double	udps_inp;
   double	udps_drop;
   double	udps_outp;
   double	drophold;

	dbsetup();
	udps_inp = nudpstat->udps_ipackets - barinfo->udps_inp;
	udps_outp = nudpstat->udps_opackets - barinfo->udps_outp;
	udps_drop = (drophold = nudpstat->udps_hdrops +
		     nudpstat->udps_badsum +
		     nudpstat->udps_badlen +
		     nudpstat->udps_noport +
		     nudpstat->udps_fullsock) - barinfo->udps_drop;
	if (udps_drop < 0)
		udps_drop = 0;
	if (cbp->cb_type == T_NUM) {
		(*drawproc)(cbp, udps_inp, udps_outp, udps_drop, 0.0);
	}
	else
		(*drawproc)(cbp, tosec(udps_drop), tosec(udps_inp),
			tosec(udps_outp), 0.0);
	barinfo->udps_inp = nudpstat->udps_ipackets;
	barinfo->udps_outp = nudpstat->udps_opackets;
	barinfo->udps_drop = drophold;
	barinfo->nticks = nticks;
}

void
drawnetiprbar(colorblock_t *cbp) {
   struct bar_s {
	double	ips_inp;
	double	ips_forw;
	double	ips_outp;
	double	ips_drop;
	double	ips_deliver;
	long	nticks;
   } *barinfo;
   double	ips_inp;
   double	ips_forw;
   double	ips_drop;
   double	ips_deliver;
   double	ips_outp;

	dbsetup();
	ips_inp = nipstat->ips_total - barinfo->ips_inp;
	ips_outp = nipstat->ips_localout - barinfo->ips_outp;
	ips_drop = nipstat->ips_odropped - barinfo->ips_drop;
	if (cbp->cb_type == T_NUM) {
		ips_forw = nipstat->ips_forward - barinfo->ips_forw;
		ips_deliver = nipstat->ips_delivered - barinfo->ips_deliver;
		(*drawproc)(cbp, ips_inp, ips_outp, ips_forw, ips_deliver,
			ips_drop);
		barinfo->ips_forw = nipstat->ips_forward;
		barinfo->ips_deliver = nipstat->ips_delivered;
	}
	else
		(*drawproc)(cbp, tosec(ips_drop), tosec(ips_inp),
			tosec(ips_outp), 0.0);
	barinfo->ips_inp = nipstat->ips_total;
	barinfo->ips_outp = nipstat->ips_localout;
	barinfo->ips_drop = nipstat->ips_odropped;
	barinfo->nticks = nticks;
}

void
drawnetbar(colorblock_t *cbp)
{
   struct bar_s {
	double	tcps_inb;
	double	tcps_outb;
	long	nticks;
   } *barinfo;
   double	tcps_inb;
   double	tcps_outb;

	dbsetup();
	tcps_inb = (ntcpstat->tcps_rcvbyte - barinfo->tcps_inb);
	if (tcps_inb < 0) tcps_inb = 0;
	tcps_outb = (ntcpstat->tcps_sndbyte - barinfo->tcps_outb);
	if (tcps_outb < 0) tcps_outb = 0;
	if (cbp->cb_type == T_NUM)
		(*drawproc)(cbp, tcps_inb/1024.0, tcps_outb/1024.0, 0.0);
	else {
		tcps_inb = ((tcps_inb/(nticks-barinfo->nticks))*HZ)/1024.0;
		tcps_outb = ((tcps_outb/(nticks-barinfo->nticks))*HZ)/1024.0;
		(*drawproc)(cbp, tcps_inb, tcps_outb, 0.0);
	}
	barinfo->tcps_inb = ntcpstat->tcps_rcvbyte;
	barinfo->tcps_outb = ntcpstat->tcps_sndbyte;
	barinfo->nticks = nticks;
}

void
drawnetipbar(colorblock_t *cbp)
{
   struct bar_s {
	struct ifreq	sifreq[MAXIP/sizeof(struct ifreq)];
	long	nticks;
   } *barinfo;
   struct ifreq		*ifr;
   struct ifreq		*ifo;
   int			i;
# define ifdiff(x,i)	(ifr[i].ifr_stats.ifs_ ##x- \
				ifo[i].ifr_stats.ifs_ ##x)

	dbsetup();
	ifr = nifreq;
	ifo = barinfo->sifreq;
	if (cbp->cb_subtype == IP_SUM) {
	   struct ifstats	ift;

		ift.ifs_ipackets = 0;
		ift.ifs_opackets = 0;
		ift.ifs_ierrors = 0;
		ift.ifs_oerrors = 0;
		ift.ifs_collisions = 0;
		for (i = 0; i < nif; i++) {
			ift.ifs_ipackets += ifdiff(ipackets, i);
			ift.ifs_opackets += ifdiff(opackets, i);
			ift.ifs_ierrors += ifdiff(ierrors, i);
			ift.ifs_oerrors += ifdiff(oerrors, i);
			ift.ifs_collisions += ifdiff(collisions, i);
		}
		if (cbp->cb_type == T_NUM)
			(*drawproc)(cbp, (double) ift.ifs_ipackets,
				(double) ift.ifs_opackets,
				(double) ift.ifs_ierrors,
				(double) ift.ifs_oerrors,
				(double) ift.ifs_collisions,
				0.0);
		else
			(*drawproc)(cbp, tosec(ift.ifs_ipackets),
				tosec(ift.ifs_opackets), 0.0);
	}
	else {
		i = cbp->cb_subtype;
		if (cbp->cb_type == T_NUM)
			(*drawproc)(cbp, 
				(double) ifdiff(ipackets, i),
				(double) ifdiff(opackets, i),
				(double) ifdiff(ierrors, i),
				(double) ifdiff(oerrors, i),
				(double) ifdiff(collisions, i),
				0.0);
		else
			(*drawproc)(cbp, tosec(ifdiff(ipackets, i)),
			      tosec(ifdiff(opackets, i)), 0.0);
	}
	memcpy(barinfo->sifreq, nifreq, nifsize);
	barinfo->nticks = nticks;
}

void
drawintrbar(colorblock_t *cbp)
{
   struct bar_s {
	double	vmer;
	double	other;
	long	nticks;
   } *barinfo;
   double	vmer;
   double	intr;
   double	other;
   int		i;

	dbsetup();
	vmer = 0;
	intr = 0;
	for (i = 0; i < ncpu; i++) {
		vmer += nsp[i]->vmeintr_svcd;
		intr += nsp[i]->intr_svcd;
	}
	other = intr - vmer;
	if (cbp->cb_type == T_NUM)
		(*drawproc)(cbp, relval(other), relval(vmer), 0.0);
	else
		(*drawproc)(cbp,calcavg(other), calcavg(vmer), 0.0);
	barinfo->other = other;
	barinfo->vmer = vmer;
	barinfo->nticks = nticks;
}

void
drawfaultbar(colorblock_t *cbp)
{
   struct bar_s {
	double	demand;
	double	swap;
	double	cache;
	double	file;
	double	cw;
	double	steal;
	double	tfault;
	double	rfault;
	long	nticks;
   } *barinfo;

	dbsetup();
	if (cbp->cb_type == T_NUM)
		(*drawproc)(cbp, mrelval(cw), mrelval(steal),
			mrelval(demand), mrelval(cache), mrelval(file),
			mrelval(swap), mrelval(tfault), mrelval(rfault), 0.0);
	else
		(*drawproc)(cbp, mcalcavg(cw), mcalcavg(steal),
			mcalcavg(demand), mcalcavg(cache), mcalcavg(file),
			mcalcavg(swap), mcalcavg(tfault), mcalcavg(rfault),
			0.0);
	barinfo->cw = nmp->cw;
	barinfo->steal = nmp->steal;
	barinfo->demand = nmp->demand;
	barinfo->swap = nmp->swap;
	barinfo->cache = nmp->cache;
	barinfo->file = nmp->file;
	barinfo->tfault = nmp->tfault;
	barinfo->rfault = nmp->rfault;
	barinfo->nticks = nticks;
}

void
drawswapbar(colorblock_t *cbp)
{
   struct bar_s {
	double	bswapin;
	double	bswapout;
	long	nticks;
   } *barinfo;
   int		i;
   double	bswapin = 0;
   double	bswapout = 0;

	dbsetup();
	for (i = 0; i < ncpu; i++) {
		bswapin += nsp[i]->bswapin;
		bswapout += nsp[i]->bswapout;
	}
	bswapin = (bswapin * NBPSCTR) / getpagesize();
	bswapout = (bswapout * NBPSCTR) / getpagesize();
	if (cbp->cb_type == T_NUM)
		(*drawproc)(cbp, relval(bswapin),
			relval(bswapout), 0.0);
	else
		(*drawproc)(cbp, calcavg(bswapin),
			calcavg(bswapout), 0.0);
	barinfo->bswapin = bswapin;
	barinfo->bswapout = bswapout;
	barinfo->nticks = nticks;
}

void
drawtlbbar(colorblock_t *cbp)
{
   struct bar_s {
	double	tdirt;
	double	twrap;
	double	tphys;
	double	tlbsync;
	double	tlbflush;
	double	tvirt;
	double	tlbpids;
	long	nticks;
   } *barinfo;

	dbsetup();
	(*drawproc)(cbp, 
		mrelval(tvirt),
		mrelval(tlbsync),
		mrelval(tlbflush),
		mrelval(twrap),
		mrelval(tlbpids),
		mrelval(tdirt),
		mrelval(tphys),
		0.0);
	barinfo->tdirt = nmp->tdirt;
	barinfo->twrap = nmp->twrap;
	barinfo->tphys = nmp->tphys;
	barinfo->tlbsync = nmp->tlbsync;
	barinfo->tlbflush = nmp->tlbflush;
	barinfo->tvirt = nmp->tvirt;
	barinfo->tlbpids = nmp->tlbpids;
	barinfo->nticks = nticks;
}

static void
remcbdraw(colorblock_t *cb, ...)
{
   va_list	ap;
   int		i;

	va_start(ap, cb);
	printf("=%d %d", S_DRAW, cb->cb_rb);

	for (i = 0; i < rembar->cb_nsects; i++)
		printf(" %.3f", va_arg(ap, double));
	printf("\n");
	fflush(stdout);

	va_end(ap);
}

void
logcbdraw(colorblock_t *cb, ...)
{
   va_list	ap;
   int		i;

	if (!logfd) return;

	va_start(ap, cb);
	fprintf(logfd, "=%d %d", S_DRAW, cbindex(cb));
	for (i = 0; i < cb->cb_nsects; i++)
		fprintf(logfd, " %.3f", va_arg(ap, double));
	fprintf(logfd, "\n");

	va_end(args);
}

static void
remcbdrawlegend(colorblock_t *cb)
{
	printf("=%d %#x\n", S_DRAWLEGEND, cb->cb_rb);
	fflush(stdout);
}

static void
remcbredraw(void)
{
	printf("=%d\n", S_REDRAW);
	fflush(stdout);
}

static void
remcbdrawabs(colorblock_t *cbp)
{
	printf("=%d %#x\n", S_DRAWABS, cbp->cb_rb);
	fflush(stdout);
}

static void
remcbmessage(colorblock_t *cbp, char *s)
{
	printf("=%d %#x\n", S_MESSAGE, cbp->cb_rb);
	printf("%s\n", s);
	fflush(stdout);
}

static void
remcblayout(char *s)
{
	if (s != 0 && *s != '\0') {
		printf("=%d\n", S_LAYOUT);
		printf("%s\n", s);
	}
	else
		printf("=%d\n", S_NSLAYOUT);
	fflush(stdout);
}

void
logcblayout(char *s)
{
	if (!logfd) return;
	if (s != 0 && *s != '\0') {
		fprintf(logfd, "=%d\n", S_LAYOUT);
		fprintf(logfd, "%s\n", s);
	}
	else
		fprintf(logfd, "=%d\n", S_NSLAYOUT);
}

static void
remcbtakedown(void)
{
	printf("=%d\n", S_TAKEDOWN);
	fflush(stdout);
}

static colorblock_t *
remcbinit(void)
{
   static int	vbars = 0;
   colorblock_t	*cbp;

	cbp = cbinit();
	if (cscript)
		cbp->cb_rb = vbars++;
	else {
		printf("=%d\n", S_INIT);
		fflush(stdout);
		cbp->cb_rb = rgetcb();
		if (debug)
			fprintf(stderr, "remcbinit: block at %d\n", cbp->cb_rb);
	}
	return(cbp);
}

static void
remmodtlimit(cb, nval)
   colorblock_t	*cb;
   double	nval;
{
	printf("=%d %d %.3f\n", S_TLIMIT, cb->cb_rb, nval);
	fflush(stdout);
}

static void
locmodtlimit(colorblock_t *cb, double nval)
{
	if (logfd)
		fprintf(logfd, "=%d %d %.3f\n", S_TLIMIT, cb->cb_rb, nval);
	cb->cb_tlimit = nval;
}

void
drawbarreset(void)
{
   colorblock_t		*tp;

	tp = nextbar(1);
	while (tp != 0) {
		if (tp->cb_info)
			memset((void *) tp->cb_info, 0, tp->cb_infolen);
		tp = nextbar(0);
	}
}

static int
doqueue(void)
{
   char		lastline[RMTBUF];
   int		index;
   char		*bpos;

	while (fgets(lastline, RMTBUF, validscript) != NULL) {
		bpos = lastline;
		if (*bpos != '=')
			return(1);
		bpos++;
		switch (nextint(bpos)) {
		case S_DRAW:
			if ((index = nextint((char *) 0)) < 0)
				return(1);
			if (pulldraw(cbptr(index)) < 0)
				return(1);
			break;
		case S_PAUSE:
			return(0);
		case S_TLIMIT: {
		   float	tlim;

			if ((index = nextint((char *) 0)) < 0)
				return(1);
			if ((tlim = nextfloat((char *) 0)) < 0)
				return(1);
			if (debug)
				fprintf(stderr, "doqueue: tlimit %d %f\n",
					index, tlim);
			cbptr(index)->cb_tlimit = tlim;
			CBDRAWLEGEND(cbptr(index));
			if (logfd)
				fprintf(logfd, "=%d %d %.3f\n", S_TLIMIT, index,
					tlim);
			break;
		}
		case S_GO:
			break;
		default:
			fprintf(stderr, "%s: error in export file\n", pname);
			return(1);
		}
	}
	return(1);
}

static jmp_buf 	sigbuf;

void
runit(void)
{
   drfunc_t	func;
   sigaction_t	sa;
   sigset_t	sset;

	initinfo();
	setinfo();
	drawproc = CBDRAW;
	if (slave)
		remotedraw();
	if (!validscript) {
		setinitproc();
		reinitbars();
	}
	else
		reinitbars();
	if (slave)
		lockmem(1);
	else if (remotehost != 0)
		lockmem(2);
	else
		lockmem(0);

	/*
	 * Now sleep and handle events as appropriate.
	 */
	if (slave) {
		pid_t pid;

		if (logfd)
			fflush(logfd);
		if ((pid = fork()) > 0)
			exit(0);
		else if (pid < 0) {
			perror("fork");
			exit(1);
		}
	}
	if (slave && !cscript)
		waitstart();

	sa.sa_flags = 0;
	sa.sa_handler = kicked;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGALRM, &sa, NULL);
	sigemptyset(&sset);

	for(;;) {
		if (setjmp(sigbuf))
			goto scan;
		startpoker(interval);
		for (;;) {
			if (!slave) {
				if (event_scan(0)) {
					if (logfd)
						fclose(logfd);
					if (scriptfd)
						fclose(scriptfd);
					exit(1);
				}
			} else
				/* wait for any signal */
				sigsuspend(&sset);
		}
	scan:
		sighold(SIGALRM);
		if (!validscript) {
		   colorblock_t		*tp;

			(*getdata)();
			if (ticks != 0) {
				tp = nextbar(1);
				while (tp != 0) {
					if (--tp->cb_count <= 0) {
						func=getdrawfunc(tp->cb_btype);
						(*func)(tp);
						tp->cb_count =
							tp->cb_interval;
					}
					tp = nextbar(0);
				}
			}
		}
		else {
			if (doqueue() && sexit) exit(1);
		}
		if (logfd)
			fprintf(logfd, "=%d\n", S_PAUSE);
		if (slave)
			printf("=%d\n", S_PAUSE);
	}
	/*NOTREACHED*/
}

static int qfd = -2;
static int maxfd;
static fd_set infds;
int nunknown;
#define MAXEVENTS 1000

int
event_scan(int dontblock)
{
   short	val;
   int		type;
   int		rv;
   int		nevents;
   int		iconified;
   int		nredraws = 0;

	sighold(SIGALRM);
	while (!qtest()) {
		/*
		 * No input events queued now, either return immediately
		 * or suspend.
		 */
		if (dontblock) {
			sigrelse(SIGALRM);
			return(0);
		} else {
			/*
			 * Get file descriptor for gfx input queue and
			 * init parameters for select if necessary.
			 */
			if (qfd == -2) {
				if ((qfd = qgetfd()) < 0) {
					perror("qgetfd failed");
					exit(1);
				}
				maxfd = qfd + 1;
				FD_ZERO(&infds);
				FD_SET(qfd, &infds);
			}
			/*
			 * Suspend until we get a SIGALRM or an input event.
			 * Note that in the SIGALRM case, the signal handler
			 * longjmps back to a higher call level.
			 */
			sigrelse(SIGALRM);
			rv = select(maxfd, &infds, NULL, NULL, NULL);
			sighold(SIGALRM);
			if ((rv == 0) || (rv < 0 && errno != EINTR)) {
				perror("select");
				exit(1);
			}
			if (rv > 0)
				break;
		}
	}

	/*
	 * Process all input that's queued, SIGALRM is blocked from above
	 * to prevent
	 * longjmping out of qread after it's retrieved a real event.
	 */
	nevents = 0;
	iconified = 0;
	while (qtest() || iconified) {
		/* XXX a hack to be sure we don;t for some reason
		 * go into an infinite loop - since we run at
		 * non-degrading pri that can be bad
		 */
		if (!iconified && ++nevents > MAXEVENTS) {
			fprintf(stderr, "gr_osview: too many queued events!\n");
			abort();
		}
		type = qread(&val);
		switch (type) {
		case LEFTMOUSE:
			mouse(val);
			break;
		case REDRAW:
			nredraws++;
			if (debug)
				fprintf(stderr, "event_scan: redraw\n");
			break;
		case WINTHAW:
			if (debug)
				fprintf(stderr, "event_scan: thaw\n");
			iconified = 0;
			break;
		case WINFREEZE:
			if (debug)
				fprintf(stderr, "event_scan: freeze\n");
			iconified = 1;
			break;
		case INPUTCHANGE:
			break;
		case ESCKEY:
			if (val != 0)
				break;
			/* quit on UP=0 transition only - FALL THROUGH */
		case WINQUIT:
		case WINSHUT:
			CBTAKEDOWN();
			exit(0);
		default:
			/*
			 * What the hell?
			 */
			nunknown++;
			break;
		}
	}
	/* don't do multiple redraws */
	if (nredraws) {
		CBLAYOUT(0);
		CBREDRAW();
	}
	sigrelse(SIGALRM);
	return(0);
}

static void
kicked(void)
{
	longjmp(sigbuf, 1);
}

void
drawbar_text_end(void) {}	/* end of text to lock */

static void
startpoker(int tenths)
{
   struct itimerval	itv;
   int			usecs;
   int			secs;

	secs = tenths / 10;
	usecs = 1000000 / 10 * (tenths % 10);
	itv.it_value.tv_sec = secs;
	itv.it_value.tv_usec = usecs;
	itv.it_interval.tv_sec = 0;
	itv.it_interval.tv_usec = 0;
	setitimer(ITIMER_REAL, &itv, (struct itimerval *) 0);
}

/*
 * Change the drawing tables to indicate the remote routines.
 */
static void
remotedraw(void)
{
	drawproc = remcbdraw;
	modtlimit = remmodtlimit;
	CBREDRAW = remcbredraw;
	CBINIT = remcbinit;
	CBDRAWLEGEND = remcbdrawlegend;
	CBDRAW = remcbdraw;
	CBDRAWABS = remcbdrawabs;
	CBMESSAGE = remcbmessage;
	CBLAYOUT = remcblayout;
	CBTAKEDOWN = remcbtakedown;
	setvbuf(stdout, NULL, _IOFBF, BUFSIZ);
	if (cscript) {
		printf(EXPORTSTR);
		printf("\n");
	}
}
