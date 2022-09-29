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

# include	"grosview.h"

# include	<sys/sysinfo.h>
# include	<sys/sysmp.h>
# include	<sys/swap.h>
# include	<sys/times.h>
# include	<sys/ioctl.h>
# include	<sys/sbd.h>
# include	<sys/buf.h>
# include	<sys/var.h>
# include	<sys/socket.h>
# include	<sys/stat.h>
# include	<sys/sysmips.h>
# include	<sys/fs/efs_fs.h>
# include	<sys/tcpipstats.h>
# include	<netinet/in_systm.h>
# include	<netinet/in.h>
# include	<netinet/ip.h>
# include	<netinet/ip_var.h>
# include	<netinet/tcp.h>
# include	<netinet/tcp_timer.h>
# include	<netinet/tcp_var.h>
# include	<netinet/udp.h>
# include	<netinet/udp_var.h>
# include	<net/if.h>
# include	<time.h>
# include	<string.h>
# include	<malloc.h>
# include	<fcntl.h>
# include	<mntent.h>
# include	<unistd.h>

static struct sysinfo	*splbuf[MAXCPU];
static struct sysinfo	*spnbuf[MAXCPU];
static struct sysinfo	*Sinbuf[2];
static int		sinfosz;
static struct minfo	*mplbuf;
static struct minfo	*mpnbuf;
static int		minfosz;
static struct kna	*knainbuf[2];
static int		cpuvalid[MAXCPU];
static int		ifsock;
static int		rminfosz;

struct sysinfo		*lsp[MAXCPU];
struct sysinfo		*nsp[MAXCPU];
struct minfo		*lmp;
struct minfo		*nmp;
struct sysinfo		*lSp;
struct sysinfo		*nSp;
struct gfxinfo		*lGp;
struct gfxinfo		*nGp;
struct ipstat		*nipstat;
struct ipstat		*lipstat;
struct udpstat		*ludpstat;
struct udpstat		*nudpstat;
struct tcpstat		*ntcpstat;
struct tcpstat		*ltcpstat;
struct ifreq		*nifreq = 0;
struct ifreq		*lifreq;
struct rminfo		*lrm;
struct rminfo		*nrm;

off_t			fswap;		/* current free swap in blocks */
off_t			mswap;		/* current max growable swap */
off_t			tswap;		/* current physical swap */
off_t			vswap;		/* current virtual swap */
off_t			rlswap;		/* current reserved logical swap */
off_t			llswap;		/* last max logical swap */
off_t			nlswap;		/* new max logical swap */
int			nif;
int			nifsize;

long			lticks;
long			nticks;
long			ticks;
int			ncpu = 1;
int			nacpu = 1;
long			nscpus = 1;
long			nmcpus = 1;

long			physmem;
buf_t*			bufaddr;
struct var*		var;

static void getgfxinfo(void);

void
initinfo(void)
{
	register int err = 0;
	register ptrdiff_t i;

	ncpu = sysmp(MP_NPROCS);
	nacpu = sysmp(MP_NAPROCS);

	stg_open();

	if ((i = sysmp(MP_KERNADDR, MPKA_PHYSMEM)) == -1) {
		fprintf(stderr, "can't get address of physmem");
		err++;
	} else
		stg_vread(i, &physmem, sizeof(physmem));

	if ((i = sysmp(MP_KERNADDR, MPKA_BUF)) == -1) {
		fprintf(stderr, "can't get address of buf[0]");
		err++;
	}
	bufaddr = (buf_t *)i;

	if ((i = sysmp(MP_KERNADDR, MPKA_VAR)) == -1) {
		fprintf(stderr, "can't get address of struct var ``v''");
		err++;
	}
	var = (struct var *)i;

	if (err)
		exit(1);
}

void
setinfo(void)
{
   register int		ssize;
   int			i;
   int			j;

	if ((sinfosz = ssize = sysmp(MP_SASZ, MPSA_SINFO)) < 0) {
		fprintf(stderr, "sysinfo scall interface not supported\n");
		exit(1);
	}

	nscpus = sysmp(MP_NPROCS);
	for (i = 0; i < nscpus; i++) {
		splbuf[i] = (struct sysinfo *) calloc(1, ssize);
		lsp[i] = splbuf[i];

		spnbuf[i] = (struct sysinfo *) calloc(1, ssize);
		nsp[i] = spnbuf[i];
	}
	Sinbuf[0] = (struct sysinfo *) calloc(1, ssize);
	Sinbuf[1] = (struct sysinfo *) calloc(1, ssize);
	lSp = Sinbuf[0];
	nSp = Sinbuf[1];
	lGp = (struct gfxinfo *)calloc(nacpu, sizeof(struct gfxinfo));
        nGp = (struct gfxinfo *)calloc(nacpu, sizeof(struct gfxinfo));



	/*
	 * If we do a per-cpu readout, then we might encounter an
	 * empty slot for CPUs.  Compress this into the given numer.
	 */
	for (i = 0, j = 0; j < MAXCPU && i < nscpus; j++) {
		if (sysmp(MP_SAGET1, MPSA_SINFO, (char *) splbuf[i],
			sinfosz, j) == -1)
			cpuvalid[j] = 0;
		else {
			cpuvalid[j] = 1;
			i++;
		}
	}

	minfosz = ssize = sysmp(MP_SASZ, MPSA_MINFO);

	mplbuf = (struct minfo *) calloc(1, ssize);
	lmp = mplbuf;

	mpnbuf = (struct minfo *) calloc(1, ssize);
	nmp = mpnbuf;

	/* init rminfo buffers */
	rminfosz = ssize = sysmp(MP_SASZ, MPSA_RMINFO);
	lrm = calloc(1, ssize);
	nrm = calloc(1, ssize);

	knainbuf[0] = (struct kna *) calloc(1, sizeof(struct kna));
	knainbuf[1] = (struct kna *) calloc(1, sizeof(struct kna));

	lipstat = &(knainbuf[0]->ipstat);
	nipstat = &(knainbuf[1]->ipstat);

	ludpstat = &(knainbuf[0]->udpstat);
	nudpstat = &(knainbuf[1]->udpstat);

	ltcpstat = &(knainbuf[0]->tcpstat);
	ntcpstat = &(knainbuf[1]->tcpstat);

	if (gi_if)
		netifinfo((char *) 0);

	nticks = 0;
	lticks = 0;
	getinfo();
}

int
netifinfo(char *s)
{
   int		i;
   struct ifreq	*ift;

	if (nifreq == 0) {
	   char			ipbuf[MAXIP];
	   struct ifconf	ifc;
	   struct ifreq		*ifr;
	   struct ifreq		*ifr2;

		if ((ifsock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
			fprintf(stderr,
				"%s: networking not enabled\n",
				pname);
			return(-1);
		}
		ifc.ifc_len = sizeof(ipbuf);
		ifc.ifc_buf = ipbuf;
		if (ioctl(ifsock, SIOCGIFCONF, &ifc) == -1) {
			fprintf(stderr,
			"%s: unable to monitor network interfaces\n",
				pname);
			close(ifsock);
			return(-1);
		}
		nifsize = ifc.ifc_len;
		nif = nifsize / sizeof(struct ifreq);
		if (nif == 0) {
			fprintf(stderr,
				"%s: no network interfaces present\n",
				pname);
			close(ifsock);
			return(-1);
		}
		nifreq = (struct ifreq *) calloc(1, nifsize);
		ifr = (struct ifreq *)ipbuf;
		ifr2 = nifreq;
		for (i = 0; i < nif; i++, ifr++) {
			if (ifr->ifr_addr.sa_family != AF_INET)
				continue;
			memcpy(ifr2++, ifr, sizeof(*ifr));
		}
		nifsize = (char*)ifr2 - (char*)nifreq;
		nif = nifsize / sizeof *ifr;
		if (nif == 0) {
			fprintf(stderr,
				"%s: no network interfaces present\n",
				pname);
			close(ifsock);
			free(nifreq);
			nifreq = 0;
			return(-1);
		}
		lifreq = (struct ifreq *) calloc(1, nifsize);
	}
	if (s != 0) {
		i = 0;
		for (ift = nifreq, i = 0; i < nif; i++, ift++) {
			if (eq(ift->ifr_name, s)) {
				if (ioctl(ifsock,SIOCGIFSTATS,ift)==-1) {
					fprintf(stderr,
					"%s: no IP status in this kernel\n",
						pname);
					return(-1);
				}
				return(i);
			}
		}
		fprintf(stderr, "%s: no network interface \"%s\" found\n",
			pname, s);
		return(-1);
	}
	else {
		if (nif > 0) {
			ift = nifreq;
			for (i = 0; i < nif; i++, ift++)
				ioctl(ifsock, SIOCGIFSTATS, ift);
			memcpy((char *) lifreq, (char *) nifreq, nifsize);
		}
		return(0);
	}
}

int
diskinfo(char *s)
{
   struct stat		sbuf;
   int			rstat;

	if (s == 0) {
	   int 		i;

		for (i = 0; i < ndisknames; i++) {
			if (dstat[i].d_sname) free(dstat[i].d_sname);
			if (dstat[i].d_name) free(dstat[i].d_name);
		}
		ndisknames = 0;
		return(0);
	}

	if (debug)
		fprintf(stderr, "diskinfo: finding \"%s\"\n", s);
	if (ndisknames == MAXDISK) {
		fprintf(stderr, "%s: too many disks to monitor\n",
			pname);
		return(-1);
	}
	rstat = ndisknames;
	dstat[rstat].d_flags = 0;

	/*
	 * Verify that this actually indicates a valid
	 * filesystem to monitor.
	 */
	if (stat(s, &sbuf) == -1) {
		fprintf(stderr, "%s: unknown filesystem\n", pname);
		return(-1);
	}
	if ((sbuf.st_mode & S_IFMT) == S_IFREG ||
	    (sbuf.st_mode & S_IFMT) == S_IFDIR) {
	   FILE		*md;

		if ((md = setmntent(MNTTAB, "r")) != NULL) {
		   struct mntent	*mp;
		   char			*fsname;
		   char			*fstyp;
		   int			len;
		   int			match;

			match = 0;
			fsname = 0;
			while ((mp = getmntent(md)) != NULL) {
				len = strlen(mp->mnt_dir);
				if (strncmp(mp->mnt_dir,s,len)==0) {
					if (len > match) {
						if (fsname != 0) {
							free(fsname);
							free(fstyp);
						}
						fsname = strdup(mp->mnt_fsname);
						fstyp = strdup(mp->mnt_type);
					}
					match = len;
				}
			}
			endmntent(md);
			if (strcmp(fstyp, FSID_EFS) == 0)
				dstat[rstat].d_flags |= DF_EFS;
			else if (strcmp(fstyp, FSID_NFS) == 0)
				dstat[rstat].d_flags |= DF_NFS;
			else if (strcmp(fstyp, FSID_XFS) == 0)
				dstat[rstat].d_flags |= DF_XFS;
			free(fstyp);
			free(fsname);
		}
		dstat[rstat].d_sname = strdup(s);

		/*
		 * Regular file or directory.  Mounted somewhere.
		 */
		if (statfs(dstat[rstat].d_sname, &dstat[rstat].d_stat,
			sizeof(struct statfs), 0) == -1) {
			fprintf(stderr, "%s: unknown filesystem \"%s\"\n",
				pname, dstat[rstat].d_sname);
			return(-1);
		}
	}
	else if ((sbuf.st_mode & S_IFMT) ==  S_IFBLK) {
	   int		fdes;
	   int		fstyp_efs;
	   int		fstyp_xfs;

		/*
		 * Block special.  Has to be an EFS or XFS filesystem.
		 */
		if ((fdes = open(s, O_RDONLY)) == -1) {
			fprintf(stderr, "%s: no permission\n", pname);
			return(-1);
		}
		fstyp_efs = sysfs(GETFSIND, FSID_EFS);
		fstyp_xfs = sysfs(GETFSIND, FSID_XFS);
		if (fstatfs(fdes, &dstat[rstat].d_stat, sizeof(struct statfs),
				fstyp_efs) != -1) {
			dstat[rstat].d_flags |= DF_EFS;
			dstat[rstat].d_fstyp = fstyp_efs;
		}
		else if (fstatfs(fdes, &dstat[rstat].d_stat,
				sizeof(struct statfs), fstyp_xfs) != -1) {
			dstat[rstat].d_flags |= DF_XFS;
			dstat[rstat].d_fstyp = fstyp_xfs;
		}
		else {
			fprintf(stderr, "%s: unknown filesystem\n", pname);
			return(-1);
		}
		dstat[rstat].d_flags |= DF_FSSTAT;
		dstat[rstat].d_fdes = fdes;
	}
	else {
		fprintf(stderr, "%s: can't find disk \"%s\"\n", pname, s);
		return(-1);
	}
	ndisknames++;
	return(rstat);
}

void
getinfo_text_start(void) {}	/* begin locked text */

void
getinfo(void)
{
   register struct sysinfo	*tsp;
   register struct gfxinfo	*tgp;
   register struct minfo	*tmp;
   register struct rminfo	*trm;
   register int			i;
   register int			j;
   struct tms			tbuf;
   struct ipstat		*tip;
   struct udpstat		*tudp;
   struct tcpstat		*ttcp;

	lticks = nticks;
	nticks = times(&tbuf);
	ticks = nticks - lticks;

	for (i = 0, j = 0; j < MAXCPU && i < nscpus; j++) {
		if (!cpuvalid[j])
			continue;
		tsp = lsp[i];
		lsp[i] = nsp[i];
		nsp[i] = tsp;
# ifdef MP_SAGET1
		if (nscpus == 1) {
# endif
			sysmp(MP_SAGET, MPSA_SINFO, (char *) tsp, sinfosz);
# ifdef FIXGFX
			tsp->cpu[CPU_WAIT] -=
				(tsp->wait[W_GFXF] + tsp->wait[W_GFXC]);
# endif
		}
# ifdef MP_SAGET1
		else {
			sysmp(MP_SAGET1, MPSA_SINFO, (char *) tsp, sinfosz, j);
# ifdef FIXGFX
			tsp->cpu[CPU_WAIT] -=
				(tsp->wait[W_GFXF] + tsp->wait[W_GFXC]);
# endif
		}
# endif
		i++;
	}
	if (nscpus > 1) {
		tsp = lSp;
		lSp = nSp;
		nSp = tsp;
		sysmp(MP_SAGET, MPSA_SINFO, (char *) tsp, sinfosz);
# ifdef FIXGFX
		tsp->cpu[CPU_WAIT] -= tsp->wait[W_GFXF] + tsp->wait[W_GFXC];
# endif
	}
	else {
		*lSp = *lsp[0];
		*nSp = *nsp[0];
	}

	/* get minfo stats */
	tmp = lmp;
	lmp = nmp;
	nmp = tmp;
	sysmp(MP_SAGET, MPSA_MINFO, (char *) tmp, minfosz);

	/* get rminfo stats */
	trm = lrm;
	lrm = nrm;
	nrm = trm;
	sysmp(MP_SAGET, MPSA_RMINFO, (char *) trm, rminfosz);

	/* get gfxinfo */
	tgp = lGp;
	lGp = nGp;
	nGp = tgp;
	getgfxinfo();

	if (gi_ip || gi_udp || gi_tcp) {
		/*
		 * Get the tcp/ip statistics for all cpu's in one call
		 */
		(void) sysmp(MP_SAGET, MPSA_TCPIPSTATS,
			    (struct kna *)nipstat, sizeof(struct kna));
		/*
		 * We get one set of stats from the kernel for all the
		 * tcp/ip protocols. We then for all sets swap the two
		 * pointers to correspond to the latest and oldest values.
		 */
		tip = lipstat;
		lipstat = nipstat;
		nipstat = tip;

		tudp = ludpstat;
		ludpstat = nudpstat;
		nudpstat = tudp;

		ttcp = ltcpstat;
		ltcpstat = ntcpstat;
		ntcpstat = ttcp;
	}

	if (gi_if)
		netifinfo((char *) 0);

	if (gi_swap) {
		llswap = nlswap;
		swapctl(SC_GETFREESWAP, &fswap);
		swapctl(SC_GETSWAPMAX, &mswap);
		swapctl(SC_GETSWAPVIRT, &vswap);
		swapctl(SC_GETSWAPTOT, &tswap);
		swapctl(SC_GETLSWAPTOT, &nlswap);
		swapctl(SC_GETRESVSWAP, &rlswap);
	}

	/* get disk info */
	for (i = 0; i < ndisknames; i++) {
		if (dstat[i].d_flags & DF_FSSTAT)
			fstatfs(dstat[i].d_fdes, &dstat[i].d_stat,
				sizeof(struct statfs),
				dstat[i].d_fstyp);
		else
			statfs(dstat[i].d_sname, &dstat[i].d_stat,
				sizeof(struct statfs), 0);
	}
}


static void
getgfxinfo(void)
{
	extern int isgfx(void);
        static off64_t gfxinfo_ptr = 0;

	if (isgfx()) {
	        if (gfxinfo_ptr == 0)
		        stg_sptr_read("gfxinfo", &gfxinfo_ptr);
		stg_vread(gfxinfo_ptr, nGp, sizeof(struct gfxinfo)*nacpu);
	}
}


void
getinfo_text_end(void) {}		/* end of locked text */
