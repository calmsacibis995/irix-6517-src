/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Id: asidbg.c,v 1.40 1999/10/22 12:54:43 steiner Exp $"

#include "sgidefs.h"
#include "sys/types.h"
#include <os/vm/anon_mgr.h>
#include "ksys/as.h"
#include "as_private.h"
#include "sys/idbgentry.h"
#include "sys/numa.h"
#include "sys/param.h"
#include "ksys/pid.h"
#include "pmap.h"
#include "sys/proc.h"
#include "os/proc/pproc_private.h"
#include "sys/systm.h"
#include "sys/vnode_private.h" /* VKEY */
#ifdef NUMA_BASE
extern void preg_pm_print(struct pm *);
#endif

static void idbg_pas(__psint_t x, __psint_t a2, int argc, char **argv);
static void idbg_vas(__psint_t x, __psint_t a2, int argc, char **argv);
static void idbg_vaslist(__psint_t);
static void idbg_pregion(__psint_t);
static void idbg_doregion(reg_t *);
static void idbg_doattrs(attr_t *, int);
static void idbg_psize(__psint_t);
static void idbg_utas(__psint_t);
static void idbg_pfnfind(__psint_t);
static void idbg_pmap(__psint_t);
static void idbg_pregpde(__psint_t, int);
static void idbg_vtop(__psint_t, __psint_t a2, int argc, char **argv);
static void idbg_anonfind(__psint_t);
static void prpas(pas_t *pas, int flags);
static void prpregion(preg_t *prp, int flags);
static pgcnt_t cntlocked(preg_t *prp);
static void forall_vas(void (*f)(vas_t *, int), int flags);

extern kqueue_t *as_list;
extern int as_listsz;
/* this is not really legal except for debug code */
#define ASID_TO_PASID(asid)	((ppas_t *)((asid).as_pasid))

/* verbosity flag values */
#define PRPAS		0x01
#define PRPREGION	0x02
#define PRREGION	0x04
#define PRPTES          0x08
#define PRMLD           0x10

void
asidbg_init(void)
{
	idbg_addfunc("vas", idbg_vas);
	idbg_addfunc("pas", idbg_pas);
	idbg_addfunc("vaslist", idbg_vaslist);
	idbg_addfunc("region", idbg_region);
	idbg_addfunc("pregpde", idbg_pregpde);
	idbg_addfunc("vtop", idbg_vtop);
	idbg_addfunc("pregion", idbg_pregion);
	idbg_addfunc("psize", idbg_psize);
	idbg_addfunc("pmap", idbg_pmap);
	idbg_addfunc("utas", idbg_utas);
	idbg_addfunc("pfnfind", idbg_pfnfind);
	idbg_addfunc("anonfind", idbg_anonfind);
}

static int
vastopid(struct proc *p, void *arg, int ctl)
{
	uthread_t *ut;

	asid_t asid;
	if (ctl == 1) {
		for (ut = p->p_proxy.prxy_threads; ut; ut = ut->ut_next) {
			asid = ut->ut_asid;
			if (ASID_TO_VAS(asid) == (vas_t *)arg) {
				qprintf(" %d", p->p_pid);
				break;
			}
		}
	}
	return 0;
}

static void
prvas(vas_t *vas, int flags)
{
	bhv_desc_t *bdp;

	qprintf("vas @0x%x ref %d gen %lld def %d next 0x%x snext 0x%x\n",
		vas, vas->vas_refcnt, vas->vas_gen, vas->vas_defunct,
		vas->vas_queue.kq_next, vas->vas_squeue.kq_next);
	bdp = BHV_HEAD_FIRST(&vas->vas_bhvh);
	qprintf(" first bhv 0x%x pdata 0x%x ops 0x%x vobj 0x%x\n",
		bdp, bdp->bd_pdata, bdp->bd_ops, bdp->bd_vobj);
	qprintf(" belongs to proc(s)");
	idbg_procscan(vastopid, vas);
	qprintf("\n");
	if (!flags)
		return;
	if ((asvo_ops_t *)bdp->bd_ops == &pas_ops)
		prpas(bdp->bd_pdata, flags);
}

/* ARGSUSED */
static void
idbg_vas(__psint_t x, __psint_t a2, int argc, char **argv)
{
	vas_t *vas;
	proc_t *pp;
	int flags;

	if (x == -1L) {
		if (curuthread)
			vas = ASID_TO_VAS(curuthread->ut_asid);
	} else if (x < 0) {
		vas = (vas_t *)x;
	} else {
		if ((pp = idbg_pid_to_proc(x)) == NULL) {
			qprintf("%d is not an active pid\n", (pid_t)x);
			return;
		}
		vas = ASID_TO_VAS(pp->p_proxy.prxy_threads->ut_asid);
	}
	if (argc && *argv[0] == '?') {
		qprintf("Print virtual AS descriptor\n");
		qprintf("Usage:vas x [flag]\n");
		qprintf("\tx - vas address or pid\n");
		qprintf("\tverbosity 1=pas 2=pregions 4=regions 8=ptes 16=mld 31=all\n");
		return;
	}

	switch (argc) {
	case 2:
		/* 'x' is who */
		flags = (int)atoi(argv[1]);
		break;
	default:
		flags = PRPAS;
		break;
	}

	prvas(vas, flags);

}

static char *tab_ppasflags [] = {
	"shared-addr",	/* 0x0001 */
	"lockprda",	/* 0x0002 */
	"iso",		/* 0x0004 */
	"ppas-stack",	/* 0x0008 */
};

static char *tab_lockdown[] = {
	"data",		/* 0x00000001 */
	"text",		/* 0x00000002 */
	"proc",		/* 0x00000004 */
	"shm",		/* 0x00000008 */
	"future",	/* 0x00000010 */
	"UNKNOWN",	/* 0x00000020 */
	"UNKNOWN",	/* 0x00000040 */
	"UNKNOWN",	/* 0x00000080 */
};

static void
prpas(pas_t *pas, int flags)
{
	ppas_t *ppas;
	preg_t *prp;

	if (flags & PRPAS) {
		qprintf("Physical layer @0x%x plist 0x%x &aspacelock 0x%x ref %d\n",
			pas, pas->pas_plist, &pas->pas_aspacelock, pas->pas_refcnt);
		qprintf(" &preglock 0x%x &brklock @0x%x brkbase 0x%x brksize 0x%x\n",
			&pas->pas_preglock, &pas->pas_brklock,
			pas->pas_brkbase, pas->pas_brksize);
		qprintf(" lastsp 0x%x nextalloc 0x%x hiattach 0x%x tsave 0x%x\n",
			pas->pas_lastspbase, pas->pas_nextaalloc,
			pas->pas_hiusrattach, pas->pas_tsave);
		qprintf(" max data %lld max vmem %lld max stk %lld max rss %lld\n",
			pas->pas_datamax, pas->pas_vmemmax, pas->pas_stkmax,
			pas->pas_rssmax);
		qprintf(" stkbase 0x%x stksize ", pas->pas_stkbase, pas->pas_stksize);
		qprintf(" flags %s%s%s%s shaddr 0x%x ulicnt %d\n",
			pas->pas_flags & PAS_SHARED ? "shared " : "",
			pas->pas_flags & PAS_PSHARED ? "pshared " : "",
			pas->pas_flags & PAS_LOCKPRDA ? "lockprda " : "",
			pas->pas_flags & PAS_NOSWAP ? "noswap " : "",
			(pas->pas_flags & PAS_ULI) >> PAS_ULISHFT);
		printflags((unsigned)pas->pas_lockdown, tab_lockdown, " lock");
		qprintf("pmap 0x%x size %d physsize %d plistlock 0x%x\n",
			pas->pas_pmap, pas->pas_size,
			pas->pas_physsize, &pas->pas_plistlock);
		qprintf(" pregs r=0x%x f=0x%x nlockpg %d rss %d maxrss %d\n",
			pas->pas_pregions.avl_root,
			pas->pas_pregions.avl_firstino,
			pas->pas_nlockpg, pas->pas_rss, pas->pas_maxrss);
#if LARGE_CPU_COUNT
		{
		int		i;
		for (i=0; i<CPUMASK_SIZE; i++)
			qprintf("%s%x%s", i?" ":" isomask [", pas->pas_isomask._bits[i], i==CPUMASK_SIZE-1?"]\n":"");
		}
#else
		qprintf(" isomask [0x%x]\n",
			pas->pas_isomask);
#endif
        	qprintf(" aspm 0x%x\n", pas->pas_aspm);
#ifdef NUMA_BASE
		aspm_print(pas->pas_aspm);
#endif
	}

	if (flags & (PRPREGION|PRPTES)) {
		qprintf(">>>shared pregions:\n");
		for (prp = PREG_FIRST(pas->pas_pregions); prp;
							prp = PREG_NEXT(prp)) {
			if (flags & PRPREGION)
				prpregion(prp, flags);
			if (flags & PRPTES)
				idbg_pregpde((__psint_t)prp, 0);
		}
	}

	/* now print all private sections */
	for (ppas = pas->pas_plist; ppas; ppas = ppas->ppas_next) {
		qprintf("-->Private @0x%x:pregs r=0x%x f=0x%x stkbase 0x%x stksize 0x%x\n",
			ppas,
			ppas->ppas_pregions.avl_root,
			ppas->ppas_pregions.avl_firstino,
			ppas->ppas_stkbase, ppas->ppas_stksize);
		printflags((unsigned)ppas->ppas_flags, tab_ppasflags, "   flags");
		qprintf(" ref %d max stk %lld pmap 0x%x\n",
			ppas->ppas_refcnt, ppas->ppas_stkmax,
			ppas->ppas_pmap);
		qprintf("   size %d physsize %d rss %d\n",
			ppas->ppas_size, ppas->ppas_physsize, ppas->ppas_rss);
		qprintf("   utas @0x%x utaslock @0x%x\n",
				ppas->ppas_utas, &ppas->ppas_utaslock);

		if (flags & (PRPREGION|PRPTES)) {
			qprintf(">>>private pregions:\n");
			for (prp = PREG_FIRST(ppas->ppas_pregions); prp;
							prp = PREG_NEXT(prp)) {
				if (flags & PRPREGION)
					prpregion(prp, flags);
				if (flags & PRPTES)
					idbg_pregpde((__psint_t)prp, 0);
			}
		}
	}
}

/*ARGSUSED*/
static void
idbg_pas(__psint_t x, __psint_t a2, int argc, char **argv)
{
	pas_t *pas;
	int flags;

	pas = (pas_t *)x;
	if (argc && *argv[0] == '?') {
		qprintf("Print physical AS descriptor\n");
		qprintf("Usage:pas x [flag]\n");
		qprintf("\tx - pas address\n");
		qprintf("\tverbosity 1=pas 2=pregions 4=regions 8=pdes 15=all\n");
		return;
	}

	switch (argc) {
	case 2:
		/* 'x' is who */
		flags = (int)atoi(argv[1]);
		break;
	default:
		flags = PRPAS;
		break;
	}
	prpas(pas, flags);
}

/* ARGSUSED */
static void
idbg_vaslist(__psint_t x)
{
	forall_vas(prvas, 0);
}

static void
forall_vas(void (*f)(vas_t *, int), int flags)
{
	kqueue_t *kq;
	vas_t *tvas;
	int i;

	for (i = 0, kq = &as_list[i]; i < as_listsz; i++, kq++) {
		for (tvas = (vas_t *)kqueue_first(kq);
		     tvas != (vas_t *)kqueue_end(kq);
		     tvas = (vas_t *)kqueue_next(&tvas->vas_queue))
			f(tvas, flags);
	}
}

char	*tab_regflags[] = {
	"nofree",	/* 0x0001 */
	"locked",	/* 0x0002 */
	"loadv",	/* 0x0004 */
	"cw",		/* 0x0008 */
	"dfill",	/* 0x0010 */
	"autogrow",	/* 0x0020 */
	"phys",		/* 0x0040 */
	"isolated",	/* 0x0080 */
	"anon",		/* 0x0100 */
	"usync",	/* 0x0200 */
	"hassanon",	/* 0x0400 */
	"text",		/* 0x0800 */
	"autoresrv",	/* 0x1000 */
	"UNKNOWN",	/* 0x2000 */
	"physio",	/* 0x4000 */
	"mapzero", 	/* 0x8000 */
	0
};

char	*tab_regtype[] = {
	"unused",	/* 0 */
	"forrent",	/* 1 */
	"forrent",	/* 2 */
	"mem",		/* 3 */
	"mapfile",	/* 4 */
	0
};

/*
++
++	kp region arg
++		arg < 0  : prints region at address addr
++		arg >= 0 : prints region for for proc with pid <arg>
++
*/
void
idbg_region(__psint_t p1)
{
	reg_t *rp;
	preg_t *prp;
	proc_t *pp;
	uthread_t *ut;
	vas_t *vas;
	pas_t *pas;
	ppas_t *ppas;
	bhv_desc_t *bdp;

	if (p1 < 0) {
		rp = (reg_t *) p1;
		idbg_doregion(rp);
		return;
	}
	if ((pp = idbg_pid_to_proc(p1)) == NULL) {
		qprintf("%d is not an active pid\n", (pid_t)p1);
		return;
	}

	for (ut = pp->p_proxy.prxy_threads; ut; ut = ut->ut_next) {
		if (AS_ISNULL(&ut->ut_asid))
			continue;
		vas = ASID_TO_VAS(ut->ut_asid);
		bdp = BHV_HEAD_FIRST(&vas->vas_bhvh);
		if ((asvo_ops_t *)bdp->bd_ops != &pas_ops) {
			qprintf("process %d AS not local\n", (pid_t)p1);
			return;
		}
		qprintf("shared regions for proc 0x%x (pid %d) >>>\n",
			pp, p1);
		pas = BHV_TO_PAS(bdp);
		for (prp = PREG_FIRST(pas->pas_pregions);
		     prp;
		     prp = PREG_NEXT(prp))
			idbg_doregion(prp->p_reg);
		break;
	}

	if (!ut) {
		qprintf("process %d has no AS\n", (pid_t)p1);
		return;
	}

	for (ut = pp->p_proxy.prxy_threads; ut; ut = ut->ut_next) {
		if (AS_ISNULL(&ut->ut_asid))
			continue;
		qprintf("private regions for uthread 0x%x >>>\n", ut);
		ppas = ASID_TO_PASID(ut->ut_asid);
		for (prp = PREG_FIRST(ppas->ppas_pregions);
		     prp;
		     prp = PREG_NEXT(prp))
			idbg_doregion(prp->p_reg);
	}
}

static void
idbg_pregpde(__psint_t p, int flags)
{
	register pde_t	*pt;
	register reg_t	*rp;
	register preg_t	*prp;
	register int	i, bytes, pages;
	auto pgno_t	sz, totsz;
	auto char	*vaddr;

	prp = (preg_t *)p;
	rp = prp->p_reg;

	if (flags != 0)
		idbg_region((__psint_t)rp);

	/* Look at all of the mappings in the pregion */
	vaddr = prp->p_regva;
	totsz = prp->p_pglen;

	while (totsz) {
		pt = pmap_probe(prp->p_pmap, &vaddr, &totsz, &sz);
		if (pt == NULL)
			return;

		totsz -= sz;

		for (i = sz; i > 0; vaddr += bytes, pt+=pages, i-=pages) {
			bytes = NBPP;
			pages = 1;
			if (!pg_isvalid(pt))
				continue;
			qprintf("0x%x: ", vaddr);
			if (rp->r_flags & RG_PHYS)
				bytes = idbg_dopde(pt, poff(vaddr), 0);
			else
				bytes = idbg_dopde(pt, poff(vaddr), DUMP_PFDAT);
			pages = bytes/NBPP;
		}
	}
}

static void
idbg_doregion(reg_t *rp)
{
	qprintf("region: addr 0x%x, ", rp);
	qprintf("type %s, flags: ", tab_regtype[rp->r_type]);
	printflags((unsigned)rp->r_flags, tab_regflags, 0);
	qprintf("\n");
	qprintf("     pgsz 0x%x fileoff %llx", rp->r_pgsz, rp->r_fileoff);
	qprintf(" maxfilesz %llx\n", rp->r_maxfsize);
	qprintf("     refcnt %d nofree %d",
		rp->r_refcnt, rp->r_nofree);
	if (rp->r_flags & RG_PHYS)
		qprintf(" fault 0x%x fltarg 0x%x\n", rp->r_fault, rp->r_fltarg);
	else
		qprintf(" v/gen 0x%x\n", rp->r_gen);
	qprintf(
	"     &mrlock 0x%x mappedsize %d color 0x%x\n",
		&rp->r_lock, rp->r_mappedsize, rp->r_color);
	if (rp->r_vnode)
		qprintf(
		"     vnode 0x%x (key 0x%x count %d) anon 0x%x\n",
			rp->r_vnode, VKEY(rp->r_vnode), rp->r_vnode->v_count, rp->r_anon);
	else
		qprintf("     vnode 0x%x anon 0x%x\n", rp->r_vnode,rp->r_anon);
#ifdef CKPT
	qprintf("     ckpt 0x%x:%d\n", rp->r_ckptflags, rp->r_ckptinfo);
#endif
}

char	*tab_pregflags[] = {
	"dup",		/* 0x0001 */
	"noptres",	/* 0x0002 */
	"noshare",	/* 0x0004 */
	"shared",	/* 0x0008 */
	"auditr",	/* 0x0010 */
	"auditw",	/* 0x0020 */
	"faulttrc",	/* 0x0040 */
	"tsave",	/* 0x0080 */
	"private",	/* 0x0100 */
	"primary",	/* 0x0200 */
	"txtpvt",	/* 0x0400 */
	"lcknprog",	/* 0x0800 */
	0
};

char	*tab_pregtype[] = {
	"unused",	/* 0 */
	"text",		/* 1 */
	"data",		/* 2 */
	"stack",	/* 3 */
	"shmem",	/* 4 */
	"mem",		/* 5 */
	"libtxt",	/* 6 */
	"libdat",	/* 7 */
	"graphics",	/* 8 */
	"mappedfile",	/* 9 */
	"prda",		/*10 */
	0
};

char	*tab_pregprots[] = {
	"rd",		/* 0x01 */
	"wr",		/* 0x02 */
	"ex",		/* 0x04 */
};

static void
idbg_doattrs(register attr_t *attr, int flags)
{
	do {
		qprintf("  attr @ 0x%x start 0x%x end 0x%x",
				attr, attr->attr_start, attr->attr_end);
		qprintf(" perm: ");
		printflags((unsigned)attr->attr_prot, tab_pregprots, 0);
		qprintf("; cc %d", attr->attr_cc);
		if (attr->attr_watchpt)
			qprintf("; watched");
		if (attr->attr_lockcnt)
			qprintf("; %d locks", attr->attr_lockcnt);
                qprintf("; pm 0x%x", attr->attr_pm);
                qprintf("\n");    
#ifdef NUMA_BASE
		if (flags&PRMLD)
			preg_pm_print(attr->attr_pm);
#endif
	} while (attr = attr->attr_next);
}

static void
prpregion(preg_t *prp, int flags)
{
	pgcnt_t nlck;

	qprintf("pregion: addr 0x%x region 0x%x p_nextino 0x%x\n",
		prp, prp->p_reg, PREG_NEXT(prp));
	qprintf("     type: %s ", tab_pregtype[prp->p_type]);

	printflags((unsigned)prp->p_maxprots, tab_pregprots, "maxprots");
	printflags((unsigned)prp->p_flags, tab_pregflags, "flags");
	qprintf("\n");

	nlck = cntlocked(prp);
	qprintf("     pmap 0x%x regva 0x%x offset 0x%x len 0x%x nvalid 0x%x\n",
			prp->p_pmap, prp->p_regva, prp->p_offset, prp->p_pglen,
			prp->p_nvalid);
	qprintf("     pghnd 0x%x locked 0x%x vchain 0x%x attrhint -> 0x%x\n",
			prp->p_pghnd, nlck, prp->p_vchain, prp->p_attrhint);
	qprintf("     noshrink 0x%x\n",
			prp->p_noshrink);

	if (prp->p_type == PT_STACK)
		qprintf("     p_stk_rglen 0x%x\n", prp->p_stk_rglen);
	
	idbg_doattrs(&prp->p_attrs, flags);
	if (!(flags & PRREGION))
		return;
	if (prp->p_reg) {
		qprintf("   ");
		idbg_doregion(prp->p_reg);
	}
}

static pgcnt_t
cntlocked(preg_t *prp)
{
	reg_t *rp = prp->p_reg;
	pgcnt_t nlck = 0;
	pde_t *pt;
	pfd_t *pfd;
	int i;
	auto pgno_t sz, totsz;
	auto uvaddr_t vaddr;

	if (prp->p_nvalid == 0 || rp->r_flags & RG_PHYS)
		return(0);

	vaddr = prp->p_regva;
	totsz = prp->p_pglen;

	while (totsz) {
		pt = pmap_probe(prp->p_pmap, &vaddr, &totsz, &sz);
		if (pt == NULL)
			return nlck;

		totsz -= sz;

		for (i = sz; i > 0; vaddr += NBPP, pt++, i--) {
			if (!pg_isvalid(pt))
				continue;
			pfd = pdetopfdat(pt);
			if (pfd->pf_rawcnt)
				nlck++;
		}
	}

	return(nlck);
}

/*
++
++	kp pregion arg
++		arg == -1 : prints pregion structure for curproc
++		arg < 0  : prints pregion structure at addr <arg>
++		arg >= 0 : prints pregion structure for proc with pid <arg>
++
*/
void
idbg_pregion(__psint_t p1)
{
	register preg_t *prp;
	vas_t *vas;
	pas_t *pas;
	ppas_t *ppas;
	bhv_desc_t *bdp;
	proc_t *pp;
	uthread_t *ut;

	if (p1 < 0 && p1 != -1L) {
		prp = (preg_t *) p1;

		prpregion(prp, PRREGION);
		return;
	}

	if (p1 == -1L) {
		if ((pp = curprocp) == NULL) {
			qprintf("no current process\n");
			return;
		}
		p1 = current_pid();
	} else if ((pp = idbg_pid_to_proc(p1)) == NULL) {
		qprintf("%d is not an active pid\n", (pid_t)p1);
		return;
	}

	for (ut = pp->p_proxy.prxy_threads; ut; ut = ut->ut_next) {
		if (AS_ISNULL(&ut->ut_asid))
			continue;
		vas = ASID_TO_VAS(ut->ut_asid);
		bdp = BHV_HEAD_FIRST(&vas->vas_bhvh);
		if ((asvo_ops_t *)bdp->bd_ops != &pas_ops) {
			qprintf("process %d AS not local\n", (pid_t)p1);
			return;
		}
		qprintf("shared pregions for proc 0x%x (pid %d) >>>\n",
			pp, p1);
		pas = BHV_TO_PAS(bdp);
		for (prp = PREG_FIRST(pas->pas_pregions);
		     prp;
		     prp = PREG_NEXT(prp))
			prpregion(prp, PRREGION);
		break;
	}

	if (!ut) {
		qprintf("process %d has no AS\n", (pid_t)p1);
		return;
	}

	for (ut = pp->p_proxy.prxy_threads; ut; ut = ut->ut_next) {
		if (AS_ISNULL(&ut->ut_asid))
			continue;
		qprintf("private pregions for uthread 0x%x >>>\n", ut);
		ppas = ASID_TO_PASID(ut->ut_asid);
		for (prp = PREG_FIRST(ppas->ppas_pregions);
		     prp;
		     prp = PREG_NEXT(prp))
			prpregion(prp, PRREGION);
	}
}

/* ARGSUSED */
static void
prpassizes(pas_t *pas, int flags, pgcnt_t *n, pgcnt_t *nval, pgcnt_t *nlck)
{
	preg_t *prp;
	ppas_t *ppas;

	for (prp = PREG_FIRST(pas->pas_pregions); prp; prp = PREG_NEXT(prp)) {
		*n += prp->p_pglen;
		*nval += prp->p_nvalid;
		*nlck += cntlocked(prp);
	}
	for (ppas = pas->pas_plist; ppas; ppas = ppas->ppas_next) {
		for (prp = PREG_FIRST(ppas->ppas_pregions); prp;
						prp = PREG_NEXT(prp)) {
			*n += prp->p_pglen;
			*nval += prp->p_nvalid;
			*nlck += cntlocked(prp);
		}
	}
}

static void
prsizes(vas_t *vas, int flags)
{
	bhv_desc_t *bdp;
	pas_t *pas;
	pgcnt_t n = 0, nval = 0, nlck = 0;

	bdp = BHV_HEAD_FIRST(&vas->vas_bhvh);
	if ((asvo_ops_t *)bdp->bd_ops == &pas_ops) {
		pas = bdp->bd_pdata;
		prpassizes(pas, flags, &n, &nval, &nlck);
		qprintf("vas @0x%x ref %d sz %d valid %d lck %d melck %d\n",
			vas, vas->vas_refcnt,
			n, nval, nlck, pas->pas_nlockpg);
	}
}

static pgcnt_t
dopsize(preg_t *prp, pgcnt_t *nval)
{
	qprintf("pregion: begin 0x%x end 0x%x size %d nval %d\n",
		prp->p_regva, prp->p_regva + ctob(prp->p_pglen),
		prp->p_pglen, prp->p_nvalid);
	*nval += prp->p_nvalid;
	return(prp->p_pglen);
}

/*
++
++	kp psize arg
++		arg == -1  : prints pregion sizes for curproc
++		arg < -1  : prints sizes for all ASs
++		arg >= 0 : prints pregion sizes for proc with pid <arg>
++
*/
void
idbg_psize(__psint_t p1)
{
	register preg_t *prp;
	register pgno_t	psize = 0, ssize = 0;
	vas_t *vas;
	pas_t *pas;
	ppas_t *ppas;
	bhv_desc_t *bdp;
	pgcnt_t nval = 0;

	if (p1 < 0 && p1 != -1L) {
		forall_vas(prsizes, 0);
	} else {
		proc_t *pp;

		if (p1 == -1L) {
			if ((pp = curprocp) == NULL) {
				qprintf("no current process\n");
				return;
			}
		} else if ((pp = idbg_pid_to_proc(p1)) == NULL) {
			qprintf("%d is not an active pid\n", (pid_t)p1);
			return;
		}
		if (AS_ISNULL(&pp->p_proxy.prxy_threads->ut_asid)) {
			qprintf("process %d has no AS\n", (pid_t)p1);
			return;
		}
		vas = ASID_TO_VAS(pp->p_proxy.prxy_threads->ut_asid);
		bdp = BHV_HEAD_FIRST(&vas->vas_bhvh);
		if ((asvo_ops_t *)bdp->bd_ops != &pas_ops) {
			qprintf("process %d AS not local\n", (pid_t)p1);
			return;
		}
		pas = BHV_TO_PAS(bdp);
		ppas = ASID_TO_PASID(pp->p_proxy.prxy_threads->ut_asid);

		qprintf("pregion sizes for proc 0x%x (pid %d) >>>\n", pp, p1);
		psize = 0;
		for (prp = PREG_FIRST(ppas->ppas_pregions); prp;
						prp = PREG_NEXT(prp))
			psize += dopsize(prp, &nval);
		
		qprintf("shared pregion sizes for proc 0x%x (pid %d) >>>\n",
						pp, p1);
		ssize = 0;
		for (prp = PREG_FIRST(pas->pas_pregions); prp;
						prp = PREG_NEXT(prp))
			ssize += dopsize(prp, &nval);
		qprintf("private size = %d shared = %d total = %d total valid %d\n",
			psize, ssize, psize+ssize, nval);
	}
}


static void
idbg_chkpreg(uthread_t *ut, preg_t *prp, pgno_t pfn)
{
	register pde_t	*pt;
	register reg_t	*rp;
	register int	i;
	auto pgno_t	sz, totsz;
	auto char	*vaddr;

	rp = prp->p_reg;

	/* Look at all of the mappings in the pregion */
	vaddr = prp->p_regva;
	totsz = prp->p_pglen;

	while (totsz) {
		pt = pmap_probe(prp->p_pmap, &vaddr, &totsz, &sz);
		if (pt == NULL)
			return;

		totsz -= sz;

		for (i = sz; i > 0; vaddr += NBPP, pt++, i--) {
			if (!pg_isvalid(pt) || pdetopfn(pt) != pfn)
				continue;
			qprintf("uthread 0x%x 0x%x: ", ut, vaddr);
			if (rp->r_flags & RG_PHYS)
				idbg_dopde(pt, poff(vaddr), 0);
			else
				idbg_dopde(pt, poff(vaddr), DUMP_PFDAT);
		}
	}
}

/*
 * helper function for anonfind
 */
static void
idbg_asanonfind(proc_t *pp, anon_hdl tag)
{
	vas_t *vas;
	pas_t *pas;
	ppas_t *ppas;
	bhv_desc_t *bdp;
	preg_t *prp;

	if (AS_ISNULL(&pp->p_proxy.prxy_threads->ut_asid)) {
		qprintf("process %d has no AS\n", pp->p_pid);
		return;
	}
	vas = ASID_TO_VAS(pp->p_proxy.prxy_threads->ut_asid);
	bdp = BHV_HEAD_FIRST(&vas->vas_bhvh);
	if ((asvo_ops_t *)bdp->bd_ops != &pas_ops) {
		qprintf("process 0x%x AS not local\n", pp);
		return;
	}
	pas = BHV_TO_PAS(bdp);
	ppas = ASID_TO_PASID(pp->p_proxy.prxy_threads->ut_asid);

	/*
	 * Go through all pregions
	 */
	for (prp = PREG_FIRST(ppas->ppas_pregions); prp; prp = PREG_NEXT(prp))
		if (prp->p_reg->r_anon == tag) {
			shortproc(pp, 0);
			prpregion(prp, PRREGION);
		}

	for (prp = PREG_FIRST(pas->pas_pregions); prp; prp = PREG_NEXT(prp))
		if (prp->p_reg->r_anon == (anon_hdl)tag) {
			shortproc(pp, 0);
			prpregion(prp, PRREGION);
		}
}

static int
print_procanon(
	proc_t	*p,
	void	*arg,
	int	ctl)
{
	anon_hdl	tag;

	if (ctl == 1) {
		tag = (anon_hdl)arg;
		idbg_asanonfind(p, tag);
	}
	return(0);
}
		
/*
++
++	kp anonfind anon_tab
++
++	Find all processes which currently have references to given anon tag.
++
*/
static void
idbg_anonfind(__psint_t tag)
{
	/*
	 *	Go through all active processes
	 */
	idbg_procscan(print_procanon, (void *)tag);
}

/*
 * helper function for pfnfind
 */
static void
idbg_aspfnfind(proc_t *pp, pgno_t pfn)
{
	vas_t *vas;
	pas_t *pas;
	ppas_t *ppas;
	bhv_desc_t *bdp;
	preg_t *prp;
	uthread_t *ut;

	if (pp->p_proxy.prxy_threads == NULL) {
		qprintf("process 0x%x has no threads\n", pp);
		return;
	}

	if (AS_ISNULL(&pp->p_proxy.prxy_threads->ut_asid)) {
		qprintf("process %d has no AS\n", pp->p_pid);
		return;
	}
	vas = ASID_TO_VAS(pp->p_proxy.prxy_threads->ut_asid);
	bdp = BHV_HEAD_FIRST(&vas->vas_bhvh);
	if ((asvo_ops_t *)bdp->bd_ops != &pas_ops) {
		qprintf("process 0x%x AS not local\n", pp);
		return;
	}

	/*
	 * Go through all pregions
	 */
	for (ut = pp->p_proxy.prxy_threads; ut; ut = ut->ut_next) {
		if (AS_ISNULL(&ut->ut_asid))
			continue;
		ppas = ASID_TO_PASID(ut->ut_asid);
		for (prp = PREG_FIRST(ppas->ppas_pregions);
		     prp;
		     prp = PREG_NEXT(prp))
			idbg_chkpreg(ut, prp, pfn);
	}

	pas = BHV_TO_PAS(bdp);

	for (prp = PREG_FIRST(pas->pas_pregions); prp; prp = PREG_NEXT(prp))
		idbg_chkpreg(pp->p_proxy.prxy_threads, prp, pfn);
}

static int
print_procpfn(
	proc_t	*p,
	void	*arg,
	int	ctl)
{
	pgno_t	pfn;

	if (ctl == 1) {
		pfn = (pgno_t)arg;
		idbg_aspfnfind(p, pfn);
	}
	return(0);
}
		
/*
++
++	kp pfnfind frame#
++
++	Find all processes which currently have references to given page
++	frame.
++
*/
void
idbg_pfnfind(__psint_t pfn)
{
	/*
	 *	Go through all active processes
	 */
	idbg_procscan(print_procpfn, (void *)pfn);
}

/*
 * helper function for onswap
 */
void
idbg_asonswap(proc_t *pp)
{
	vas_t *vas;
	pas_t *pas;
	ppas_t *ppas;
	bhv_desc_t *bdp;
	preg_t *prp;
	reg_t *rp;

	if (AS_ISNULL(&pp->p_proxy.prxy_threads->ut_asid)) {
		qprintf("process %d has no AS\n", pp->p_pid);
		return;
	}
	vas = ASID_TO_VAS(pp->p_proxy.prxy_threads->ut_asid);
	bdp = BHV_HEAD_FIRST(&vas->vas_bhvh);
	if ((asvo_ops_t *)bdp->bd_ops != &pas_ops) {
		qprintf("process 0x%x AS not local\n", pp);
		return;
	}
	pas = BHV_TO_PAS(bdp);
	ppas = ASID_TO_PASID(pp->p_proxy.prxy_threads->ut_asid);

	/*
	 * Go through all pregions
	 */
	for (prp = PREG_FIRST(ppas->ppas_pregions); prp; prp = PREG_NEXT(prp)) {
		rp = prp->p_reg;
		if (rp->r_anon == NULL)
			continue;
		print_aswaptree(find_anonroot(rp->r_anon), rp);
	}

	for (prp = PREG_FIRST(pas->pas_pregions); prp; prp = PREG_NEXT(prp)) {
		rp = prp->p_reg;
		if (rp->r_anon == NULL)
			continue;
		print_aswaptree(find_anonroot(rp->r_anon), rp);
	}
}

/*
 * helper function for pmfind (numa)
 */
static void
idbg_preg_checkpm(proc_t *pp, preg_t *prp, struct pm *pm)
{
        attr_t *attr;
        
        attr = &prp->p_attrs;
        while (attr) {
                if (attr->attr_pm == pm) {
                        qprintf("proc[0x%x] preg[0x%x] attr[0x%x]\n",
                                pp, prp, attr);
                }
                attr = attr->attr_next;
        }
}

void
idbg_aspmfind(proc_t *pp, struct pm *pm)
{
	vas_t *vas;
	pas_t *pas;
	ppas_t *ppas;
	bhv_desc_t *bdp;
	preg_t *prp;

	if (AS_ISNULL(&pp->p_proxy.prxy_threads->ut_asid)) {
		qprintf("process %d has no AS\n", pp->p_pid);
		return;
	}
	vas = ASID_TO_VAS(pp->p_proxy.prxy_threads->ut_asid);
	bdp = BHV_HEAD_FIRST(&vas->vas_bhvh);
	if ((asvo_ops_t *)bdp->bd_ops != &pas_ops) {
		qprintf("process 0x%x AS not local\n", pp);
		return;
	}
	pas = BHV_TO_PAS(bdp);
	ppas = ASID_TO_PASID(pp->p_proxy.prxy_threads->ut_asid);

	/*
	 * Go through all pregions
	 */
	for (prp = PREG_FIRST(ppas->ppas_pregions); prp; prp = PREG_NEXT(prp))
		idbg_preg_checkpm(pp, prp, pm);

	for (prp = PREG_FIRST(pas->pas_pregions); prp; prp = PREG_NEXT(prp))
		idbg_preg_checkpm(pp, prp, pm);
}

/* Print a second-level page table (i.e. a segment table)
 * 
 * 32bit processes have a single second level page table.  
 * 64bit processes have multiple second level tables pointed to by a
 *       third level page table.
 */

static void
idbg_print_segtbl(pde_t **pte2, uvaddr_t addrstart, uvaddr_t addrlimit)
{
	uvaddr_t vaddr;
	pde_t *pte;
	int j;
	pgcnt_t nval, nsent;

	for (vaddr = addrstart; vaddr < addrlimit; vaddr += stob(1), pte2++) {
		if (*pte2) {
			pte = *pte2;
			nval = 0;
			nsent = 0;
			for (j = NPGPT; --j >= 0; pte++) {
				if (pte->pgi == 0)
					continue;
				if (pg_getpfn(pte) <= PG_SENTRY)
					nsent++;
				else
					nval++;
			}
			qprintf("0x%x-0x%x\t0x%x %d valid %d sentinel\n",
				vaddr, vaddr + stob(1) - 1, *pte2, nval, nsent);
		}
	}
}

/* Print TRI-LEVEL segment table (64-bit process) */
static void
idbg_print_segtbl3(pmap_t *pp)
{
	pde_t ***pte;
	uvaddr_t vaddr;

	pte = (pde_t ***)pp->pmap_ptr;
	for (vaddr = 0; vaddr < (uvaddr_t)HIUSRATTACH_64; vaddr += NBPSEGTAB) {
		if (*pte)
			idbg_print_segtbl(*pte, vaddr, vaddr+NBPSEGTAB);
		pte = (pde_t***)((uvaddr_t)pte + (1<<PCOM_TSHIFT));
	}
}

char	*tab_utasflags[] = {
	"lck",			/* 0x0001 */
	"lpg-tlbmiss-swtch",	/* 0x0002 */
	"tlbmiss",		/* 0x0004 */
	"uli",			/* 0x0008 */
};

static void
idbg_do_utas(utas_t *utas)
{
	int idx;

	qprintf("Printing utas at 0x%x flags:", utas);
	printflags((unsigned)utas->utas_flag, tab_utasflags, 0);
	qprintf("\n");
	qprintf(" tlbid %lld segflags %d tlbcnt %d utlbswtch %d\n",
		utas->utas_tlbid, utas->utas_segflags,
		utas->utas_tlbcnt, utas->utas_utlbmissswtch);
	qprintf(" segtbl 0x%x segtbl_wired 0x%x"
		" shrdsegtbl 0x%x shrdsegtbl_wired 0x%x\n",
		utas->utas_segtbl, utas->utas_segtbl_wired,
		utas->utas_shrdsegtbl, utas->utas_shrdsegtbl_wired);
	qprintf(" tlbpid[s]");
	for (idx = 0; idx < maxcpus; idx++)
		qprintf(" %u", utas->utas_tlbpid[idx]);
	qprintf("\n");
#if TFP
	qprintf(" icachepid[s]");
	for (idx = 0; idx < maxcpus; idx++)
		qprintf(" %u", utas->utas_icachepid[idx]);
	qprintf("\n");
#endif
}

static void
idbg_do_uthread_utas(uthread_t *ut)
{
	qprintf("Printing utas for uthread 0x%x: ", ut);
	idbg_do_utas(&ut->ut_as);
}

/*
++
++	kp utas arg - print utas for a process
++		arg == -1 : prints utas for curproc
++		arg >= 0 : prints utas for proc with pid <arg>
++		arg < 0  : prints utas at addr <arg>
++
*/
static void
idbg_utas(__psint_t p1)
{
	if (p1 < 0 && p1 != -1L) {
		idbg_do_utas((utas_t *)p1);
		return;
	}

	(void) idbg_doto_uthread(p1, idbg_do_uthread_utas, 1, 0);
}


#ifdef notdef
#include <ksys/exception.h>

void
idbg_uthread_as_sanity(uthread_t *ut, pas_t *pas, ppas_t *ppas)
{
	tlbpde_t *pd;
	char *q, **qp;
	exception_t *up;
	unsigned long x;
	pfn_t	a, b;
	int i;
	pde_t *	pde;
	preg_t	*prp;

	if (!ppas->ppas_pmap)
		return;

	up = ut->ut_exception;
	ASSERT((__psunsigned_t)up > K0BASE);

	qp = up->u_tlbhi_tbl;
	pd = up->u_ubptbl;
	for (i = 0; i < NWIREDENTRIES-TLBWIREDBASE; i++, pd++, qp++) {
		/*
		 * Look through the wired entries for uthread ut.
		 * If the page is in the KPTESEG, figure out what
		 * segment it represents.  This is determined by
		 * lopping off KPTEBASE, then dividing by sizeof segment.
		 */
		if (!(q = *qp) || (unsigned long)q < KPTEBASE)
			continue;

		q -= KPTEBASE;
		x = btoct(q);	/* which segment? */
		x /= 2;
		x *= 2;

		if (x >= NPGTBLS) {
			qprintf(" junk calc from 0x%x, seg 0x%x\n",
				*qp, x);
			ASSERT(x < NPGTBLS);
		}

		if (pd->pde_low.pgi) {
			b = pdetopfn(&pd->pde_low);
		} else {
			ASSERT(pd->pde_hi.pgi);
			b = pdetopfn(&pd->pde_hi);
			x++;
		}

		if (x >= NPGTBLS) {
			qprintf(" 2nd-junk calc from 0x%x, seg 0x%x\n",
				*qp, x);
			ASSERT(x < NPGTBLS);
		}
		x = stob(x);

		mraccess(&pas->pas_aspacelock);
		prp = findfpreg(pas, ppas, (uvaddr_t)x, (uvaddr_t)(x+stob(1)));
		if (!prp)
			continue;

		pde = pmap_pte(prp->p_pmap, (char *)x, VM_NOSLEEP);
		ASSERT(pde);
		mrunlock(&pas->pas_aspacelock);

		a = kvtophyspnum(pde);

		if (a != b) {
			qprintf(
	" Botch on 0x%x, a 0x%x b 0x%x pregion 0x%x, pde 0x%x, pd 0x%x\n",
				*qp, a, b, prp, pde, pd);
			ASSERT(a == b);
		}
	}
}
#endif

char *tab_pmap_types[] = {
	"INACTIVE",
	"SEGMENT",
	"TRILEVEL",
	"FORWARD",
	"REVERSE"
};

static void
idbg_print_pmap(pmap_t *pp, int verbose)
{
	if (pp == NULL)
		return;
	qprintf("   pmap 0x%x type %s flags 0x%x scount 0x%x trimtime %d\n",
		pp, tab_pmap_types[pp->pmap_type], pp->pmap_flags,
		pp->pmap_scount, pp->pmap_trimtime);
	if (verbose) {
		qprintf("   segptr 0x%x bitmap 0x%x", pp->pmap_ptr, pp+1);
#if DEBUG
		qprintf(" resv %d", pp->pmap_resv);
#endif
		qprintf("\n");
	}

	if (pp->pmap_type == PMAP_SEGMENT) {
		idbg_print_segtbl(pp->pmap_ptr, (uvaddr_t)0,
						(uvaddr_t)HIUSRATTACH_32);
	} else if (pp->pmap_type == PMAP_TRILEVEL) {
		idbg_print_segtbl3(pp);
	} else
		qprintf("pmap @0x%x doesn't support segments\n", pp);

#ifdef PMAP_SEG_DEBUG
	{
		int j;
		int *k;

		pp++;
		k = (int *)pp;
		for (j = 0; j < NPGTBLS/32; j++, k++) {
			qprintf("	0x%x", *k);
			if ((j & 3) == 3)
				qprintf("\n");
		}
	}
#endif
}

/* ARGSUSED */
static void
prpmap(vas_t *vas, int arg)
{
	bhv_desc_t *bdp;
	pas_t *pas;
	ppas_t *ppas;

	bdp = BHV_HEAD_FIRST(&vas->vas_bhvh);
	if ((asvo_ops_t *)bdp->bd_ops == &pas_ops) {
		pas = bdp->bd_pdata;
		if (pas->pas_pmap) {
			qprintf("VAS 0x%x - Shared segment table\n", vas);
			idbg_print_pmap(pas->pas_pmap, 0);
		}
		for (ppas = pas->pas_plist; ppas; ppas = ppas->ppas_next) {
			if (ppas->ppas_pmap) {
				qprintf(" Private segment table\n");
				idbg_print_pmap(ppas->ppas_pmap, 0);
			}
		}
	}
}

static void
idbg_pmap(__psint_t p1)
{
	proc_t *p;
	pmap_t *pp;
	vas_t *vas;
	pas_t *pas;
	ppas_t *ppas;
	bhv_desc_t *bdp;
	uthread_t *ut;

	if (p1 == -2) {
		forall_vas(prpmap, 0);
		return;
	} else if (p1 < 0 && p1 != -1L) {
		pp = (pmap_t *)p1;
		idbg_print_pmap(pp, 1);
		return;
	}

	if (p1 == -1L) {
		if (!curprocp) {
			qprintf("no current process\n");
			return;
		}
		p = curprocp;
	} else {
		if ((p = idbg_pid_to_proc(p1)) == NULL) {
			qprintf("%d is not an active pid\n", (pid_t)p1);
			return;
		}
	}

	for (ut = p->p_proxy.prxy_threads; ut; ut = ut->ut_next) {
		if (AS_ISNULL(&ut->ut_asid))
			continue;
		vas = ASID_TO_VAS(ut->ut_asid);
		bdp = BHV_HEAD_FIRST(&vas->vas_bhvh);
		if ((asvo_ops_t *)bdp->bd_ops != &pas_ops) {
			qprintf("process %d AS not local\n", (pid_t)p1);
			return;
		}
		qprintf("segment table[s] for proc 0x%x (pid %d) >>>\n",
			p, p1);
		pas = BHV_TO_PAS(bdp);
		if (pas->pas_pmap) {
			qprintf("Shared segment table\n");
			idbg_print_pmap(pas->pas_pmap, 1);
		}
		break;
	}

	if (!ut) {
		qprintf("process %d has no AS\n", (pid_t)p1);
		return;
	}

	for (ut = p->p_proxy.prxy_threads; ut; ut = ut->ut_next) {
		if (AS_ISNULL(&ut->ut_asid))
			continue;
		ppas = ASID_TO_PASID(ut->ut_asid);
		if (ppas->ppas_pmap) {
			qprintf("Private segment table for uthread 0x%x\n",
				ut);
			idbg_print_pmap(ppas->ppas_pmap, 1);
		} else
			qprintf("No private segment table for uthread 0x%x\n",
				ut);
	}
}

/*
 * Given a process and virtual address, get the physical page it maps to.
 */
/* ARGSUSED */
void
idbg_vtop(__psint_t arg1, __psint_t a2, int argc, char **argv)
{
	proc_t  *p;
	pde_t   *pt;
	int	i;
	preg_t *prp;
	vas_t *vas;
	pas_t *pas;
	ppas_t *ppas;
	bhv_desc_t *bdp;

	if (arg1 < 0 ) {
		if (arg1 == -1) {
			if (curprocp) 
				p = curprocp;
			else {
				qprintf("No current process\n");
				return;
			}
		} else {
			p = (proc_t *)arg1;
		}
	} else {
		p = idbg_pid_to_proc(arg1);
		if (!p) {
			qprintf("pid %d(0x%x) invalid pid \n", arg1, arg1);
			qprintf("Usage: vtop proc|pid virt-addr\n");
			return;
		}
	}
	if (!p->p_proxy.prxy_threads ||
	    AS_ISNULL(&p->p_proxy.prxy_threads->ut_asid)) {
		qprintf("process %d has no AS\n", p->p_pid);
		return;
	}
	vas = ASID_TO_VAS(p->p_proxy.prxy_threads->ut_asid);
	bdp = BHV_HEAD_FIRST(&vas->vas_bhvh);
	if ((asvo_ops_t *)bdp->bd_ops != &pas_ops) {
		qprintf("process 0x%x AS not local\n", p);
		return;
	}
	pas = BHV_TO_PAS(bdp);
	ppas = ASID_TO_PASID(p->p_proxy.prxy_threads->ut_asid);

	if (argc < 2) {
		qprintf("Usage: vtop proc|pid addr addr...\n");
		return;
	}
	for (i = 1; i < argc; i++) {
		void	*vaddr;
	
		vaddr = (void *)strtoull(argv[i], (char **)NULL, 16);

		if (!IS_KUSEG(vaddr)) {
			qprintf("invalid vaddr 0x%x\n", vaddr);
			continue;
		}

		if ((prp = PREG_FINDRANGE(&ppas->ppas_pregions,
					(__psunsigned_t) vaddr)) == NULL)
			prp = PREG_FINDRANGE(&pas->pas_pregions, (__psunsigned_t) vaddr);
		if (prp == NULL) {
			qprintf("vaddr 0x%x not in address space\n", vaddr);
			continue;
		}

		pt = pmap_pte(pas, prp->p_pmap, vaddr, VM_NOSLEEP);
		if (pt == NULL) {
			qprintf("vaddr 0x%x not in page table\n", vaddr);
			continue;
		}
		qprintf("proc 0x%x(%d) vaddr 0x%x pde @0x%x\n",
			p, p->p_pid, vaddr, pt);
		idbg_dopde(pt, poff(vaddr), DUMP_PFDAT);
	}
}
