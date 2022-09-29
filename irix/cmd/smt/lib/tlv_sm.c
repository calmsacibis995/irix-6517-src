/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.9 $
 */

#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>

#include <sm_types.h>
#include <sm_log.h>
#include <smt_parse.h>
#include <smt_asn1.h>
#include <smt_snmp_api.h>
#include <smtd_parm.h>
#include <smtd.h>
#include <smtd_fs.h>
#include <tlv.h>

#ifndef NULL
#define NULL	0
#endif

#define	CHECK_SRC(x,y) { int sts; \
	sts = tlv_parse_tl(data, length, &type, &len); \
	if (sts) \
		return(RC_PARSE); \
	src = (x)(*data); \
	if (type != (y) || len != sizeof(*src)) \
		return(RC_PARSE); }

#define	CHECK_DST(x,y) { \
	if (tlv_build_tl(data, length, (y), sizeof(x))) \
		return(RC_PARSE); \
	dst = (x *)(*data); }

#define	RET_SRC {*length -= len; *data += len; return(RC_SUCCESS);}
#define	RET_DST {*length -= sizeof(*dst); *data += sizeof(*dst); \
		return(RC_SUCCESS);}


/*
 * Upstream Nbr.
 */
int
tlv_parse_una(
	register char		**data,
	register u_long		*length,
	register LFDDI_ADDR	*una)
{
	u_long		type, len;
	PARM_UNA	*src;

	CHECK_SRC(PARM_UNA *, PTYPE_UNA);
	*una = src->una;
	RET_SRC;
}
int
tlv_build_una(
	register char		**data,
	register u_long		*length,
	register LFDDI_ADDR	*una)
{
	PARM_UNA	*dst;

	CHECK_DST(PARM_UNA, PTYPE_UNA);
	dst->pad = 0;
	dst->una = *una;
	RET_DST;
}

/*
 * Station Description.
 */
int
tlv_parse_std(
	char		**data,		/* IN - ptr to object */
	register u_long *length,	/* IN/OUT- valid bytes left */
	PARM_STD	*dst)		/* OUT */
{
	u_long		type,len;
	PARM_STD	*src;

	CHECK_SRC(PARM_STD *, PTYPE_STD);
	dst->nodeclass = src->nodeclass;
	dst->mac_ct = src->mac_ct;
	dst->nonmaster_ct = src->nonmaster_ct;
	dst->master_ct = src->master_ct;
	RET_SRC;
}
int
tlv_build_std(
	char		**data,		/* IN - ptr to object */
	register u_long *length,	/* IN/OUT- valid bytes left */
	PARM_STD	*src)		/* OUT */
{
	PARM_STD	*dst;

	CHECK_DST(PARM_STD, PTYPE_STD);
	dst->nodeclass = src->nodeclass;
	dst->mac_ct = src->mac_ct;
	dst->nonmaster_ct = src->nonmaster_ct;
	dst->master_ct = src->master_ct;
	RET_DST;
}

/*
 * Station Status.
 */
int
tlv_parse_stat(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_STAT	*dst)		/* OUT */
{
	u_long			type, len;
	register PARM_STAT	*src;

	CHECK_SRC(PARM_STAT *, PTYPE_STAT);
	dst->pad = 0;
	dst->topology = src->topology;
	dst->dupa = src->dupa;
	RET_SRC;
}
int
tlv_build_stat(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_STAT	*src)		/* IN */
{
	register PARM_STAT	*dst;

	CHECK_DST(PARM_STAT, PTYPE_STAT);
	dst->pad = 0;
	dst->topology = src->topology;
	dst->dupa = src->dupa;
	RET_DST;
}

/*
 * Message Time Stamp.
 */
int
tlv_parse_mts(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register struct timeval	*dst)		/* OUT */
{
	u_long type, len;
	register struct timeval	*src;

	CHECK_SRC(struct timeval *, PTYPE_MTS);
	dst->tv_sec = ntohl(src->tv_sec);
	dst->tv_usec = ntohl(src->tv_usec);
	RET_SRC;
}
int
tlv_build_mts(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register struct timeval	*src)		/* IN */
{
	register struct timeval	*dst;

	CHECK_DST(struct timeval, PTYPE_MTS);
	dst->tv_sec = htonl(src->tv_sec);
	dst->tv_usec = htonl(src->tv_usec);
	RET_DST;
}

/*
 * Station Policy.
 */
int
tlv_parse_sp(
	char			**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_SP	*dst)		/* OUT */
{
	u_long			type, len;
	register PARM_SP	*src;

	CHECK_SRC(PARM_SP *, PTYPE_SP);
	dst->conf_policy = ntohs(src->conf_policy);
	dst->conn_policy = ntohs(src->conn_policy);
	RET_SRC;
}
int
tlv_build_sp(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_SP	*src)		/* IN */
{
	register PARM_SP	*dst;

	CHECK_DST(PARM_SP, PTYPE_SP);
	dst->conf_policy = htons(src->conf_policy);
	dst->conn_policy = htons(src->conn_policy);
	RET_DST;
}

/*
 * Path Latency.
 */
int
tlv_parse_pl(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_PL	*dst)		/* OUT */
{
	u_long			type,len;
	register PARM_PL	*src;

	CHECK_SRC(PARM_PL *, PTYPE_PL);
	dst->indx1 = ntohs(src->indx1);
	dst->ring1 = ntohs(src->ring1);
	dst->indx2 = ntohs(src->indx2);
	dst->ring2 = ntohs(src->ring2);
	RET_SRC;
}
int
tlv_build_pl(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_PL	*src)		/* IN */
{
	register PARM_PL	*dst;

	CHECK_DST(PARM_PL, PTYPE_PL);
	dst->indx1 = htons(src->indx1);
	dst->ring1 = htons(src->ring1);
	dst->indx2 = htons(src->indx2);
	dst->ring2 = htons(src->ring2);
	RET_DST;
}

/*
 * MAC neighbors.
 */
int
tlv_parse_macnbrs(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_MACNBRS	*dst)		/* OUT */
{
	u_long			type, len;
	register PARM_MACNBRS	*src;

	CHECK_SRC(PARM_MACNBRS *, PTYPE_MACNBRS);
	dst->pad = 0;
	dst->macidx = ntohs(src->macidx);
	dst->una = src->una;
	dst->dna = src->dna;
	RET_SRC;
}
int
tlv_build_macnbrs(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_MACNBRS	*src)		/* IN */
{
	register PARM_MACNBRS	*dst;

	CHECK_DST(PARM_MACNBRS, PTYPE_MACNBRS);
	dst->pad = 0;
	dst->macidx = htons(src->macidx);
	dst->una = src->una;
	dst->dna = src->dna;
	RET_DST;
}

/*
 * Path Descriptors.
 *
 *	pd is one per phy or mac.
 *	so it is caller's responsibility to make sure
 *	output buf has enough space.
 *
 *	Also, because pd is union of phy_pd and mac_pd,
 *	it is caller's reponsibility to clear phy_pd's
 *	pad so that we can use only mac_pd for convinience.
 */
int
tlv_parse_pds(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_PD_MAC	*dst,		/* OUT */
	register u_long		*entry)		/* IN */
{
	u_long			type, len;
	register PARM_PD_MAC	*src;

	if (tlv_parse_tl(data, length, &type, &len))
		return(RC_PARSE);
	if (type != PTYPE_PDS)
		return(RC_PARSE);
	if (len > *length)
		return(RC_PARSE);

	src = (PARM_PD_MAC *)(*data);
	if (len % sizeof(*src))
		return(RC_PARSE);

	*entry = len / sizeof(*src);
	*length -= len;
	*data += len;

	while (len) {
		dst->addr = src->addr;
		dst->rid = ntohs(src->rid);
		len -= sizeof(*src);
		if ((int)len < 0)
			return(RC_PARSE);
		src++;
		dst++;
	}

	return(RC_SUCCESS);
}
int
tlv_build_pds(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_PD_MAC	*src,		/* IN */
	register u_long		entry)		/* IN */
{
	register PARM_PD_MAC	*dst;

	CHECK_DST(PARM_PD_MAC, PTYPE_PDS);
	*length -= (entry * sizeof(*src));
	*data += (entry * sizeof(*src));

	while (entry) {
		dst->addr = src->addr;
		dst->rid = htons(src->rid);
		src++;
		dst++;
		entry--;
	}
	return(RC_SUCCESS);
}

/*
 * MAC status.
 */
int
tlv_parse_mac_stat(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_MAC_STAT	*dst)		/* OUT */
{
	u_long			type, len;
	register PARM_MAC_STAT	*src;

	CHECK_SRC(PARM_MAC_STAT *, PTYPE_MAC_STAT);
	dst->pad = 0;
	dst->macidx = ntohs(src->macidx);
	dst->treq = ntohl(src->treq);
	dst->tneg = ntohl(src->tneg);
	dst->tmax = ntohl(src->tmax);
	dst->tvx = ntohl(src->tvx);
	dst->tmin = ntohl(src->tmin);
	dst->sba = ntohl(src->sba);
	dst->frame_ct = ntohl(src->frame_ct);
	dst->error_ct = ntohl(src->error_ct);
	dst->lost_ct = ntohl(src->lost_ct);
	RET_SRC;
}
int
tlv_build_mac_stat(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_MAC_STAT	*src)		/* IN */
{
	register PARM_MAC_STAT	*dst;

	CHECK_DST(PARM_MAC_STAT, PTYPE_MAC_STAT);
	dst->pad = 0;
	dst->macidx = htons(src->macidx);
	dst->treq = htonl(src->treq);
	dst->tneg = htonl(src->tneg);
	dst->tmax = htonl(src->tmax);
	dst->tvx = htonl(src->tvx);
	dst->tmin = htonl(src->tmin);
	dst->sba = htonl(src->sba);
	dst->frame_ct = htonl(src->frame_ct);
	dst->error_ct = htonl(src->error_ct);
	dst->lost_ct = htonl(src->lost_ct);
	RET_DST;
}

/*
 * PHY LEM Status.
 */
int
tlv_parse_ler(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_LER	*dst)		/* OUT */
{
	u_long			type, len;
	register PARM_LER	*src;

	CHECK_SRC(PARM_LER *, PTYPE_LER);
	dst->pad = 0;
	dst->phyidx = ntohs(src->phyidx);
	dst->pad1 = 0;
	dst->ler_cutoff = src->ler_cutoff;
	dst->ler_alarm = src->ler_alarm;
	dst->ler_estimate = src->ler_estimate;
	dst->lem_reject_ct = ntohl(src->lem_reject_ct);
	dst->lem_ct = ntohl(src->lem_ct);
	RET_SRC;
}
int
tlv_build_ler(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_LER	*src)		/* IN */
{
	register PARM_LER	*dst;

	CHECK_DST(PARM_LER, PTYPE_LER);
	dst->pad = 0;
	dst->phyidx = htons(src->phyidx);
	dst->pad1 = 0;
	dst->ler_cutoff = src->ler_cutoff;
	dst->ler_alarm = src->ler_alarm;
	dst->ler_estimate = src->ler_estimate;
	dst->lem_reject_ct = htonl(src->lem_reject_ct);
	dst->lem_ct = htonl(src->lem_ct);
	RET_DST;
}

/*
 * MAC frmae counters.
 */
int
tlv_parse_mfc(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_MFC	*dst)		/* OUT */
{
	u_long			type, len;
	register PARM_MFC	*src;

	CHECK_SRC(PARM_MFC *, PTYPE_MFC);
	dst->pad = 0;
	dst->macidx = ntohs(src->macidx);
	dst->recv_ct = ntohl(src->recv_ct);
	dst->xmit_ct = ntohl(src->xmit_ct);
	RET_SRC;
}
int
tlv_build_mfc(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_MFC	*src)		/* IN */
{
	register PARM_MFC	*dst;

	CHECK_DST(PARM_MFC, PTYPE_MFC);
	dst->pad = 0;
	dst->macidx = htons(src->macidx);
	dst->recv_ct = htonl(src->recv_ct);
	dst->xmit_ct = htonl(src->xmit_ct);
	RET_DST;
}

/*
 * MAC FRAME NOT COPIED COUNTER.
 */
int
tlv_parse_mfncc(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_MFNCC	*dst)		/* OUT */
{
	u_long			type, len;
	register PARM_MFNCC	*src;

	CHECK_SRC(PARM_MFNCC *, PTYPE_MFNCC);
	dst->pad = 0;
	dst->macidx = ntohs(src->macidx);
	dst->notcopied_ct = ntohl(src->notcopied_ct);
	RET_SRC;
}
int
tlv_build_mfncc(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_MFNCC	*src)		/* IN */
{
	register PARM_MFNCC	*dst;

	CHECK_DST(PARM_MFNCC, PTYPE_MFNCC);
	dst->pad = 0;
	dst->macidx = htons(src->macidx);
	dst->notcopied_ct = htonl(src->notcopied_ct);
	RET_DST;
}

/*
 * MAC Priority Value.
 */
int
tlv_parse_mpv(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_MPV	*dst)		/* OUT */
{
	u_long			type, len;
	register PARM_MPV	*src;

	CHECK_SRC(PARM_MPV *, PTYPE_MPV);
	dst->pad = 0;
	dst->macidx = ntohs(src->macidx);
	dst->pri0 = ntohl(src->pri0);
	dst->pri1 = ntohl(src->pri1);
	dst->pri2 = ntohl(src->pri2);
	dst->pri3 = ntohl(src->pri3);
	dst->pri4 = ntohl(src->pri4);
	dst->pri5 = ntohl(src->pri5);
	dst->pri6 = ntohl(src->pri6);
	RET_SRC;
}
int
tlv_build_mpv(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_MPV	*src)		/* OUT */
{
	register PARM_MPV	*dst;

	CHECK_DST(PARM_MPV, PTYPE_MPV);
	dst->pad = 0;
	dst->macidx = htons(src->macidx);
	dst->pri0 = htonl(src->pri0);
	dst->pri1 = htonl(src->pri1);
	dst->pri2 = htonl(src->pri2);
	dst->pri3 = htonl(src->pri3);
	dst->pri4 = htonl(src->pri4);
	dst->pri5 = htonl(src->pri5);
	dst->pri6 = htonl(src->pri6);
	RET_DST;
}

/*
 * PHY Elasticy Count.
 */
int
tlv_parse_ebs(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_EBS	*dst)		/* OUT */
{
	u_long			type, len;
	register PARM_EBS	*src;

	CHECK_SRC(PARM_EBS *, PTYPE_EBS);
	dst->pad = 0;
	dst->phyidx = ntohs(src->phyidx);
	dst->eberr_ct = ntohl(src->eberr_ct);
	RET_SRC;
}
int
tlv_build_ebs(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_EBS	*src)		/* IN */
{
	register PARM_EBS	*dst;

	CHECK_DST(PARM_EBS, PTYPE_EBS);
	dst->pad = 0;
	dst->phyidx = htons(src->phyidx);
	dst->eberr_ct = htonl(src->eberr_ct);
	RET_DST;
}

/*
 * Manufacturer data.
 *
 *	Currently no detail is defined.
 *	So treat it as just sting for the moment.
 */
int
tlv_parse_mf(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register SMT_MANUF_DATA	*dst)		/* OUT */
{
	u_long			type, len;
	register SMT_MANUF_DATA	*src;

	CHECK_SRC(SMT_MANUF_DATA *, PTYPE_MF);
	*dst = *src;
	RET_SRC;
}
int
tlv_build_mf(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register SMT_MANUF_DATA	*src)		/* IN */
{
	register SMT_MANUF_DATA	*dst;

	CHECK_DST(SMT_MANUF_DATA, PTYPE_MF);
	*dst = *src;
	RET_DST;
}

/*
 * USER DATA.
 *
 *	Currently no detail is defined.
 *	So treat it as just sting for the moment.
 */
int
tlv_parse_usr(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register USER_DATA	*dst)		/* OUT */
{
	u_long			type, len;
	register USER_DATA	*src;

	CHECK_SRC(USER_DATA *, PTYPE_USR);
	*dst = *src;
	RET_SRC;
}
int
tlv_build_usr(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register USER_DATA	*src)		/* IN */
{
	register USER_DATA	*dst;

	CHECK_DST(USER_DATA, PTYPE_USR);
	*dst = *src;
	RET_DST;
}

/*
 * Echo
 */
int
tlv_parse_echo(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register char		*dp,		/* OUT */
	register u_long		dplen)		/* IN */
{
	u_long type, len;

	if (tlv_parse_tl(data, length, &type, &len))
		return(RC_PARSE);
	if ((type != PTYPE_ECHO) || (len > dplen) || (len > *length))
		return(RC_PARSE);
	bcopy(*data, dp, len);
	*length -= len;
	*data += len;
	return(RC_SUCCESS);
}
int
tlv_build_echo(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register char		*dp,		/* IN */
	register u_long		dplen)		/* IN */
{
	if (tlv_build_tl(data, length, PTYPE_ECHO, dplen))
		return(RC_PARSE);
	bcopy(dp, *data, dplen);
	*length -= dplen;
	*data += dplen;
	return(RC_SUCCESS);
}

/*
 * Reason Code.
 */
int
tlv_parse_rc(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register u_long		*rc)		/* OUT */
{
	u_long			type, len;
	register PARM_RC	*src;

	CHECK_SRC(PARM_RC *, PTYPE_RC);
	*rc = ntohl(src->reason);
	RET_SRC;
}
int
tlv_build_rc(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register u_long		rc)		/* IN */
{
	register PARM_RC	*dst;

	CHECK_DST(PARM_RC, PTYPE_RC);
	dst->reason = htonl(rc);
	RET_DST;
}

/*
 * Return Frame Begin.
 */
int
tlv_parse_rfb(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register char		*dp,		/* OUT */
	register u_long		*dplen)		/* IN */
{
	u_long	type, len;

	if (tlv_parse_tl(data, length, &type, &len))
		return(RC_PARSE);
	if (type != PTYPE_RFB)
		return(RC_PARSE);

	if (len < *dplen)
		*dplen = len;
	bcopy(*data, dp, *dplen);
	*length -= len;
	*data += len;
	return(RC_SUCCESS);
}
int
tlv_build_rfb(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register char		*dp,		/* IN */
	register u_long		*dplen)		/* IN */
{
	if (*dplen > *length)
		*dplen = *length;
	if (tlv_build_tl(data, length, PTYPE_RFB, *dplen))
		return(RC_PARSE);
	bcopy(dp, *data, *dplen);
	*length -= *dplen;
	*data += *dplen;
	return(RC_SUCCESS);
}

/*
 * VERSION.
 *
 *	for versions, all we needs are op, hi, lo versions.
 */
int
tlv_parse_vers(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register u_long		*opvers,	/* OUT */
	register u_long		*hivers,	/* OUT */
	register u_long		*lovers)	/* OUT */
{
	u_long			type, len;
	register u_short	*usp;
	register u_long		numvers;

	if (tlv_parse_tl(data, length, &type, &len))
		return(RC_PARSE);
	if (type != PTYPE_VERS)
		return(RC_PARSE);
	if (len > *length)
		return(RC_PARSE);
	numvers = (u_long)((*data)[2]);
	*opvers = (u_long)((*data)[3]);
	usp = (u_short *)&((*data)[4]);
	*lovers = usp[0];
	*hivers = usp[numvers-1];

	*length -= len;
	*data += len;
	return(RC_SUCCESS);
}
int
tlv_build_vers(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register u_long		opvers,		/* IN */
	register u_long		hivers,		/* IN */
	register u_long		lovers)		/* IN */
{
	register int		len;
	register int		i;
	register u_short	*usp;
	register u_long		numvers;

	numvers = hivers - lovers + 1;
	len = TLV_HDRSIZE + (numvers * sizeof(short));
	if (numvers & 1)
		len += sizeof(short);		/* pad for alignment */

	if (tlv_build_tl(data, length, PTYPE_VERS, (u_long)len))
		return(RC_PARSE);

	(*data)[0] = 0;
	(*data)[1] = 0;
	(*data)[2] = numvers;
	(*data)[3] = opvers;
	usp = (u_short *)&((*data)[4]);
	for (i = lovers; i <= hivers; i++)
		usp[i - lovers] = i;
	if (numvers & 1)
		usp[numvers-1] = 0;

	*length -= len;
	*data += len;
	return(RC_SUCCESS);
}

/*
 * Extended Service Frame.
 */
int
tlv_parse_esf(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register LFDDI_ADDR	*esfid)		/* OUT */
{
	u_long			type, len;
	register PARM_ESF	*src;

	CHECK_SRC(PARM_ESF *, PTYPE_ESF);
	*esfid = src->esfid;
	RET_SRC;
}
int
tlv_build_esf(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register LFDDI_ADDR	*esfid)		/* IN */
{
	register PARM_ESF	*dst;

	CHECK_DST(PARM_ESF, PTYPE_ESF);
	dst->pad = 0;
	dst->esfid = *esfid;
	RET_DST;
}

/*
 * mac frame counter condition.
 */
int
tlv_parse_mfc_cond(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_MFC_COND	*dst)		/* OUT */
{
	u_long			type, len;
	register PARM_MFC_COND	*src;

	CHECK_SRC(PARM_MFC_COND *, PTYPE_MFC_COND);
	dst->cs = ntohs(src->cs);
	dst->macidx = ntohs(src->macidx);
	dst->mfc = ntohl(src->mfc);
	dst->mec = ntohl(src->mec);
	dst->mlc = ntohl(src->mlc);
	dst->mbfc = ntohl(src->mbfc);
	dst->mbec = ntohl(src->mbec);
	dst->mblc = ntohl(src->mblc);
	dst->mbmts.tv_sec = ntohl(src->mbmts.tv_sec);
	dst->mbmts.tv_usec = ntohl(src->mbmts.tv_usec);
	dst->mer = ntohs(src->mer);
	RET_SRC;
}
int
tlv_build_mfc_cond(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_MFC_COND	*src)		/* IN */
{
	register PARM_MFC_COND	*dst;

	CHECK_DST(PARM_MFC_COND, PTYPE_MFC_COND);
	dst->cs = htons(src->cs);
	dst->macidx = htons(src->macidx);
	dst->mfc = htonl(src->mfc);
	dst->mec = htonl(src->mec);
	dst->mlc = htonl(src->mlc);
	dst->mbfc = htonl(src->mbfc);
	dst->mbec = htonl(src->mbec);
	dst->mblc = htonl(src->mblc);
	dst->mbmts.tv_sec = htonl(src->mbmts.tv_sec);
	dst->mbmts.tv_usec = htonl(src->mbmts.tv_usec);
	dst->mer = htons(src->mer);
	RET_DST;
}

/*
 * phy ler condition.
 */
int
tlv_parse_pler_cond(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_PLER_COND	*dst)		/* OUT */
{
	u_long			type, len;
	register PARM_PLER_COND	*src;

	CHECK_SRC(PARM_PLER_COND *, PTYPE_PLER_COND);
	dst->cs = ntohs(src->cs);
	dst->phyidx = ntohs(src->phyidx);
	dst->pad = 0;
	dst->cutoff = src->cutoff;
	dst->alarm = src->alarm;
	dst->estimate = src->estimate;
	dst->ler_reject_ct = ntohl(src->ler_reject_ct);
	dst->ler_ct = ntohl(src->ler_ct);
	dst->pad1[0] = 0;
	dst->pad1[1] = 0;
	dst->pad1[2] = 0;
	dst->ble = src->ble;
	dst->blrc = ntohl(src->blrc);
	dst->blc = ntohl(src->blc);
	dst->bmts.tv_sec = ntohl(src->bmts.tv_sec);
	dst->bmts.tv_usec = ntohl(src->bmts.tv_usec);
	RET_SRC;
}
int
tlv_build_pler_cond(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_PLER_COND	*src)		/* IN */
{
	register PARM_PLER_COND	*dst;

	CHECK_DST(PARM_PLER_COND, PTYPE_PLER_COND);
	dst->cs = htons(src->cs);
	dst->phyidx = htons(src->phyidx);
	dst->pad = 0;
	dst->cutoff = src->cutoff;
	dst->alarm = src->alarm;
	dst->estimate = src->estimate;
	dst->ler_reject_ct = htonl(src->ler_reject_ct);
	dst->ler_ct = htonl(src->ler_ct);
	dst->pad1[0] = 0;
	dst->pad1[1] = 0;
	dst->pad1[2] = 0;
	dst->ble = src->ble;
	dst->blrc = htonl(src->blrc);
	dst->blc = htonl(src->blc);
	dst->bmts.tv_sec = htonl(src->bmts.tv_sec);
	dst->bmts.tv_usec = htonl(src->bmts.tv_usec);
	RET_DST;
}

/*
 * mac frame not copied condition.
 */
int
tlv_parse_mfnc_cond(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_MFNC_COND	*dst)		/* OUT */
{
	u_long			type, len;
	register PARM_MFNC_COND	*src;

	CHECK_SRC(PARM_MFNC_COND *, PTYPE_MFNC_COND);
	dst->cs = ntohs(src->cs);
	dst->macidx = ntohs(src->macidx);
	dst->ncc = ntohl(src->ncc);
	dst->rcc = ntohl(src->rcc);
	dst->bncc = ntohl(src->bncc);
	dst->bmts.tv_sec = ntohl(src->bmts.tv_sec);
	dst->bmts.tv_usec = ntohl(src->bmts.tv_usec);
	dst->brcc = ntohl(src->brcc);
	dst->nc_ratio = ntohs(src->nc_ratio);
	RET_SRC;
}
int
tlv_build_mfnc_cond(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_MFNC_COND	*src)		/* IN */
{
	register PARM_MFNC_COND	*dst;

	CHECK_DST(PARM_MFNC_COND, PTYPE_MFNC_COND);
	dst->cs = htons(src->cs);
	dst->macidx = htons(src->macidx);
	dst->ncc = htonl(src->ncc);
	dst->rcc = htonl(src->rcc);
	dst->bncc = htonl(src->bncc);
	dst->bmts.tv_sec = htonl(src->bmts.tv_sec);
	dst->bmts.tv_usec = htonl(src->bmts.tv_usec);
	dst->brcc = htonl(src->brcc);
	dst->pad = 0;
	dst->nc_ratio = htons(src->nc_ratio);
	RET_DST;
}

/*
 * mac dupa condition.
 */
int
tlv_parse_dupa_cond(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_DUPA_COND	*dst)		/* OUT */
{
	u_long			type, len;
	register PARM_DUPA_COND	*src;

	CHECK_SRC(PARM_DUPA_COND *, PTYPE_DUPA_COND);
	dst->cs = ntohs(src->cs);
	dst->macidx = ntohs(src->macidx);
	dst->dupa = src->dupa;
	dst->una_dupa = src->una_dupa;
	RET_SRC;
}
int
tlv_build_dupa_cond(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_DUPA_COND	*src)		/* IN */
{
	register PARM_DUPA_COND	*dst;

	CHECK_DST(PARM_DUPA_COND, PTYPE_DUPA_COND);
	dst->cs = htons(src->cs);
	dst->macidx = htons(src->macidx);
	dst->dupa = src->dupa;
	dst->una_dupa = src->una_dupa;
	RET_DST;
}

/*
 * UICA EVNT.
 */
int
tlv_parse_uica_evnt(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_UICA_EVNT	*dst)		/* OUT */
{
	u_long			type, len;
	register PARM_UICA_EVNT	*src;

	CHECK_SRC(PARM_UICA_EVNT *, PTYPE_UICA_EVNT);
	dst->phyidx = ntohs(src->phyidx);
	dst->pc_type = src->pc_type;
	dst->conn_state = src->conn_state;
	dst->pc_nbr = src->pc_nbr;
	dst->accepted = src->accepted;
	RET_SRC;
}
int
tlv_build_uica_evnt(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_UICA_EVNT	*src)		/* IN */
{
	register PARM_UICA_EVNT	*dst;

	CHECK_DST(PARM_UICA_EVNT, PTYPE_UICA_EVNT);
	dst->pad = 0;
	dst->phyidx = htons(src->phyidx);
	dst->pc_type = src->pc_type;
	dst->conn_state = src->conn_state;
	dst->pc_nbr = src->pc_nbr;
	dst->accepted = src->accepted;
	RET_DST;
}

/*
 * TRACE EVNT.
 */
int
tlv_parse_trac_evnt(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_TRAC_EVNT	*dst)		/* OUT */
{
	u_long			type, len;
	register PARM_TRAC_EVNT	*src;

	CHECK_SRC(PARM_TRAC_EVNT *, PTYPE_TRAC_EVNT);
	dst->pad = 0;
	dst->trace_start = src->trace_start;
	dst->trace_term = src->trace_term;
	dst->trace_prop = src->trace_prop;
	RET_SRC;
}
int
tlv_build_trac_evnt(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_TRAC_EVNT	*src)		/* IN */
{
	register PARM_TRAC_EVNT	*dst;

	CHECK_DST(PARM_TRAC_EVNT, PTYPE_TRAC_EVNT);
	dst->pad = 0;
	dst->trace_start = src->trace_start;
	dst->trace_term = src->trace_term;
	dst->trace_prop = src->trace_prop;
	RET_DST;
}

/*
 * MAC NBR changed evnt.
 */
int
tlv_parse_nbrchg_evnt(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_NBRCHG_EVNT	*dst)		/* OUT */
{
	u_long			type, len;
	register PARM_NBRCHG_EVNT	*src;

	CHECK_SRC(PARM_NBRCHG_EVNT *, PTYPE_NBRCHG_EVNT);
	dst->cs = ntohs(src->cs);
	dst->macidx = ntohs(src->macidx);
	dst->old_una = src->old_una;
	dst->una = src->una;
	dst->old_dna = src->old_dna;
	dst->dna = src->dna;
	RET_SRC;
}
int
tlv_build_nbrchg_evnt(
	register char		**data,		/* IN - ptr to object */
	register u_long		*length,	/* IN/OUT- valid bytes left */
	register PARM_NBRCHG_EVNT	*src)		/* IN */
{
	register PARM_NBRCHG_EVNT	*dst;

	CHECK_DST(PARM_NBRCHG_EVNT, PTYPE_NBRCHG_EVNT);
	dst->cs = htons(src->cs);
	dst->macidx = htons(src->macidx);
	dst->old_una = src->old_una;
	dst->una = src->una;
	dst->old_dna = src->old_dna;
	dst->dna = src->dna;
	RET_DST;
}

