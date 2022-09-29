/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

# include	<sys/types.h>
# include	<sys/param.h>
# include	<sys/sysinfo.h>
# include	<sys/sysmp.h>
# include	<sys/sysget.h>
# include	<sys/times.h>
# include	<sys/ioctl.h>
# include	<sys/socket.h>
# include	<sys/pda.h>

# include	<sys/tcpipstats.h>
# include	<net/if.h>
# include	<stdio.h>
# include	<stdlib.h>
# include	<unistd.h>
# include	<memory.h>
# include	<time.h>
# include	"osview.h"
#define		GET_IPSTAT	0
#define		GET_UDPSTAT	1
#define		GET_TCPSTAT	2

#if (_MIPS_SZLONG == 64)
#define	MAXCPU	512
#else
#define	MAXCPU	64
#endif
#define	MAXIP	2048

static struct sysinfo	*c_splbuf;
static struct sysinfo	*c_spnbuf;
static struct sysinfo	*cell_splbuf;
static struct sysinfo	*cell_spnbuf;
static struct sysinfo	*Sinbuf[2];
static int		sinfosz;
static struct minfo	*mplbuf;
static struct minfo	*mpnbuf;

static struct kna	*knainbuf[2];

static struct igetstats	*igetinbuf[2];
static struct xfsstats	*xfsinbuf[2];
static struct getblkstats	*getblkinbuf[2];
static struct vnodestats	*vnodeinbuf[2];
static struct ncstats	*ncinbuf[2];
int		cpuvalid[MAXCPU];
static int		minfosz;
static int		rminfosz;
#ifdef TILES_TO_LPAGES
static int		tileinfosz;
#endif
static int		ifsock;
static int		igetinfosz;
static int		xfsinfosz;
static int		ncinfosz;
static int		getblkinfosz;
static int		vnodeinfosz;
static int		node_infosz;
static int		lpgstatinfosz;

struct sysinfo		*lsp[MAXCPU];
struct sysinfo		*nsp[MAXCPU];
struct minfo		*lmp;
struct minfo		*nmp;
struct sysinfo		*lSp;
struct sysinfo		*nSp;
struct sysinfo		*lCSp;
struct sysinfo		*nCSp;
struct gfxinfo		*lGp;
struct gfxinfo		*nGp;
struct ipstat		*nipstat;
struct ipstat		*lipstat;
struct udpstat		*nudpstat;
struct udpstat		*ludpstat;
struct tcpstat		*ntcpstat;
struct tcpstat		*ltcpstat;
struct mbstat		*nmbstat;
struct mbstat		*lmbstat;
struct ifreq		*nifreq;
struct ifreq		*lifreq;
struct igetstats	*nigetstat;
struct igetstats	*ligetstat;
struct xfsstats		*nxfsstat;
struct xfsstats		*lxfsstat;
struct getblkstats	*ngetblkstat;
struct getblkstats	*lgetblkstat;
struct vnodestats	*nvnodestat;
struct vnodestats	*lvnodestat;
struct lpg_stat_info	*llpg_stat;
struct lpg_stat_info	*nlpg_stat;
struct ncstats		*nncstat;
struct ncstats		*lncstat;
struct nodeinfo		*node_info;
struct pda_stat		pdastat[MAXCPU];
int			nif;
int			nifsize;
int			avenrun[3];
long			vn_vnodes;
int			vn_epoch, vn_nfree;
int			min_file_pages;
int			syssegsz;

struct rminfo		rminfo;
#ifdef TILES_TO_LPAGES
struct tileinfo		*ltileinfo, *ntileinfo;
#endif

long			lticks;
long			nticks;
long			ticks;
int			nscpus = 1;
int			nacpus;
int			updatelast = 1; /* update 'last' info */
int			numnodes; 	/* Number of nodes in system */ 
int			numcells; 	/* Number of cells in system */ 

extern int  have_gfx;
static void getgfxinfo(void);
static void getinfo_text_start(void);
static void getinfo_text_end(void);

void
initinfo(void)
{
	nscpus = (int)sysmp(MP_NPROCS);
	nacpus = (int)sysmp(MP_NAPROCS);
	mpin((void *)getinfo_text_start,
	     (char *)getinfo_text_end - (char *)getinfo_text_start);
}

void
setinfo(void)
{
	register int		ssize;
	int			i, j, err;
	sgt_cookie_t		ck;
	sgt_info_t		info;

	if ((sinfosz = ssize = (int)sysmp(MP_SASZ, MPSA_SINFO)) < 0) {
		fprintf(stderr, "sysinfo scall interface not supported\n");
		exit(1);
	}
	rminfosz = (int)sysmp(MP_SASZ, MPSA_RMINFO);

#ifdef TILES_TO_LPAGES
	if ((tileinfosz = (int)sysmp(MP_SASZ, MPSA_TILEINFO)) != -1) {
		if (tileinfosz < sizeof (struct tileinfo))
			tileinfosz = sizeof(struct tileinfo);
		ltileinfo = calloc(1, tileinfosz);
		ntileinfo = calloc(1, tileinfosz);
		if (ltileinfo == NULL || ntileinfo == NULL) {
			fprintf(stderr, "tileinfo calloc failed\n");
			exit(1);
		}
	}
#endif /* TILES_TO_LPAGES */

	c_splbuf = (struct sysinfo *) calloc(nscpus, ssize);
	c_spnbuf = (struct sysinfo *) calloc(nscpus, ssize);
	if (!c_splbuf || !c_spnbuf) {
		fprintf(stderr, "Couldn't allocate %d bytes for splbuf\n",
			nscpus * ssize);
		exit(1);
	}
	Sinbuf[0] = (struct sysinfo *) calloc(1, ssize);
	Sinbuf[1] = (struct sysinfo *) calloc(1, ssize);
	lSp = Sinbuf[0];
	nSp = Sinbuf[1];
	lGp = (struct gfxinfo *)calloc(1, nacpus*sizeof(struct gfxinfo));
	nGp = (struct gfxinfo *)calloc(1, nacpus*sizeof(struct gfxinfo));

	/*
	 * If we do a per-cpu readout, then we might encounter an
	 * empty slot for CPUs.  Compress this into the given numer.
	 */
	for (i = 0, j = 0; j < MAXCPU && i < nscpus; j++) {
		if (stg_sysget_cpu1( MPSA_SINFO, (char *) &c_splbuf[i],
			sinfosz, j) == -1)
			cpuvalid[j] = 0;
		else {
			cpuvalid[j] = 1;
			lsp[i] = &c_splbuf[i];
			nsp[i] = &c_spnbuf[i];
			i++;
		}
	}

	minfosz = ssize = (int)sysmp(MP_SASZ, MPSA_MINFO);

	mplbuf = (struct minfo *) calloc(1, ssize);
	lmp = mplbuf;

	mpnbuf = (struct minfo *) calloc(1, ssize);
	nmp = mpnbuf;

	knainbuf[0] = (struct kna *) calloc(1, sizeof(*knainbuf[0]));
	knainbuf[1] = (struct kna *) calloc(1, sizeof(*knainbuf[1]));

	lipstat = &(knainbuf[0]->ipstat);
	nipstat = &(knainbuf[1]->ipstat);

	ludpstat = &(knainbuf[0]->udpstat);
	nudpstat = &(knainbuf[1]->udpstat);

	ltcpstat = &(knainbuf[0]->tcpstat);
	ntcpstat = &(knainbuf[1]->tcpstat);

	lmbstat = &(knainbuf[0]->mbstat);
	nmbstat = &(knainbuf[1]->mbstat);

	if ((ifsock = socket(AF_INET, SOCK_DGRAM, 0)) != -1) {
	   struct ifconf	ifc;
	   char			ifbuf[MAXIP];

		ifc.ifc_len = MAXIP;
		ifc.ifc_buf = ifbuf;
		if (ioctl(ifsock, SIOCGIFCONF, &ifc) == -1) {
			close(ifsock);
			nif = 0;
		}
		else {
		   struct ifreq		*ifr;
		   struct ifreq		*ifr2;

			nif = (int)(ifc.ifc_len / sizeof *ifr);
			nifreq = (struct ifreq *) calloc(1, ifc.ifc_len);
			ifr = (struct ifreq *)ifbuf;
			ifr2 = nifreq;
			for (i = 0; i < nif; i++, ifr++) {
				if (ifr->ifr_addr.sa_family != AF_INET)
					continue;
				memcpy(ifr2++, ifr, sizeof(*ifr));
			}
			nifsize = (int)((char*)ifr2 - (char*)nifreq);
			nif = (int)(nifsize / sizeof *ifr);
			lifreq = (struct ifreq *) calloc(1, nifsize);

			ifr = nifreq;
			for (i = 0; i < nif; i++, ifr++) {
				if (ioctl(ifsock, SIOCGIFSTATS, ifr) == -1) {
					close(ifsock);
					nif = 0;
					break;
				}
			}
			memcpy((char *) lifreq, (char *) nifreq, nifsize);
		}
	}
	else
		nif = 0;

	ncinfosz = (int)sysmp(MP_SASZ, MPSA_NCSTATS);
	ncinbuf[0] = calloc(1, ncinfosz);
	ncinbuf[1] = calloc(1, ncinfosz);
	lncstat = ncinbuf[0];
	nncstat = ncinbuf[1];

	igetinfosz = (int)sysmp(MP_SASZ, MPSA_EFS);
	igetinbuf[0] = calloc(1, igetinfosz);
	igetinbuf[1] = calloc(1, igetinfosz);
	ligetstat = igetinbuf[0];
	nigetstat = igetinbuf[1];

	xfsinfosz = (int)sysmp(MP_SASZ, MPSA_XFSSTATS);
	xfsinbuf[0] = calloc(1, xfsinfosz);
	xfsinbuf[1] = calloc(1, xfsinfosz);
	lxfsstat = xfsinbuf[0];
	nxfsstat = xfsinbuf[1];

	getblkinfosz = (int)sysmp(MP_SASZ, MPSA_BUFINFO);
	getblkinbuf[0] = calloc(1, getblkinfosz);
	getblkinbuf[1] = calloc(1, getblkinfosz);
	lgetblkstat = getblkinbuf[0];
	ngetblkstat = getblkinbuf[1];

	vnodeinfosz = (int)sysmp(MP_SASZ, MPSA_VOPINFO);
	vnodeinbuf[0] = calloc(1, vnodeinfosz);
	vnodeinbuf[1] = calloc(1, vnodeinfosz);
	lvnodestat = vnodeinbuf[0];
	nvnodestat = vnodeinbuf[1];

	lpgstatinfosz = (int)sysmp(MP_SASZ, MPSA_LPGSTATS);
	llpg_stat = calloc(1, lpgstatinfosz);
	nlpg_stat = calloc(1, lpgstatinfosz);

	numnodes = (int)sysmp(MP_NUMNODES);
	if (numnodes == -1)  
		suppress_node_info();
	else {
		node_infosz = (int)sysmp(MP_SASZ, MPSA_NODE_INFO);
		node_info = calloc(numnodes, node_infosz);
	}

	SGT_COOKIE_INIT(&ck);
	err = sysget(SGT_SINFO, (char *)&info, sizeof(info), SGT_INFO, &ck);
	if (err < 0) {
		perror("sysget");
		fprintf(stderr, "SGT_INFO of SGT_SINFO failed\n");
		pbyebye();
	}
	numcells = info.si_num;
	if (numcells == 1) {
		suppress_cell_info();
	}
	else {
		cell_splbuf = (struct sysinfo *)calloc(numcells, sinfosz);
		cell_spnbuf = (struct sysinfo *)calloc(numcells, sinfosz);
		if (!cell_splbuf || !cell_spnbuf) {
			fprintf(stderr, "Couldn't allocate %d bytes for cell_splbuf\n",
				numcells * ssize);
			exit(1);
		}
		lCSp = cell_splbuf;
		nCSp = cell_spnbuf;
		stg_sysget_cells(SGT_SINFO, (char *)cell_splbuf,
			 numcells * sinfosz);
	}

	nticks = 0;
	lticks = 0;
	getinfo();
}

static
void getinfo_text_start(void) {}		/* begin locked text */

void
getinfo(void)
{
   register struct sysinfo	*tsp;
   register struct gfxinfo	*tgp;
   register struct minfo	*tmp;
   register int			i;
   register int			j;
   struct ipstat		*tip;
   struct udpstat		*tudp;
   struct mbstat		*tmb;
   struct tcpstat		*ttcp;
   struct tms			tbuf;
   struct ncstats		*tncstat;
   struct igetstats		*tigetstat;
   struct xfsstats		*txfsstat;
   struct getblkstats		*tgetblkstat;
   struct vnodestats		*tvnodestat;
   struct lpg_stat_info		*tlpg_stat;
#ifdef TILES_TO_LPAGES
   struct tileinfo		*ttileinfo;
#endif

	if (updatelast) {
		lticks = nticks;
		tsp = c_splbuf;
		c_splbuf = c_spnbuf;
		c_spnbuf = tsp;
	}

	stg_sysget_cpus( MPSA_SINFO, (char *) c_spnbuf, sinfosz * nscpus); 

	for (i = 0, j = 0; j < MAXCPU && i < nscpus; j++) {
		if (!cpuvalid[j])
			continue;
		if (updatelast) {
			tsp = lsp[i];
			lsp[i] = nsp[i];
			nsp[i] = tsp;
		} else {
			tsp = nsp[i];
# ifdef FIXGFX
			tsp->cpu[CPU_WAIT] -=
				(tsp->wait[W_GFXF] + tsp->wait[W_GFXC]);
# endif
		}
		i++;
	}

	if (nscpus > 1) {
		if (updatelast) {
			tsp = lSp;
			lSp = nSp;
			nSp = tsp;
		}
		stg_sysget( MPSA_SINFO, (char *) nSp, sinfosz);
# ifdef FIXGFX
		nSp->cpu[CPU_WAIT] -= nSp->wait[W_GFXF] + nSp->wait[W_GFXC];
# endif
	}
	else {
		*lSp = *lsp[0];
		*nSp = *nsp[0];
	}

	if (numcells > 1) {
		if (updatelast) {
			tsp = lCSp;
			lCSp = nCSp;
			nCSp = tsp;
		}
		stg_sysget_cells( MPSA_SINFO, (char *) nCSp,
			 sinfosz * numcells);
# ifdef FIXGFX
		for (i = 0; i < numcells; i++) {
			nCSp[i].cpu[CPU_WAIT] -= nCSp[i].wait[W_GFXF] + 
				nCSp[i].wait[W_GFXC];
# endif
	}

	/*
	 * Now that all the stuff has been read in, calculate the interval.
	 */
	nticks = times(&tbuf);
	ticks = nticks - lticks;

	if (updatelast) {
		tmp = lmp;
		lmp = nmp;
		nmp = tmp;
	}
	stg_sysget( MPSA_MINFO, (char *) nmp, minfosz);
	stg_sysget( MPSA_RMINFO, (char *) &rminfo, rminfosz);

	if (updatelast) {
		tip = lipstat;
		lipstat = nipstat;
		nipstat = tip;

		tudp = ludpstat;
		ludpstat = nudpstat;
		nudpstat = tudp;

		ttcp = ltcpstat;
		ltcpstat = ntcpstat;
		ntcpstat = ttcp;

		tmb = lmbstat;
		lmbstat = nmbstat;
		nmbstat = tmb;
	}
	/*
	 *get network stats for system (each cpu added to total)
	 */
	stg_sysget( MPSA_TCPIPSTATS, (char *)nipstat, sizeof(struct kna));

#ifdef TILES_TO_LPAGES
	if (updatelast) {
		ttileinfo = ltileinfo;
		ltileinfo = ntileinfo;
		ntileinfo = ttileinfo;
	}
	stg_sysget( MPSA_TILEINFO, (char *)ntileinfo, tileinfosz);
#endif /* TILES_TO_LPAGES */

	stg_sread("avenrun", avenrun, sizeof(avenrun));
	stg_sread("vn_nfree", &vn_nfree, sizeof(vn_nfree));
	stg_sread("vn_epoch", &vn_epoch, sizeof(vn_epoch));
	stg_sread("vn_vnumber", &vn_vnodes, sizeof(vn_vnodes));
	stg_sread("syssegsz", &syssegsz, sizeof(syssegsz));
	stg_sread("min_file_pages", &min_file_pages, sizeof(min_file_pages));

	if (updatelast) {
		tgp = lGp;
		lGp = nGp;
		nGp = tgp;
	}

	if (have_gfx)
		getgfxinfo();

	if (nif > 0) {
	   struct ifreq		*tif;

		if (updatelast) {
			tif = lifreq;
			lifreq = nifreq;
			nifreq = tif;
		} else
			tif = nifreq;
		for (i = 0; i < nif; i++, tif++)
			ioctl(ifsock, SIOCGIFSTATS, tif);
	}

	if (updatelast) {
		tncstat = lncstat;
		lncstat = nncstat;
		nncstat = tncstat;
	}
	stg_sysget( MPSA_NCSTATS, (char *) nncstat, ncinfosz);

	if (updatelast) {
		tigetstat = ligetstat;
		ligetstat = nigetstat;
		nigetstat = tigetstat;
	}
	stg_sysget( MPSA_EFS, (char *) nigetstat, igetinfosz);

	if (updatelast) {
		txfsstat = lxfsstat;
		lxfsstat = nxfsstat;
		nxfsstat = txfsstat;
	}
	stg_sysget( MPSA_XFSSTATS, (char *) nxfsstat, xfsinfosz);

	if (updatelast) {
		tgetblkstat = lgetblkstat;
		lgetblkstat = ngetblkstat;
		ngetblkstat = tgetblkstat;
	}
	stg_sysget( MPSA_BUFINFO, (char *) ngetblkstat, getblkinfosz);

	if (updatelast) {
		tvnodestat = lvnodestat;
		lvnodestat = nvnodestat;
		nvnodestat = tvnodestat;
	}
	stg_sysget( MPSA_VOPINFO, (char *) nvnodestat, vnodeinfosz);

	if (updatelast) {
		tlpg_stat = llpg_stat;
		llpg_stat = nlpg_stat;
		nlpg_stat = tlpg_stat;
	}

	stg_sysget( MPSA_LPGSTATS, (char *) nlpg_stat, lpgstatinfosz);

	if (counts)
		updatelast = 0;

	stg_sysget( MPSA_NODE_INFO, (char *) node_info, numnodes * node_infosz);
}

static void
getgfxinfo(void)
{
	int i;

	stg_sread("gfxinfo", nGp, (int)sizeof(struct gfxinfo)*nacpus);
/*
fprintf(stderr,"gfxinfo[%d]->gswitch=%d\n",0,nGp[0].gswitch);
fprintf(stderr,"gfxinfo[%d]->griioctl=%d\n",0,nGp[0].griioctl);
fprintf(stderr,"gfxinfo[%d]->gintr=%d\n",0,nGp[0].gintr);
fprintf(stderr,"gfxinfo[%d]->gintr=%d\n",0,nGp[0].gswapbuf);
fprintf(stderr,"gfxinfo[%d]->gintr=%d\n",0,nGp[0].fifowait);
fprintf(stderr,"gfxinfo[%d]->gintr=%d\n",0,nGp[0].fifonowait);
*/
	for (i=1; i<nacpus; i++) {
/*
fprintf(stderr,"gfxinfo[%d]->gswitch=%d\n",i,nGp[0].gswitch);
fprintf(stderr,"gfxinfo[%d]->griioctl=%d\n",i,nGp[0].griioctl);
fprintf(stderr,"gfxinfo[%d]->gintr=%d\n",i,nGp[0].gintr);
fprintf(stderr,"gfxinfo[%d]->gintr=%d\n",i,nGp[0].gswapbuf);
fprintf(stderr,"gfxinfo[%d]->gintr=%d\n",i,nGp[0].fifowait);
fprintf(stderr,"gfxinfo[%d]->gintr=%d\n",i,nGp[0].fifonowait);
*/
		nGp[0].gswitch  += nGp[i].gswitch;
		nGp[0].griioctl  += nGp[i].griioctl;
		nGp[0].gintr  += nGp[i].gintr;
		nGp[0].gswapbuf  += nGp[i].gswapbuf;
		nGp[0].fifowait  += nGp[i].fifowait;
		nGp[0].fifonowait  += nGp[i].fifonowait;
	}
/*
fprintf(stderr,"new\n");
fprintf(stderr,"gfxinfo->gswitch=%d\n",nGp[0].gswitch);
fprintf(stderr,"gfxinfo->griioctl=%d\n",nGp[0].griioctl);
fprintf(stderr,"gfxinfo->gintr=%d\n",nGp[0].gintr);
fprintf(stderr,"gfxinfo->gintr=%d\n",nGp[0].gswapbuf);
fprintf(stderr,"gfxinfo->gintr=%d\n",nGp[0].fifowait);
fprintf(stderr,"gfxinfo->gintr=%d\n",nGp[0].fifonowait);
fprintf(stderr,"old\n");
fprintf(stderr,"gfxinfo->gswitch=%d\n",lGp[0].gswitch);
fprintf(stderr,"gfxinfo->griioctl=%d\n",lGp[0].griioctl);
fprintf(stderr,"gfxinfo->gintr=%d\n",lGp[0].gintr);
fprintf(stderr,"gfxinfo->gintr=%d\n",lGp[0].gswapbuf);
fprintf(stderr,"gfxinfo->gintr=%d\n",lGp[0].fifowait);
fprintf(stderr,"gfxinfo->gintr=%d\n",lGp[0].fifonowait);
*/
}

static
void getinfo_text_end(void) {}		/* end of locked text */


