/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.23 $
 */

#include <errno.h>
#include <stdlib.h>
#include <bstring.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>

#include <sm_types.h>
#include <sm_log.h>
#include <smt_asn1.h>
#include <smt_parse.h>

#include <smtd_parm.h>
#include <smtd.h>
#include <tlv_get.h>
#include <smtd_fs.h>
#include <smtd_svc.h>
#include <smt_subr.h>
#include <smtd_nn.h>
#include <ma.h>

static void smt_frame(SMT_MAC *, void *, int);


static int
send_frame(SMT_MAC *mac, char *dp, int len)
{
	int cc;

	/* Terrible kludge: board cannot transmit & receive at same time */
	if (!bcmp(&FBTOMH(dp)->mac_da,&mac->addr,sizeof(FBTOMH(dp)->mac_da))) {
		/* Copy the packet in case a reply is generated */
		static SMT_FB	fb;
		bcopy(dp, &fb, len);
		smt_frame(mac, &fb, len);
		return(RC_SUCCESS);
	}

	/* If iface DOWN, then drop request quietly. */
	if ((mac->ifflag&IFF_UP) == 0)
		return(RC_SUCCESS);

	/* Raw socket has no routing, so bypass routing. */
	cc = send(mac->smtsock, dp, len, MSG_DONTROUTE);
	if (cc == len) {
		mac->flips = 0;
		mac->tflip = curtime.tv_sec;
	} else {
		/* Do not fill the system log with complaints. */
		if (mac->flips == 0)
			sm_log(LOG_EMERG, SM_ISSYSERR,
			       "send_frame: %d bytes not sent", len);
		if ((curtime.tv_sec - mac->tflip > T_NOTIFY) &&
		    (mac->flips < mac->maxflops)) {
			mac->flips++;
			sm_log(LOG_EMERG, 0,
			       "No SMT frames sent for %d seconds; reset #%d",
			       curtime.tv_sec - mac->tflip, mac->flips);
			mac->tflip = curtime.tv_sec;
			/* do not keep resetting if we cannot */
			if (sm_reset(mac->name, 1))
				mac->flips = mac->maxflops;
			mac->changed = 1;
		}
	}
	return(RC_SUCCESS);
}

/*
 *
 * ANOUNCE, REQUEST, RESPONSE
 */
static void
fs_nif1(SMT_MAC *mac, SMT_FB *dp, u_long len)
{
	register smt_header	 *fp;

	FSDEBUG((LOG_DBGFS, 0, "SMT_FS_NIF\n"));
	fp = FBTOFP(dp);

	if ((fp->type == SMT_FT_REQUEST) || (fp->type == SMT_FT_ANNOUNCE)) {
		nn_respond(mac, dp, len);
		return;
	}

	/* verify response and to some action for that */
	if (fp->type == SMT_FT_RESPONSE) {
		nn_receive(mac, dp);
		return;
	}

	sm_log(LOG_ERR, 0, "SMT_FS_NIF: unknown type(%d)",fp->type);
}

/*
 *
 * REQUEST, RESPONSE
 */
static void
fs_conf_sif1(SMT_MAC *mac, SMT_FB *dp, u_long len)
{
	register smt_header *fp = FBTOFP(dp);
	LFDDI_ADDR da;

	sm_log(LOG_DEBUG, 0, "SMT_FS_CSIF: %d", fp->type);

	switch (fp->type) {
	case SMT_FT_REQUEST:
		da = dp->mac_hdr.mac_sa;
		fs_respond(mac,SMT_FC_CONF_SIF,&da, dp,len,RC_SUCCESS);
		return;

	case SMT_FT_RESPONSE:
		/* just pass to MAP iff registered */
		return;
	}

	/* illegal request ingnored */
	sm_log(LOG_ERR, 0, "Bad CONF_SIF frame type(%d) received.", fp->type);
}

/*
 * REQUEST, RESPONSE
 */
static void
fs_op_sif1(SMT_MAC *mac, SMT_FB *dp, u_long len)
{
	register smt_header *fp = FBTOFP(dp);
	LFDDI_ADDR da;

	sm_log(LOG_DEBUG, 0, "SMT_FS_OSIF: %d", fp->type);

	switch (fp->type) {
	case SMT_FT_REQUEST:
		da = dp->mac_hdr.mac_sa;
		fs_respond(mac,SMT_FC_OP_SIF,&da, dp,len,RC_SUCCESS);
		return;

	case SMT_FT_RESPONSE:
		/* just pass to MAP iff registered */
		return;
	}
	sm_log(LOG_ERR, 0, "Bad OP SIF frame type(%d) received.", fp->type);
}

/*
 * REQUEST, RESPONSE
 */
static void
fs_ecf1(SMT_MAC *mac, SMT_FB *dp, u_long len)
{
	register struct lmac_hdr *mh;
	register smt_header *fp;

	sm_log(LOG_DEBUG, 0, "SMT_FS_ECF");

	mh = FBTOMH(dp);
	fp = FBTOFP(dp);

	if (fp->type == SMT_FT_REQUEST) {
		LFDDI_ADDR da;

		da = mh->mac_sa;
		FSDEBUG((LOG_DBGFS,0,
			"ecf: reponding to %s",midtoa(&da)));
		fs_respond(mac, SMT_FC_ECF, &da, dp, len, RC_SUCCESS);
		return;
	}

	if (fp->type != SMT_FT_RESPONSE) {
		sm_log(LOG_ERR,0,
		       "Bad echo frame type(%d) received.",fp->type);
		return;
	}
}

/*
 *
 * ANOUNCE, REQUEST, RESPONSE
 *
 * NOTE - The protocol definition is still TBD in LBC 7.2
 */
static void
fs_raf1(SMT_MAC *mac, SMT_FB *dp, u_long len)
{
	LFDDI_ADDR		da;

	sm_log(LOG_DEBUG, 0, "SMT_FS_RAF");
	da = dp->mac_hdr.mac_sa;
	fs_respond(mac, SMT_FC_RDF, &da, dp, len, RC_AUTH);
	return;
}

/*
 * RESPONSE
 */
/* ARGSUSED */
static void
fs_rdf1(SMT_MAC *mac, SMT_FB *dp, u_long len)
{
	smt_header *fp;
	char	*info;
	u_long	rc, infolen;

	fp = FBTOFP(dp);
	info = FBTOINFO(dp);
	infolen = len - sizeof(struct lmac_hdr) - sizeof(*fp);
	if (tlv_parse_rc(&info, &infolen, &rc) != RC_SUCCESS) {
		sm_log(LOG_ERR, 0, "SMT_FS_RDF: bad length");
		return;
	}

	sm_log(LOG_ERR, 0, "SMT_FS_RDF: req denied by %s, rc=%x",
	       sidtoa(&fp->sid), (int)rc);
}

/*
 * ANOUNCEMENT only frame.
 */
/* ARGSUSED */
static void
fs_srf1(SMT_MAC *mac, SMT_FB *dp, u_long length)
{
	sm_log(LOG_INFO, 0, "SMT_FS_SRF");
	/* Just pass to MAP */
}

/*
 * REQUEST, RESPONSE
 */
static void
fs_get_pmf1(SMT_MAC *mac, SMT_FB *dp, u_long len)
{
	LFDDI_ADDR da;

	sm_log(LOG_DEBUG, 0, "SMT_FS_PMF");
	da = dp->mac_hdr.mac_sa;
	fs_respond(mac, SMT_FC_RDF, &da, dp, len, RC_AUTH);
}

/*
 * REQUEST, RESPONSE
 */
static void
fs_chg_pmf1(SMT_MAC *mac, SMT_FB *dp, u_long len)
{
	LFDDI_ADDR da;

	sm_log(LOG_DEBUG, 0, "SMT_FS_CHG_PMF");
	da = dp->mac_hdr.mac_sa;
	fs_respond(mac, SMT_FC_RDF, &da, dp, len, RC_AUTH);
}

/*
 * REQUEST, RESPONSE
 */
static void
fs_add_pmf1(SMT_MAC *mac, SMT_FB *dp, u_long len)
{
	LFDDI_ADDR da;

	sm_log(LOG_DEBUG, 0, "SMT_FS_ADD_PMF");
	da = dp->mac_hdr.mac_sa;
	fs_respond(mac, SMT_FC_RDF, &da, dp, len, RC_AUTH);
}

/*
 * REQUEST, RESPONSE
 */
static void
fs_rmv_pmf1(SMT_MAC *mac, SMT_FB *dp, u_long len)
{
	LFDDI_ADDR da;

	sm_log(LOG_DEBUG, 0, "SMT_FS_RMV_PMF");
	da = dp->mac_hdr.mac_sa;
	fs_respond(mac, SMT_FC_RDF, &da, dp, len, RC_AUTH);
}

/*
 *
 * ANOUNCE, REQUEST, RESPONSE
 *
 * Let MAP respond to this request.
 */
/* ARGSUSED */
static void
fs_esf1(SMT_MAC *mac, SMT_FB *dp, u_long len)
{
	sm_log(LOG_DEBUG, 0, "SMT_FS_ESF");
}

/*
 * version 1 frame receive.
 */
static void
smtd_fs_recv_1(SMT_MAC *mac, SMT_FB *dp, u_long len)
{
#define DISPATCH(x) {x(mac, dp, len); return;}

	/*
	 * A valid frame means something is ok on the board.
	 */
	mac->tflop = curtime.tv_sec;
	mac->flops = 0;

	switch (dp->smt_hdr.fc) {
	  case SMT_FC_NIF:	/* OK */
		DISPATCH(fs_nif1);
	  case SMT_FC_CONF_SIF:	/* OK */
		DISPATCH(fs_conf_sif1);
	  case SMT_FC_OP_SIF:	/* OK */
		DISPATCH(fs_op_sif1);
	  case SMT_FC_ECF:	/* OK */
		DISPATCH(fs_ecf1);
	  case SMT_FC_RAF:	/* OK */
		DISPATCH(fs_raf1);
	  case SMT_FC_RDF:	/* OK */
		DISPATCH(fs_rdf1);
	  case SMT_FC_SRF:	/* OK */
		DISPATCH(fs_srf1);
	  case SMT_FC_GET_PMF:
		DISPATCH(fs_get_pmf1);
	  case SMT_FC_CHG_PMF:
		DISPATCH(fs_chg_pmf1);
	  case SMT_FC_ADD_PMF:
		DISPATCH(fs_add_pmf1);
	  case SMT_FC_RMV_PMF:
		DISPATCH(fs_rmv_pmf1);
	  case SMT_FC_ESF:	/* OK */
		DISPATCH(fs_esf1);
	  default:
		sm_log(LOG_ERR, 0, "Bad FC=%x recv",(int)dp->smt_hdr.fc);
	}
#undef DISPATCH
}

/*
 * Handle Frame Service requests.
 */
void
smtd_fs(SMT_MAC *mac)
{
	int		len;
	static SMT_FB	rfb;

	FSDEBUG((LOG_DBGFS, 0, "Frame Service req from %s\n", mac->name));

	if ((len = recv(mac->smtsock, &rfb, sizeof(rfb), 0)) == -1) {
		if (errno != EINTR)
			sm_log(LOG_ERR, SM_ISSYSERR, "Recv Frame error");
		return;
	}

	smt_frame(mac, &rfb, len);
}

static void
smt_frame(SMT_MAC *mac, void *rbuf, int len)
{
	struct lmac_hdr *mh;
	smt_header	*fp;
	int		drop = 0;

	mh = FBTOMH(rbuf);
	if (len < (sizeof(struct lmac_hdr) + sizeof(*fp))) {
		if (mh->mac_fc != 0) {
			sm_log(LOG_ERR, 0,
				"smtd_fs: frame too small (len=%d)", len);
			return;
		}
	} else if (len >= sizeof(SMT_FB)) {
		sm_log(LOG_ERR, 0, "smtd_fs: frame too large (len=%d)", len);
		return;
	}

	if (mh->mac_fc == 0) {
		smtd_indicate(mac);
		return;
	}

	/*
	 * FC_SMTINFO and FC_NSA are NOT interchangable.
	 *	Directed NSA requests from non nbr are still honored.
	 */
	if (mh->mac_fc == FC_NSA) {
		if (mh->mac_bits&MAC_A_BIT)
			drop = 1;
	} else if (mh->mac_fc != FC_SMTINFO) {
		sm_log(LOG_ERR, 0, "Unknown Frame, FC=%x", (int)mh->mac_fc);
		return;
	}
	if (mh->mac_bits&MAC_E_BIT)
		drop = 1;

	/*
	 * Check frame header
	 */
	fp = FBTOFP(rbuf);
	fp->vers = ntohs(fp->vers);
	fp->xid = ntohl(fp->xid);
	fp->len = ntohs(fp->len);
	if (fp->len > sizeof(SMT_FB)-sizeof(*fp)-sizeof(*mh)) {
		sm_log(LOG_ERR, 0, "frame too large: len = %d", (int)fp->len);
		return;
	}

	FSDEBUG((LOG_DBGFS, 0, "rcv: len %d, type %d, xid %x, v.%d",
			len, fp->type, fp->xid, fp->vers));
	fsq_log((u_char)fp->fc, (u_char)fp->type, 1);
	if ((fp->type < SMT_FT_ANNOUNCE) || (fp->type > SMT_FT_RESPONSE)) {
		LFDDI_ADDR da;
		sm_log(LOG_ERR, 0, "Unknown frame type: 0x%x", (int)fp->type);
		da = mh->mac_sa;
		fs_respond(mac, SMT_FC_RDF, &da, rbuf, len, RC_NOCLASS);
		return;
	}
	if ((fp->vers < SMT_VER_LO) || (fp->vers > SMT_VER_HI)) {
		LFDDI_ADDR da;

		if (fp->type != SMT_FT_REQUEST) {
			sm_log(LOG_INFO, 0,
			       "discarded req-vers(%d)", fp->vers);
		} else {
			sm_log(LOG_ERR, 0, "Denied REQUEST: bad version %d",
				fp->vers);
			da = mh->mac_sa;
			fs_respond(mac, SMT_FC_RDF, &da, rbuf,
				   len, RC_NOVERS);
		}
		drop = 1;
	}
	/*
	 * Deliver a copy of the packet to waiting clients before processing.
	 * A response to a request might overwrite the original packet
	 * (e.g., nn_respond).
	 *
	 * XXX - should recover to host order in LITTLE-ENDIAN machine.
	 *	 but in IRIX, it is O.K.
	 */
	if (smtp->fs)
		smtd_resp_map((int)fp->fc, (int)fp->type, rbuf, len);

	if (drop)
		return;

	updatemac(mac);
	switch (fp->vers) {
		case 0:		/* pre rev6.0 */
		case 1:		/* rev 6.0/1/2 */
			smtd_fs_recv_1(mac, rbuf, len);
			break;
		default:
			sm_log(LOG_ERR, 0,
				"version %d not supported.", (int)fp->vers);
	}
}


#define GETTLV(x,y) {					\
	char *ridp; u_long ridlen; u_short rid[2];	\
	rid[0] = 0;					\
	rid[1] = htons((x)->rid);			\
	ridp = (char*)&rid[0];				\
	ridlen = sizeof(rid);				\
	(void)tlvget(y, &ridp, &ridlen, &info, &infolen); }

/*
 * Send out frame REQUESTS.
 *
 * - if (data and len == 0), request from daemon
 *   else request is from MAP.
 */
int
fs_request(SMT_MAC *mac, int fc, LFDDI_ADDR *da, void *data, int len)
{
	register struct lmac_hdr *mh;
	register smt_header *fp;
	u_long	infolen;
	char	*info;
	int	sts = RC_SUCCESS;
	char	*tbuf = 0;
	u_long xid = SMTXID;
	int	mac_fc = FC_SMTINFO;

	sm_log(LOG_DEBUG, 0, "fs_request");
	switch (fc) {
	  case SMT_FC_NIF:
		/*
		 * - per MAC operation.
		 * - REQUEST by MAP and DAEMON.
		 */
		/* use nn_xid iff NI-protocol */
		if ((len == 0) && (data == 0)) {
			xid = mac->nn_xid;
			mac_fc = FC_NSA;
		}
		infolen = 0x28;
		tbuf = Malloc(infolen+sizeof(*mh)+sizeof(*fp));
		info = FBTOINFO(tbuf);
		if ((sts=tlv_build_una(&info,&infolen,&mac->una))!=RC_SUCCESS)
			goto badreq;
		GETTLV(mac, PTYPE_STD);
		GETTLV(mac, PTYPE_STAT);
		GETTLV(mac, 0x200b);

		infolen = info - FBTOINFO(tbuf);
		break;

	  case SMT_FC_CONF_SIF:
	  case SMT_FC_OP_SIF:
		infolen = 0;
		tbuf = Malloc(sizeof(*mh)+sizeof(*fp));
		break;

	  case SMT_FC_ECF:
		/*
		 * requested only by MAP
		 *
		 *	data -- pattern.
		 *	len --- pattern size.
		 */
		infolen = len + TLV_HDRSIZE;
		tbuf = Malloc(infolen+sizeof(*mh)+sizeof(*fp));
		info = FBTOINFO(tbuf);
		if (tlv_build_tl(&info, &infolen, PTYPE_ECHO, len)) {
			sts = RC_NOMORE;
			goto badreq;
		}
		bcopy((char *)data, info, len);
		infolen = len + TLV_HDRSIZE;
		break;

	  /*
	   * ESF and PMF frames' info fields  must be built/parsed by MAP.
	   * SMTD just relays this frame.
	   */
	  case SMT_FC_ESF:
	  case SMT_FC_GET_PMF:
	  case SMT_FC_CHG_PMF:
	  case SMT_FC_ADD_PMF:
	  case SMT_FC_RMV_PMF:
	  case SMT_FC_SRF:	/* ANOUNCE only but da=MULTICAST */
		tbuf = Malloc(len+sizeof(*mh)+sizeof(*fp));
		info = FBTOINFO(tbuf);
		bcopy((char *)data, info, len);
		infolen = len;
		break;

	  case SMT_FC_RAF:	/* TBD */
		sm_log(LOG_ERR, 0, "REQUEST: Unsupported FC=%x", fc);
		sts = RC_AUTH;
		goto badreq;

	  case SMT_FC_RDF:	/* RESPOND only */
	  default:
		sm_log(LOG_ERR, 0, "REQUEST: Illegal FC=%x", fc);
		sts = RC_PARSE;
		goto badreq;
	}

	/* set up mac header */
	mh = FBTOMH(tbuf);
	mh->mac_fc = mac_fc;
	mh->mac_da = *da;
	mh->mac_sa = mac->addr;

	/* set up smt frame header */
	fp = FBTOFP(tbuf);
	fp->fc = fc;
	fp->type = SMT_FT_REQUEST;
	fp->vers = htons(smtp->vers_op);
	fp->xid = htonl(xid);
	fp->sid = smtp->sid;
	fp->pad = 0;
	fp->len = htons(infolen);

	/* ship it */
	infolen += sizeof(*mh) + sizeof(*fp);
	sts = send_frame(mac, tbuf, (int)infolen);
badreq:
	if (tbuf)
		free(tbuf);
	fsq_log((u_char)fc, (u_char)SMT_FT_REQUEST, 0);
	return(sts);
}

/*
 * Send out frame ANNOUNCEMENT.
 *	- consumes data-buf.
 */
int
fs_announce(SMT_MAC *mac, int fc, void *data, int len)
{
	register struct lmac_hdr *mh;
	register smt_header	*fp;
	u_long	infolen;
	char	*info;
	LFDDI_ADDR bda;
	char	*tbuf = 0;
	u_long	xid = SMTXID;
	int	sts = RC_SUCCESS;
	int	mac_fc = FC_SMTINFO;

	bda = smtd_broad_mid;
	switch (fc) {
	  case SMT_FC_NIF:
		/*
		 * - per MAC operation.
		 * - REQUEST by SMT.
		 */
		mac_fc = FC_NSA;
		xid = mac->nn_xid;

		infolen = 0x28;
		tbuf = Malloc(infolen+sizeof(*mh)+sizeof(*fp));
		info = FBTOINFO(tbuf);
		if ((sts=tlv_build_una(&info,&infolen,&mac->una))!=RC_SUCCESS)
			goto badannounce;
		GETTLV(mac, PTYPE_STD);
		GETTLV(mac, PTYPE_STAT);
		GETTLV(mac, 0x200b);
		infolen = info - FBTOINFO(tbuf);
		break;

	  /*
	   * ESF and PMF frames' info fields  must be built/parsed by MAP.
	   * SMTD just relay this frame.
	   */
	  case SMT_FC_SRF:
		bda = smtp->sr_mid;
		/* fall thru */
	  case SMT_FC_ESF:
		tbuf = Malloc(len+sizeof(*mh)+sizeof(*fp));
		info = FBTOINFO(tbuf);
		bcopy((char *)data, info, len);
		infolen = len;
		break;

	  /* ANNOUNCE but TBD */
	  case SMT_FC_RAF:

	  /* REQUEST only frames. */
	  case SMT_FC_CONF_SIF:
	  case SMT_FC_OP_SIF:
	  case SMT_FC_ECF:
	  case SMT_FC_GET_PMF:
	  case SMT_FC_CHG_PMF:
	  case SMT_FC_ADD_PMF:
	  case SMT_FC_RMV_PMF:

	  /* RESPOND only */
	  case SMT_FC_RDF:
	  default:
		sm_log(LOG_ERR, 0, "ANNOUNCE: Illegal FC=%x", fc);
		sts = RC_PARSE;
		goto badannounce;
	}

	/* set up mac header */
	mh = FBTOMH(tbuf);
	mh->mac_fc = mac_fc;
	mh->mac_da = bda;
	mh->mac_sa = mac->addr;

	/* set up smt frame header */
	fp = FBTOFP(tbuf);
	fp->fc = fc;
	fp->type = SMT_FT_ANNOUNCE;
	fp->vers = htons(smtp->vers_op);
	fp->xid = htonl(xid);
	fp->sid = smtp->sid;
	fp->pad = 0;
	fp->len = htons(infolen);

	infolen += sizeof(*mh) + sizeof(*fp);
	sts = send_frame(mac, tbuf, (int)infolen);
badannounce:
	if (tbuf)
		free(tbuf);
	fsq_log((u_char)fc, (u_char)SMT_FT_ANNOUNCE, 0);
	return(sts);
}

/*
 * Send out RESPOND frame.
 *
 *	- data points to receive buf.
 *	- len = size of mh+fp+infolen
 *	- reuse receive iff possible.
 *	- REQUEST by SMT or MAP.
 *	- per MAC operation.
 *	- consumes data-buf.
 */
void
fs_respond(SMT_MAC *mac,int fc,LFDDI_ADDR *da,SMT_FB *data,int len,int reason)
{
	register struct lmac_hdr *mh;
	register smt_header	*fp;
	u_long	infolen;
	char	*info, *parm;
	SMT_PHY	*phy;
	SMT_MAC *m;
	char	*tbuf = 0;
	int	freetbuf = 0;

	mh = FBTOMH(data);
	fp = FBTOFP(data);
	info = parm = FBTOINFO(data);
	infolen = sizeof(*data) - sizeof(*mh) - sizeof(*fp);

	switch (fc) {
	  case SMT_FC_NIF:
		tbuf = (char *)data;
		if (tlv_build_una(&info,&infolen,&mac->una) != RC_SUCCESS)
			goto badrespond;
		GETTLV(mac, PTYPE_STD);
		GETTLV(mac, PTYPE_STAT);
		GETTLV(mac, 0x200b);
		break;

	  case SMT_FC_CONF_SIF:
		tbuf = (char *)data;
		GETTLV(mac, PTYPE_MTS);
		GETTLV(mac, PTYPE_STD);
		GETTLV(mac, PTYPE_VERS);
		GETTLV(mac, PTYPE_STAT);
		GETTLV(mac, PTYPE_SP);
#ifdef OPTIONAL
		GETTLV(mac, PTYPE_PL);
#endif
		for (m = smtp->mac; m; m = m->next)
			GETTLV(m, PTYPE_MACNBRS);
		GETTLV(mac, PTYPE_PDS);
#ifdef PMF
		set_count.
#endif
		break;

	  case SMT_FC_OP_SIF:
		tbuf = (char *)data;
		GETTLV(mac, PTYPE_MTS);
		for (m = smtp->mac; m; m = m->next)
			GETTLV(m, PTYPE_MAC_STAT);
		for (phy = smtp->phy; phy; phy = phy->next)
			GETTLV(phy, PTYPE_LER);
		for (m = smtp->mac; m; m = m->next)
			GETTLV(m, PTYPE_MFC);
		for (m = smtp->mac; m; m = m->next)
			GETTLV(m, PTYPE_MFNCC);
		for (m = smtp->mac; m; m = m->next)
			GETTLV(m, PTYPE_MPV);
		for (phy = smtp->phy; phy; phy = phy->next)
			GETTLV(phy, PTYPE_EBS);
		GETTLV(mac, PTYPE_MF);
		GETTLV(mac, PTYPE_USR);
		break;

	  case SMT_FC_ECF: {
		u_long t, l;

		tbuf = (char *)data;

		/* support REAL zero len echo */
		if (fp->len == 0)
			break;
		if (tlv_parse_tl(&info, &infolen, &t, &l) ||
		    (t != PTYPE_ECHO) || (l > infolen))
			return;
		info += l;
		break;
	  }

	  /*
	   * ESF and PMF frames info fields  must be built/parsed by MAP.
	   * SMTD just relay this frame.
	   */
	  case SMT_FC_ESF:
	  case SMT_FC_GET_PMF:
	  case SMT_FC_CHG_PMF:
	  case SMT_FC_ADD_PMF:
	  case SMT_FC_RMV_PMF:
		tbuf = (char *)data;
		info = tbuf + len;
		break;

	  /* RESPOND only and called by only SMT */
	  case SMT_FC_RDF:
		if (reason != RC_NOVERS) {
			tbuf = (char *)data;
			(void)tlv_build_rc(&info, &infolen, reason);
			break;
		}
		tbuf = Malloc(sizeof(*mh)+sizeof(*fp)+len+160);
		freetbuf = 1;
		mh = FBTOMH(tbuf);
		fp = FBTOFP(tbuf);
		info = parm = FBTOINFO(tbuf);
		infolen = sizeof(*data) - sizeof(*mh) - sizeof(*fp);

		(void)tlv_build_rc(&info, &infolen, reason);
		GETTLV(mac, PTYPE_VERS);
		len -= FDDI_FILLER;
		(void)tlv_build_rfb(&info,&infolen,
				(u_char*)data+FDDI_FILLER, (u_long *)&len);
		break;

	  case SMT_FC_SRF:	/* ANNOUNCE only */
	  case SMT_FC_RAF:	/* TBD */
	  default:
		sm_log(LOG_ERR, 0, "ANNOUNCE: Illegal FC=%x", fc);
		goto badrespond;
	}

	/* set up mac header */
	mh->mac_fc = FC_SMTINFO;
	mh->mac_da = *da;
	mh->mac_sa = mac->addr;

	/* set up smt frame header */
	fp->fc = fc;
	fp->type = SMT_FT_RESPONSE;
	fp->vers = htons(smtp->vers_op);
	fp->xid = htonl(fp->xid);
	fp->sid = smtp->sid;
	fp->pad = 0;
	infolen = info - parm;
	fp->len = htons((u_short)infolen);
	len = infolen + sizeof(*mh) + sizeof(*fp);

	sm_log(LOG_DEBUG, 0, "fs_respond: fc %d, xid %x, len %d, ilen %d",
		fc, fp->xid, len, infolen);

	/* ship it */
	(void)send_frame(mac, tbuf, (int)len);
badrespond:
	if (freetbuf && tbuf)
		free(tbuf);
	fsq_log((u_char)fc, (u_char)SMT_FT_RESPONSE, 0);
}
