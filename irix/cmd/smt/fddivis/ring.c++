/*
 * Copyright 1989,1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Ring functions for fddivis
 *
 *	$Revision: 1.18 $
 */

#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
extern "C" {
#endif
#include <osfcn.h>
#include <string.h>
#include <bstring.h>
#include <math.h>
#include <assert.h>
#include <malloc.h>
#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
}
#endif
#include "fddivis.h"
#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
extern "C" {
#endif
#include <smt_subr.h>
#include <smt_parse.h>
#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
}
#endif

#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
extern "C" {
#endif
extern Object vobj;
extern void setvobj(void);
#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
}
#endif

Ring::Ring(void)
{
	srch = 0;
}

Ring::~Ring(void)
{
}

Symbol *
Ring::sidhash_lookup(SMT_STATIONID *sid)
{
	return(sc_lookupsym(FV.sidtbl, &sid->ieee, sizeof(sid->ieee)));
}

Symbol *
Ring::sidhash_add(RING *rp)
{
	Symbol *s;
	s = sc_addsym(FV.sidtbl,
		&rp->sid.ieee, sizeof(rp->sid.ieee), SYM_INUSE);
	if (s)
		s->sym_data = rp;
	return s;
}

void
Ring::sidhash_del(RING *rp)
{
	sc_deletesym(FV.sidtbl, &rp->sid.ieee, sizeof(rp->sid.ieee));
}

Symbol*
Ring::sahash_lookup(LFDDI_ADDR *addr)
{
	return sc_lookupsym(FV.satbl, addr, sizeof(*addr));
}

Symbol*
Ring::sahash_add(RING *rp)
{
	Symbol *s, *s1;

	assert(!NULLADR(rp->sm.sa));
	s = sc_addsym(FV.satbl, &rp->sm.sa, sizeof(rp->sm.sa), SYM_INUSE);
	s->sym_data = rp;
	if (rp->smac && !NULLADR(rp->dm.sa)) {
		s1 = sc_addsym(FV.satbl,
			&rp->dm.sa, sizeof(rp->dm.sa), SYM_INUSE);
		s1->sym_data = rp;
	}
	return s;
}

void
Ring::sahash_del(RING *rp)
{
	sc_deletesym(FV.satbl, &rp->sm.sa, sizeof(rp->sm.sa));
	if (rp->smac)
		sc_deletesym(FV.satbl, &rp->dm.sa, sizeof(rp->dm.sa));
}

void
Ring::free_a_info(register SINFO *ip)
{
	if (ip->nbrs != 0)	::free(ip->nbrs);
	if (ip->pds != 0)	::free(ip->pds);
	if (ip->mac_stat != 0)	::free(ip->mac_stat);
	if (ip->ler != 0)	::free(ip->ler);
	if (ip->mfc != 0)	::free(ip->mfc);
	if (ip->mfncc != 0)	::free(ip->mfncc);
	if (ip->mpv != 0)	::free(ip->mpv);
	if (ip->ebs != 0)	::free(ip->ebs);
}

void
Ring::free_a_ring(register RING *tp)
{
	if (!tp)
		return;

	if ((tp->ghost&SHADOW) == 0) {
		free_a_info(&tp->sm);
		free_a_info(&tp->dm);
	}
	free(tp);
	FV.recal++;
}

void
Ring::flush_ring(void)
{
	if (FV.ring) {
		register int i;
		register RING *rp, *tp;

		for (i = FV.RingNum, rp = FV.ring->next; i > 0; i--) {
			tp = rp;
			rp = rp->next;
			sidhash_del(tp);
			sahash_del(tp);
			if (tp->shadow) {
				assert((tp->shadow->ghost&SHADOW) != 0);
				list_remove(tp->shadow);
				free_a_ring(tp->shadow);
				tp->shadow = 0;
			}
			list_remove(tp);
			free_a_ring(tp);
		}
		assert(rp == FV.ring);
	}

	srch = 0;
	FV.ring = 0;
	FV.magrp = 0;
	FV.pickrp = 0;
	FV.RingOpr = RINGOPR;
	FV.RINGWindow->RINGView->resettoken();
}

void
Ring::list_insert(RING *rp, RING *old)
{
	assert(old != 0);
	assert(rp != 0);
	assert(rp != old);
	rp->prev=old;
	rp->next=old->next;
	old->next->prev=rp;
	old->next=rp;
	FV.RingNum++;
}

void
Ring::list_remove(RING *rp)
{
	assert(rp != 0);
	rp->prev->next = rp->next;
	rp->next->prev = rp->prev;
	FV.RingNum--;
	if (srch == rp)	srch = 0;
	if (FV.magrp == rp)  FV.magrp = 0;
	if (FV.pickrp == rp) FV.pickrp = 0;
	if (FV.RINGWindow->RINGView->tkrp == rp)
		FV.RINGWindow->RINGView->tkrp = 0;
	if (FV.RINGWindow->RINGView->stkrp == rp)
		FV.RINGWindow->RINGView->stkrp = 0;
}

int
Ring::update_ring(register RING *rp, register SMT_FB *fb)
{
	SINFO ri;
	int need_redraw;
	register SINFO *si;
	register smt_header *fp = FBTOFP(fb);
	register struct lmac_hdr *mh = FBTOMH(fb);
	int maybe_redraw = 0;

	bzero((char*)&ri, sizeof(ri));
	if (SAMEADR(rp->sm.sa, mh->mac_sa)) {
		ri.std = rp->sm.std;
		si = &rp->sm;
		need_redraw = 0;
	} else if (SAMEADR(rp->dm.sa, mh->mac_sa)) {
		ri.std = rp->dm.std;
		si = &rp->dm;
		need_redraw = 0;
	} else if (NULLADR(rp->sm.sa)) {
		ri.std = rp->sm.std;
		si = &rp->sm;
		need_redraw = 1;
	} else if (NULLADR(rp->dm.sa)) {
		ri.std = rp->dm.std;
		si = &rp->dm;
		need_redraw = 1;
	} else {
		/* could be floating MAC but just ignore */
		return(0);
	}

	/* process packet */
	if (update_sif(&ri, fb) != RC_SUCCESS) {
		need_redraw = 0;
		goto usif_ret;
	}

	/* get time stamp for aging */
	smt_gts(&rp->tms);

	/* update station */
	if (fp->fc == SMT_FC_NIF) {
		if (si == &rp->dm)
			rp->smac = 1;
		if (!SAMEADR(si->una, ri.una)) {
			si->una = ri.una;
			si->sif |= FSI_UNA;
			need_redraw = 1;
		}
	} else if (fp->fc == SMT_FC_CONF_SIF) {
		register int i,j, macrid;
		register PARM_PD_PHY *pdp;
		register PARM_PD_MAC *pdm;
		register int ports;
		register PARM_MACNBRS *nbrs;

		if (fp->type != SMT_FT_RESPONSE) {
			need_redraw = 0;
			goto usif_ret;
		}

		/*
		 * if this frame does not have valid Station Descriptor,
		 * just drop it.
		 */
		if (!FSISET(ri.sif,FSI_STD) || !FSISET(ri.sif,FSI_PDS)) {
			need_redraw = 0;
			goto usif_ret;
		}

		/*
		 * Adjust MACs' locations.
		 * XXX - first mac is assumed to be the primary mac.
		 *	 And from 3rd mac and on are ignored.
		 */
		ports = ri.r_master_ct + ri.r_nonmaster_ct;
		pdm = (PARM_PD_MAC*)(&ri.pds[ports]);
		if (SAMEADR(pdm[0].addr, mh->mac_sa)) {
			si = &rp->sm;
			macrid = ports+1;
			if (ri.r_mac_ct > 1)
				rp->dm.sa = pdm[1].addr;
		} else if (ri.r_mac_ct > 1 &&
				SAMEADR(pdm[1].addr, mh->mac_sa)) {
			si = &rp->dm;
			macrid = ports+2;
			rp->sm.sa = pdm[0].addr;
			pdm = &pdm[1];
		} else {
			need_redraw = 0;
			goto usif_ret;
		}

		if (FSISET(ri.sif,FSI_VERS) && !SAMEADR(si->vers, ri.vers)) {
			si->vers = ri.vers;
			si->sif |= FSI_VERS;
			need_redraw = 1;
		} else if (si->vers != fp->vers) {
			si->vers = fp->vers;
			si->sif |= FSI_VERS;
			need_redraw = 1;
		}

		if (FSISET(ri.sif,FSI_SP) && !SAMEADR(si->sp, ri.sp)) {
			si->sp = ri.sp;
			si->sif |= FSI_SP;
			need_redraw = 1;
		}

		if ((ri.pds) && FSISET(ri.sif,FSI_PDS)) {
			PARM_PD_PHY *prm = 0, *sec = 0;

			/* pdm is found above */
			for (j = 0, pdp = ri.pds; j < ports; pdp++, j++) {
				if (pdp->pc_type==PC_B || pdp->pc_type==PC_S)
					prm = pdp;
			}
			if (!prm) {
				need_redraw = 0;
				goto usif_ret;
			}

			for (j = 0, pdp = ri.pds; j < ports; pdp++, j++) {
				if (pdp->pc_type == PC_A)
					sec = pdp;
			}

			if (si->pds != 0)
				free(si->pds);
			si->pds = ri.pds;
			ri.pds = 0;
			si->primary = prm;
			si->secondary = sec;
			si->sif |= FSI_PDS;
		}

		if ((ri.nbrs != 0) && FSISET(ri.sif,FSI_MACNBRS)) {
			nbrs = ri.nbrs;
			for (i = 0; i < ri.r_mac_ct; i++, nbrs++) {
				if ((ri.r_mac_ct == 1) ||
				    (nbrs->macidx == macrid)) {
					if (!SAMEADR(si->una, nbrs->una) ||
					    !SAMEADR(si->dna, nbrs->dna))
						need_redraw = 1;
					si->una = nbrs->una;
					si->dna = nbrs->dna;
					if (si->nbrs != 0)
						free(si->nbrs);
					si->nbrs = ri.nbrs;
					ri.nbrs = 0;
					si->sif |= (FSI_MACNBRS|FSI_UNA);
					break;
				}
			}
		}
	} else if (fp->fc == SMT_FC_OP_SIF) {
		if (fp->type != SMT_FT_RESPONSE) {
			need_redraw = 0;
			goto usif_ret;
		}

		if (FSISET(ri.sif,FSI_USR) && !SAMEADR(ri.usr,si->usr)) {
			si->usr = ri.usr;
			si->sif |= FSI_USR;
			maybe_redraw = 1;
		}
		if (FSISET(ri.sif,FSI_MF) && !SAMEADR(ri.mf,si->mf)) {
			si->mf = ri.mf;
			si->sif |= FSI_MF;
			maybe_redraw = 1;
		}
		if ((ri.mac_stat) && FSISET(ri.sif,FSI_MAC_STAT)) {
			if (si->mac_stat != 0) free(si->mac_stat);
			si->mac_stat = ri.mac_stat;
			ri.mac_stat = 0;
			si->sif |= FSI_MAC_STAT;
		}
		if ((ri.ler) && FSISET(ri.sif,FSI_LER)) {
			int prev = 0;
			if (si->ler != 0) {
				prev = ler_alarm(si, 0);
				free(si->ler);
			}
			si->ler = ri.ler;
			ri.ler = 0;
			si->sif |= FSI_LER;
			need_redraw = prev != ler_alarm(si, 0);
		}
		if ((ri.mfc) && FSISET(ri.sif,FSI_MFC)) {
			if (si->mfc != 0) free(si->mfc);
			si->mfc = ri.mfc;
			ri.mfc = 0;
			si->sif |= FSI_MFC;
		}
		if ((ri.mfncc) && FSISET(ri.sif,FSI_MFNCC)) {
			if (si->mfncc != 0) free(si->mfncc);
			si->mfncc = ri.mfncc;
			ri.mfncc = 0;
			si->sif |= FSI_MFNCC;
		}
		if ((ri.mpv) && FSISET(ri.sif,FSI_MPV)) {
			if (si->mpv != 0) free(si->mpv);
			si->mpv = ri.mpv;
			ri.mpv = 0;
			si->sif |= FSI_MPV;
		}
		if ((ri.ebs) && FSISET(ri.sif,FSI_EBS)) {
			if (si->ebs != 0) free(si->ebs);
			si->ebs = ri.ebs;
			ri.ebs = 0;
			si->sif |= FSI_EBS;
		}
	}

	rp->sid = fp->sid;
	si->sa = mh->mac_sa;
	if (FSISET(ri.sif,FSI_STAT) && !SAMEADR(si->stat, ri.stat)) {
		si->stat = ri.stat;
		si->sif |= FSI_STAT;
		need_redraw = 1;
	}
	if (FSISET(ri.sif,FSI_STD) && !SAMEADR(si->std, ri.std)) {
		si->std = ri.std;
		si->sif |= FSI_STD;
		if (si->std.nonmaster_ct > 1)
			rp->datt = 1;
		rp->mports = si->std.master_ct;
		rp->smac = (si->r_mac_ct > 1) ? 1 : 0;
		need_redraw = 1;
	}
	if (maybe_redraw && rp == FV.magrp)
		need_redraw = 1;

	/* see iff rooted */
	register PARM_PD_PHY *pdp, *sdp;
	pdp = rp->sm.primary;
	sdp = rp->sm.secondary;
	rp->rooted = 1;
	if (((pdp && ((pdp->pc_nbr == PC_M) || (pdp->pc_type == PC_S)))) ||
	    ((sdp && ((sdp->pc_nbr == PC_M) || (sdp->pc_type == PC_S)))))
		rp->rooted = 0;
	if (rp->rooted && rp->smac) {
		pdp = rp->dm.primary;
		sdp = rp->dm.secondary;
		if (((pdp&&((pdp->pc_nbr==PC_M) || (pdp->pc_type==PC_S)))) ||
		    ((sdp && ((sdp->pc_nbr==PC_M) || (sdp->pc_type==PC_S)))))
			rp->rooted = 0;
		if (rp->rooted) {
			if (FSISET(rp->sm.sif,FSI_STAT) &&
			   ((rp->sm.stat.topology|SMT_ROOTSTA) == 0))
				rp->rooted = 0;
			else if (FSISET(rp->dm.sif,FSI_STAT) &&
			   ((rp->dm.stat.topology|SMT_ROOTSTA) == 0))
				rp->rooted = 0;
		}
	}

usif_ret:
	free_a_info(&ri);
	return(need_redraw);
}

/*
 * buf has whole frame.
 * just update it.
 */
int
Ring::buildring(register SMT_FB *fb)
{
	register RING *una, *rp, *sp = 0;
	Symbol *unasym, *rsym, *msym;
	int need_redraw = 0;
	register smt_header *fp = FBTOFP(fb);

	// if SID is not set just discard the frame.
	if (SAMEADR(fp->sid.ieee, zero_mac_addr))
		return(0);

	/* Get rp. If exists already then just update it */
	rsym = sidhash_lookup(&fp->sid);
	if (rsym == NULL) {
		msym = sahash_lookup(&fb->mac_hdr.mac_sa);
		if (msym) {
			/* ghost */
			rp = (RING *)msym->sym_data;
			assert(rp);
			if ((rp->ghost&GHOST) == 0)
				checkring(FV.RingNum, rp);
		} else {
			/* brand new */
			rp = (RING *)Malloc(sizeof(RING));
		}
	} else {
		msym = 0;
		rp = (RING *)rsym->sym_data;
		assert(rp);
		assert((rp->ghost&GHOST)==0);
	}

	need_redraw = update_ring(rp, fb);
	if (!need_redraw) {
		if (!msym && !rsym) {
			if (rp->shadow) {
				assert((rp->shadow->ghost&SHADOW) != 0);
				free_a_ring(rp->shadow);
				rp->shadow = 0;
			}
			free_a_ring(rp);
			rp = 0;
		}
		goto build_ret;
	}

#ifdef DOSHADOW
	if (rp->smac && rp->rooted == 0) {
		register PARM_PD_PHY *pdp, *sdp;
		pdp = rp->dm.primary;
		sdp = rp->dm.secondary;
		if (pdp && (pdp->pc_nbr==PC_S) || sdp && (sdp->pc_nbr==PC_S))
			rp->rooted = 0;

		sp = rp->shadow;
		if (sp == 0) {
			sp = (RING *)Malloc(sizeof(RING));
			rp->shadow = sp;
			sp->ghost |= SHADOW;
		}
		sp->sm = rp->dm;
		sp->tms = rp->tms;
		sp->datt = 0;
		sp->smac = 0;
		sp->sid = rp->sid;
	}
#endif

	/* If we reach here, it can not be a ghost! */
	rp->ghost &= ~GHOST;
	rp->cursort = FV.cursort;
	rp->health++;	// 1 point just be enter
	if (!rsym)
		rsym = sidhash_add(rp);
	if (rp->smac) {
		RING *ck1, *ck2;
		Symbol *s1, *s2;

		s1=(NULLADR(rp->sm.sa))?0:sahash_lookup(&rp->sm.sa);
		ck1 = (s1 != 0) ? (RING*)s1->sym_data : 0;
		if (ck1 && (ck1 != rp)) {
			assert(ck1 != FV.ring);
			list_insert(rp, ck1);
			if (ck1->shadow) {
				assert((ck1->shadow->ghost&SHADOW) != 0);
				list_remove(ck1->shadow);
				free_a_ring(ck1->shadow);
			}
			list_remove(ck1);
			free_a_ring(ck1);
			s1->sym_data = rp;
		}

		if (!rp->rooted) {
			s2=(NULLADR(rp->dm.sa))?0: sahash_lookup(&rp->dm.sa);
			ck2 = (s2 != 0) ? (RING*)s2->sym_data : 0;
			if (ck2 && (ck2 != ck1) && (ck2 != rp)) {
				if (!ck1)
					list_insert(rp, ck2);
				if (ck1->shadow) {
					assert((ck2->shadow->ghost&SHADOW)!=0);
					list_remove(ck2->shadow);
					free_a_ring(ck2->shadow);
				}
				list_remove(ck2);
				free_a_ring(ck2);
				s2->sym_data = rp;
			}
		}
		msym = sahash_add(rp);
	} else if (!msym)
		msym = sahash_add(rp);
	assert(rsym->sym_data == msym->sym_data);

	/* Find node's upstream nbr and insert this node next to it. */
	if (NULLADR(rp->sm.una)) {
		unasym = 0;
	} else {
		unasym = sahash_lookup(&rp->sm.una);
		if (!unasym) {
			unasym =0;
		} else if (!unasym->sym_data) {
			unasym =0;
		} else if (!((RING *)unasym->sym_data)->next ||
			   !((RING *)unasym->sym_data)->prev) {
			unasym = 0;
		}
	}

	if (unasym) {
		una = (RING *)unasym->sym_data;
		assert(una);
		assert(una->next);
		assert(una->prev);

		if (rp == una) {
			/* this is wrpped dm or piggy backed mac case */
			/* just redraw */
			need_redraw = 1;
		} else if (rp == una->next && rp->prev == una) {
			/* already good connection */
			if (!SAMEADR(una->sm.sa, rp->sm.una)) {
				assert(SAMEADR(una->dm.sa, rp->sm.una));
			}
		} else if (rp->prev == 0) {
			/* new station */
			need_redraw = 1;
			if (SAMEADR(una->sm.sa, una->next->sm.una)) {
				/* dup linked station */
				if (SAMEADR(una->sm.dna, rp->sm.sa)) {
					list_insert(rp, una);
					RING *tp = una->next;
					list_remove(tp);
					hole_insert(tp);
				} else {
					hole_insert(rp);
				}
			} else {
				/* brand new station */
				list_insert(rp, una);
			}
		} else if (need_redraw) {
			/* old station with new info */
			list_remove(rp);
			list_insert(rp, una);
		}

		if (SAMEADR(una->sm.sa, rp->sm.una))
			rp->health++;
		if (SAMEADR(una->sm.dna, rp->sm.sa))
			rp->health++;
		if (una->smac != 0 && SAMEADR(una->dm.sa, rp->sm.una))
			rp->health++;

	} else if (rp->prev == NULL) {
		/* No upstream nbr found -- create a ghost for it */
		if (!NULLADR(rp->sm.una) && !rp->smac &&
		    !(SAMEADR(rp->sm.una, rp->dm.sa) ||
		    SAMEADR(rp->sm.sa, rp->dm.una))) {
			RING *gp = (RING *) Malloc(sizeof(RING));

			gp->ghost |= GHOST;
			gp->sm.sa = rp->sm.una;
			gp->cursort = FV.cursort;
			gp->tms = rp->tms;
			(void)sahash_add(gp);
			hole_insert(gp);
			list_insert(rp, gp);
		} else {
			hole_insert(rp);
		}
		need_redraw = 1;
	}

	if (sp && sp->next == 0) {
		/* new shadow */
		list_insert(sp,rp);
	}

	if (need_redraw) {
		register int i;
		for (i = FV.RingNum, rp = FV.ring; i > 0; i--, rp = rp->next)
			SetStationColor(rp);
		FV.needsort++;
	}

build_ret:
	checkring(FV.RingNum, rp == 0 ? FV.ring : rp);
	return(need_redraw);
}

void
Ring::hole_insert(RING *np)
{
	register int i;
	register RING *rp;
	RING *hole = FV.ring;

	/* scan upstream cause una is the ONLY reliable key */
	for (i = FV.RingNum, rp = FV.ring; i > 0; i--, rp = rp->prev) {
		if (SAMEADR(rp->sm.una, rp->prev->sm.sa))
			continue;
#ifdef DOHEALTH
		if (rp->health < hole->health)
			hole = rp;
#endif
	}
	list_insert(np, hole);
}

#define move_seg(from, to, sh, st) \

int
Ring::sortring(void)
{
	register int i;
	RING *realuna;
	Symbol *unasym;
	int need_redraw = 0, wrapped = 0;
	register RING *una, *rp = FV.ring, *last = NULL, *next, *tp;

	checkring(FV.RingNum, rp);
	if (rp == 0)
		return(0);

	/* Check for expired entries */
#ifdef STASH
	if (!FV.Freeze) {
		struct timeval sample;
		register RING *exp;

		smt_gts(&sample);
		for (exp = rp->next; exp != rp; exp = next) {
			assert(exp != NULL);
			next = exp->next;
			if (((exp->ghost&SHADOW) == 0) &&
			    (exp->tms.tv_sec+FV.AgeFactor <= sample.tv_sec)) {
				sidhash_del(exp);
				sahash_del(exp);
				if (exp->shadow) {
					assert((exp->shadow->ghost&SHADOW)!=0);
					list_remove(exp->shadow);
					free_a_ring(exp->shadow);
				}
				list_remove(exp);
				free_a_ring(exp);
				need_redraw = 1;
			}
		}
		checkring(FV.RingNum, rp);
	}
#else
	if (!FV.Freeze) {
		struct timeval sample;
		register RING *exp;

		smt_gts(&sample);
		for (i = FV.RingNum, exp = FV.ring; i > 0; i--, exp = next) {
			assert(exp != NULL);
			next = exp->next;
			if (exp == FV.ring)
				continue;
			if (((exp->ghost&SHADOW) == 0) &&
			    (exp->tms.tv_sec+FV.AgeFactor <= sample.tv_sec)) {
				sidhash_del(exp);
				sahash_del(exp);
				if (exp->shadow) {
					assert((exp->shadow->ghost&SHADOW)!=0);
					list_remove(exp->shadow);
					free_a_ring(exp->shadow);
				}
				list_remove(exp);
				free_a_ring(exp);
				need_redraw = 1;
			}
		}
	}
#endif

	FV.cursort++;
	// rp = FV.ring already
	for (i = FV.RingNum; i > 0; i--, rp = una) {
		register PARM_PD_PHY *pdp, *sdp;

		una = rp->prev;

		/* Check for wrapped ring */
		pdp = rp->sm.primary;
		sdp = rp->sm.secondary;
		if ((rp->flt < LT_STAR && rp->blt < LT_STAR) && ((pdp != 0) &&
		   (pdp->pc_nbr>=PC_UNKNOWN||pdp->conn_state!=PC_ACTIVE) ||
		   ((rp->datt != 0) && (sdp != 0) &&
		   (sdp->pc_nbr>=PC_UNKNOWN||sdp->conn_state!=PC_ACTIVE)))) {
			wrapped = 1;
		}

		if ((rp->ghost&GHOST) || rp->cursort == FV.cursort) {
			last = NULL;
			continue;
		}

//#ifdef STASH
		if (rp->blt == LT_WRAP) {
			tp = FV.ring->prev;
			for (realuna = 0; tp != FV.ring; tp = tp->prev) {
				if (tp->flt != LT_WRAP)
					continue;
				realuna = tp;
				break;
			}
			if (!realuna) {
				if (tp->flt != LT_WRAP) {
					last = NULL;
					rp->cursort = FV.cursort;
					continue;
				}
				realuna = tp;
			}
		} else
//#endif
		if (SAMEADR(rp->sm.una, una->sm.sa)) {
			if (last == NULL)
				last = rp;
			rp->cursort = FV.cursort;
			continue;
		} else if ((unasym = sahash_lookup(&rp->sm.una)) == NULL) {
			last = NULL;
			continue;
		} else {
			realuna = (RING *)unasym->sym_data;
			assert(realuna);
		}

		if (last == NULL)
			last = rp;
		/* don't visit this node again*/
		rp->cursort = FV.cursort;

		/* make sure that realuna is NOT on the arc. */
		for (tp = last; tp != rp; tp = tp->prev) {
			if (tp == realuna)
				break;
		}
		if (tp != rp) {
			/* duplicate point case */
			last = rp;
			continue;
		} else if (rp == realuna) {
			/* wrpped dual MAC case */
			last = NULL;
			continue;
		}

#ifdef DOHEALTH
		/* check health here */
		if (una->next->health > rp->health)
			continue;
#endif

		/* remove arc from list */
		rp->prev->next = last->next;
		last->next->prev = rp->prev;

		/* insert arc between the una and the una's next */
		last->next = realuna->next;
		realuna->next->prev = last;

		realuna->next = rp;
		rp->prev = realuna;
		last = NULL;
		need_redraw = 1;
	}

	if (wrapped && !(FV.RingOpr&WRAPPED)) {
		FV.RingOpr |= WRAPPED;
		need_redraw = 1;
	} else if (!wrapped && (FV.RingOpr&WRAPPED)) {
		FV.RingOpr &= ~WRAPPED;
		need_redraw = 1;
	}

	for (i = FV.RingNum, rp = FV.ring; i > 0; i--, rp = rp->next)
		SetStationColor(rp);

	checkring(FV.RingNum, rp);
	return(need_redraw);
}

/*
 * start, and angles ==> 0 - 360 and counter-clockwise.
 */
int
Ring::inrange(register Coord ang, register Coord s, register Coord e)
{
	if (FV.FullScreenMode || (FV.RingNum <= SIZETHRESHOLD))
		return(1);
	if ((ang < s) && (ang+3600 > e))
		return(0);
	if ((ang > e) && (ang-3600 < s))
		return(0);
	return(1);
}

int
matchstr(char *canop, char *wildp)
{
	char *token;
	char targ[128];

	strcpy(targ, wildp);
	token = strtok(targ, "*?");
	while (token) {
		canop = strstr(canop, token);
		if (canop == NULL)
			return(-1);
		token = strtok(NULL, "*?");
	}
	return(0);
}

/*
 *
 */
int
Ring::search(int CCWise, char *buf)
{
	char addr[128];
	register int i;
	register RING *rp;

	if ((srch == 0) && ((srch = FV.ring) == 0)) {
		return(-1);
	}

	for (i = FV.RingNum, rp = srch; i > 0; i--) {
		rp = (CCWise > 0) ? rp->prev : rp->next;
		// check primary mac
		if (matchstr(rp->sm.saname, buf) == 0)
			goto srchfound;
		if (matchstr(midtoa(&rp->sm.sa), buf) == 0)
			goto srchfound;
		if (matchstr(fddi_ntoa(&rp->sm.sa, addr), buf) == 0)
			goto srchfound;

		// check secondary mac
		if (rp->smac) {
			if (matchstr(rp->dm.saname, buf) == 0)
				goto srchfound;
			if (matchstr(midtoa(&rp->dm.sa), buf) == 0)
				goto srchfound;
			if (matchstr(fddi_ntoa(&rp->dm.sa, addr), buf) == 0)
				goto srchfound;
		}
	}
	return(-1);

srchfound:
	srch = rp;
	FV.magrp = rp;
	FV.Vang = rp->ang;
	return(i);
}

/*
 *
 */
RING *
Ring::findsel(Point& p)
{
	register int i;
	register RING *rp;

	Coord wx1, wy1, wz1, wx2, wy2, wz2;
	register Coord x, y;
	register double radang;
	register float radi;	/* radius of ring */

	if (!FV.ring)
		return(0);

	x = p.getX();
	y = p.getY();
	mapw(vobj, (Screencoord)x, (Screencoord)y,
		&wx1, &wy1, &wz1, &wx2, &wy2, &wz2);
	x = (wx1+wx2)/2.0; y = (wy1+wy2)/2.0;

	if (!FV.FullScreenMode && (FV.RingNum > SIZETHRESHOLD)) {
		radi = (float)(FV.RingNum*RADIUS)/(float)SIZETHRESHOLD;
		radang = RAD(FV.Vang);
		x += (radi * (float)(cos(radang)));
		y += (radi * (float)(sin(radang)));
	} else {
		radi = RADIUS;
	}
	wx2 = x + (FV.sfact*5.0); wx1 = x - (FV.sfact*5.0);
	wy2 = y + (FV.sfact*5.0); wy1 = y - (FV.sfact*5.0);

	/* search UNAs */
	for (i = FV.RingNum, rp = FV.ring; i > 0; i--, rp = rp->next) {
		radang = rp->radian;
		if (rp->flt > LT_TWIST) {
			x = (float)cos(radang)*(radi+HALFGAP);
			y = (float)sin(radang)*(radi+HALFGAP);
		} else {
			x = (float)cos(radang)*radi;
			y = (float)sin(radang)*radi;
		}
		if (x >= wx1 && x <= wx2 && y >= wy1 && y <= wy2)
			return(rp);
	}

	return(0);
}

void
Ring::init(SMT_FB *fb)
{
	int tmp;

	if (!FV.ring)	setvobj();
	if (FV.sidtbl)
		sc_destroy(FV.sidtbl);
	FV.sidtbl = scope(512, "sid");
	if (FV.satbl)
		sc_destroy(FV.satbl);
	FV.satbl = scope(1024, "sa");
	flush_ring();

	tmp = FV.FrameSelect;
	FV.FrameSelect |= SEL_NIF;

	FV.ring = (RING*)Malloc(sizeof(RING));
	FV.ring->prev = FV.ring;
	FV.ring->next = FV.ring;
	FV.RingNum = 1;	// one and only special case
	if (!update_ring(FV.ring, fb)) {
		DBGVERB(("aborting\n"));
		abort();
	}
	FV.FrameSelect = tmp;

	(void)sidhash_add(FV.ring);
	(void)sahash_add(FV.ring);
	SetStationColor(FV.ring);
	FV.RINGWindow->redrawWindow();
	FV.MAPWindow->redrawWindow();
}
