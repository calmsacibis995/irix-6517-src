/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.21 $
 */

#include <errno.h>
#include <string.h>
#include <bstring.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <sys/ioctl.h>
#include <net/multi.h>
#include <netinet/in.h>
#include <cap_net.h>

#include <sm_types.h>
#include <sm_log.h>
#include <smt_asn1.h>
#include <smt_snmp.h>

#include <smtd.h>
#include <ma.h>
#include <smtd_svc.h>
#include <smtd_fs.h>
#include <smtd_nn.h>

/*
 * Approximate upper bound on socket buffer space reservation.
 */
#define MAXBUFSPACE     60000


int
sm_multi(char *ifname, LFDDI_ADDR *maddr, int add, int s)
{
	int bufspace;
	struct ifreq ifr;
	union mkey *key;

	bzero(&ifr, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	key = (union mkey*)&ifr.ifr_addr;
	key->mk_family = AF_RAW;
	bcopy(maddr, key->mk_dhost, sizeof(*maddr));
	if (ioctl(s, (add==1?SIOCADDMULTI:SIOCDELMULTI), (caddr_t)&ifr) < 0) {
		sm_log(LOG_EMERG, SM_ISSYSERR, "sm_multi: %s",
			add == 1 ? "SIOCADDMULTI" : "SIOCDELMULTI");
		return(-1);
	}
	return(0);
}

int
sm_open(char *nam, int port, int *fd_ptr, fd_set *fdset_ptr)
{
	int s, bufspace;
	struct sockaddr_raw sr;

	s = cap_socket(PF_RAW, SOCK_RAW, RAWPROTO_DRAIN);
	if (s < 0) {
		sm_log(LOG_EMERG, SM_ISSYSERR, "sm_open: socket");
		return(-1);
	}

	bzero(&sr, sizeof(sr));
	sr.sr_family = AF_RAW;
	sr.sr_port = port;
	strncpy(sr.sr_ifname, nam, sizeof(sr.sr_ifname));
	if (cap_bind(s, (struct sockaddr *) &sr, sizeof(sr)) < 0) {
		sm_log(LOG_EMERG, SM_ISSYSERR,
			"sm_open: bind port(%d, %s)", port, nam);
		(void)close(s);
		return(-1);
	}
	bufspace = 60000;
	if (setsockopt(s, SOL_SOCKET, SO_RCVBUF,
		(char *) &bufspace, sizeof bufspace) < 0) {
		sm_log(LOG_EMERG, SM_ISSYSERR,
			"sm_open: setsockopt (%s)", port, nam);
		(void)close(s);
		return(-1);
	}

	*fd_ptr = s;
	FD_SET(s, fdset_ptr);
	return(0);
}

void
sm_close(int *fd_ptr, fd_set *fdset_ptr)
{
	(void)close(*fd_ptr);
	FD_CLR(*fd_ptr, fdset_ptr);
	*fd_ptr = -1;
}


/*
 * cache an open socket for uncoupled functions
 */
int cached_soc = -1;
static fd_set cached_fds;
static struct sockaddr_raw cached_addr;

static int
cache_soc(char *name, char* logmsg)
{
	if (cached_soc >= 0
	    && bcmp(cached_addr.sr_ifname, name,
		       sizeof(cached_addr.sr_ifname)))
		sm_close(&cached_soc, &cached_fds);
	if (cached_soc < 0) {
		if (sm_open(name, 0, &cached_soc, &cached_fds)) {
			sm_log(LOG_EMERG, 0, logmsg);
			return(-1);
		}
		strncpy(cached_addr.sr_ifname, name,
			sizeof(cached_addr.sr_ifname));
	}
	return(0);
}


/*
 * NOT coupled with smtd.
 */
static int
sm_conf(char *name, int phyidx, struct smt_conf *conf, int cmd)
{
	SMT_SIOC sioc;

	if (0 > cache_soc(name,"sm_conf: oport failed"))
		return(SNMP_ERR_NOSUCHNAME);

	sioc.smt_ptr_u.conf = conf;
	sioc.len = sizeof(*conf);
	sioc.phy = phyidx;
	if (ioctl(cached_soc, cmd, &sioc) < 0) {
		sm_log(LOG_EMERG, SM_ISSYSERR,
		       "%s.%d: ioctl(%s)", name, phyidx,
		       cmd==SIOC_SMTGET_CONF?"SIOC_SMTGET_CONF":
		       "SIOC_SMTSET_CONF");
		sm_close(&cached_soc, &cached_fds);
		return(SNMP_ERR_NOSUCHNAME);
	}

	return(SNMP_ERR_NOERROR);
}

/*
 * NOT coupled with smtd.
 */
int
sm_getconf(char *name, int phyidx, struct smt_conf *conf)
{
	return(sm_conf(name, phyidx, conf, SIOC_SMTGET_CONF));
}

/*
 * NOT coupled with smtd.
 */
int
sm_setconf(char *name, int phyidx, struct smt_conf *conf)
{
	return(sm_conf(name, phyidx, conf, SIOC_SMTSET_CONF));
}

/*
 * NOT coupled with smtd.
 */
int
sm_stat(char *name, int phyidx, struct smt_st *stat)
{
	SMT_SIOC sioc;

	if (0 > cache_soc(name,"SMT_STAT: oport failed"))
		return(SNMP_ERR_NOSUCHNAME);

	sioc.smt_ptr_u.st = stat;
	sioc.len = sizeof(*stat);
	sioc.phy = phyidx;
	if (ioctl(cached_soc, SIOC_SMTGET_ST, &sioc) < 0) {
		sm_log(LOG_EMERG, SM_ISSYSERR,
			"%s.%d: ioctl(SIOC_SMTGET_ST)", name, phyidx);
		sm_close(&cached_soc, &cached_fds);
		return(SNMP_ERR_NOSUCHNAME);
	}

	return(SNMP_ERR_NOERROR);
}

/*
 * NOT coupled with smtd.
 */
int
sm_trace(char *name, int phyidx)
{
	SMT_SIOC sioc;

	sm_log(LOG_EMERG, 0, "%s.%d: TRACE", name, phyidx);

	if (0 > cache_soc(name,"SMT_TRACE: oport failed"))
		return(SNMP_ERR_NOSUCHNAME);

	sioc.smt_ptr_u.st = 0;
	sioc.len = 0;
	sioc.phy = phyidx;
	if (ioctl(cached_soc, SIOC_SMT_TRACE, &sioc) < 0) {
		sm_log(LOG_EMERG, SM_ISSYSERR,
			"%s.%d: ioctl(TRACE)", name, phyidx);
		sm_close(&cached_soc, &cached_fds);
		return(SNMP_ERR_NOSUCHNAME);
	}

	return(SNMP_ERR_NOERROR);
}

/*
 * NOT coupled with smtd.
 */
int
sm_maint(char *name, int phyidx, enum pcm_ls ls)
{
	SMT_SIOC sioc;

	if (0 > cache_soc(name,"SMT_MAINT: oport failed"))
		return(SNMP_ERR_NOSUCHNAME);

	sioc.smt_ptr_u.ls = ls;
	sioc.len = sizeof(ls);
	sioc.phy = phyidx;
	if (ioctl(cached_soc, SIOC_SMT_MAINT, &sioc) < 0) {
		sm_log(LOG_EMERG, SM_ISSYSERR,
			"%s.%d: ioctl(SIOC_SMT_MAINT)", name, phyidx);
		sm_close(&cached_soc, &cached_fds);
		return(SNMP_ERR_NOSUCHNAME);
	}

	return(SNMP_ERR_NOERROR);
}

/*
 * coupled with smtd.
 */
int
sm_arm(char *name, int s, u_long *arm, int phyidx)
{
	SMT_SIOC sioc;

	sioc.len = sizeof(*arm);
	sioc.phy = phyidx;
	if (ioctl(s, SIOC_SMT_ARM, &sioc) < 0) {
		sm_log(LOG_EMERG, SM_ISSYSERR,
			"%s.%d: ioctl(SIOC_SMT_ARM)", name, phyidx);
		return(SNMP_ERR_NOSUCHNAME);
	}
	if (arm)
		*arm = sioc.smt_ptr_u.alarm;

	return(SNMP_ERR_NOERROR);
}

/*
 * coupled with smtd.
 */
int
sm_lem_fail(char *name, int s, int phyidx)
{
	SMT_SIOC sioc;

	sm_log(LOG_EMERG, 0, "%s.%d: LEM_FAIL", name, phyidx);
	sioc.len = 0;
	sioc.phy = phyidx;
	if (ioctl(s, SIOC_SMT_LEM_FAIL, &sioc) < 0) {
		sm_log(LOG_EMERG, SM_ISSYSERR,
			"%s.%d: ioctl(SIOC_SMT_LEM_FAIL)", name, phyidx);
		return(SNMP_ERR_NOSUCHNAME);
	}

	return(SNMP_ERR_NOERROR);
}

/*
 * coupled with smtd.
 */
void
sm_d_bec(char *name, void *frame, int len, int s)
{
	SMT_SIOC sioc;

	sioc.len = len;
	sioc.smt_ptr_u.d_bec = frame;
	sioc.phy = 0;
	if (ioctl(s, SIOC_SMT_DIRECT_BEC, &sioc) < 0) {
		sm_log(LOG_EMERG, SM_ISSYSERR,
			"%s: ioctl(SIOC_SMT_DIRECT_BEC)", name);
	}
}

/*
 * coupled with smtd.
 */
int
sm_reset(char *name, int up)
{
	int s;
	struct	ifreq ifr;

	sm_log(LOG_EMERG, 0, "Reset %s", name);
	/* open socket and save if_flags */
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		sm_log(LOG_EMERG, SM_ISSYSERR, "sm_reset: socket");
		return(SNMP_ERR_NOSUCHNAME);		
	}

	/* save flags */
	strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
		sm_log(LOG_EMERG,SM_ISSYSERR,"sm_reset: ioctl(SIOCGIFFLAGS)");
		(void)close(s);
		return(SNMP_ERR_NOSUCHNAME);		
	}

	/* down */
	ifr.ifr_flags &= ~IFF_UP;
	strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCSIFFLAGS, (caddr_t)&ifr) < 0) {
		sm_log(LOG_EMERG, SM_ISSYSERR, "sm_reset: ioctl(~IFF_UP)");
		(void)close(s);
		return(SNMP_ERR_NOSUCHNAME);		
	}

	/* finished if it is supposed to be left down */
	if (!up) {
		(void)close(s);
		return(SNMP_ERR_NOERROR);
	}

	ifr.ifr_flags |= IFF_UP;
	strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCSIFFLAGS, (caddr_t)&ifr) < 0) {
		sm_log(LOG_EMERG,SM_ISSYSERR,"sm_reset: ioctl(IFF_UP)");
		(void)close(s);
		return(SNMP_ERR_NOSUCHNAME);
	}

	/* verify flags */
	strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
		sm_log(LOG_EMERG, SM_ISSYSERR,
		       "sm_reset: ioctl(SIOCGIFFLAGS)");
		(void)close(s);
		return(SNMP_ERR_NOSUCHNAME);
	}
	if ((ifr.ifr_flags & IFF_UP) == 0) {
		sm_log(LOG_EMERG,SM_ISSYSERR,
		       "sm_reset: ioctl(IFF_UP) failed");
		(void)close(s);
		return(SNMP_ERR_NOSUCHNAME);
	}

	(void)close(s);
	return(SNMP_ERR_NOERROR);
}

/*
 * NOT coupled with smtd.
 *	This can only work when the SMT daemon is not entirely alive,
 *	and when the interface is down.
 *
 *	More code should be added to take the interface down, tell smtd
 *	that its station number should change, and so on and so forth.
 */
void
sm_set_macaddr(char *name, LFDDI_ADDR *addr)
{
	int s;
	struct	ifreq ifr;

	/* open socket and save if_flags */
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		sm_log(LOG_EMERG, SM_ISSYSERR, "sm_set_macaddr: socket");
		exit(1);
	}

	/*
	 * addr is Canonical.
	 */
	strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ifr.ifr_addr.sa_family = AF_RAW;
	bitswapcopy(addr, &ifr.ifr_addr.sa_data[0], sizeof(*addr));

	if (ioctl(s, SIOCSIFADDR, (caddr_t)&ifr) < 0)
		sm_log(LOG_EMERG, SM_ISSYSERR,
			"sm_set_macaddr: ioctl(SIOCSIFADDR)");

	(void)close(s);
}
