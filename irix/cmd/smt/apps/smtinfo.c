/*
 * smtinfo --
 *
 *	Show version, configuration or operation SIF info.
 *
 * Copyright 1990,1991 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.7 $"

#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <sys/types.h>

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <getopt.h>
#include <bstring.h>
#include <string.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>
#include <ctype.h>
#include <netdb.h>

#include <sm_types.h>
#include <sm_log.h>
#include <sm_mib.h>
#include <smtd_parm.h>
#include <smtd.h>
#include <smt_subr.h>
#include <smt_snmp_clnt.h>
#include <smtd_fs.h>
#include <sm_map.h>
#include <sm_addr.h>
#include <tlv_sm.h>

static void setsmtd(int);
static int ck_src(char*, char);
static void pr_vers(char *, int);

#define	VERS		0x1	/* do version */
#define	CONFSIF		0x2	/* do CONF SIF */
#define	OPSIF		0x4	/* do OP_SIF */
int flags = 0;

/* multi ring stuff */
int	Iflag = 0;
int	devnum = 0;
char	*devname = NULL;		/* Device name only */
char	interface[16];			/* Device name & unit # -- ipg0 */

SMTD smtd;
SMT_FS_INFO preg;
LFDDI_ADDR targ;
int timo = 10;

int s;				/* Socket file descriptor for recv frame */
struct sockaddr_in s_sin;
int s_fc;

char *name;

#define	MAXPACKET	(4500)	/* max fddi frame size */
static char packet[MAXPACKET];

int nreceived = 1;		/* # of nodes on the ring */

int smtdebug = LOG_ERR;
char usage[] = "Usage:\n\
smtinfo [-covd] [-I interface] [-t timeout] {address|name}\n";

static void
quit()
{
	if (s_fc != 0) {
		preg.ft = SMT_FT_RESPONSE;
		preg.fc = s_fc;
		preg.fsi_to = s_sin;
		if (map_smt(0, FDDI_SNMP_UNREGFS, &preg, sizeof(preg), devnum)
		    != STAT_SUCCESS) {
			printf("smtinfo: unregister failed\n");
			map_exit(2);
		}
	}
	map_exit(0);
}

main(int argc, char *argv[])
{
	int c, cc;
	char *cp;
	struct timeval tp;
	fd_set fdset;

	while ((c = getopt(argc, argv, "cdovt:I:")) != EOF)
		switch(c) {
		case 'd':
			smtdebug++;
			break;
		case 't':
			timo = atoi(optarg);
			if (timo <= 0) {
				(void)fprintf(stderr,
				    "smtinfo: invalid timeout: %d\n",
				    timo);
				exit(1);
			}
			break;
		case 'c':
			flags |= CONFSIF;
			break;
		case 'o':
			flags |= OPSIF;
			break;
		case 'v':
			flags |= VERS;
			break;
		case 'I':
			devname = optarg;
			cp = devname+strlen(devname)-1;
			if (*devname == '\0' || !isdigit(*cp)) {
				fprintf(stderr, "Invalid device name: \"%s\"\n",
					optarg);
				exit(1);
			}
			Iflag++;
			strcpy(interface, devname);
			while (isdigit(*cp))
				cp--;
			*cp = 0;
			break;
		default:
			fprintf(stderr, usage);
			exit(1);
		}

	if (optind != argc-1) {
		fprintf(stderr, usage);
		exit(1);
	}
	name = argv[optind];
	if (smt_getmid(name, &targ)) {
		fprintf(stderr, "smtinfo: can't find MAC address for %s\n",
			name);
		map_exit(1);
	}

	/* default to the biggest questions */
	if (flags == 0)
		flags = CONFSIF | OPSIF | VERS;

	sm_openlog(SM_LOGON_STDERR, smtdebug, 0, 0, 0);

	signal(SIGINT, quit);
	signal(SIGHUP, quit);
	signal(SIGTERM, quit);

	/* ask all of the requested questions,
	 *	1st the version
	 *	2nd the configuration
	 *	3rd the current state
	 */
	for (;;) {
		bzero(&preg,sizeof(preg));
		if (flags & (CONFSIF | VERS)) {
			setsmtd(SMT_FC_CONF_SIF);
		} else {
			setsmtd(SMT_FC_OP_SIF);
		}

		preg.ft = 0;
		preg.fsi_da = targ;
		if (map_smt(0, FDDI_SNMP_SENDFRAME, &preg,sizeof(preg),devnum)
		    != STAT_SUCCESS) {
			sm_log(LOG_ERR,0, "sendframe failed");
			map_exit(2);
		}

		FD_ZERO(&fdset);
		FD_SET(s,&fdset);
		tp.tv_sec = timo;
		tp.tv_usec = 0;
		cc = select(s+1, &fdset,0,0, &tp);
		if (cc < 0) {
			perror("smtinfo: select");
			map_exit(1);
		}
		if (cc == 0) {
			sm_log(LOG_ERR,0, "smtinfo: no answer from %s", name);
			quit();
		}
		if ((cc = recv(s, packet, sizeof(packet), 0)) < 0) {
			perror("smtinfo: recv");
			map_exit(1);
		}

		if (flags & VERS) {
			pr_vers(packet, cc);
			flags &= ~VERS;
		} else {
			if (0 > ck_src(packet, preg.fc))
				continue;

			pr_parm(stdout, packet, cc);
			if (flags & CONFSIF)
				flags &= ~CONFSIF;
			else
				flags &= ~OPSIF;
		}

		if (flags == 0)
			quit();
		printf("\n");
	}
}


static void
setsmtd(int fc)
{
	char name[10];
	SMT_MAC mac;
	int i, namelen;
	static int vers = -1;
	struct timeval tp;
	struct hostent *hp;

	/* do not register more than once */
	if (fc == s_fc)
		return;
	if (s_fc != 0) {
		preg.ft = SMT_FT_RESPONSE;
		preg.fc = s_fc;
		preg.fsi_to = s_sin;
		if (map_smt(0, FDDI_SNMP_UNREGFS, &preg, sizeof(preg), devnum)
		    != STAT_SUCCESS) {
			printf("smtinfo: unregister failed\n");
			map_exit(2);
		}
		(void)close(s);
	}

	if (vers == -1) {
		sm_openlog(SM_LOGON_STDERR, 0, 0, 0, LOG_USER);
		vers = -1;
		if (map_smt(0, FDDI_SNMP_VERSION, &vers, sizeof(vers),
			    devnum)
		    != STAT_SUCCESS) {
			printf("Can't get smtd version\n");
			map_exit(1);
		}
		if (vers != SMTD_VERS) {
			printf("smtinfo version %d doesn't match daemon's (%d)\n",
			       SMTD_VERS, vers);
			map_exit(1);
		}

		if (devname) {
			int i, j;
			IPHASE_INFO ifi;

			for (j = 0; j < NSMTD; j++) {
				if (map_smt(0,FDDI_SNMP_INFO,&ifi,
					    sizeof(ifi),j)
				    != STAT_SUCCESS) {
					break;
				}
				devnum = j;
				for (i = 0; i < ifi.mac_ct; i++) {
					if (!strcmp(ifi.names[i],
						    interface)) {
						goto devfound;
					}
				}
			}
			printf("Invalid device name: %s\n", interface);
			map_exit(1);
		}
devfound:
		sm_openlog(SM_LOGON_STDERR, LOG_ERR, 0, 0, LOG_USER);

		if (map_smt(0, FDDI_SNMP_DUMP, &smtd, sizeof(smtd), devnum)
		    != STAT_SUCCESS) {
			printf("smt status failed\n");
			map_exit(3);
		}
		sm_log(LOG_DEBUG, 0, "setsmtd: dump done, trunk=%s", smtd.trunk);

		i = strlen(smtd.trunk);
		bcopy(smtd.trunk, name, i);
		name[i] = '0';
		name[i+1] = 0;
		if (map_smt(name, FDDI_SNMP_DUMP, &mac, sizeof(mac),
			    devnum)
		    != STAT_SUCCESS) {
			printf("mac dump for %s failed\n", name);
			map_exit(3);
		}
	}

	if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		perror("smtinfo: recv socket");
		map_exit(3);
	}

	/* bind recv socket */
	if ((hp = gethostbyname("localhost")) == 0) {
		herror("localhost");
		map_exit(3);
	}
	bzero(&s_sin, sizeof(s_sin));
	s_sin.sin_port = 0;
	s_sin.sin_family = hp->h_addrtype;
	bcopy(hp->h_addr, &s_sin.sin_addr, hp->h_length);
	namelen = sizeof(s_sin);

	if (bind(s, (struct sockaddr *)&s_sin, namelen) < 0) {
		perror("bind");
		map_exit(3);
	}

	/* register ring */
	namelen = sizeof(s_sin);
	if (getsockname(s, (struct sockaddr *)&s_sin, &namelen) < 0) {
		perror("getsockname");
		map_exit(3);
	}
	sm_log(LOG_DEBUG, 0, "setsmtd: udp port=%d", (int)s_sin.sin_port);

	gettimeofday(&tp, NULL);
	preg.timo = tp.tv_sec + timo;
	preg.ft = SMT_FT_RESPONSE;
	preg.fc = fc;
	preg.fsi_to = s_sin;
	if (map_smt(0, FDDI_SNMP_REGFS, &preg, sizeof(preg), devnum)
		!= STAT_SUCCESS) {
		printf("smtinfo register failed\n");
		map_exit(3);
	}
	sm_log(LOG_DEBUG, 0, "smtinfo: ring registered");
}


static int				/* 0=ok, -1=bad, -2=dump bad packet */
ck_src(char *buf, char fc)
{
	char	str1[128], str2[128];
	SMT_FB *fb = (SMT_FB *)buf;
	struct lmac_hdr *mh = &fb->mac_hdr;
	smt_header *fp = &fb->smt_hdr;

	if (bcmp(&targ, &mh->mac_sa, sizeof(targ))) {
		if (fddi_ntohost(&mh->mac_sa, str1) < 0)
			(void)fddi_ntoa(&mh->mac_sa, str1);
		if (fddi_ntohost(&targ, str2) < 0)
			(void)fddi_ntoa(&targ, str2);
		sm_log(LOG_ERR, 0,"response from %s instead of %s",
		       str1, str2);

		if (smtdebug != LOG_ERR)
			return -2;
		return -1;
	}

	if (fp->type != SMT_FT_RESPONSE) {
		sm_log(LOG_ERR, 0, "invalid response type: %d", fp->type);
		return -1;
	}

	if (fp->fc != fc) {
		sm_log(LOG_ERR, 0,
			"Unexpected FC = %s",ma_fc_str((u_char)fp->fc));
		return -1;
	}

	return 0;
}


static void
pr_vers(char *buf, int cc)
{
	SMT_FB *fb = (SMT_FB *)buf;
	struct lmac_hdr *mh = &fb->mac_hdr;
	smt_header *fp = &fb->smt_hdr;
	char	*info;
	u_long	infolen;
	char	str1[128], str2[128];
	u_long op, hi, lo;

	switch (ck_src(buf, SMT_FC_CONF_SIF)) {
	case 0:
		break;
	case -1:
		return;
	case -2:
		goto bad;
	}

	info = FBTOINFO(buf);

	cc -= info - buf;
	infolen = MIN(ntohs(fp->len), cc);
	while ((int)infolen > 0) {
		u_long type, len;

		/* peek type */
		type = *((u_short *)info);

		switch (type) {
		    case PTYPE_VERS:
			tlv_parse_vers(&info,&infolen,&op,&hi,&lo);
			if (fddi_ntohost(&mh->mac_sa, str1) < 0)
				(void)fddi_ntoa(&mh->mac_sa, str1);
			printf(
			"%s uses SMT version %u (supported versions: %u-%u)\n",
				str1, op, lo, hi);
			return;
		    default:
			if (tlv_parse_tl(&info, &infolen, &type, &len)) {
				infolen = 0;
				break;
			}
			sm_log(LOG_INFO, 0,
				"type=%x, len=%d: suppressed", type, len);
			info += len;
			infolen -= len;
			break;
		}
	}
	return;
bad:
	sm_hexdump(LOG_ERR, buf, cc);
}
