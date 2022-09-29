/*
 * Copyright (c) 1984 by Sun Microsystems, Inc.
 * @(#)rpc.rstatd.c	1.1 88/03/07 4.0NFSSRC Copyr 1988 Sun Micro
 *
 * rstat demon:  called from inet
 */
/* define NON_INET_DEBUG 1 */

#include <signal.h>
#include <stdio.h>
#include <sys/param.h>

#include <sys/types.h>
#ifdef NON_INET_DEBUG
#include <sys/stat.h>
#endif
#include <sys/ioctl.h>
#include <sys/sbd.h>
#include <sys/immu.h>
#include <invent.h>

#include <fcntl.h>
#include <nlist.h>
#include <syslog.h>
#include <unistd.h>
#include <bstring.h>
#include <net/if.h>
#include <rpc/rpc.h>
#include <rpcsvc/rstat.h>
#include <sys/buf.h>
#include <sys/dvh.h>		/* must precede dksc.h */
#include <sys/elog.h>		/* must precede dksc.h */
#include <sys/iobuf.h>		/* must precede dksc.h */
#include <sys/scsi.h>		/* must precede dksc.h */
#include <sys/dkio.h>
#include <sys/dksc.h>
#include <sys/dmamap.h>
#include <sys/disksar.h>
#include <sys/errno.h>

#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/sysmp.h>
#include <sys/times.h>
#include <sys/wd93.h>

#if _MIPS_SZLONG == 64
#define KMEM_OFFSET_MASK 0x7fffffffffffffff
#else
#define KMEM_OFFSET_MASK 0x7fffffff
#endif

#define SINFO 6
static int tblmap[SINFO];

#define NDEVS 96

static off_t	klseek(int, off_t, int);
static void	do_exit(int);
static void	setup(void);
#ifdef HWG
extern void 	sgi_disk_setup(struct diosetup * (alloc_fn)(void));
extern int	dkiotime_get(char *,struct iotime *);
#else
static void	sgi_disk_setup(void);
#endif
static void	updatestat(void);

#define MAXCTLRS 4 /* XXX move me from io/dkip.c to dkipreg.h */

#if _MIPS_SZLONG == 64
#define __NLIST	nlist64
#else
#define __NLIST	nlist
#endif

struct __NLIST nl[] = {
#define	X_DKSCIOTIME	0
	{ "dkiotime" },
#define	X_DKSCINFO	1
	{ "dkscinfo" },
#define	X_AVENRUN	2
	{ "avenrun" },
#define N_KPTBL         3
	{ "kptbl" },
#define X_KERNEL_MAGIC	4
	{ "kernel_magic" },
#define LAST_K0_SYMB	X_KERNEL_MAGIC
#define X_END		5
	{ "end" },
	{""},
};

int	numdisks;
struct sysinfo sinfo;
struct minfo minfo;

struct diosetup {
	struct diosetup		*next;		/* Pointer to next */
#ifdef HWG
	char			dname[12];
#endif
	struct iotime		*iotim_addrs;	/* Pointer to iotim_addrs */
};
struct diosetup *Ndiosetup, *CurNdiosetup, *AllocdNdiosetup;
int NdiosetupIndex;

void sgi_dio_setup(void);
struct diosetup *sgi_dio_alloc(void);

int kmem;
static void stats_service();

int sincelastreq = 0;		/* number of alarms since last request */
#define CLOSEDOWN 20		/* how long to wait before do_exiting */

union {
    struct stats s1;
    struct statsswtch s2;
    struct statstime s3;
} stats;

/*
 * avenrun is a long scaled by FSCALE - internally in kernel we keep
 * it at 1024 *, its not clear what the statd protocol really wants
 * sun ref port seems to pass doubles! and places into long variables.
 * SVR4 uses the same FSCALE in kernel - so we change from our 1024
 * to FSCALE (256)
 */
#ifndef FSCALE
#define FSCALE (1 << 8)
#endif

static int kern_error = 0;	/* TRUE if error getting info from kernel */

int
main()
{
	SVCXPRT *transp;
	struct sockaddr_in addr;
	int len;
#ifdef NON_INET_DEBUG
	int s, inetd_started;
#endif
	openlog("rstatd", LOG_PID, LOG_DAEMON);
	len = sizeof(struct sockaddr_in);

#ifdef NON_INET_DEBUG
        if (issock(0)) {
		inetd_started = 1;
#endif
                /*
                 * Started from inetd
                 */
		if (getsockname(0, &addr, &len) != 0) {
			syslog(LOG_ERR,"rstat: getsockname: %m");
			do_exit(1);
		}
		transp = svcudp_bufcreate(0, RPCSMALLMSGSIZE, RPCSMALLMSGSIZE);
		if (transp == NULL) {
			syslog(LOG_ERR, "svc_rpc_udp_create: error");
			do_exit(1);
		}
		if (!svc_register(transp, RSTATPROG,
			 RSTATVERS_ORIG,stats_service, 0)) {
			syslog(LOG_ERR, "svc_rpc_register: error");
			do_exit(1);
		}
		if (!svc_register(transp, RSTATPROG,
			 RSTATVERS_SWTCH,stats_service,0)) {
			syslog(LOG_ERR, "svc_rpc_register: error");
			do_exit(1);
		}
		if (!svc_register(transp, RSTATPROG,
			 RSTATVERS_TIME,stats_service,0)) {
			syslog(LOG_ERR, "svc_rpc_register: error");
			do_exit(1);
		}
#ifdef NON_INET_DEBUG
        }
	else {
		inetd_started = 0;
		/*
		 * Started from shell
		 */		
		if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
			perror("inet: socket");
			return -1;
		}
		if (bind(s, &addr, sizeof(addr)) < 0) {
			perror("bind");
			return -1;
		}
		if (getsockname(s, &addr, &len) != 0) {
			perror("inet: getsockname");
			(void)close(s);
			return -1;
		}
		pmap_unset(RSTATPROG, RSTATVERS_ORIG);
		pmap_set(RSTATPROG,
			 RSTATVERS_ORIG, IPPROTO_UDP,ntohs(addr.sin_port));

		pmap_unset(RSTATPROG, RSTATVERS_SWTCH);
		pmap_set(RSTATPROG, RSTATVERS_SWTCH,
			 IPPROTO_UDP,ntohs(addr.sin_port));

		pmap_unset(RSTATPROG, RSTATVERS_TIME);

		pmap_set(RSTATPROG, RSTATVERS_TIME,
			 IPPROTO_UDP,ntohs(addr.sin_port));

		if (dup2(s, 0) < 0) {
			perror("dup2");
			do_exit(1);
		}
	}
#endif
	setup();
	updatestat();
	alarm(1);
	signal(SIGALRM, updatestat);

	svc_run();
	syslog(LOG_ERR, "svc_run should never return");
	/* NOTREACHED */
}

#ifdef NON_INET_DEBUG
/*
 * Determine if a descriptor belongs to a socket or not
 */
int
issock(fd)
	int fd;
{
	struct stat st;

	if (fstat(fd, &st) < 0) {
                 return (0);
         }
	/*
	 * SunOS returns S_IFIFO for sockets, while 4.3 returns 0 and does not
	 * even have an S_IFIFO mode.  Since there is confusion about what the
	 * mode is, we check for what it is not instead of what it is.
	 */
	switch (st.st_mode & S_IFMT) {
		case S_IFCHR:
		case S_IFREG:
		case S_IFLNK:
		case S_IFDIR:
		case S_IFBLK:
			return (0);
		default:
			return (1);
	}
}
#endif

static void
stats_service(reqp, transp)
	 struct svc_req  *reqp;
	 SVCXPRT  *transp;
{
	long have;
	
	if (kern_error) {
	    svcerr_systemerr(transp);
	    exit(0);	/* so inetd won't log that we had an error  */
	}
	switch (reqp->rq_proc) {
		case RSTATPROC_STATS:
			sincelastreq = 0;
			if (reqp->rq_vers == RSTATVERS_ORIG) {
				if (svc_sendreply(transp, xdr_stats,
				    &stats.s1) == FALSE) {
					syslog(LOG_ERR,
						"err: svc_rpc_send_results");
					do_exit(1);
				}
				return;
			}
			if (reqp->rq_vers == RSTATVERS_SWTCH) {
				if (svc_sendreply(transp, xdr_statsswtch,
				    &stats.s2) == FALSE) {
					syslog(LOG_ERR,
						"err: svc_rpc_send_results");
					do_exit(1);
				    }
				return;
			}
			if (reqp->rq_vers == RSTATVERS_TIME) {
				if (svc_sendreply(transp, xdr_statstime,
				    &stats.s3) == FALSE) {
					syslog(LOG_ERR,
						"err: svc_rpc_send_results");
					do_exit(1);
				    }
				return;
			}
		case RSTATPROC_HAVEDISK:
			have = numdisks != 0;
			if (svc_sendreply(transp,xdr_long, &have) == 0){
			    syslog(LOG_ERR, "err: svc_sendreply");
			    do_exit(1);
			}
			return;
		case 0:
			if (svc_sendreply(transp, xdr_void, 0)
			    == FALSE) {
				syslog(LOG_ERR, "err: svc_rpc_send_results");
				do_exit(1);
			    }
			return;
		default: 
			svcerr_noproc(transp);
			return;
		}
}

void
updatestat()
{
	ssize_t readlen;
	int i;
	off_t off;
	struct iotime iotim;
	__int32_t	avrun[3];
	struct diosetup *ds;
	struct ifconf ifc;		/* buffer for interface names */
	char buf[BUFSIZ];
	struct ifreq ifs, *ifr;

	if (sincelastreq >= CLOSEDOWN) {
		do_exit(0);
	}
	sincelastreq++;

	if (sysmp(MP_SAGET, MPSA_SINFO, &sinfo, sizeof(struct sysinfo)) < 0) {
		syslog(LOG_ERR, "sysinfo: sysmp: %m");
		kern_error = 1; return;
	}
	stats.s1.cp_time[CP_IDLE] = sinfo.cpu[CPU_IDLE] + sinfo.cpu[CPU_WAIT];
	stats.s1.cp_time[CP_SYS] =
	    sinfo.cpu[CPU_KERNEL] + sinfo.cpu[CPU_INTR] + sinfo.cpu[CPU_SXBRK];
	stats.s1.cp_time[CP_USER] = sinfo.cpu[CPU_USER];
	stats.s1.v_pswpin = sinfo.bswapin;
	stats.s1.v_pswpout = sinfo.bswapout;
	stats.s1.v_intr = sinfo.intr_svcd;
	stats.s2.v_swtch = sinfo.pswitch;

	if (klseek(kmem, (long)nl[X_AVENRUN].n_value, 0) < 0) {
		syslog(LOG_ERR, "avenrun: can't seek in kmem");
		kern_error = 1; return;
	}
	if (read(kmem, avrun, sizeof (avrun)) != sizeof (avrun)) {
		syslog(LOG_ERR, "avenrun: read: %m");
		kern_error = 1; return;
	}
	stats.s2.avenrun[0] = avrun[0]/(1024/FSCALE);
	stats.s2.avenrun[1] = avrun[1]/(1024/FSCALE);
	stats.s2.avenrun[2] = avrun[2]/(1024/FSCALE);

	if ( sysmp(MP_SAGET, MPSA_MINFO, &minfo, sizeof(struct minfo)) ) {
		syslog(LOG_ERR, "minfo: sysmp: %m");
		kern_error = 1; return;
	}
	stats.s1.v_pgpgin = minfo.swap + minfo.file;

	numdisks = tblmap[0];
	off = MIN(DK_NDRIVE, numdisks);

	for ( i = 0, ds = Ndiosetup;  (i < off) && ds; i++, ds = ds->next ) {
#ifdef HWG
		dkiotime_get(ds->dname,&iotim);
#else
		if ( klseek(kmem, (long)ds->iotim_addrs, 0) == -1 ) {
			syslog(LOG_ERR, "klseek: iotime: %m");
			kern_error = 1; return;
		}

		readlen = read(kmem, &iotim, sizeof(iotim));
		if (readlen != sizeof(iotim)) {
			syslog(LOG_ERR, "read: iotim: %m");
			kern_error = 1; return;
		}
#endif
		stats.s1.dk_xfer[i] = iotim.io_cnt;
	}

	stats.s1.if_ipackets = 0;
	stats.s1.if_opackets = 0;
	stats.s1.if_ierrors = 0;
	stats.s1.if_oerrors = 0;
	stats.s1.if_collisions = 0;
	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	if (ioctl(0, SIOCGIFCONF, &ifc) < 0) {
		syslog(LOG_ERR, "SIOCGIFCONF: %m");
		kern_error = 1; return;
	}
	ifr = ifc.ifc_req;
	i = ifc.ifc_len/(int)sizeof(struct ifreq);
	while (i > 0) {
		bzero(&ifs, sizeof(ifs));
		strncpy(ifs.ifr_name, ifr->ifr_name, sizeof(ifs.ifr_name));
		if (ioctl(0, SIOCGIFSTATS, &ifs) < 0) {
			syslog(LOG_ERR, "SIOCGIFSTATS %s: %m",
			       ifr->ifr_name);
			kern_error = 1; return;
		}
		stats.s1.if_ipackets += ifs.ifr_stats.ifs_ipackets;
		stats.s1.if_opackets += ifs.ifr_stats.ifs_opackets;
		stats.s1.if_ierrors += ifs.ifr_stats.ifs_ierrors;
		stats.s1.if_oerrors += ifs.ifr_stats.ifs_oerrors;
		stats.s1.if_collisions += ifs.ifr_stats.ifs_collisions;
		i--;
		ifr++;
	}
	gettimeofday(&stats.s3.curtime, 0);
	signal(SIGALRM, updatestat);
	alarm(1);
}

static void
setup(void)
{
	off_t off;

	struct tms	tms;
	long	kern_end;

	if (__NLIST("/unix", nl) < 0) {
		syslog(LOG_ERR, "/unix: bad namelist: %m");
		kern_error = 1; return;
	}

	for ( off = 0; off <= LAST_K0_SYMB; off++ )
		nl[off].n_value	&= KMEM_OFFSET_MASK;
	if ((kmem = open("/dev/kmem", 0)) < 0) {
		syslog(LOG_ERR, "can't open kmem");
		kern_error = 1; return;
	}

	if (nl[X_KERNEL_MAGIC].n_type == 0 ||
	    nl[X_END].n_type == 0 ||
	    klseek(kmem, (long)nl[X_KERNEL_MAGIC].n_value, 0) < 0 ||
	    read(kmem, &kern_end, sizeof(kern_end)) != sizeof(kern_end) ||
	    kern_end != nl[X_END].n_value) {
		syslog(LOG_ERR, "kernel namelist wrong");
		kern_error = 1; return;
	}

	sgi_dio_setup();
#ifdef HWG
	sgi_disk_setup(sgi_dio_alloc);
#else
	sgi_disk_setup();
#endif
	stats.s2.boottime.tv_sec =  time(0) - times(&tms)/HZ;
	stats.s2.boottime.tv_usec =  0;
}

static void
do_exit(int stat)
{

#ifndef DEBUG
	if (stat != 0) {
		/*
		 * something went wrong.  Sleep awhile and drain our queue
		 * before exiting.  This keeps inetd from immediately retrying.
		 */
		int cc, fromlen;
		char buf[RPCSMALLMSGSIZE];
		struct sockaddr from;

		sleep(CLOSEDOWN);
		for (;;) {
			fromlen = sizeof (from);
			cc = recvfrom(0, buf, sizeof (buf), 0, &from, &fromlen);
			if (cc <= 0)
				break;
		}
	}
#endif
	exit(stat);
}

#ifndef HWG
/*
 * Build a table of pointers to disk iotime structs
 *
 * The iotime structs in the scsi driver are not in a linear array
 * and furthermore there are different kinds of disk drivers: scsi,
 * dkip & xylogics. This routine finds all of the disks in each driver 
 * and builds one array of pointers to each drives iotime struct. 
 * It also builds a parallel array of the device names; these will be
 * explicitly embedded in the data records since not all devices 
 * may be equipped and there may be gaps between drive numbers.
 *
 * By the way, don't forget that rpc.rstatd is just like this.
 */
static void
sgi_disk_setup(void)
{
	register int j,w,c,d;
	struct diosetup *ds;

	if ( nl[X_DKSCIOTIME].n_value != 0 ) /* have dksc disks */
	{
		struct iotime	 thisiot;
		struct dkscinfo	 dkscinfo;
		uintptr_t	*dksciotime;
		uint		 dksciotimesz;

		klseek(kmem, (off_t) nl[X_DKSCINFO].n_value, 0);
		read(kmem, &dkscinfo, sizeof(dkscinfo));

		dksciotimesz = (uint) sizeof(void *) * dkscinfo.maxctlr *
			              dkscinfo.maxtarg * SCSI_MAXLUN;
		dksciotime = (uintptr_t *) malloc(dksciotimesz);
		klseek(kmem, (off_t) nl[X_DKSCIOTIME].n_value, 0);
		read(kmem, dksciotime, dksciotimesz);

		c = 0;
		for ( j = 0; j < dkscinfo.maxctlr; j++ )
		    for ( w = 0; w < dkscinfo.maxtarg; w++ )
			for ( d = 0; d < SCSI_MAXLUN; d++, c++ )
			{
			    if (dksciotime[c] != NULL &&
			        klseek(kmem, (off_t) dksciotime[c], 0 ) != -1 &&
			        read(kmem, &thisiot, sizeof(thisiot))==sizeof(thisiot))
			    {
				ds = sgi_dio_alloc();
				ds->iotim_addrs = (struct iotime *) dksciotime[c];
			    }
			}
	}
}
#endif 
/*
 * CPU Board identifiers
 */
int
cpuboard()
{
	static int board = 0;
	inventory_t *inv;

	if ( board )
		return board;
	
	setinvent();
	while (inv = getinvent())
		if ( inv->inv_class == INV_PROCESSOR &&
			inv->inv_type  == INV_CPUBOARD ) {
			board = inv->inv_state;
			break;
		}
	endinvent();

	return board;
}

/*
 * Seek into the kernel for a value.
 */
off_t
klseek(int fd, off_t base, int whence)
{
	base &= KMEM_OFFSET_MASK;
	return (lseek(fd, base, whence));
}

/*
 * This routine sets up the initial dio data structures
 */
void
sgi_dio_setup()
{
	/*
	 * Allocate space for the first NDEVS elements.
	 */
	AllocdNdiosetup = (struct diosetup *)
				calloc(1, NDEVS * sizeof(struct diosetup));
	if (!AllocdNdiosetup) {
		syslog(LOG_ERR, "No memory for diosetup table\n");
		do_exit(1);
	}
	
	/*
	 * Set the initial index to zero.
	 */
	NdiosetupIndex = 0;
	
	return;	
}

/*
 * Allocate another element.
 */
struct diosetup *
sgi_dio_alloc()
{
	struct diosetup *ds;

	/*
	 * Check to see if we're past the end of allocated elements.
	 */
	if (NdiosetupIndex >= NDEVS) {
		/*
		 * We need to allocate a new chunk of elements.
		 */
		sgi_dio_setup();	
	}
	
	/*
	 * We should have an element.  These should be zero'd out by
	 * sgi_dio_setup.
	 */
	ds = &AllocdNdiosetup[NdiosetupIndex++];
	if (Ndiosetup) {
		CurNdiosetup->next = ds;	
	} else {
		Ndiosetup = ds;
	}
	CurNdiosetup = ds;
	
	/*
	 * Increment the tblmap[0] counter.
	 */
	tblmap[0]++;

	return ds;	
}
