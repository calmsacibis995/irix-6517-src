/*
 * os/vm/vmidbg.c
 *
 *
 * Copyright 1997 Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/anon.h>
#include <sys/pfdat.h>
#include <sys/immu.h>
#include <sys/page.h>
#include <sys/lpage.h>
#include <ksys/rmap.h>
#include <sys/var.h>
#include <sys/nodepda.h>
#include <os/as/region.h>
#include <os/proc/pproc_private.h>
#include <sys/idbg.h>
#include <sys/idbgentry.h>
#include "anon_mgr.h"
#include "pcache.h"
#include "pcache_mgr.h"
#include "scache.h"
#include "scache_mgr.h"
#include "os/as/pmap.h"

void	idbg_anontree(anon_t *);
void	idbg_anonswap(anon_t *);
void	idbg_anon(anon_t *);
void	idbg_sanon(__psint_t);
void	idbg_dopfdat(register pfd_t *);
void	idbg_kptbl(__psint_t);
void	idbg_onswap(__psint_t);
void	idbg_pcache(pcache_t *);
void	idbg_pde(__psint_t);
void	idbg_pfindtag(__psint_t);
void	idbg_pfn(__psint_t);
void	idbg_findbad(__psint_t);
void	idbg_pvfind(__psint_t p1);
void	idbg_scachepool(void);
#ifdef NUMA_BASE
void    idbg_print_pgdata_interface(cnodeid_t, void *, int, char **);
void    idbg_print_lpg_stats(int);
#endif


static void print_anontree(anon_t *ap);
static char *interp_cc(uint);
static void dump_pcache(pcache_t *);

#define	INTEGER_ARGS	0		/* Standard (int) args */
#define STRING_ARGS	1		/* String (char *) args */

#define VD	(void (*)())

static struct vmidbg {
	char	*name;
	void	(*func)();
	int	argstype;
	char	*help;
} vmidbg_funcs[] = {
    "anon",	VD idbg_anon,
    	INTEGER_ARGS,	"Print single anon struct",
    "anontree",	VD idbg_anontree,
    	INTEGER_ARGS,	"Print the full anon tree",
    "anonswap",	VD idbg_anonswap,
    	INTEGER_ARGS,	"Print the swap handles for anon",
    "findbad",	VD idbg_findbad,
    	INTEGER_ARGS,	"Print P_BAD pages",
    "kptbl",	VD idbg_kptbl,
    	INTEGER_ARGS,	"Print kptbl",
    "onswap",	VD idbg_onswap,
    	INTEGER_ARGS,	"Print pages per logical swap",
    "pcache",	VD idbg_pcache,
	INTEGER_ARGS,	"Print pages in a page cache",
    "pde",	VD idbg_pde,
    	INTEGER_ARGS,	"Print pde",
    "pfdat",	VD idbg_pfn,
    	INTEGER_ARGS,	"Pfdat structures",
    "pagetag",	VD idbg_pfindtag,
    	INTEGER_ARGS,	"Find pages with tag ",
    "pfn",	VD idbg_pfn,
    	INTEGER_ARGS,	"Pfdat structures",
    "pvfind",	VD idbg_pvfind,
    	INTEGER_ARGS,	"Hashed vnode pages",
    "sanon",	VD idbg_sanon,
    	INTEGER_ARGS,	"Print shared anon page list",
    "scachepool",	VD idbg_scachepool,
    	INTEGER_ARGS,	"Print scache pool structure",
#ifdef NUMA_BASE
    "pgdata",	VD idbg_print_pgdata_interface,
	INTEGER_ARGS,	"Print numa page pool info",
    "lpgstats",	VD idbg_print_lpg_stats,
	INTEGER_ARGS,	"Print numa page pool info",
#endif
    0,          0,	0,	0
};


void
vmidbg_init(void)
{
	struct vmidbg	*p;

	for (p = vmidbg_funcs; p->name; p++)
		idbg_addfunc(p->name, p->func);
}


/*
 * pfdat dump.
 */
char	*tab_pfdatflags[] = {
	"q",		/* 0x000001 */
	"bad",		/* 0x000002 */
	"hash",		/* 0x000004 */
	"done",		/* 0x000008 */
	"swap",		/* 000x0010 */
	"wait",		/* 0x000020 */
	"dirty",	/* 0x000040 */
	"dq",		/* 0x000080 */
	"anon",		/* 0x000100 */
	"sq",		/* 0x000200 */
	"cw",		/* 0x000400 */
	"dump",		/* 0x000800 */
	"hole",		/* 0x001000 */
	"cstale",	/* 0x002000 */
        "lpgindex1",    /* 0x004000 */
        "lpgindex2",    /* 0x008000 */
        "lpgindex3",    /* 0x010000 */
        "lpgpoison",    /* 0x020000 */
        "sanon",        /* 0x040000 */
        "recycle",      /* 0x080000 */
        "eccstale",     /* 0x100000 */
        "replica",      /* 0x200000 */
#ifndef _VCE_AVOIDANCE
        "poisoned",     /* 0x400000 */
        "migrating",    /* 0x800000 */
	"error",	/* 0x1000000*/
	"hwbad",	/* 0x2000000*/
        "xp",		/* 0x4000000*/
	"lock",		/* 0x8000000*/
	"userdma",	/* 0x10000000*/
	"bulkdata",	/* 0x20000000*/
#else
	"userdma",	/* 0x400000 */
	"multicolor",	/* 0x800000 */
	"rawwait",	/*0x1000000 */
#endif
	0
};

/*
 * Print a 1-line description of the pfdat. Note that no EOL is
 * printed in case caller wants to add more info.
 */
void
idbg_dopfdat1(pfd_t *pfd)
{
	qprintf("0x%x [%d 0x%x] ",
		pfd, pfdattopfn(pfd), pfdattopfn(pfd));
	qprintf("pgno 0x%x use %d raw %d ",
		pfd->pf_pageno,
		pfd->pf_use,
		pfd->pf_rawcnt);
	printflags((unsigned)pfd->pf_flags,
		   tab_pfdatflags,
		   "flags");
}


/*
 * Print a multi-line description of the pfdat. 
 */
void
idbg_dopfdat(register pfd_t *pdp)
{
	qprintf("pfdat: addr 0x%x [%d 0x%x], ", pdp,
			pfdattopfn(pdp), pfdattopfn(pdp));
#ifdef _VCE_AVOIDANCE
	qprintf("\n");
	qprintf("  vcolor %d %s, ", pfd_to_vcolor(pdp), 
		pfde_to_rawwait(pdp) ? "RAWWAIT" : "");
#endif
	printflags((unsigned)pdp->pf_flags, tab_pfdatflags, "flags");
	qprintf("\n");
	qprintf("  pageno 0x%x tag 0x%x use %d rawcnt %d\n",
		pdp->pf_pageno, pdp->pf_vp,
		pdp->pf_use, pdp->pf_rawcnt);
	qprintf("  next 0x%x [%d] prev 0x%x [%d] ",
		pdp->pf_next, (pdp->pf_next) ? pfdattopfn(pdp->pf_next) : -1,
		pdp->pf_prev, (pdp->pf_prev) ? pfdattopfn(pdp->pf_prev) : -1);
	qprintf("pgsz index %d hchain 0x%x [%d]\n",
		PFDAT_TO_PGSZ_INDEX(pdp), pdp->pf_hchain,
		(pdp->pf_hchain && !IS_LINK_INDEX(pdp->pf_hchain)) ? pfdattopfn(pdp->pf_hchain) : -1);
	qprintf("   Bitmap base (pure) %x (tainted) %x\n",
			pfdat_to_pure_bm(pdp),pfdat_to_tainted_bm(pdp));

	qprintf("  pdep1: 0x%x", pdp->pf_pdep1);
        qprintf(" %s: 0x%x ", IS_LONG_RMAP(pdp) ? "rmap" : "pdep2",
				pdp->pf_rmapp);

	qprintf("\n");

#ifdef NUMA_BASE        
	idbg_pfms(pfdattopfn(pdp));
#endif /* NUMA_BASE */        

#ifdef	NUMA_REPLICATION1
	if (pfd_replicated(pdp) && pdp->pf_repchain)
		qprintf(" repchain 0x%x", pfctopfd(pdp->pf_repchain));
#endif	/* NUMA_REPLICATION */

        qprintf("\n");
}

/*
++
++	kp pfn P
++		P is absolute page frame number
++		or address of page frame descriptor (database, whatever...).
++
*/

#if CELL_IRIX
extern pfd_t* idbg_pfntopfdat(pfn_t);
#endif

void
idbg_pfn(__psint_t p1)
{
	cnodeid_t	node;

	pfd_t *pfd;

	if (p1 < 0) {
		pfd = (pfd_t *)p1;
#if CELL_IRIX
	} else if ( (pfd=idbg_pfntopfdat((pfn_t)p1))) {	
		;
#endif
	} else {
		node = NASID_TO_COMPACT_NODEID(pfn_nasid(p1));


		/* XXX - This code assumes that compact node ids will be
		 * ordered the same way as NASIDs
		 */

		if (node < 0 || node >= numnodes || p1 < pfdattopfn(PFD_LOW(0))) {
		    qprintf("pfn 0x%x is beyond memory: 0x%x < pfn < 0x%x\n",
			p1, pfdattopfn(PFD_LOW(0)), 
			pfdattopfn(PFD_HIGH(numnodes -1)));
		    return;
		}

		if (p1 < pfdattopfn(PFD_LOW(node)) ||
		    p1 > pfdattopfn(PFD_HIGH(node))) {
		    qprintf("pfn 0x%x is outside pfdat range for node %d: 0x%x < pfn < 0x%x\n",
			p1, node, pfdattopfn(PFD_LOW(node)), 
			pfdattopfn(PFD_HIGH(node)));
		    return;
		}

		pfd = pfntopfdat(p1);
		if (!page_validate_pfdat((pfd_t*)pfd)) {
			qprintf("pfn 0x%x is not a valid pfd (hole in pfdat)\n", p1);
			return;
		}
	}
	idbg_dopfdat(pfd);
}


#if defined(NUMA) && defined(NUMA_BASE)
/* Find all pages having the tag p1 */
void
idbg_pfindtag(__psint_t p1)
{
	int	node, slot, slot_psize, pfn, pfn_base;
	struct pfdat	*pfd;

	for (node=0; node < numnodes; node++){
	    for (slot = 0; slot < node_getnumslots(node); slot++)
		slot_psize = slot_getsize(node,slot);
		pfn_base = slot_getbasepfn(node, slot);
		for (pfn = pfn_base; pfn < pfn_base+slot_psize; ++pfn) {
			pfd = pfntopfdat(pfn);
			if (pfd->pf_tag  == (void *)p1)
				idbg_dopfdat(pfd);
		}
	}
}

/*
 * find all pages with a specific raw cnt.
 */
void
idbg_pfrawcnt(__psint_t p1)
{
	int	node, slot, slot_psize, pfn_base, pfn;
	struct pfdat	*pfd;

	for (node=0; node < numnodes; node++){
	    for (slot = 0; slot < node_getnumslots(node); slot++)
		slot_psize = slot_getsize(node,slot);
		pfn_base = slot_getbasepfn(node,slot);
		for (pfn = pfn_base; pfn < pfn_base+slot_psize; ++pfn) {
			pfd = pfntopfdat(pfn);
			if (pfd->pf_rawcnt  == p1)
				idbg_dopfdat(pfd);
		}
	}
}
#else
/* Find all pages having the tag p1 */
void
idbg_pfindtag(__psint_t p1)
{
	int     node;

	for (node=0; node < numnodes; node++){
		pfd_t   *pfd;

		for (pfd = PFD_LOW(node); pfd <= PFD_HIGH(node); pfd++) {
			if (!page_validate_pfdat(pfd))
				continue;
			if (pfd->pf_tag  == (void *)p1)
				idbg_dopfdat(pfd);
		}
	}
}
#endif	/* NUMA_BASE */


/*
++
++	kp findbad
++		list all pfd's marked P_BAD
*/
void
idbg_findbad(__psint_t p1)
{
	pfd_t *pfd;
	cnodeid_t node;

	for (node = 0; node < numnodes; node++)
		for (pfd = PFD_LOW(node); pfd <= PFD_HIGH(node); pfd++) {
			if (!page_validate_pfdat(pfd))
				continue;
			if ( (p1 > 0 || pfd->pf_flags & P_BAD) && 
					(pfd->pf_use || !(pfd->pf_flags&P_QUEUE))) {
				idbg_dopfdat1(pfd);
				qprintf("\n");
			}
		}
}


/*
++
++	kp pvfind address
++		p1 < 0 : prints hash chain for vnode @ address p1
++		p1 >= 0 : not supported!
++
*/
void
idbg_pvfind(__psint_t p1)
{
	vnode_t	*vp;
	int 	num;

	if (p1 < 0) {
		vp = (vnode_t *)p1;
		num = vp->v_number;
	} else {
		qprintf("0x%x is not a valid vnode descriptor\n");
		return;
	}

	qprintf("pages hashed for vnode @ 0x%x num %d\n", vp, num);
	dump_pcache(&vp->v_pcache);
}


#if R4000
static char *r4k_cachmode[] = {
	"reserved",
	"reserved",
	"uncach",
	"cach non-coherent",
	"cach coher excl",
	"cach coher excl-wr",
	"cach coher upd-wr",
	"reserved for cach write-through",
	"invalid",
};

#ifndef R10000
static char *
interp_cc(uint cc)
#else
static char *
r4k_interp_cc(uint cc)
#endif /* !R10000 */
{
	if (cc > 7)
		return r4k_cachmode[8];
	else
		return r4k_cachmode[cc];
}
#endif

#if TFP
static char *tfp_cachmode[] = {
	"uncach proc-ordered",
	"reserved",
	"uncach seq-ordered",
	"cach non-coher",
	"cach coher excl",
	"cach coher excl-wr",
	"reserved",
	"reserved for cach write-through",
	"invalid",
};
static char *
interp_cc(cc)
uint cc;
{
	if (cc > 7)
		return tfp_cachmode[8];
	else
		return tfp_cachmode[cc];
}
#endif

#if R10000
static char *r10k_cachmode[] = {
	"reserved",
	"reserved",
	"uncach",
	"cach non-coher",
	"cach coher excl",
	"cach coher shared",
	"reserved ",
	"uncached accelerated",
	"invalid",
};

#ifndef R4000
static char *
interp_cc(uint cc)
#else
static char *
r10k_interp_cc(uint cc)
#endif /* !R4000 */
{
	if (cc > 7)
		return r10k_cachmode[8];
	else
		return r10k_cachmode[cc];
}
#endif /* R10000 */

#if R10000 && R4000
static char *
interp_cc(uint cc)
{
	if (IS_R10000())
		return r10k_interp_cc(cc);
	else
		return r4k_interp_cc(cc);
}
#endif /* R10000 && R4000 */

#if BEAST
static char *beast_cachmode[] = {
	"reserved",
	"reserved",
	"uncach seq-ordered",
	"cach non-coher",
	"cach coher excl",
	"cach coher shared",
	"reserved ",
	"uncached accelerated",
	"invalid",
};

static char *
interp_cc(uint cc)
{
	if (cc > 7)
		return beast_cachmode[8];
	else
		return beast_cachmode[cc];
}
#endif /* BEAST */

/* ARGSUSED */
int
printas(void *obj, void *n1, void *n2)
{
	pas_t *pas = (pas_t *)obj;

	printf(" pas : 0x%x ", pas);
	return(0);
}

int
idbg_dopde(pde_t *pde, off_t offset, int flag)
{
	pfd_t *pf;

	qprintf("%s ", interp_cc(pde->pte.pg_cc));
#ifdef SN0
	qprintf("%x ", pde->pte.pg_uc);
#endif
	if (pde->pte.pg_m) qprintf("m ");
	if (pde->pte.pg_vr) qprintf("vr ");
#ifndef TFP
	if (pde->pte.pg_g) qprintf("g ");
#endif
	if (pde->pte.pg_dirty) qprintf("d/w ");
	if (pde->pte.pg_sv) qprintf("sv ");
	qprintf("nr %d; pfn 0x%x ", pde->pte.pg_nr, pde->pte.pg_pfn);
	if ((flag & DUMP_PFDAT) && pde->pte.pg_pfn && pde->pte.pg_pfn > PG_SENTRY) {
		pf = pfntopfdat(pde->pte.pg_pfn);
		printflags((unsigned)pf->pf_flags, tab_pfdatflags, "pf_flags");
		qprintf("use %d", pf->pf_use);
	}
        if (pde->pte.pg_pfnlock)
                qprintf(" Lockedpfn");

	if (pde->pte.pg_pfn && pde->pte.pg_pfn > PG_SENTRY)
		qprintf(", paddr 0x%x",(pde->pte.pg_pfn<<PNUMSHFT)|offset);
        
#ifdef DEBUG_PFNLOCKS        
        qprintf(" Holder: [%d %d]",
                (pde->pte.pte_resvd >> 16) & 0xF, pde->pte.pte_resvd & 0xFFFF);
#endif /* DEBUG_PFNLOCKS */
        
#ifdef  RMAP_PTE_INDEX
	qprintf(" Rmapi %d",pde->pte.pte_rmap_index);
	if (pde->pte.pte_pgmaskshift)
		qprintf(" pmi %d",pde->pte.pte_pgmaskshift);
#endif  /*RMAP_PTE_INDEX */
	/* pmap_pte_scan(pde, printas, 0, 0, AS_SCAN); */
	qprintf("\n");
#ifdef  RMAP_PTE_INDEX
	return(PAGE_MASK_INDEX_TO_SIZE(pde->pte.pte_pgmaskshift));
#else
	return NBPP;
#endif
}

/*
++
++	kp pde pde-addr
++		pde-addr < 0 : prints pde at pde-addr
++		pde-addr >= 0 : prints kptbl[pde-addr] pde
++
*/
void
idbg_pde(__psint_t p1)
{
	pde_t *pde;

	if (p1 < 0) {
		pde = (pde_t *) p1;
		idbg_dopde(pde, 0, DUMP_PFDAT);
	} else {
		if (p1 > syssegsz) {
			qprintf("pde %d overflows kptbl\n", p1);
			return;
		}
		pde = kptbl + p1;
		idbg_dopde(pde, 0, DUMP_PFDAT);
	}
}

/*
++
++	kp kptbl addr
++		addr >= 0 : prints kptbl[addr] pde
++		addr < 0  : prints kptbl entry for k2seg vaddr addr
++		addr = -1 : prints every non-empty kptbl entry
++
*/
void
idbg_kptbl(__psint_t p)
{
	int i;
	pde_t *pde;
	pgno_t pno;

	if (p >= 0) {
		pde = &kptbl[p];
		idbg_dopde(pde, 0, DUMP_PFDAT);
	} else	if (p  == -1L) {
		for (i = 0, pde = &kptbl[0]; i < syssegsz; i++, pde++) {
			if (pde->pgi) {
				qprintf("kptbl[0x%x] ", i);
				idbg_dopde(pde, 0, DUMP_PFDAT);
			}
		}
	} else { /* p < 0) */
		if (!IS_KSEG2(p)) {
			qprintf("no kernel page table for address 0x%x\n", p);
			return;
		}
		pno = pnum((__psint_t)(p) - (__psint_t)K2BASE);
		qprintf("kptbl[%d] @0x%x:", pno, &kptbl[pno]);
		pde = kvtokptbl(p);
		idbg_dopde(pde, 0, DUMP_PFDAT);
	}
}

/*
++
++	kp sanon [flag]
++		print shared anon page list
++			flag:  -1 or omitted, print only sanon page counts
++					(nodes with 0 counts are not printed)
++			       other - print list of pages in each sanon queue
++
*/
/* ARGSUSED */
void
idbg_sanon(__psint_t x)
{
	extern sanon_list_head_t	sanon_list[];	/* list of shared anon pages */
	sanon_list_head_t		*salist;
#ifdef DEBUG
	extern int	psanonmem;
#endif
	pfd_t *pfd;
	int i,n,cnode;


	/* Count entries on sanon list */
	n = 0;
	for (cnode=0; cnode<numnodes; cnode++) {
		salist = &SANON_LIST_FOR_CNODE(cnode);
		for (pfd = salist->sal_listhead.pf_next, i=0; pfd != (pfd_t *)&salist->sal_listhead;
					pfd = pfd->pf_next, i++)
			;
		if (i == 0)
			continue;
		qprintf("sanon list: cnode %d, psanonmem %d\n", cnode, i);
		if (x != (__psint_t)-1) {
			for (pfd = salist->sal_listhead.pf_next; pfd != (pfd_t *)&salist->sal_listhead;
						pfd = pfd->pf_next) {
				qprintf("  0x%x[%d] ", pfd, pfdattopfn(pfd));
				printflags((unsigned)pfd->pf_flags, tab_pfdatflags, "pf_flags");
				qprintf(" pgno 0x%x tag 0x%x use %d raw %d\n",
					pfd->pf_pageno,  pfd->pf_tag,
					pfd->pf_use, pfd->pf_rawcnt);
			}
		}
		n += i;
	}
#ifdef DEBUG
	if (n != psanonmem)
		qprintf("psanonmem %d != num on list %d\n", psanonmem, n);
#endif
}

static void
print_onswap(anon_t *ap, reg_t *rp)
{
	if (scache_has_swaphandles(&ap->a_scache)) {
		qprintf("rp 0x%x anon 0x%x ", rp, ap);
		scache_scan(&ap->a_scache, SCAN, 0, MAXPAGES, (void (*)())qprintf, "[%d 0x%x]");
		qprintf("\n");
	}
}

void
print_aswaptree(anon_t *ap, reg_t *rp)
{
	print_onswap(ap, rp);

	if (ap->a_left)
		print_aswaptree(ap->a_left, rp);

	if (ap->a_right)
		print_aswaptree(ap->a_right, rp);
}


/*
 * dump out all pages on swap for process
++
++	kp onswap arg - look for pages out on swap for process <arg>
++		arg == -1 use curproc
++		arg < 0 use arg as address of proc
++		arg > 0 look up proc with pid <arg>
 *
 */
void
idbg_onswap(__psint_t p1)
{
	proc_t *pp;

	if (p1 == -1L) {
		pp = curprocp;
		if (pp == NULL) {
			qprintf("no current process\n");
			return;
		}
	} else if (p1 < 0) {
		pp = (proc_t *) p1;
		if (!procp_is_valid(pp)) {
			qprintf("WARNING:0x%x is not a proc address\n", pp);
		}
	} else {
		if ((pp = idbg_pid_to_proc(p1)) == NULL) {
			qprintf("%d is not an active pid\n", (pid_t)p1);
			return;
		}
	}
	idbg_asonswap(pp);
}

/*
 * Print routines for anon structs and trees
 */

anon_t *
find_anonroot(anon_t *ap)
{
	while (ap->a_parent)
		ap = ap->a_parent;

	return ap;
}

void
idbg_anontree(anon_t *ap)
{
	if ((__psint_t)ap == -1L) {
		qprintf("Usage: anontree <addr of any node in tree>\n");
		return;
	}

	print_anontree(find_anonroot(ap));
}

static void
print_anontree(anon_t *ap)
{
	idbg_anon(ap);

	if (ap->a_left)
		print_anontree(ap->a_left);

	if (ap->a_right)
		print_anontree(ap->a_right);
}


/*
 * Dump an anon node.  If there are no more than MAX_PCACHE_SIZE pages in
 * its cache, then print the individual pages as well.
 */

#define MAX_PCACHE_SIZE		10

void
idbg_anon(anon_t *ap)
{

	if ((__psint_t)ap == -1L) {
		qprintf("Usage: anon <addr of anon node>\n");
		return;
	}

	qprintf("-----------------------------------------------------------\n");
	qprintf("Node at 0x%8x: a_lock 0x%x ", ap, ap->a_lock);

	if (ap->a_lock)
		qprintf("(%s, refcnt %d)\n", ap->a_lock->l_un.l_st.l_treelock == 1 ?
			"Locked" : "unlocked", ap->a_lock->l_un.l_st.l_refcnt);

	qprintf("	a_parent 0x%x, a_left 0x%x, a_right 0x%x\n", 
			ap->a_parent, ap->a_left, ap->a_right);
	qprintf("	a_refcnt %d, a_depth %d, a_limit 0x%x\n",
			ap->a_refcnt, ap->a_depth, ap->a_limit);
	qprintf("	a_pcache 0x%x has %d (0x%x) pages, a_scache 0x%x is ", 
			&ap->a_pcache, ap->a_pcache.pc_count, 
			ap->a_pcache.pc_count, &ap->a_scache);

	if (ap->a_scache.sc_mode == SC_NONE)
		qprintf("not allocated\n");
	else if (ap->a_scache.sc_mode == SC_ARRAY)
		qprintf("array, max lpn %d (0x%x)\n",
			ap->a_scache.sc_maxlpn, ap->a_scache.sc_maxlpn);
	else
		qprintf("hash table with %d (0x%x) blocks\n",
			ap->a_scache.sc_count, ap->a_scache.sc_count);

	if (ap->a_pcache.pc_count <= MAX_PCACHE_SIZE)
		dump_pcache(&ap->a_pcache);
}

void
idbg_anonswap(anon_t *ap)
{
	if ((__psint_t)ap == -1L) {
		qprintf("Usage: anonswap <addr of anon node>\n");
		return;
	}

	if (scache_has_swaphandles(&ap->a_scache)) {
		qprintf("anon 0x%x ", ap);
		scache_scan(&ap->a_scache, SCAN, 0, MAXPAGES, (void (*)())qprintf, "[%d 0x%x]");
		qprintf("\n");
	} else
		qprintf("anon 0x%x - no swaphandles\n", ap);
}


/*
 * Print the contents of a pcache.
 */

void
idbg_pcache(pcache_t *cache)
{

	if ((__psint_t)cache == -1L) {
		qprintf("Usage: pcache <addr of pcache>\n");
		return;
	}

	qprintf("pcache @ 0x%x, %d (0x%x) buckets, %d (0x%x) pages cached\n",
		cache, cache->pc_size, cache->pc_size,
		cache->pc_count, cache->pc_count);

	dump_pcache(cache);
}


/*
 * Print a list of the pages in the given pcache.  Cycle through the
 * page numbers sequentially so we can print the list in sorted order.
 * This is a waste of CPU time, but what the heck, we're in the debugger
 * anyway.
 */

static void
dump_pcache(pcache_t *cache)
{
	pfd_t	*pfd;
	pgno_t	pgcnt, pgno;

	pgcnt = cache->pc_count;

	for (pgno = 0; pgcnt ; pgno++) {
		/*
		 * Use the pcache functions directly instead
		 * of going via vnode_pcache, as we could try
		 * to do this while in debugger, and some other
		 * cpu may already be holding the lock!!.
		 */ 
		pfd = pcache_find(cache, pgno);
		while (pfd) {
			pgcnt--;
			idbg_dopfdat1(pfd);
			qprintf("\n");

			/* Check if there is a second page with same pgno.
			 * Happens mostly for vnodes with replicated pags.
			 * But this could be used to catch duplicate 
			 * insertions !!
			 */
			pfd = pcache_next(cache, pfd);
		}
	}
}


/*
 * Print out the scache pool structure.
 */

extern scache_pool_t	scache_pool;

void
idbg_scachepool(void)
{
	qprintf("scache pool:	reserved pages %d (0x%x)\n",
		scache_pool.num_reserved_pages, scache_pool.num_reserved_pages);
	qprintf("			# of pages needed %d (0x%x)\n",
		scache_pool.num_pages_needed, scache_pool.num_pages_needed);
	qprintf("			pfd list starts with 0x%x\n",
		scache_pool.swap_reserve_page_list);
}


#ifdef  NUMA_BASE
static void
idbg_print_pgdata(cnodeid_t node, int colors)
{
	int i, j, n;
	pg_free_t *pfl;
	nodepda_t *pda;
	pg_data_t *pgdata;

	pda = NODEPDA(node);
	if (!pda) {
		qprintf("NODEPDA(%d) is null\n", node);
		return;
	}

	pgdata = &pda->node_pg_data;

	qprintf("Node %d nodepda 0x%x pg_data 0x%x\n",
		node, pda, pgdata);
	qprintf(" pg_freelst 0x%x &pg_freelst_lock 0x%x\n",
		pgdata->pg_freelst,
		&pgdata->pg_freelst_lock);
	qprintf(" freemem %d future_freemem %d empty_mem %d total_mem %d\n",
		pgdata->node_freemem,
		pgdata->node_future_freemem,
		pgdata->node_emptymem,
		pgdata->node_total_mem);

	pfl = GET_NODE_PFL(node);

	qprintf("lpage information for node %d   pfl 0x%x\n", node, pfl);
	qprintf("  %8s %18s %18s %5s %5s %18s\n",
			"pgsz", "phead", "end", "mask", "shift", "rotor");
	for(i=0; i<NUM_PAGE_SIZES; ++i) {
		qprintf("  %8d 0x%16x 0x%16x 0x%03x %5d 0x%16x\n",
			PGSZ_INDEX_TO_SIZE(i),
			pfl->phead[i],
			pfl->pheadend[i],
			pgdata->pheadmask[i],
			pgdata->pheadshift[i],
			pgdata->pheadrotor[i]);
	}
	qprintf(" pg_free     pfd_low 0x%x pfd_high 0x%x\n",
		pgdata->pg_freelst->pfd_low,
		pgdata->pg_freelst->pfd_high);


	qprintf("  %8s  %11s  %11s  %11s",
		"pgsz", "free_pages", "total_pages", "hiwat");
	qprintf("  %18s", "&lpage_stats");
	qprintf("\n");
	for(i=0; i<NUM_PAGE_SIZES; ++i) {
		qprintf("  %8d  %11d  %11d  %11d",
			PGSZ_INDEX_TO_SIZE(i),
			pgdata->num_free_pages[i],
			pgdata->num_total_pages[i],
			pfl->hiwat[i]);
		qprintf("  0x%16x", &pgdata->lpage_stats[i][0]);
		qprintf("\n");
	}

	if (colors) {
        	qprintf("freepages by page color\n");
        	qprintf("      pgsz    ------------------------------- page counts by color"
			"-------------------------------------------\n");
        	for(i=0; i<NUM_PAGE_SIZES; ++i) {
                	qprintf("%10d    ", PGSZ_INDEX_TO_SIZE(i));
                	for (j=0,n=0; j<=pgdata->pheadmask[i]; j++, n++) {
                        	if (n==16) n=0, qprintf("\n              ");
                        	qprintf("%6d", pfl->phead[i][j].ph_count);
                	}
                	if (n) qprintf("\n");
        	}
	}

}



/* ARGSUSED */
void
idbg_print_pgdata_interface(cnodeid_t node, void *a2, int argc, char **argv)
{
	int i, colors=0;

	if (argc >= 2)
		colors = atoi(argv[1]);
	if (node == (cnodeid_t)-1) {
		for (i = 0; i < numnodes; i++) {
			idbg_print_pgdata(i, colors);
		}
	} else {
		if (node >= numnodes) {
			qprintf("Only %d nodes\n", node);
		} else {
			idbg_print_pgdata(node, colors);
		}
	}
}

static char	*stat_strings[] = {
	"Coalescing daemon recursive calls",
	"Page coalescing attempts",
	"Page alloc time",
	"Page free time",
	"Page coalescing success",
	"Page split attempts",	
	"Page split success",
	"Coalescing daemon attempts",
	"Coalescing daemon hit without lock",
	"Page split wakeups",
	"Migration fails",
	"Coalescing daemon success",
	"Page split fails",
	"Migration success",
	"Test migration returns failure",
	"Test migration returns success",
	"Trying to move a kernel page",
	"Large page vfault att",
	"Large page vfault success",
	"Large page pfault att",
	"Large page pfault success",
	"Large page pagealloc  success",
	"Large page pagealloc  fail",
	"large page downgrade",
	"Large pagealloc NBPP page",
	"Buf cache release successfull",
	"Vfault retry same page size",
	"Vfault retry lower page size",
	"Pfault retry same page size",
	"Pfault retry lower page size",
	"Swapin retry same page size",
	"Swapin retry lower page size"
};

/*
 * Print lpage statistics.
 *	Argument:
 *		-2 			reset the statistics
 *		(other/default)		print statistics for all page sizes
 */
void
idbg_print_lpg_stats(int arg)
{
	int	i, j, k, pindx;
	int	sum, freepages;

	if (arg == -2) {
		for (i = 0; i < NUM_PAGE_SIZES; i++)
			for ( j = 0; j < NUM_STAT_WORDS; j++)
				for (k = 0; k < numnodes; k++)
					SET_LPG_STAT(k, i, j, 0);
		return;
	}


	qprintf("       16k       64k      256k        1m        4m       16m      -- Description ---\n");
	for ( i = 0; i < NUM_STAT_WORDS; i++) {
		for ( pindx=0; pindx < NUM_PAGE_SIZES; pindx++) {
			for ( sum=0, j = 0; j < numnodes; j++)
				sum += GET_LPG_STAT(j, pindx, i);
			qprintf("%10d", sum);
		}
		qprintf("    %s\n", stat_strings[i]);
	}


	for ( pindx=0; pindx < NUM_PAGE_SIZES; pindx++) {
		for ( freepages = 0, j = 0; j < numnodes; j++)
			freepages += GET_NUM_FREE_PAGES(j, pindx);
		qprintf("%10d", freepages);
	}

	qprintf("    Number of free pages\n");
}

#endif /* NUMA_BASE */
