/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Visual FDDI Managment System
 *
 *	$Revision: 1.30 $
 */

#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
extern "C" {
#endif
#include <osfcn.h>
#include <string.h>
#include <bstring.h>
#include <malloc.h>
#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
}
#endif
#include "fddivis.h"
#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
extern "C" {
#endif
#include <sm_log.h>
#include <smt_parse.h>
#include <smt_subr.h>
#include <sm_str.h>
#include <ma_str.h>
#include <tlv_sm.h>
#include <tlv_pr.h>
#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
}
#endif

struct wshcol {
	int text;
	int page;
	int hilite;
	int cursor;
	int selfg;
	int selbg;
};
static struct wshcol wshcoltbl[] = {
	{  7,   0,   1,   3, 27, 86},	/* 0: white on black */
	{  2,   0,   3,   1, 27, 86},	/* 1: white on black */
	{  7,   0,   3,   1, 27, 86},	/* 2: white on black */
	{  7,  41,   3,   1, 27, 86},	/* 3: white on dark grey */
	{  7,  98,   3,   1, 27, 86},	/* 4: white on dark green */
	{  7, 112,   3,   2, 27, 86},	/* 5: white on red chocolate */
	{  7, 138,   3,   1, 27, 86},	/* 6: white on med green blue */
	{  7, 145,   3,   1, 27, 86},	/* 7: white on purplish blue */
	{  0,  84, 194,   3, 27, 86},	/* 8: black on yellowgreen */
	{  0, 116, 231, 235, 27, 86},	/* 9: black on blue */
	{  0, 221,   3,   3, 27, 86},	/* 10: black on lite blue */
	{  0,  17, 160, 183, 27, 86},	/* 11: white on dark grey */
	{  0,  25,  37, 108, 27, 86},	/* 12: black on white */
	{  0, 141,  31,  35, 27, 86},	/* 13: black on light blue */
	{  7, 106,  55,  35, 27, 86},	/* 14: white on green */
	{  0, 204,   1, 224, 27, 86},	/* 15: black on purple */
	{  0, 173,   1,  94, 27, 86},	/* 16: black on flesh */
	{  0, 167,   1, 139, 27, 86},	/* 17: black on yellow green */
	{  0, 198,   1, 225, 27, 86},	/* 18: black on lite blue-green */
};
#define WSHCOLS	(sizeof(wshcoltbl)/sizeof(struct wshcol))

void
getwshcol(char *cp, int idx)
{
	register struct wshcol *wp = &wshcoltbl[idx % (long)WSHCOLS];

	sprintf(cp,"%d,%d,%d,%d,%d,%d",
		wp->text,wp->page,wp->hilite,wp->cursor,wp->selfg,wp->selbg);
}

/*
 * Draw Char str
 */
void
drawstr(Coord x, Coord y, char *msg)
{
#ifdef CYPRESS_XGL
	fmsetfont(FV.font1->getFontHandle());
#else
	fmsetfont(FV.font1);
#endif
	cmov2(x, y); fmprstr(msg);
}

void
vert3f(float x, float y, float z)
{
	float v[3];

	v[0] = x; v[1] = y; v[2] = z;
	v3f(v);
}

int hd_cc;
static char *hdf = "/usr/tmp/fddivis.log";
void
dh(char *hdr, SMT_FB *fb)
{
	if (!FV.doerrlog)
		return;
	static FILE *f;
	int i, len = hd_cc;
	if (len > sizeof(SMT_FB))
		len = sizeof(SMT_FB);

	if ((f = fopen(hdf, "a+")) == NULL)
		return;

#define STBL(lbl, itm) { \
		fprintf(f, "%s", lbl); \
		char *scp = (char*)&itm; \
		for (i = 0; i < sizeof(itm); i++, scp++) { \
			fprintf(f, " %02X", (int)(*scp)); \
		} \
		fprintf(f, "\n"); \
	}

	smt_header *fp = FBTOFP(fb);
	struct lmac_hdr *mh = FBTOMH(fb);

	time_t ts = time(0);
	fprintf(f, "\n\n****** %s", ctime(&ts));
	fprintf(f, "%s\n", hdr);
	STBL("MAC FC: ", mh->mac_fc);
	STBL("MAC DA: ", mh->mac_da);
	STBL("MAC SA: ", mh->mac_sa);
	STBL("SMT FC: ", fp->fc);
	STBL("SMT TYPE:", fp->type);
	STBL("SMT Vers:", fp->vers);
	STBL("SMT XID: ", fp->xid);
	STBL("SMT SID: ", fp->sid);
	STBL("SMT pad: ", fp->pad);
	STBL("SMT len: ", fp->len);

	char *info = (char*)FBTOINFO(fb);
	int infolen = fp->len;
	if (infolen > (hd_cc-sizeof(*mh)-sizeof(*fp)))
		infolen = hd_cc-sizeof(*mh)-sizeof(*fp);

	while (infolen > 0) {
		int i;
		short t = ntohs(*((u_short *)info));
		short l = ntohs(((u_short *)info)[1]);
		fprintf(f, "%04X %04X: ", (int)t, (int)l);
		info += 4; infolen -= 4;
		int count = (l > infolen) ? infolen : l;
		infolen -= count;
		for (i = 0; i < count; i++) {
			fprintf(f, " %02X", info[i]);
			if ((i % 16) == 15) {
				fprintf(f, "\n           ");
			}
		}
		info += count;
		if (i%16 != 0)
			fprintf(f, "\n");
	}
	fflush(f);
	fclose(f);
}

/*
 *
 */
int
update_sif(SINFO *r, SMT_FB *fb)
{
	char *info, *nextparam;
	u_long infolen, peeklen, nextlen;
	u_long opvers, hivers, lovers;
	int sts;
	u_long n, type, len, curfsi;
	u_long maxinfo;
	int nnbrs = 0;
	int nmstat = 0;
	int nler = 0;
	int nmfc = 0;
	int nmfncc = 0;
	int nmpv = 0;
	int nebs = 0;
	char hstr[64], lstr[128];
	register smt_header *fp = FBTOFP(fb);

	info = FBTOINFO(fb);
	infolen = ntohs(fp->len);
	maxinfo = (u_long)((char*)fb+sizeof(*fb));
	DBGSIF(("usif: infolen = %d\n", infolen));
	sts = RC_SUCCESS;

	while (((int)infolen > 0) && (sts == RC_SUCCESS)) {
		if ((int)info & 1) {	/* unaligned! */
			sm_log(LOG_ERR, 0, "unaligned SIF buffer");
			return(RC_PARSE);
		}

		/* peek type */
		type = ntohs(*((u_short *)info));
		peeklen = ntohs(((u_short *)info)[1]);
		nextparam = info + peeklen + TLV_HDRSIZE;
		nextlen = infolen - peeklen - TLV_HDRSIZE;
		if ((u_long)nextparam >= maxinfo) {
			sprintf(lstr, "%s -- BAD %s len=%d\n",
				fddi_ntoa(&FBTOMH(fb)->mac_sa, hstr),
				sm_ptype_str(type), peeklen);
			printf("%s", lstr);
			dh(lstr, fb);
			return(RC_PARSE);
		}

		switch (type) {
		case PTYPE_UNA:
			DBGSIF(("unif: una\n"));
			sts = tlv_parse_una(&info, &infolen, &r->una);
			curfsi = FSI_UNA;
			break;

		case PTYPE_STD:
			sts = tlv_parse_std(&info, &infolen, &r->std);
			DBGSIF(("usif: mac=%d nonmaster=%d master=%d\n",
				r->r_mac_ct,r->r_nonmaster_ct,r->r_master_ct));
			curfsi = FSI_STD;
			break;

		case PTYPE_VERS:
			DBGSIF(("unif: vers\n"));
			sts = tlv_parse_vers(&info, &infolen,
					&opvers, &hivers, &lovers);
			r->vers = (int)opvers;
			curfsi = FSI_VERS;
			break;

		case PTYPE_STAT:
			sts = tlv_parse_stat(&info, &infolen, &r->stat);
			DBGSIF(("usif: topology=%d dupa=%d\n",
					r->r_topology, r->r_dupa));
			curfsi = FSI_STAT;
			break;

		case PTYPE_SP:
			sts = tlv_parse_sp(&info, &infolen, &r->sp);
			DBGSIF(("usif: conf=%d conn=%d\n",
					r->r_conf_policy, r->r_conn_policy));
			curfsi = FSI_SP;
			break;

		case PTYPE_MACNBRS:
			nnbrs++;
			DBGSIF(("usif: macnbrs=%d\n", nnbrs));
			n = r->r_mac_ct;
			if ((n == 0) || (nnbrs > n))
				goto skipskip;
			if (!r->nbrs) {
				n *= sizeof(PARM_MACNBRS);
				r->nbrs = (PARM_MACNBRS*)Malloc(n);
				if (!r->nbrs) goto skipskip;
			}
			sts = tlv_parse_macnbrs(&info, &infolen,
						&r->nbrs[nnbrs-1]);
			curfsi = FSI_MACNBRS;
			break;

		case PTYPE_PDS:
			n = ((info[2]) << 8) + info[3];	/* peek length */
			n /= sizeof(PARM_PD_MAC);
			DBGSIF(("usif: pds=%d\n", n));
			if (n != r->r_mac_ct+r->r_master_ct+r->r_nonmaster_ct) {
				goto skipskip;
			}
			r->pds = (PARM_PD_PHY *)Malloc(n*sizeof(PARM_PD_MAC));
			if (!r->pds)
				goto skipskip;
			sts = tlv_parse_pds(&info, &infolen,
						(PARM_PD_MAC*)r->pds, &n);
			curfsi = FSI_PDS;
			break;

		case PTYPE_MAC_STAT:
			nmstat++;
			DBGSIF(("usif: nmstat=%d\n", nmstat));
			if ((r->r_mac_ct == 0) || (nmstat > r->r_mac_ct))
				goto skipskip;
			if (!r->mac_stat) {
				n = r->r_mac_ct * sizeof(PARM_MAC_STAT);
				r->mac_stat = (PARM_MAC_STAT*)Malloc(n);
				if (!r->mac_stat)
					goto skipskip;
			}
			sts = tlv_parse_mac_stat(&info, &infolen,
						&r->mac_stat[nmstat-1]);
			curfsi = FSI_MAC_STAT;
			break;

		case PTYPE_MFC:
			nmfc++;
			DBGSIF(("usif: nmfc=%d\n", nmfc));
			if ((r->r_mac_ct == 0) || (nmfc > r->r_mac_ct))
				goto skipskip;
			if (!r->mfc) {
				n = r->r_mac_ct * sizeof(PARM_MFC);
				r->mfc = (PARM_MFC*)Malloc(n);
				if (!r->mfc)
					goto skipskip;
			}
			sts = tlv_parse_mfc(&info, &infolen, &r->mfc[nmfc-1]);
			curfsi = FSI_MFC;
			break;

		case PTYPE_MFNCC:
			nmfncc++;
			DBGSIF(("usif: nmfncc=%d\n", nmfncc));
			if ((r->r_mac_ct == 0) || (nmfncc > r->r_mac_ct))
				goto skipskip;
			if (!r->mfncc) {
				n = r->r_mac_ct * sizeof(PARM_MFNCC);
				r->mfncc = (PARM_MFNCC*)Malloc(n);
				if (!r->mfncc)
					goto skipskip;
			}
			sts = tlv_parse_mfncc(&info, &infolen,
						&r->mfncc[nmfncc-1]);
			curfsi = FSI_MFNCC;
			break;

		case PTYPE_MPV:
			nmpv++;
			DBGSIF(("usif: nmpv=%d\n", nmpv));
			if ((r->r_mac_ct == 0) || (nmpv > r->r_mac_ct))
				goto skipskip;
			if (!r->mpv) {
				n = r->r_mac_ct * sizeof(PARM_MPV);
				r->mpv = (PARM_MPV*)Malloc(n);
				if (!r->mpv)
					goto skipskip;
			}
			sts = tlv_parse_mpv(&info, &infolen, &r->mpv[nmpv-1]);
			curfsi = FSI_MPV;
			break;

		case PTYPE_LER:
			nler++;
			DBGSIF(("usif: nler=%d\n", nler));
			n = r->r_master_ct+r->r_nonmaster_ct;
			if ((n == 0) || (nler > n))
				goto skipskip;
			if (!r->ler) {
				n *= sizeof(PARM_LER);
				r->ler = (PARM_LER*)Malloc(n);
				if (!r->ler)
					goto skipskip;
			}
			sts = tlv_parse_ler(&info, &infolen, &r->ler[nler-1]);
			curfsi = FSI_LER;
			break;

		case PTYPE_EBS:
			nebs++;
			DBGSIF(("usif: nebs=%d\n", nebs));
			n = r->r_master_ct+r->r_nonmaster_ct;
			if ((n == 0) || (nebs > n))
				goto skipskip;
			if (!r->ebs) {
				n *= sizeof(PARM_EBS);
				r->ebs = (PARM_EBS*)Malloc(n);
				if (!r->ebs)
					goto skipskip;
			}
			sts = tlv_parse_ebs(&info, &infolen, &r->ebs[nebs-1]);
			curfsi = FSI_EBS;
			break;

		case PTYPE_MF:
			DBGSIF(("unif: mf\n"));
			sts = tlv_parse_mf(&info, &infolen, &r->mf);
			curfsi = FSI_MF;
			break;

		case PTYPE_USR:
			DBGSIF(("unif: usr\n"));
			sts = tlv_parse_usr(&info, &infolen, &r->usr);
			curfsi = FSI_USR;
			break;

		default: ;
			if (tlv_parse_tl(&info, &infolen, &type, &len) != 0) {
				sts = RC_PARSE;
				break;
			}
			DBGSIF(("usif: skip %d bytes for type %x\n",len,type));
			info += len;
			infolen -= len;
			curfsi = 0;
			break;
		}
skipskip:
		if (info != nextparam || infolen != nextlen) {
			sprintf(lstr, "%s -- BAD %s len=%d\n",
				fddi_ntoa(&FBTOMH(fb)->mac_sa, hstr),
				sm_ptype_str(type), peeklen);
			printf("%s", lstr);
			dh(lstr, fb);
			infolen = nextlen;
			info = nextparam;
		} else
			r->sif |= (int)curfsi;
	}

	if (sts != RC_SUCCESS) {
		sm_log(LOG_DEBUG, 0,
			"usif failed: sa=%s fc=%x type=%x len=%x\n",
			getlbl(&FBTOMH(fb)->mac_sa),
			(int)fp->fc, (int)fp->type, (int)fp->len);
	}
	DBGSIF(("usif: done\n"));
	return(sts);
}

/*
 *
 */
void
dumpsif(FILE *f, SINFO *r)
{

	char str[128], str1[128];
	char str2[128], str3[128];
	register int i, j;
	register PARM_PD_PHY *pdp;
	register PARM_PD_MAC *pdm;
	register PARM_MACNBRS *nbrs;

	if ((r->sif&FSI_SIF) == 0)
		return;

	/* Show title in reverse video */
	fprintf(f, "\n\033[7mStation Configuration Information\033[0m\n\n");

	fprintf(f, "Station Descriptor: %d MAC, %d masters, %d nonmaster %s\n",
				r->r_mac_ct,
				r->r_master_ct,
				r->r_nonmaster_ct,
				sm_nodeclass_str(r->r_nodeclass));

	fprintf(f, "Supported Version: %d\n", r->vers);

	fprintf(f, "Station States: topology = %s, dupa = %x\n",
			sm_topology_str(r->r_topology, str),
			r->r_dupa);

	fprintf(f, "Station Policy: conf = %s, conn = %s\n",
			sm_conf_policy_str(r->r_conf_policy, str),
			sm_conn_policy_str(r->r_conn_policy, str1));

	if ((r->r_mac_ct>0) && (r->nbrs!=0) && FSISET(r->sif,FSI_MACNBRS)) {
		nbrs = r->nbrs;
		for (i = 0, nbrs = r->nbrs; i < r->r_mac_ct; i++, nbrs++) {
			fprintf(f,
			"MAC%d Nbrs:      una=%s %s\n\t        dna=%s %s\n",
				nbrs->macidx,
				fddi_ntoa(&nbrs->una,str),
				!fddi_ntohost(&nbrs->una,str1) ? str1 : "",
				fddi_ntoa(&nbrs->dna,str2),
				!fddi_ntohost(&nbrs->dna,str3) ? str3 : "");
		}
	}

	if (!r->pds || !FSISET(r->sif, FSI_PDS))
		goto sif_ret;

	fprintf(f, "Path Descriptors:\n");
	for (i=0, pdp=r->pds;
	     i < (r->r_master_ct+r->r_nonmaster_ct);
	     i++,pdp++) {
		fprintf(f, "\tport%d:  pc=%s state=%s pc_nbr=%s"
			" remotemac=%d conn_rid=%d\n",
			i+1,
			ma_pc_type_str((enum fddi_pc_type)pdp->pc_type),
			sm_conn_state_str((PHY_CONN_STATE)pdp->conn_state),
			ma_pc_type_str((enum fddi_pc_type)pdp->pc_nbr),
			pdp->rmac_indecated,
			(int)pdp->conn_rid);
	}
	for (j=0, pdm=(PARM_PD_MAC*)pdp; j<r->r_mac_ct; j++,pdm++) {
		fprintf(f, "\tmac%d:   addr=%s  %s\n",
			i+j+1,
			fddi_ntoa(&pdm->addr, str),
			!fddi_ntohost(&pdm->addr, str1) ? str1 : "");
		fprintf(f, "\t        conn_rid=%d\n", (int)pdm->rid);
	}

sif_ret:
	fflush(f);
}


void
paintsif(RING *rp, float l, float h)
{

	char str[128];
#ifdef STASH
	char str1[128];
#endif
	char str2[128];
	register int i, j;
	SINFO *r = &rp->sm;

	if ((r->sif&FSI_SIF) == 0)
		return;

#ifdef CYPRESS_XGL
	fmsetfont(FV.font2->getFontHandle());
#else
	fmsetfont(FV.font2);
#endif

	pushmatrix();
	c3i(ltbluecol);
	sprintf(str2,"%s (SMT version %d)",sidtoa(&rp->sid),r->vers);
	cmov(l, h, 10.0); fmprstr(str2); h -= 1.0;

	if (FSISET(r->sif, FSI_MF)) {
		sprintf(str2, "%.29s", r->mf.manf_data);
		cmov(l, h, 10.0); fmprstr(str2); h -= 1.0;
	}
	if (FSISET(r->sif, FSI_USR)) {
		sprintf(str2, "%.32s", r->usr.b);
		cmov(l, h, 10.0); fmprstr(str2); h -= 1.0;
	}
	cmov(l, h, 10.0); h -= 1.0; /* blank line */

	c3i(goldcol);
	if (FSISET(r->sif, FSI_STAT)) {
		u_long topology = r->r_topology;
		sprintf(str2, "Station topology: %s",
				sm_topology_str(topology, str));
		cmov(l, h, 10.0); fmprstr(str2); h -= 1.0;
		if (r->r_dupa) {
			c3i(redcol);
			sprintf(str2, "Duplicate address detected");
			cmov(l, h, 10.0); fmprstr(str2); h -= 1.0;
			c3i(goldcol);	/* gold */
		}
	}
	if (FSISET(r->sif, FSI_PDS)) {
		register PARM_PD_PHY *pdp;
		register PARM_PD_MAC *pdm;
		register PARM_MACNBRS *nbrs;
		PARM_LER	*ler = r->ler;

		for (i=0, pdp=r->pds;
		     pdp && i < (r->r_master_ct + r->r_nonmaster_ct);
		     i++, pdp++) {

			sprintf(str2, "port%d:  %s   %s -> %s  output %s=%d",
				i+1,
				sm_conn_state_str((PHY_CONN_STATE)pdp->conn_state),
				ma_pc_type_str((enum fddi_pc_type)pdp->pc_type),
				ma_pc_type_str((enum fddi_pc_type)pdp->pc_nbr),
		(int)pdp->conn_rid > (r->r_master_ct+r->r_nonmaster_ct) ?
			"MAC" : "PORT",
				(int)pdp->conn_rid);
			cmov(l, h, 10.0); fmprstr(str2); h -= 1.0;
			if (ler) {
				sprintf(str2,
					"%*sLink error rate=%d, LEM count=%d",
					12, " ", ler->ler_estimate,
					ler->lem_ct);
				cmov(l, h, 10.0); fmprstr(str2); h -= 1.0;

				if ((ler->ler_estimate <= ler->ler_alarm) &&
				    (ler->ler_estimate > MIN_LER)) {
					sprintf(str, "%*sLink error %s",
					    12, " ",
					    (ler->ler_cutoff >=
						    ler->ler_estimate) ?
					    "Cutoff!" : "Alarm!");

					c3i(redcol);
					cmov(l, h, 10.0);
					fmprstr(str);
					h -= 1.0;
					c3i(goldcol);
				}
				ler++;
			}
		}
		cmov(l, h, 10.0); h -= 1.0; /* blank line */

		c3i(greencol);
		nbrs = r->nbrs;
		for (j=0,pdm=(PARM_PD_MAC*)pdp;pdm && j<r->r_mac_ct; j++,pdm++){
			sprintf(str2, "mac%d:  %s   output %s=%d",
				i+j+1,
				getlbl(&pdm->addr),
			(int)pdm->rid > (r->r_master_ct+r->r_nonmaster_ct) ?
			"MAC" : "PORT",
				(int)pdm->rid);
			cmov(l, h, 10.0); fmprstr(str2); h -= 1.0;
			if (nbrs) {
				sprintf(str2, "            upstream=%s",
					getlbl(&nbrs->una));
				cmov(l, h, 10.0); fmprstr(str2); h -= 1.0;
				sprintf(str2, "            downstream=%s",
					getlbl(&nbrs->dna));
				cmov(l, h, 10.0); fmprstr(str2); h -= 1.0;
				nbrs++;
			}
		}
	}
#ifdef STASH
	if (FSISET(r->sif, FSI_SP)) {
		sprintf(str2, "Config policy: %s",
			sm_conf_policy_str(r->r_conf_policy, str));
		cmov(l, h, 10.0); fmprstr(str2); h -= 1.0;

		sprintf(str2, "Connection policy: %s",
			sm_conn_policy_str(r->r_conn_policy, str1));
		cmov(l, h, 10.0); fmprstr(str2); h -= 1.0;
	}
	if (FSISET(r->sif, FSI_MAC_STAT)) {
		PARM_MAC_STAT	*mstat;

		sprintf(str2, "MAC%d Status:treq=%3.3f tneg=%3.3f tmax=%3.3f tvx=%3.3f tmin=%3.3f",
			r->mac_stat->macidx,
			FROMBCLK(r->mac_stat->treq),
			FROMBCLK(r->mac_stat->tneg),
			FROMBCLK(r->mac_stat->tmax),
			FROMBCLK(r->mac_stat->tvx),
			FROMBCLK(r->mac_stat->tmin));
		cmov(l, h, 10.0); fmprstr(str2); h -= 1.0;

		sprintf(str2, "tsba=%d, frame_ct=%d, err_ct=%d, lost_ct=%dn",
			r->mac_stat->sba,
			r->mac_stat->frame_ct,
			r->mac_stat->error_ct,
			r->mac_stat->lost_ct);
		cmov(l, h, 10.0); fmprstr(str2); h -= 1.0;
	}
	if (FSISET(r->sif, FSI_MFC)) {
		PARM_MFC	*mfc;
	}
	if (FSISET(r->sif, FSI_MFNCC)) {
		PARM_MFNCC	*mfncc;
	}
	if (FSISET(r->sif, FSI_MPV)) {
		PARM_MPV	*mpv;
	}
	if (FSISET(r->sif, FSI_EBS)) {
		PARM_EBS	*ebs;
	}

#endif
	popmatrix();
}

/*
 *
 */
int
ler_alarm(register SINFO *r, int query)
{
	register PARM_LER	*ler;

	if ((r->sif&FSI_SIF) == 0 || !FSISET(r->sif, FSI_PDS) || !(ler=r->ler))
		return(0);

	if (query == 0) {
		register int i;
		for (i=0; i < (r->r_master_ct + r->r_nonmaster_ct); i++,ler++) {
			if ((ler->ler_estimate <= ler->ler_alarm) &&
			    (ler->ler_estimate > MIN_LER))
				return(1);
		}
	} else if (--query < (r->r_master_ct + r->r_nonmaster_ct) &&
		   ler[query].ler_estimate > MIN_LER &&
		   ler[query].ler_estimate <= ler[query].ler_alarm) {
			return(1);
	}
	return(0);
}

/* ---------------------------------------------------------------------- */

static char *sifname;
static char sif_tmplt[] =  "/tmp/.ringsifXXXXXX";
static char *tfname;
static char dump_tmplt[] =  "/tmp/.ringdumpXXXXXX";

void
cleanup_tmp(void)
{
	if (tfname)
		unlink(tfname);
	if (sifname)
		unlink(sifname);
}

static char *
win_pos(int x, int y, int xoff, int yoff, register int *cnt)
{
	static char pos[20];

	sprintf(pos, "%d,%d", x + (*cnt * xoff), y + (*cnt * yoff));
	*cnt = (*cnt + 1) % 5;
	return pos;
}

#define STAT_X_ORIGIN	(int)(SCRXMAX-SCRYMAX+AEDGE)
#define STAT_Y_ORIGIN	(AEDGE)
#define STAT_X_DIF	10
#define STAT_Y_DIF	10

#define DUMP_X_ORIGIN	(int)(SCRXMAX-SCRYMAX+AEDGE+100)
#define DUMP_Y_ORIGIN	(AEDGE)
#define DUMP_X_DIF	10
#define DUMP_Y_DIF	10

#define SIF_X_ORIGIN	520
#define SIF_Y_ORIGIN	AEDGE
#define SIF_X_DIF	-10
#define SIF_Y_DIF	10

static char *wshfont = "Courier9";

/*
 *
 */
void
dostat(char *iphase)
{
	pid_t id;
	char colopt[128], *pos;
	static int statwincnt;

	if (strcmp(FV.Agent, "localhost")) {
		FV.post(NULLMSG, "SMT stat is only for localhost");
		return;
	}

	pos = win_pos(STAT_X_ORIGIN, STAT_Y_ORIGIN, STAT_X_DIF, STAT_Y_DIF,
			&statwincnt);

	if (id = fork()) {
		if (id < 0)
			printf("dostat fork failed\n");
		return;
	}

	/* child */
	getwshcol(colopt, 16);
	execlp("/bin/wsh",
	     "wsh",
	     "-d",
	     "-f", wshfont,
	     "-s", "80,24",
	     "-n", "smtstat",
	     "-t", "Station Status",
	     "-C", colopt,
	     "-p", pos,
	     "-c", "/usr/etc/smtstat", "-s", "-I", iphase,
	     (char*)0);
	perror("exec stat");
	abort();

}



struct dmptbl {
	char	*title;
	u_long	mask;
	int	floattype;
};
static struct dmptbl dmp[] = {
	{"Logical Ring Dump", 0},
	{"Treq Dump", 0},
	{"Tneg Dump", 0},
	{"Tmax Dump", 0},
	{"Tvx Dump", 0},
	{"Tmin Dump", 0},
	{"SBA Dump", 0},
	{"Frame Count Dump", 0},
	{"Error Count Dump", 0},
	{"Lost Count Dump", 0},
	{"LER Dump", 0},
	{"LEM Count Dump", 0},
	{"Receive Count Dump", 0},
	{"Transmit Count Dump", 0},
	{"Not Copied Count Dump", 0},
	{"EBS Dump", 0}
};

void
dumpval(FILE *f, SINFO *r, int cmd)
{
	switch (cmd) {
	case DUMP_TREQ:
		fprintf(f,"%-3.3f",
			r->mac_stat ? FROMBCLK(r->mac_stat->treq) : 0);
		break;
	case DUMP_TNEG:
		fprintf(f,"%-3.3f",
			r->mac_stat ? FROMBCLK(r->mac_stat->tneg) : 0);
		break;
	case DUMP_TMAX:
		fprintf(f,"%-3.3f",
			r->mac_stat ? FROMBCLK(r->mac_stat->tmax) : 0);
		break;
	case DUMP_TVX:
		fprintf(f,"%-3.3f",
			r->mac_stat ? FROMBCLK(r->mac_stat->tvx) : 0);
		break;
	case DUMP_TMIN:
		fprintf(f,"%-3.3f",
			r->mac_stat ? FROMBCLK(r->mac_stat->tmin) : 0);
		break;
	case DUMP_SBA:
		fprintf(f,"%-10u", r->mac_stat ? r->mac_stat->sba : 0);
		break;
	case DUMP_FRAME:
		fprintf(f,"%-10u", r->mac_stat?r->mac_stat->frame_ct:0);
		break;
	case DUMP_ERR:
		fprintf(f,"%-10u", r->mac_stat?r->mac_stat->error_ct:0);
		break;
	case DUMP_LOST:
		fprintf(f,"%-10u", r->mac_stat?r->mac_stat->lost_ct:0);
		break;
	case DUMP_LER:
		fprintf(f,"%-10u",r->ler?r->ler->ler_estimate:0);
		break;
	case DUMP_LEM:
		fprintf(f,"%-10u",r->ler?r->ler->lem_ct:0);
		break;
	case DUMP_RECV:
		fprintf(f,"%-10u",r->mfc?r->mfc->recv_ct:0);
		break;
	case DUMP_XMIT:
		fprintf(f,"%-10u",r->mfc?r->mfc->xmit_ct:0);
		break;
	case DUMP_NCC:
		fprintf(f,"%-10u",
			r->mfncc?r->mfncc->notcopied_ct:0);
		break;
	case DUMP_EBS:
		fprintf(f,"%-10u", r->ebs?r->ebs->eberr_ct:0);
		break;
	}
}

/*
 *
 */
void
dumpring(int nodes, RING *ring, int cmd)
{
	int i;
	pid_t id;
	FILE *f;
	RING *rp;
	char unaa[128], saa[128], *pos;
	static int dumpwincnt;

	if (!tfname && (tfname = mktemp(dump_tmplt)) == NULL) {
		printf("mktemp failed\n");
		return;
	}
	if ((f = fopen(tfname, "w")) == NULL) {
		printf("fopen %s failed\n", tfname);
		return;
	}

	/* Print title in reverse video (^[[7m) */
	fprintf(f, "\n\033[7m%s\033[0m\n\n", dmp[cmd].title);
	if (!cmd) {
		fprintf(f, "%-23s %-5s  %-17s    %-17s\n",
			"Station ID", "#MACs", "Upstream Nbr", "MAC Address");
	} else if (cmd <= DUMP_TMIN) {
		fprintf(f, "%-23s %-7s    %-17s\n",
			"Station ID", "Value", "MAC Address");
	} else {
		fprintf(f, "%-23s %-10s    %-17s\n",
			"Station ID", "Value", "MAC Address");
	}

	for (i=nodes, rp=ring; i > 0; i--, rp = rp->next) {
		if (FV.FddiBitMode != 0)
			strcpy(saa, sidtoa(&rp->sid));
		else
			strcpy(saa, fddi_sidtoa(&rp->sid,unaa));
		fprintf(f, "%23s   ", saa);

		if (!cmd) {
			if (!FV.SymAdrMode ||
			    fddi_ntohost(&rp->sm.una, unaa) != 0) {
				if (FV.FddiBitMode != 0)
					strcpy(unaa,
						midtoa(&rp->sm.una));
				else
					fddi_ntoa(&rp->sm.una, unaa);
			}
			fprintf(f, "%-4d %-17s -> ", rp->smac+1, unaa);
		} else {
			dumpval(f, &rp->sm, cmd);
		}

		if (!FV.SymAdrMode || fddi_ntohost(&rp->sm.sa, saa) != 0) {
			if (FV.FddiBitMode != 0)
				strcpy(saa, midtoa(&rp->sm.sa));
			else
				fddi_ntoa(&rp->sm.sa, saa);
		}
		fprintf(f, "%-17s\n", saa);

		if (rp->smac != 0) {
			fprintf(f,"%23s   "," ");
			if (!cmd) {
				if (!FV.SymAdrMode ||
				     fddi_ntohost(&rp->dm.una,unaa)!=0) {
				    if (FV.FddiBitMode != 0)
					strcpy(unaa,
						midtoa(&rp->dm.una));
					else
						fddi_ntoa(&rp->dm.una, unaa);
				}
				fprintf(f,"%6s %-17s -> "," ",unaa);
			} else {
				dumpval(f, &rp->dm, cmd);
			}

			if (!FV.SymAdrMode||fddi_ntohost(&rp->dm.sa,saa) != 0) {
				if (FV.FddiBitMode != 0)
					strcpy(saa,midtoa(&rp->dm.sa));
				else
					fddi_ntoa(&rp->dm.sa, saa);
			}

			fprintf(f, "%-17s\n",  saa);
		}
	}
	fflush(f);
	fclose(f);

	pos = win_pos(DUMP_X_ORIGIN, DUMP_Y_ORIGIN, DUMP_X_DIF, DUMP_Y_DIF,
			&dumpwincnt);

	if (id = fork()) {
		if (id < 0)
			printf("ring dump fork failed\n");
		return;
	}

	/* child */
	getwshcol(unaa, 18);
	execlp("/bin/wsh",
	     "wsh",
	     "-d",
	     "-H",
	     "-f", wshfont,
	     "-s", "80,30",
	     "-n", dmp[cmd].title,
	     "-t", dmp[cmd].title,
	     "-C", unaa,
	     "-p", pos,
	     "-c", "/bin/cat", tfname,
	     (char*)0);
	perror("exec dump");
	abort();
}


/*
 * print SIF packets.
 */

void
pr_sif(char *buf, int cc, int rnum, RING *rp)
{
	FILE *f;
	pid_t id;
	char colopt[128];
	char *pos;
	register int i;
	register RING *r;
	register struct lmac_hdr *mh = FBTOMH(buf);
	static int sifwincnt;

	if (!sifname && (sifname = mktemp(sif_tmplt)) == NULL) {
		printf("mktemp failed\n");
		return;
	}
	if ((f = fopen(sifname, "w")) == NULL) {
		printf("fopen failed\n");
		return;
	}

	for (r = rp, i = 0; i < rnum; r = r->next, i++) {
		if ((r->sm.sif&FSI_SIF) != 0 &&
			SAMEADR(mh->mac_sa, r->sm.sa)) {
			dumpsif(f, &r->sm);
			break;
		}
		if ((r->dm.sif&FSI_SIF) != 0 &&
			SAMEADR(mh->mac_sa, r->dm.sa)) {
			dumpsif(f, &r->dm);
			break;
		}
	}

	if (cc > 0) {
		fprintf(f,
			"\n\n\033[7mStation Operation Information\033[0m\n\n");
		pr_parm(f, buf, cc);
	}
	fclose(f);


	pos = win_pos(SIF_X_ORIGIN, SIF_Y_ORIGIN, SIF_X_DIF, SIF_Y_DIF,
			&sifwincnt);

	if (id = fork()) {
		if (id < 0)
			printf("sif_print: fork failed.\n");
		return;
	}

	/* child */
	getwshcol(colopt, 6);
	execlp("/bin/wsh",
	     "wsh",
	     "-d",
	     "-H",
	     "-f", wshfont,
	     "-s", "80,30",
	     "-n", "SIF",
	     "-t", "Station Info",
	     "-C", colopt,
	     "-p", pos,
	     "-c", "/bin/cat", sifname,
	     (char*)0);
	perror("exec sif");
	abort();
}


void
visit(LFDDI_ADDR *n)
{
	pid_t id;
	char hname[128];
	char colopt[128];
	static int wshcolidx;

	if (fddi_ntohost(n, hname) != 0) {
		char f[30];
		(void)fddi_ntoa(n, f);
		sprintf(colopt, "No host name for %s", f);
		dopost(colopt);
		return;
	}

	wshcolidx++;

	if (id = fork()) {
		if (id < 0) {
			sprintf(colopt, "visit failed for %s", hname);
			dopost(colopt);
		}
		return;
	}

	/* child */
	getwshcol(colopt, wshcolidx);
	execlp("/bin/wsh",
	     "wsh",
	     "-d",
	     "-E",
	     "-f", wshfont,
	     "-s", "80,40",
	     "-t", hname,
	     "-n", hname,
	     "-C", colopt,
	     "-c", "rlogin", hname, "-l", "guest",
	     (char*)0);
	perror("exec visit");
	abort();
}

char *
getlbl(LFDDI_ADDR *addr)
{
	static char str1[128];

	if (!FV.SymAdrMode || fddi_ntohost(addr, str1) != 0) {
		if (FV.FddiBitMode != 0)
			return(midtoa(addr));
		(void)fddi_ntoa(addr, str1);
	}
	return(str1);
}

char *
getinfolbl(SINFO *ip) {
	static char ilstr[128];

	if (!FV.SymAdrMode || NULLADR(ip->sa))
		return(getlbl(&ip->sa));

	if (ip->saname[0] == 0) {
		if (fddi_ntohost(&ip->sa, ilstr) != 0) {
			if (FV.FddiBitMode != 0)
				return(midtoa(&ip->sa));
			(void)fddi_ntoa(&ip->sa, ilstr);
			return(ilstr);
		}
		strcpy(ip->saname, ilstr);
	}
	return(ip->saname);
}


void
SetStationColor(RING *rp)
{
	rp->lcp = blackcol;
	rp->acp = redcol;

	if (SAMEADR(rp->sm.una, rp->prev->sm.sa)) {
		rp->pcp = tancol;
		rp->scrcp = ltbluecol;
	} else {
		rp->pcp = dgreycol;
		rp->scrcp = bgreycol;
	}

	if (rp == FV.ring) {
		rp->scrcp = purplecol;
		rp->acp = bbluecol;
	}
	if (rp->ghost&GHOST) {
		rp->scrcp = blackcol;
	}

	if (ler_alarm(&rp->sm, 0)) {
		rp->pcp = redcol;
		rp->acp = greycol;
	}

	if (((rp->sm.sif&FSI_SIF) != 0) || ((rp->dm.sif&FSI_SIF) != 0)) {
		rp->litecp = greencol;
	} else {
		rp->litecp = blackcol;
	}

	register PARM_PD_PHY *pdp, *sdp;
	pdp = rp->sm.primary;
	sdp = rp->sm.secondary;

	/* set upstream link first */
	if ((rp->datt != 0) && (sdp != 0)) {
		if ((sdp->pc_nbr>=PC_UNKNOWN || sdp->conn_state!=PC_ACTIVE)) {
			rp->blcp = redcol;	/* backward link color */
			rp->bls = 0;		/* backward link style */
			if ((sdp->pc_nbr==PC_M) || (pdp && pdp->pc_nbr==PC_M))
				rp->blt = LT_STARWRAP;	/* backward link type */
			else
				rp->blt = LT_WRAP;	/* backward link type */
		} else {
			rp->blcp = whitecol;	/* backward link color */
			rp->bls = 0;		/* backward link style */
			if ((sdp->pc_nbr==PC_M) || (pdp && pdp->pc_nbr==PC_M))
				rp->blt = LT_STAR;
			else
				rp->blt = LT_LINK;	/* backward link type */
		}
	}

	/* set downstream link */
	if (pdp&&(pdp->pc_nbr>=PC_UNKNOWN||pdp->conn_state!=PC_ACTIVE)) {
		rp->flcp = redcol;	/* forward link color */
		rp->fls = 0;		/* forward link style */
		if ((pdp->pc_nbr==PC_M) || (sdp && sdp->pc_nbr==PC_M))
			rp->flt = LT_STARWRAP;	/* forward link type */
		else
			rp->flt = LT_WRAP;	/* forward link type */
	} else {
		if (!SAMEADR(rp->next->sm.una, rp->sm.sa)) {
			rp->fls = DASHED;
			rp->fsls = DASHED;
		} else {
			rp->fls = 0;
			rp->fsls = 0;
		}

		if (rp->datt != 0 &&
		   ((rp->sm.r_topology&(SMT_AA_TWIST|SMT_BB_TWIST)) != 0)) {
			rp->flcp = whitecol;
			rp->flt = LT_TWIST;
		} else {
			if (pdp && pdp->pc_nbr==PC_M)
				rp->flt = LT_STAR;
			else
				rp->flt = LT_LINK;
			if (ler_alarm(&rp->next->sm, 2))
				rp->flcp = redcol;
			else
				rp->flcp = whitecol;
			if (ler_alarm(&rp->sm, 1))
				rp->fslcp = redcol;
			else
				rp->fslcp = whitecol;
		}
	}

#ifdef BACKTRACE
	int hitroot;
	register RING *sp, *foundp;

	if (rp->fls == DASHED) {
		/* see if any dm's multi mac's una matches sa
		   upto the first rooted node */
		sp = rp->next;
		foundp = 0;
		hitroot = 0;
		while ((sp != rp) && (hitroot == 0)) {
			if (sp->smac) {
				if (SAMEADR(sp->sm.una, rp->sm.sa) ||
				    SAMEADR(sp->dm.una, rp->sm.sa)) {
					foundp = sp;
					break;
				}
				if (rp->smac) {
					if (SAMEADR(sp->sm.una, rp->dm.sa) ||
					    SAMEADR(sp->dm.una, rp->dm.sa)) {
						foundp = sp;
						break;
					}
				}
			}
			hitroot = sp->rooted;
			sp = sp->next;
		}
		if (foundp) {
			rp->fls = 0;
			rp->fsls = 0;
		}
	}

	if (rp->pcp == dgreycol) {
		/* see if any dm's multi mac's una matches sa
		   upto the first rooted node */
		sp = rp->next;
		foundp = 0;
		hitroot = 0;
		while ((sp != rp) && (hitroot == 0)) {
			if (sp->smac) {
				if (SAMEADR(sp->sm.sa, rp->sm.una) ||
				    SAMEADR(sp->dm.sa, rp->sm.una)) {
					foundp = sp;
					break;
				}
				if (rp->smac) {
					if (SAMEADR(sp->sm.sa, rp->dm.una) ||
					    SAMEADR(sp->dm.sa, rp->dm.una)) {
						foundp = sp;
						break;
					}
				}
			}
			hitroot = sp->rooted;
			sp = sp->next;
		}
		if (foundp) {
			rp->pcp = tancol;
			rp->scrcp = ltbluecol;
		}
	}
#endif
	if ((rp->ghost&SHADOW) != 0) {
		rp->flt = LT_STAR;
		rp->blt = LT_STAR;
	}
}

#ifdef CHECKRING
/*
 *
 */
void
checkring(int nodes, RING *ring)
{
	FILE *f;
	RING *rp;
	int id, i, realnodes;
	char unaa[128], saa[128], *pos;
	static int dumpwincnt;

	if (ring == 0) {
		realnodes = 0;
	} else {
		realnodes = 1;
		for (rp = ring->next; rp != ring; rp = rp->next) {
			realnodes++;
			if (realnodes > nodes+30)
				break;
		}

	}
	if (nodes == realnodes)
		goto chk_ret;
	else if (FV.Dreplay) {
		printf(
		    "\n\033[7mLogical Ring Dump, %d nodes(real=%d)\033[0m\n\n",
		    nodes, realnodes);
		return;
	}

	if (!tfname && (tfname = mktemp(dump_tmplt)) == NULL) {
		printf("mktemp failed\n");
		goto chk_ret;
	}
	if ((f = fopen(tfname, "w")) == NULL) {
		printf("fopen %s failed\n", tfname);
		goto chk_ret;
	}

	/* Print title in reverse video (^[[7m) */
	fprintf(f, "\n\033[7mLogical Ring Dump, %d nodes(real=%d)\033[0m\n\n",
		nodes, realnodes);

	fprintf(f, "%-23s %-5s  %-17s    %-17s\n",
		"Station ID", "#MACs", "Upstream Nbr", "MAC Address");
	if (realnodes == 0)
		nodes = realnodes;
	for (i = realnodes, rp=ring; i > 0; i--, rp = rp->next) {
		if (FV.FddiBitMode != 0)
			strcpy(saa, sidtoa(&rp->sid));
		else
			strcpy(saa, fddi_sidtoa(&rp->sid,unaa));
		fprintf(f, "%23s   %-4d ", saa, rp->smac+1);

		if (!FV.SymAdrMode || fddi_ntohost(&rp->sm.una, unaa) != 0) {
			if (FV.FddiBitMode != 0)
				strcpy(unaa, midtoa(&rp->sm.una));
			else
				fddi_ntoa(&rp->sm.una, unaa);
		}

		if (!FV.SymAdrMode || fddi_ntohost(&rp->sm.sa, saa) != 0) {
			if (FV.FddiBitMode != 0)
				strcpy(saa, midtoa(&rp->sm.sa));
			else
				fddi_ntoa(&rp->sm.sa, saa);
		}
		fprintf(f, "%-17s -> %-17s\n", unaa, saa);

		if (rp->smac != 0) {
			if (!FV.SymAdrMode||fddi_ntohost(&rp->dm.una,unaa)!=0) {
				if (FV.FddiBitMode != 0)
					strcpy(unaa,
						midtoa(&rp->dm.una));
				else
					fddi_ntoa(&rp->dm.una, unaa);
			}

			if (!FV.SymAdrMode||fddi_ntohost(&rp->dm.sa,saa) != 0) {
				if (FV.FddiBitMode != 0)
					strcpy(saa,midtoa(&rp->dm.sa));
				else
					fddi_ntoa(&rp->dm.sa, saa);
			}

			fprintf(f, "%23s %6s %-17s -> %-17s\n",
				" ", " ", unaa, saa);
		}
	}
	fflush(f);
	fclose(f);

	pos = win_pos(DUMP_X_ORIGIN, DUMP_Y_ORIGIN, DUMP_X_DIF, DUMP_Y_DIF,
			&dumpwincnt);

	if (id = fork()) {
		if (id < 0)
			printf("ring dump fork failed\n");
chk_ret:
		if (nodes != realnodes)
			abort();
		return;
	}

	/* child */
	getwshcol(unaa, 18);
	execlp("/bin/wsh",
	     "wsh",
	     "-d",
	     "-H",
	     "-f", wshfont,
	     "-s", "80,30",
	     "-n", "Ring Dump",
	     "-t", "Ring Dump",
	     "-C", unaa,
	     "-p", pos,
	     "-c", "/bin/cat", tfname,
	     (char*)0);
	perror("exec dump");
	abort();
}
#endif
