/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.10 $
 */

#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>

#include <netdb.h>
#include <arpa/inet.h>

#include <sm_types.h>
#include <sm_log.h>
#include <sm_mib.h>
#include <smtd.h>
#include <smt_subr.h>
#include <smt_snmp_clnt.h>
#include <smtd_fs.h>
#include <sm_map.h>
#include <sm_addr.h>
#include <smtd_parm.h>
#include <ma_str.h>

typedef struct {
	int port;
	int timo;
} SMT_PING;

static void setsmtd(void);
static void pr_pack(char *, int);
static void pinger(void);

LFDDI_ADDR tma, mma;
SMTD smtd;
SMT_FS_INFO preg;
int recvsock;		/* Socket file descriptor for recv frame */
int sendsock;		/* Socket file descriptor for send frame */

static void
quit()
{
	map_exit(0);
}

main(argc, argv)
char *argv[];
{
	int cc;
	struct hostent *hp;
	SMT_FB packet;
	char *hostname;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s address|name\n", argv[0]);
		exit(1);
	}
	hostname = argv[1];
	if (!fddi_aton(hostname, &tma)) {
		if ((hp = gethostbyname(hostname)) == 0) {
			fprintf(stderr, "%s: ", argv[0]);
			herror(hostname);
			exit(1);
		}
		if (fddi_hostton(hp->h_name, &tma)) {
			fprintf(stderr,
				"%s: can't find MAC address for %s\n",
				argv[0], hostname);
			exit(1);
		}
	}

	/* Check permission here so don't need to be root to validate args. */
	if (geteuid() != 0) {
		fprintf(stderr, "You are not superuser\n");
		exit(1);
	}
	sm_openlog(SM_LOGON_STDERR, LOG_ERR, 0, 0, 0);
	signal(SIGINT, quit);

	setsmtd();
	pinger();

	if ((cc = recv(recvsock, &packet, sizeof(packet), 0)) < 0)
		perror("smtvers: recv");
	else
		pr_pack((char*)&packet, cc);

	map_exit(0);
}

static char outpack[sizeof(SMT_FB)];
static int datalen;

static void
pinger(void)
{
	int i;
	register SMT_FB *fb = (SMT_FB *)outpack;
	register struct lmac_hdr *mh = &fb->mac_hdr;
	register smt_header *fp = &fb->smt_hdr;
	register char *info = &fb->smt_info[0];
	SMT_PING *r;
	u_short *usp = (u_short *)info;
	static int ntransmitted = 0;

	/* setup mac header */
	mh->mac_fc = FC_SMTINFO;
	mh->mac_da = tma;
	mh->mac_sa = mma;
	/* setup smt header */
	fp->fc = SMT_FC_ECF;
	fp->type = SMT_FT_REQUEST;
	fp->vers = 99;
	fp->sid = smtd.sid;
	fp->pad = 0;
	datalen = sizeof(*mh)+sizeof(*fp)+sizeof(*r) + sizeof(int);
	fp->len = htons(datalen);
	*usp = PTYPE_ECHO;
	usp++;
	*usp = datalen;
	usp++;
	r = (SMT_PING *)usp;
	r->port = preg.fsi_to.sin_port;
	r->timo = preg.timo;

	fp->xid = htonl((unsigned short)(ntransmitted++));

	i = send(sendsock, outpack, datalen, MSG_DONTROUTE);
	if ((i < 0) || (i != datalen))  {
		if (i < 0)
			perror("send");
		fflush(stdout);
	}
}

static void
pr_pack(char *buf, int cc)
{
	register SMT_FB *fb = (SMT_FB *)buf;
	register struct lmac_hdr *mh = &fb->mac_hdr;
	register smt_header *fp = &fb->smt_hdr;
	u_long	infolen;
	char *info = &fb->smt_info[0];
	char	str1[128], str2[128];
	u_long	reason;
	u_long	op, hi, lo;
	char	rfb[sizeof(*fb)];
	u_long	rfblen;

	if (fp->type != SMT_FT_RESPONSE) {
		sm_log(LOG_ERR, 0, "invalid response type: %d", fp->type);
		goto bad;
	}

	if (bcmp(&tma, &mh->mac_sa, sizeof(tma))) {
		if (fddi_ntohost(&mh->mac_sa, str1) < 0)
			(void)fddi_ntoa(&mh->mac_sa, str1);
		if (fddi_ntohost(&tma, str2) < 0)
			(void)fddi_ntoa(&tma, str2);
		sm_log(LOG_ERR, 0,"response from %s instead of %s", str1, str2);
		goto bad;
	}

	if (fp->fc != SMT_FC_RDF) {
		if (fp->fc == SMT_FC_ECF) {
			if (fddi_ntohost(&mh->mac_sa, str1) < 0)
				(void)fddi_ntoa(&mh->mac_sa, str1);
			sm_log(LOG_ERR, 0, "%s supports strange version %d",
					str1, (int)fp->vers);
		} else
			sm_log(LOG_ERR, 0,
				"Unexpected FC = %s",ma_fc_str((u_char)fp->fc));
		goto bad;
	}

	infolen = ntohs(fp->len);
	if (tlv_parse_rc(&info, &infolen, &reason) != RC_SUCCESS) {
		sm_log(LOG_ERR, 0, "parse RC failed");
		goto bad;
	}
	if (reason != RC_NOVERS) {
		sm_log(LOG_ERR, 0, "Unexpected Reason = %s", ma_rc_str(reason));
		goto bad;
	}

	if (tlv_parse_vers(&info, &infolen, &op, &hi, &lo) != RC_SUCCESS) {
		sm_log(LOG_ERR, 0, "parse VERS failed");
		goto bad;
	}

	rfblen = datalen-FDDI_FILLER;
	if (tlv_parse_rfb(&info, &infolen, rfb, &rfblen) != RC_SUCCESS) {
		sm_log(LOG_ERR, 0, "parse FRB failed");
		goto bad;
	}
	if (bcmp(rfb, outpack+FDDI_FILLER, datalen-FDDI_FILLER)) {
		sm_log(LOG_ERR, 0, "WARNING: bad RFB(rfblen=%d)", rfblen);
		sm_hexdump(LOG_ERR, buf, cc);
		sm_hexdump(LOG_ERR, rfb, rfblen);
		sm_log(LOG_ERR, 0, "packet sent:");
		sm_hexdump(LOG_ERR, outpack+FDDI_FILLER, datalen-FDDI_FILLER);
	}

	if (fddi_ntohost(&mh->mac_sa, str1) < 0)
		(void)fddi_ntoa(&mh->mac_sa, str1);
	printf("%s uses SMT version %u (supported versions: %u-%u)\n",
		str1, op, lo, hi);
	return;
bad:
	sm_hexdump(LOG_ERR, buf, cc);
}

static void
setsmtd(void)
{
	SMT_MAC mac;
	char name[10];
	int i, namelen;
	struct sockaddr_in sin;
	struct timeval tp;
	struct hostent *hp;	/* Pointer to host info */
	struct sockaddr_raw sr;
	int pingtimo = 60;

	if (map_smt(0, FDDI_SNMP_DUMP, &smtd, sizeof(smtd), 0)
		!= STAT_SUCCESS) {
		printf("smtd dump\n");
		map_exit(2);
	}

	i = strlen(smtd.trunk);
	bcopy(smtd.trunk, name, i);
	name[i] = '0';
	name[i+1] = 0;
	if (map_smt(name, FDDI_SNMP_DUMP, &mac, sizeof(mac), 0)
		!= STAT_SUCCESS) {
		printf("mac dump for %s failed\n", name);
		map_exit(2);
	}
	mma = mac.addr;

	/* open sockets */
	if ((sendsock = socket(PF_RAW, SOCK_RAW, RAWPROTO_DRAIN)) < 0) {
		perror("smtvers: send socket");
		map_exit(2);
	}
	bzero(&sr, sizeof(sr));
	sr.sr_family = AF_RAW;
	sr.sr_port = 55; /* something illegal */
	strncpy(sr.sr_ifname, name, strlen(name));
	if (bind(sendsock, &sr, sizeof(sr)) < 0) {
		sm_log(LOG_ERR, SM_ISSYSERR, "bind port(%s)", name);
		map_exit(2);
	}

	if ((recvsock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		perror("smtvers: recv socket");
		map_exit(2);
	}

	/* bind recv socket */
	if ((hp = gethostbyname("localhost")) == 0) {
		sm_log(LOG_ERR, SM_ISSYSERR, "gethostbyname");
		map_exit(2);
	}
	bzero(&sin, sizeof(sin));
	sin.sin_port = 0;
	sin.sin_family = AF_INET;
	bcopy(hp->h_addr, (caddr_t)&sin.sin_addr, sizeof(sin.sin_addr));
	namelen = sizeof(sin);

	if (bind(recvsock, (struct sockaddr *)&sin, namelen) < 0) {
		sm_log(LOG_ERR, SM_ISSYSERR, "bind");
		map_exit(2);
	}

	/* register ping */
	namelen = sizeof(sin);
	if (getsockname(recvsock, (struct sockaddr *)&sin, &namelen) < 0) {
		printf("getsockname failed(%d)\n", errno);
		map_exit(2);
	}
	gettimeofday(&tp, 0);
	preg.timo = tp.tv_sec + pingtimo;
	preg.fc = SMT_FC_ECF;
	preg.ft = SMT_FT_RESPONSE;
	preg.fsi_to = sin;
	if (map_smt(0, FDDI_SNMP_REGFS, &preg, sizeof(preg), 0)
		!= STAT_SUCCESS) {
		printf("smtvers register failed\n");
		map_exit(2);
	}

	preg.timo = tp.tv_sec + pingtimo;
	preg.fc = SMT_FC_RDF;
	preg.ft = SMT_FT_RESPONSE;
	preg.fsi_to = sin;
	if (map_smt(0, FDDI_SNMP_REGFS, &preg, sizeof(preg), 0)
		!= STAT_SUCCESS) {
		printf("smtvers register failed\n");
		map_exit(2);
	}
}
