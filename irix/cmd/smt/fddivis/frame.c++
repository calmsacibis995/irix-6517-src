/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Visual FDDI Managment System
 *
 *	$Revision: 1.17 $
 */

#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
extern "C" {
#endif
#include <osfcn.h>
#include <string.h>
#include <bstring.h>
#include <malloc.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
}
#endif
#include "fddivis.h"
#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
extern "C" {
#endif
#include <sm_log.h>
#include <sm_map.h>
#include <sm_mib.h>
#include <smt_subr.h>
#include <smt_parse.h>
#include <smt_snmp_clnt.h>
#include <tlv_sm.h>
#include <ma.h>
#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
}
#endif

Frame::Frame()
{
	poketms.tv_sec = 0;
	poketms.tv_usec = 0;
	frmcnt = 0;
	DBGVERB(("constructor Frame called\n"));
}

Frame::~Frame()
{
}

int
check_smtd(void)
{
	int sts, i;
	int vers = -1;

	if (FV.BypassSmtd)
		return(0);;

	FV.Gates = 0;
	sm_openlog(SM_LOGON_STDERR, 0, 0, 0, 0);
	for (i = 0; i < NSMTD; i++) {
		sts = map_smt(0,FDDI_SNMP_VERSION, &vers,sizeof(vers),i);
		if (sts != STAT_SUCCESS)
			break;
		if (vers != SMTD_VERS)
			break;
		FV.Gates++;
	}
	sm_openlog(SM_LOGON_STDERR, LOG_ERR, 0, 0, 0);

	// If old DevNum does not make sense, then go back to primary.
	if (FV.DevNum > FV.Gates-1)
		FV.DevNum = 0;
	if (FV.Gates > 0)
		return(0);

	if (sts != STAT_SUCCESS)
		printf("%s: could not contact smtd for version\n",PROGRAM);
	else if (vers != SMTD_VERS)
		printf("%s SMT version %d doesn't match daemon's (%d)\n",
		       PROGRAM, SMTD_VERS, vers);

	map_close();
	return(-1);
}

static int regDevNum = 0;
/*
 *
 */
void
Frame::unreg_svc()
{
	int sts;
	SMT_FS_INFO *pp = &FV.c_preg;

	if (FV.BypassSmtd || !FV.svcreged)
		return;
	FV.svcreged = 0;

	pp->fc = SMT_FC_SRF;
	sts = map_smt(0, FDDI_SNMP_UNREGFS, (char*)pp, sizeof(*pp), regDevNum);
	if (sts != STAT_SUCCESS)
		sm_log(LOG_ERR, 0, "SMT_FC_SRF unregister failed");

	pp->fc = SMT_FC_NIF;
	sts = map_smt(0, FDDI_SNMP_UNREGFS, (char*)pp, sizeof(*pp), regDevNum);
	if (sts != STAT_SUCCESS)
		sm_log(LOG_ERR, 0, "SMT_FC_NIF unregister failed");

	pp->fc = SMT_FC_OP_SIF;
	sts = map_smt(0, FDDI_SNMP_UNREGFS, (char*)pp, sizeof(*pp), regDevNum);
	if (sts != STAT_SUCCESS)
		sm_log(LOG_ERR, 0, "SMT_FC_OP_SIF unregister failed");

	pp->fc = SMT_FC_CONF_SIF;
	sts = map_smt(0, FDDI_SNMP_UNREGFS, (char*)pp, sizeof(*pp), regDevNum);
	if (sts != STAT_SUCCESS)
		sm_log(LOG_ERR, 0, "SMT_FC_CONF_SIF unregister failed");
}

void
Frame::reg_svc(SMT_FS_INFO *pp)
{
	int sts;

	if (FV.BypassSmtd)
		return;
	pp->ft = 0;
	if (pp != &FV.c_preg)
		FV.c_preg = *pp;

	pp->fc = SMT_FC_SRF;
	sts = map_smt(0, FDDI_SNMP_REGFS, (char*)pp, sizeof(*pp), FV.DevNum);
	if (sts != STAT_SUCCESS) {
		sm_log(LOG_ERR, 0, "SRF register failed");
		map_exit(2);
	}

	pp->fc = SMT_FC_NIF;
	sts = map_smt(0, FDDI_SNMP_REGFS, (char*)pp, sizeof(*pp), FV.DevNum);
	if (sts != STAT_SUCCESS) {
		sm_log(LOG_ERR, 0, "NIF register failed");
		map_exit(2);
	}

	pp->fc = SMT_FC_OP_SIF;
	sts = map_smt(0, FDDI_SNMP_REGFS, (char*)pp, sizeof(*pp), FV.DevNum);
	if (sts != STAT_SUCCESS) {
		sm_log(LOG_ERR, 0, "OP_SIF register failed");
		map_exit(2);
	}

	pp->fc = SMT_FC_CONF_SIF;
	sts = map_smt(0, FDDI_SNMP_REGFS, (char*)pp, sizeof(*pp), FV.DevNum);
	if (sts != STAT_SUCCESS) {
		sm_log(LOG_ERR, 0, "CONF_SIF register failed");
		map_exit(2);
	}
	FV.svcreged = 1;
	FV.frmcnt = frmcnt;
	regDevNum = FV.DevNum;
}

#define CONGAP	5
#define NBRPOS  4
void
Frame::do_demo()
{
	int i;
	short *sp;
	int congap = 0;

	if (setsmtd(FV.Agent))
		return;

	smtd.topology = 0;
	smtd.nodeclass = SMT_STATION;
	smtd.mac->primary->conn_state = PC_ACTIVE;
	smtd.mac->primary->pcnbr = (FV.DemoStaType==1)?PC_M:PC_A;
	if (smtd.mac->secondary != 0) {
		smtd.mac->secondary->conn_state = PC_ACTIVE;
		smtd.mac->secondary->pcnbr = (FV.DemoStaType==1)?PC_M:PC_B;
	}
	if (FV.DemoOrphs != 0) {
		if (FV.DemoUnas == 0) {
			if (FV.DemoConnType == 1)
				smtd.topology = SMT_AA_TWIST;
			else
				smtd.topology = SMT_WRAPPED;
			smtd.mac->secondary->conn_state = PC_DISABLED;
		}
	}
	smtd.mac->una = smtd.mac->addr;
	smtd.mac->dna = smtd.mac->addr;
	sp = (short*)&smtd.mac->dna.b[NBRPOS]; (*sp)--;
	sp = (short*)&smtd.mac->una.b[NBRPOS];
	(*sp) -= (FV.DemoDnas+FV.DemoUnas+FV.DemoOrphs);

	buildnif();
	buildsif();
	FV.RINGWindow->redrawWindow();
	FV.MAPWindow->redrawWindow();

	sm_log(LOG_DEBUG, 0, "Simulating %d DNAs", FV.DemoDnas);
	smtd.topology = 0;
	smtd.mac->primary->conn_state = PC_ACTIVE;
	if (smtd.mac->secondary != 0)
		smtd.mac->secondary->conn_state = PC_ACTIVE;
	for (i = 1; i <= FV.DemoDnas; i++) {
		smtd.mac->una = smtd.mac->addr;
		smtd.mac->addr = smtd.mac->dna;
		sp = (short*)&smtd.sid.ieee.b[NBRPOS]; (*sp)--;
		sp = (short*)&smtd.mac->dna.b[NBRPOS]; (*sp)--;

		if (FV.DemoStaType == 1) {
			if (congap%CONGAP == 0) {
				smtd.nodeclass = SMT_CONCENTRATOR;
				smtd.mac->primary->pcnbr = PC_A;
				if (smtd.mac->secondary != 0)
					smtd.mac->secondary->pcnbr = PC_B;
			} else {
				smtd.nodeclass = SMT_STATION;
				smtd.mac->primary->pcnbr = PC_M;
				if (smtd.mac->secondary != 0)
					smtd.mac->secondary->pcnbr = PC_M;
			}
			congap++;
		} else {
			smtd.nodeclass = SMT_STATION;
			smtd.mac->primary->pcnbr = PC_A;
			if (smtd.mac->secondary != 0)
				smtd.mac->secondary->pcnbr = PC_B;
		}

		buildnif();
		buildsif();
	}
	FV.Ring.sortring();
	FV.RINGWindow->redrawWindow();
	FV.MAPWindow->redrawWindow();

	sm_log(LOG_DEBUG, 0, "Simulating %d Orphans", FV.DemoOrphs);
	for (i = 0; i < FV.DemoOrphs; i++) {
		smtd.mac->una = smtd.mac->addr;
		smtd.mac->addr = smtd.mac->dna;
		sp = (short*)&smtd.sid.ieee.b[NBRPOS]; (*sp)--;
		sp = (short*)&smtd.mac->dna.b[NBRPOS]; (*sp)--;

		smtd.topology = 0;
		if (FV.DemoStaType == 1) {
			if (congap%CONGAP == 0) {
				smtd.nodeclass = SMT_CONCENTRATOR;
				smtd.mac->primary->pcnbr = PC_A;
				smtd.mac->primary->conn_state = PC_ACTIVE;
				if (smtd.mac->secondary != 0) {
					smtd.mac->secondary->pcnbr = PC_B;
					smtd.mac->secondary->conn_state =
							PC_ACTIVE;
				}
			} else {
				smtd.nodeclass = SMT_STATION;
				smtd.mac->primary->pcnbr = PC_M;
				smtd.mac->primary->conn_state = PC_ACTIVE;
				if (smtd.mac->secondary != 0) {
					smtd.mac->secondary->pcnbr = PC_M;
					smtd.mac->secondary->conn_state =
							PC_ACTIVE;
				}
			}
			congap++;
		} else {
			smtd.nodeclass = SMT_STATION;
			smtd.mac->primary->conn_state = PC_ACTIVE;
			smtd.mac->primary->pcnbr = PC_A;
			if (smtd.mac->secondary != 0) {
				smtd.mac->secondary->conn_state = PC_ACTIVE;
				smtd.mac->secondary->pcnbr = PC_B;
			}
		}
		if (i == 0) {
			if (FV.DemoDnas == 0)
				bzero((char*)&smtd.mac->una,sizeof(LFDDI_ADDR));
			else {
				sp = (short*)&smtd.mac->una.b[2]; (*sp)--;
			}
		} else if (i == FV.DemoOrphs - 1) {
			if (FV.DemoConnType != 1) {
				smtd.topology = SMT_WRAPPED;
				smtd.mac->primary->pcnbr = PC_UNKNOWN;
			}
		}

		buildnif();
		buildsif();
	}
	FV.Ring.sortring();
	FV.RINGWindow->redrawWindow();
	FV.MAPWindow->redrawWindow();

	sm_log(LOG_DEBUG, 0, "Simulating %d UNAs", FV.DemoUnas);
	for (i = 0; i < FV.DemoUnas; i++) {
		smtd.mac->una = smtd.mac->addr;
		smtd.mac->addr = smtd.mac->dna;
		sp = (short*)&smtd.sid.ieee.b[NBRPOS]; (*sp)--;
		sp = (short*)&smtd.mac->dna.b[NBRPOS]; (*sp)--;

		smtd.topology = 0;
		if (FV.DemoStaType == 1) {
			if (congap%CONGAP == 0) {
				smtd.nodeclass = SMT_CONCENTRATOR;
				smtd.mac->primary->pcnbr = PC_A;
				smtd.mac->primary->conn_state = PC_ACTIVE;
				if (smtd.mac->secondary != 0) {
					smtd.mac->secondary->pcnbr = PC_B;
					smtd.mac->secondary->conn_state =
							PC_ACTIVE;
				}
			} else {
				smtd.nodeclass = SMT_STATION;
				smtd.mac->primary->pcnbr = PC_M;
				smtd.mac->primary->conn_state = PC_ACTIVE;
				if (smtd.mac->secondary != 0) {
					smtd.mac->secondary->pcnbr = PC_M;
					smtd.mac->secondary->conn_state =
							PC_ACTIVE;
				}
			}
			congap++;
		} else {
			smtd.nodeclass = SMT_STATION;
			smtd.mac->primary->conn_state = PC_ACTIVE;
			smtd.mac->primary->pcnbr = PC_A;
			if (smtd.mac->secondary != 0) {
				smtd.mac->secondary->conn_state = PC_ACTIVE;
				smtd.mac->secondary->pcnbr = PC_B;
			}
		}

		if ((i == 0) && (FV.DemoOrphs != 0)) {
			if (FV.DemoConnType == 1) {
				smtd.topology = SMT_BB_TWIST;
				smtd.mac->primary->pcnbr = PC_A;
				smtd.mac->secondary->pcnbr = PC_B;
			} else {
				smtd.topology = SMT_WRAPPED;
				smtd.mac->primary->pcnbr = PC_A;
				smtd.mac->secondary->pcnbr = PC_UNKNOWN;
			}
		}

		buildnif();
		buildsif();
	}

	FV.Ring.sortring();
	FV.RINGWindow->redrawWindow();
	FV.MAPWindow->redrawWindow();
}

#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
extern "C" {
#endif
extern void setsigs();
#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
}
#endif
/*
 *
 */
void
Frame::init_frame()
{
	int tmp;

	sm_openlog(SM_LOGON_STDERR, LOG_ERR, 0, 0, 0);

	for (tmp = 0; tmp < NSMTD; tmp++)
		sprintf(&FV.gatenames[tmp][0], "gate%d", tmp);

	FV.xfp = (SMT_FB *)Malloc(sizeof(SMT_FB));
	FV.rfp = (SMT_FB *)Malloc(sizeof(SMT_FB));

	/* set interface to smtd */
	if (setsmtd(FV.Agent)) {
		FV.post(NORESTARTMSG, FV.Agent);
		return;
	}

	tmp = FV.FrameSelect;
	FV.FrameSelect = SEL_NIF;
	buildnif();
	FV.FrameSelect = tmp;
	setsigs();
}

/*
 *
 */
void
Frame::xmit(int fc, LFDDI_ADDR *targ)
{
	int sts;
	SMT_FS_INFO preg;

	if (FV.BypassSmtd)
		return;
	preg.fc = fc;
	preg.fsi_da = *targ;
#ifdef STASH
	sts = map_smt(0, FDDI_SNMP_SENDFRAME, (char*)&preg,
			sizeof(preg), FV.DevNum);
#endif
	sts = map_smt(FV.trunkname, FDDI_SNMP_SENDFRAME, (char*)&preg,
			sizeof(preg), 0);
	if (sts != STAT_SUCCESS) {
		sm_log(LOG_ERR, 0, "ring: xmit failed\n");
	}
}

/*
 *
 */
void
Frame::kickpoke(void)
{
	poketms.tv_sec = 0;
	// asdf dopoke();
}

static LFDDI_ADDR broad_mid = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
void
Frame::dopoke(void)
{
	struct timeval tm;

	if ((FV.Freeze) || (FV.Dreplay))
		return;

	if (getsmtd(0))
		return;

	if (!smtd.mac)
		return;

	if (((FV.FrameSelect&SEL_BCN)== SEL_BCN) &&
	    ((smtd.mac->rmt_flag&(RMF_T5|RMF_MYBCN|RMF_OTRBCN)) != 0))
		FV.RingOpr |= BEACONING;
	else
		FV.RingOpr &= ~BEACONING;

	if (((FV.FrameSelect&SEL_CLM) == SEL_CLM) &&
	   ((smtd.mac->rmt_flag&(RMF_T4|RMF_MYCLM|RMF_OTRCLM)) != 0))
		FV.RingOpr |= CLAIMING;
	else
		FV.RingOpr &= ~CLAIMING;

	// send to ourself
	xmit(SMT_FC_CONF_SIF, &smtd.mac->addr);

	if (FV.ActiveMode == POLL_PASSIVE)
		return;

	smt_gts(&tm);
	if (tm.tv_sec - poketms.tv_sec >= FV.Qint) {
		poketms = tm;
		if ((FV.FrameSelect&SEL_NIF) == SEL_NIF) {
			xmit(SMT_FC_NIF, &broad_mid);
		}

		if ((FV.FrameSelect&SEL_CSIF) == SEL_CSIF) {
			xmit(SMT_FC_CONF_SIF, &broad_mid);
		}

	} else if (!FV.NeedOpSIF) {
		return;
	}

	if ((FV.FrameSelect&SEL_OPSIF) == SEL_OPSIF) {
		xmit(SMT_FC_OP_SIF, &broad_mid);
		xmit(SMT_FC_OP_SIF, &smtd.mac->addr);
	}
}

static u_long misize = 0;
void
Frame::dummysmtd(int cmd)
{
	int i;
	FILE *f;
	u_long newmisize;
#define tfname ".fddivis.smtd"

	if ((f = fopen(tfname, cmd == 0 ? "r" : "w")) == NULL) {
		printf("fopen %s failed\n", tfname);
		exit(-1);
	}

	if (cmd == 0) {
		if (fread(&smtd, 1, sizeof(smtd), f) != sizeof(smtd))
			goto dsret;
		if (fread(&ifi, 1, sizeof(ifi), f) != sizeof(ifi))
			goto dsret;

		if ((newmisize = sizeof(*mi)*ifi.mac_ct) > misize) {
			if (mi)
				free(mi);
			misize = newmisize;
			mi = (MAC_INFO *)Malloc(misize);
			if (!mi) {
				sm_log(LOG_ERR, 0, "MAC info alloc failed");
				goto dsret;
			}
		}

		smtd.mac = 0;
		smtd.phy = 0;
	} else {
		if (fwrite(&smtd, 1, sizeof(smtd), f) != sizeof(smtd))
			goto dsret;
		if (map_smt(0, FDDI_SNMP_INFO,(char*)&ifi,sizeof(ifi),FV.DevNum)
			!= STAT_SUCCESS) {
			sm_log(LOG_ERR, 0, "Station info failed");
			map_exit(2);
		}
		if (fwrite(&ifi, 1, sizeof(ifi), f) != sizeof(ifi))
			goto dsret;
	}


	for (i = 0; i < ifi.mac_ct; i++) {
		register SMT_MAC *macp;
		register SMT_PHY *phyp;

		if (cmd == 0) {
			if (fread(&mi[i],1,sizeof(mi[0]), f) != sizeof(mi[0]))
				exit(-1);
		} else {
			if (fwrite(&mi[i],1,sizeof(mi[0]), f) != sizeof(mi[0]))
				exit(-1);
		}

		macp = &mi[i].mac;
		if (smtd.mac == 0)
			smtd.mac = macp;

		phyp = &mi[i].phy_0;
		macp->primary = phyp;
		if (smtd.phy == 0)
			smtd.phy = phyp;

		if (macp->secondary != 0) {
			phyp->next = &mi[i].phy_1;
			phyp = phyp->next;
			macp->secondary = phyp;
		}

		if (macp->next != 0) {
			macp->next = &mi[i+1].mac;
			phyp->next = &mi[i+1].phy_0;
		} else {
			macp->next = 0;
			phyp->next = 0;
		}
	}

dsret:
	if (cmd != 0)
		fflush(f);
	fclose(f);
}


int
Frame::getsmtd(int needset)
{
	int i;
	u_long newmisize;

	if (FV.BypassSmtd)
		return(0);

	if (needset) {
		if (map_smt(0, FDDI_SNMP_DUMP, (char*)&smtd,
			    sizeof(smtd), FV.DevNum)
		    != STAT_SUCCESS) {
			sm_log(LOG_ERR, 0, "can't get SMT status");
			return(-1);
		}
		sm_log(LOG_DEBUG, 0, "getsmtd: dump done, trunk=%s",
		       smtd.trunk);
		i = strlen(smtd.trunk);
		bcopy(smtd.trunk, FV.trunkname, i);
		FV.trunkname[i] = '0' + FV.DevNum;
		FV.trunkname[i+1] = 0;
		if (FV.gatenames[FV.DevNum][0] != FV.trunkname[0])
			strcpy(&FV.gatenames[FV.DevNum][0], FV.trunkname);

		if (map_smt(0, FDDI_SNMP_INFO, (char*)&ifi, sizeof(ifi),
			    FV.DevNum)
		    != STAT_SUCCESS) {
			sm_log(LOG_ERR, 0, "Station info failed");
			return(-1);
		}

		if ((newmisize = sizeof(*mi)*ifi.mac_ct) > misize) {
			if (mi)
				free(mi);
			misize = newmisize;
			mi = (MAC_INFO *)Malloc(misize);
			if (!mi) {
				sm_log(LOG_ERR, 0, "MAC info alloc failed");
				return(-1);
			}
		}
	}

	smtd.mac = 0;
	smtd.phy = 0;
	for (i = 0; i < ifi.mac_ct; i++) {
		register SMT_MAC *macp;
		register SMT_PHY *phyp;
		register char *name = (char *)&ifi.names[i][0];

		if (map_smt(name, FDDI_SNMP_DUMP, (char*)&mi[i],sizeof(*mi),0)
			!= STAT_SUCCESS) {
			sm_log(LOG_ERR,0,"mac dump for %s failed",name);
			return(-1);
		}

		macp = &mi[i].mac;
		if (smtd.mac == 0)
			smtd.mac = macp;

		phyp = &mi[i].phy_0;
		macp->primary = phyp;
		if (smtd.phy == 0)
			smtd.phy = phyp;

		if (macp->secondary != 0) {
			phyp->next = &mi[i].phy_1;
			phyp = phyp->next;
			macp->secondary = phyp;
		}

		if (macp->next != 0) {
			macp->next = &mi[i+1].mac;
			phyp->next = &mi[i+1].phy_0;
		} else {
			macp->next = 0;
			phyp->next = 0;
		}
	}

	if (FV.SaveSmtd)
		dummysmtd(1);
	return(0);
}

#define MAXBUFSPACE     60000
/*
 *
 */
int
Frame::setsmtd(char *ahost)
{
	int namelen;
	struct sockaddr_in rsin;
	struct timeval tp;
	struct hostent *hp;	/* Pointer to host info */
	struct timezone tz;
	int bufspace = MAXBUFSPACE;
	SMT_FS_INFO preg;
	char hname[128];

	if (FV.BypassSmtd) {
		dummysmtd(0);
		return(0);
	}

	if ((hp = gethostbyname(ahost)) == 0) {
		sm_log(LOG_ERR, SM_ISSYSERR, "gethostbyname");
		return(-1);
	}
	map_setpeer(ahost);
	if (*ahost != '\0' || strcmp(ahost, "localhost") != 0)
		map_settimo(5, 1);
	else
		map_resettimo();

	if (check_smtd())
		return(-1);

	if (getsmtd(1))
		return(-1);

	(void)close(FV.smtsock);
	if ((FV.smtsock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		perror("ring: recv socket");
		return(-1);
	}

	/* bind recv socket */
	if (gethostname(hname, sizeof(hname)) != 0) {
		sm_log(LOG_EMERG, SM_ISSYSERR, "couldn't get hostname");
		return(-1);
	}
	if ((hp = gethostbyname(hname)) == 0) {
		sm_log(LOG_ERR, SM_ISSYSERR, "%s", hname);
		return(-1);
	}
	bzero(&rsin, sizeof(rsin));
	rsin.sin_port = 0;
	rsin.sin_family = hp->h_addrtype;
	bcopy(hp->h_addr, (caddr_t)&rsin.sin_addr, hp->h_length);
	namelen = sizeof(rsin);
	if (bind(FV.smtsock, (struct sockaddr *)&rsin, namelen) < 0) {
		sm_log(LOG_ERR, SM_ISSYSERR, "bind to %s:%d",
		       inet_ntoa(rsin.sin_addr), (int)rsin.sin_port);
		return(-1);
	}

	/* register ring */
	namelen = sizeof(rsin);
	if (getsockname(FV.smtsock, (struct sockaddr *)&rsin, &namelen) < 0) {
		sm_log(LOG_ERR, SM_ISSYSERR, "getsockname");
		return(-1);
	}
	sm_log(LOG_DEBUG, 0, "setsmtd: udp port=%d", (int)rsin.sin_port);
	gettimeofday(&tp, &tz);
	preg.timo = (int)tp.tv_sec + REGTIMO;
	preg.ft = 0;
	preg.fsi_to = rsin;
	reg_svc(&preg);
	sm_log(LOG_DEBUG, 0, "setsmtd: ring registered");

	(void)setsockopt(FV.smtsock, SOL_SOCKET, SO_RCVBUF, (char*)&bufspace,
			 sizeof(bufspace));

	return(0);
}

void
Frame::buildsif(void)
{
	u_long	 off, i;
	u_long	infolen;
	char	*info, *parm;
	SMT_FB *fb = FV.xfp;
	register SMT_MAC *mac;
	register SMT_PHY *phy;
	register PARM_UNION *pu;
	register struct lmac_hdr *mh;
	register smt_header *fp;
	register PARM_PD_PHY *pdp;
	register PARM_PD_MAC *pdm;

	if ((FV.FrameSelect&SEL_CSIF) == 0)
		return;

	mh = FBTOMH(fb);
	fp = FBTOFP(fb);

	mac = smtd.mac;
	phy = smtd.phy;

	info = parm = FBTOINFO(fb);
	infolen = sizeof(*fb) - sizeof(*mh) - sizeof(*fp);

#define	CHECK_DST(x,y) { \
	off = sizeof(x); \
	if (tlv_build_tl(&info, &infolen, (y), off) != 0) abort(); \
	pu = (PARM_UNION *)(info); }

	CHECK_DST(PARM_STD, PTYPE_STD);
	pu->std.nodeclass  = smtd.nodeclass;
	pu->std.mac_ct = smtd.mac_ct;
	pu->std.nonmaster_ct = smtd.nonmaster_ct;
	pu->std.master_ct = smtd.master_ct;
	infolen -= off; info += off;
DBGSIF(("ring: mac_ct=%d nonmaster_ct=%d master_ct=%d\n",
		pu->std.mac_ct, pu->std.nonmaster_ct, pu->std.master_ct));

	CHECK_DST(PARM_STAT, PTYPE_STAT);
	pu->stat.pad = 0;
	pu->stat.topology = smtd.topology;
	pu->stat.dupa  = mac->dupa_flag;
	infolen -= off; info += off;
DBGSIF(("ring: topology=%d dupa=%d\n", pu->stat.topology, pu->stat.dupa));

	CHECK_DST(PARM_SP, PTYPE_SP);
	pu->sp.conf_policy = htons(smtd.conf_policy);
	pu->sp.conn_policy = htons(smtd.conn_policy);
	infolen -= off; info += off;
DBGSIF(("ring: conf=%d conn=%d\n", pu->sp.conf_policy, pu->sp.conn_policy));

	for (mac = smtd.mac, i = 0; i < smtd.mac_ct; i++, mac = mac->next) {
		CHECK_DST(PARM_MACNBRS, PTYPE_MACNBRS);
		pu->macnbrs.pad = 0;
		pu->macnbrs.macidx = (u_char)htons(mac->primary->macplacement);
		pu->macnbrs.una = mac->una;
		pu->macnbrs.dna = mac->dna;
		infolen -= off; info += off;
	}
DBGSIF(("ring: nbrs=%d\n", i));

	i = smtd.mac_ct + smtd.phy_ct;
	off = i * sizeof(PARM_PD_MAC);
	if (tlv_build_tl(&info, &infolen, PTYPE_PDS, off) != 0) {
		sm_log(LOG_ERR, 0, "out of info space");
		abort();
	}

	phy = smtd.phy;
	pdp = (PARM_PD_PHY *)(info);
	while (phy) {
DBGSIF(("ring: phy\n"));
		pdp->pad = 0;
		pdp->pc_type = phy->pctype;
		pdp->conn_state = phy->conn_state;
		pdp->pc_nbr = phy->pcnbr;
		pdp->rmac_indecated= (u_char)phy->remotemac;
		pdp->conn_rid = (u_short)htons(phy->macplacement);
		phy = phy->next;
		pdp++;
	}

	mac = smtd.mac;
	pdm = (PARM_PD_MAC *)(pdp);
	while (mac) {
DBGSIF(("ring: mac\n"));
		pdm->addr = mac->addr;
		pdm->rid = (u_short)htons(mac->primary->rid);
		mac = mac->next;
		pdm++;
	}
	infolen -= off; info += off;

	mac = smtd.mac;

	mh->mac_fc = FC_SMTINFO;
	mh->mac_sa = mac->addr;
	mh->mac_da = mac->addr;

	fp->fc = SMT_FC_CONF_SIF;
	fp->type = SMT_FT_RESPONSE;
	fp->vers = (u_short)smtd.vers_op;
	fp->sid = smtd.sid;
	infolen = info - parm;
	fp->len = htons((u_short)infolen);
DBGSIF(("ring: infolen = %d\n", infolen));

	(void)recvFrame(fb, (int)(infolen+sizeof(*mh)+sizeof(*fp)));
}

void
Frame::buildnif(void)
{
	u_long	 off;
	u_long	infolen;
	char	*info, *parm;
	register SMT_MAC *mac;
	register PARM_UNION *pu;
	register struct lmac_hdr *mh;
	register smt_header *fp;
	SMT_FB *fb = FV.xfp;

	mh = FBTOMH(fb);
	fp = FBTOFP(fb);

	mac = smtd.mac;
	info = parm = FBTOINFO(fb);
	infolen = sizeof(*fb) - sizeof(*mh) - sizeof(*fp);

	if (tlv_build_una(&info, &infolen, &smtd.mac->una)
		!= RC_SUCCESS) {
		sm_log(LOG_ERR, 0, "build una failed");
		map_exit(4);
	}

	CHECK_DST(PARM_STD, PTYPE_STD);
	pu->std.nodeclass  = smtd.nodeclass;
	pu->std.mac_ct = (u_char)smtd.mac_ct;
	pu->std.nonmaster_ct = (u_char)smtd.nonmaster_ct;
	pu->std.master_ct = (u_char)smtd.master_ct;
	infolen -= off; info += off;
DBGSIF(("ring: mac_ct=%d nonmaster_ct=%d master_ct=%d\n",
		pu->std.mac_ct, pu->std.nonmaster_ct, pu->std.master_ct));

	CHECK_DST(PARM_STAT, PTYPE_STAT);
	pu->stat.pad = 0;
	pu->stat.topology = (u_char)smtd.topology;
	pu->stat.dupa  = (u_char)mac->dupa_flag;
	infolen -= off; info += off;
DBGSIF(("ring: topology=%d dupa=%d\n", pu->stat.topology, pu->stat.dupa));

	mh->mac_fc = FC_SMTINFO;
	mh->mac_sa = mac->addr;
	mh->mac_da = mac->addr;

	fp->fc = SMT_FC_NIF;
	fp->type = SMT_FT_RESPONSE;
	fp->vers = (u_short)smtd.vers_op;
	fp->sid = smtd.sid;
	infolen = info - parm;
	fp->len = htons((u_short)infolen);
DBGSIF(("ring: infolen = %d\n", infolen));

	if (!FV.ring) {
		FV.Ring.init(fb);
		return;
	}

	(void)recvFrame(fb, (int)(infolen+sizeof(*mh)+sizeof(*fp)));
}

int
Frame::recvFrame(SMT_FB *fb, int cc)
{
	smt_header *fp = FBTOFP(fb);
	struct lmac_hdr *mh = FBTOMH(fb);

#define FRSET(x) ((FV.FrameSelect&x) == x)
	switch (fp->fc) {
	case SMT_FC_NIF:
		if (FRSET(SEL_NIF))
			return(FV.Ring.buildring(fb));
		break;
	case SMT_FC_CONF_SIF:
		if (FRSET(SEL_CSIF))
			return(FV.Ring.buildring(fb));
		break;
	case SMT_FC_OP_SIF:
		if (fp->type != SMT_FT_RESPONSE)
			break;
		if (FV.sifrp && SAMEADR(mh->mac_sa, FV.sifrp->sm.sa)) {
			FV.sifrp = 0;
			pr_sif((char*)fb, cc, FV.RingNum, FV.ring);
		}
		if (FRSET(SEL_OPSIF))
			return(FV.Ring.buildring(fb));
		break;

	case SMT_FC_SRF:
		break;

	default:
		printf("unknown frame(%x) ignored\n", (int)fp->fc);
		DBGVERB(("child: BAD from ring \n"));
		break;
	}
	return(0);
}

extern int hd_cc;
static int fr_intr = 0;
void
recvframe()
{
	u_long	fds;
	struct timeval time_out;
	SMT_FB *fb = FV.rfp;
	int	needdraw = 0;

	if (fr_intr)
		return;
	fr_intr = 1;

	fds = (1 << FV.smtsock);
	time_out.tv_sec = 0;
	time_out.tv_usec = 0;
	while (select(32, (fd_set *)&fds, 0, 0, &time_out) > 0) {
		hd_cc = recv(FV.smtsock, (char *)fb, sizeof(*fb), 0);
		if (hd_cc > 0) {
			if ((!FV.Freeze) && (!FV.Dreplay)) {
				needdraw |= FV.Frame.recvFrame(fb, hd_cc);
			}
			FV.Frame.frmcnt++;
		}
	}
	fr_intr = 0;

	if (needdraw) {
		FV.RINGWindow->redrawWindow();
		FV.MAPWindow->redrawWindow();
	}
}

/*
 *
 */
void
fr_finish(void)
{
	FV.Frame.unreg_svc();
	map_exit(0);
}
