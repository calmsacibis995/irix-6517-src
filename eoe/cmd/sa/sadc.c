/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/
/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)sa:sadc.c	1.25" */
#ident	"$Revision: 1.62 $"
/*	sadc.c 1.25 of 11/27/85	*/
/*
	sadc.c - writes system activity binary data from /dev/kmem to a
		file or stdout.
	Usage: sadc [t n] [file]
		if t and n are not specified, it writes
		a dummy record to data file. This usage is
		particularly used at system booting.
		If t and n are specified, it writes system data n times to
		file every t seconds.
		In both cases, if file is not specified, it writes
		data to stdout.
*/
#undef SA_DEBUG
#define _KMEMUSER
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <stddef.h>
#include <sys/var.h>
#include <sys/sema.h>
#include <sys/iobuf.h>
#include <sys/stat.h>

#include <sys/vnode.h>
#include <sys/sbd.h>
#include <sys/immu.h>
#include <sys/sysmp.h>
#include <sys/syssgi.h>
#include <sys/sysinfo.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/kmem.h>
#include <sys/utsname.h>
#include <sys/sysget.h>
#include "sa.h"

#include <sys/buf.h>
#include <sys/dvh.h>
#include <sys/scsi.h>
#include <sys/dksc.h>
#include <sys/dmamap.h>
#include <sys/ksa.h>
#include <sys/errno.h>
#include <sys/signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/attributes.h>
#include <sys/invent.h>
#include <sys/conf.h>
#include <sys/iograph.h>
#include <diskinfo.h>
#include <ftw.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>

struct disk_iostat_info *Ndiosetup, *CurNdiosetup, *AllocdNdiosetup;
int NdiosetupIndex;
struct sgidio *Sgidio;

struct stat ubuf,syb;
struct var *tbl;
struct syserr *err;
struct vnodestats vnodestat;

struct sa *d;
struct sysinfo *si;
struct minfo *mi;
struct rminfo *ri;
struct dinfo *di;
struct gfxinfo *gi;
struct infomap infomap;
int i,j,k;
int nsites;
int sysinfo_sz;
int f;

void sgi_dio_setup(int count);
struct disk_iostat_info *sgi_dio_alloc();
void sgi_disk_read(void), sgi_sys_setup(void);
void write_rec(char *, char *, int, int, char *);
void kread(char *, void **, int, int);
void ksaread(void);

int ncpus, ndisk;
struct sysinfo_cpu *cst;
sgt_stat_t *cstat;
int fp;

extern int errno;

#ifdef SA_DEBUG
void sadc_iotime_print(struct sgidio *);
#endif /* SA_DEBUG */

static struct pageconst pageconst;

/*
 * These defines have been added based on the use of
 * the following pagesize-dependent macros:
 *
 *	ctob
 *	dtop
 */
#define	_PAGESZ		pageconst.p_pagesz
#define	PNUMSHFT	pageconst.p_pnumshft

struct kname {
        char *name;
        int  count;
};
struct kname knames[] = {
	"v",			-1,
        "_physmem_start",       -1,
        "vn_vnumber",           -1,
        "vn_nfree",             -1,
        "vn_epoch",             -1,
        "syssegsz",             -1,
	"file_zone",		-1,
	"gfxinfo",		-1,
	"min_file_pages",	-1,
};
int num_knames = sizeof(knames) / sizeof(struct kname);

static int proctbl(int);
static int zonefree(char *);

main(int argc, char *const argv[])
{
	int ct;
	unsigned ti;
	time_t min;
	struct stat buf;
	char *fname;
	struct sysinfo_cpu *cstp;

#ifdef SA_DEBUG
	printf("sadc : argv[1] = %s, argv[2] = %s\n",argv[1],argv[2]);
#endif
	ct = argc >= 3? atoi(argv[2]): 0;
	min = time((time_t *) 0);
	ti = argc >= 3? atoi(argv[1]): 0;

	if (syssgi(SGI_CONST, SGICONST_PAGESZ, &pageconst,
					sizeof(pageconst), 0) == -1)
		perrexit("");

	sgi_dio_setup(0);

	/* Save machine/unix info. This will only change at restarts */

	sgi_sys_setup();
	if (uname(&infomap.uname) < 0) {
		perrexit("uname failed");
	}

	/* Save machine attributes */

	infomap.clk_tck = HZ;
	infomap.long_sz = sizeof(long);

	if (argc == 3 || argc == 1){

		/* no data file is specified, direct data to stdout */

		signal(SIGPIPE, SIG_IGN); /* For write to fail on pipe close */
		fp = 1;
		write_rec(INFO_MAGIC, (char *)&infomap, sizeof(infomap),
			1, "Could not write header record");
#ifdef NOTYET
		write_rec(SCNAME_MAGIC, (char *)syscallname, MAXSCNAME,
			MAXSYSENT, "Error writing syscallname");
		write_rec(LIDNAME_MAGIC, (char *)lid_name, MAXLIDNAME,
			MAXLID, "Error writing lid_name");
#endif

	}
	else {
		fname = argc==2? argv[1]: argv[3];
		/*	check if the data file is there	*/
		/*	check if data file is too old	*/
		if ((stat(fname, &buf) == -1) ||
		 ((min - buf.st_mtime) > 86400))
			goto credfl;
		if ((fp = open(fname, O_RDWR)) == -1){
credfl:

			/* data file does not exist:
			 * create one and write the header record.
			 */

			if ((fp = creat(fname,00644)) == -1)
				perrexit(fname);
			close(fp);
			fp = open (fname,2);
			lseek(fp,0L,SEEK_SET);
			write_rec(INFO_MAGIC, (char *)&infomap, sizeof(infomap),
				1, "Could not write header record");
#ifdef NOTYET
			write_rec(SCNAME_MAGIC, (char *)syscallname, MAXSCNAME,
				MAXSYSENT, "Error writing syscallname");
			write_rec(LIDNAME_MAGIC, (char *)lid_name, MAXLIDNAME,
				MAXLID, "Error writing lid_name");
#endif
		
		}
		else{
			/* position the write pointer to end of the file */

			lseek(fp, 0, SEEK_END);
		}
	}

	/* if n =0 , write the additional dummy record	*/

	if (ct == 0){
		d->apstate = CPUDUMMYTIME;
		d->ts = min;

		/* Save uname data in case unix has changed */

		strcpy(((struct revname *)d)->release, infomap.uname.release);
		strcpy(((struct revname *)d)->version, infomap.uname.version);

		for (i = 1; i < nsites; i++) {

			/* Copy this too all them */

			d[i] = *d;
		}

		write_rec(SA_MAGIC, (char *)d, sizeof(struct sa),
			nsites, "Could not write dummy record");

#ifdef SA_DEBUG
		printf("sadc : wrote dummy fd = %d ,d.numdisks = %d\n",fp,d.numdisks);
#endif

		/* clean it out once we've written it */

		for (i = 0; i < nsites; i++) {
			memset(&d[i], 0, sizeof(struct revname));
		}
	}

	sgi_neo_disk_setup(sgi_dio_alloc);

	/* get memory for tables */

	kread("v", (void **)&tbl, sizeof(struct var), 1);

	for(;;) {

		/* read sysinfo, minfo, dinfo, syserr, vopinfo */
		ksaread();

		sgi_disk_read();

		/* compute size of system tables */

		for (i = 0; i < nsites; i++) {
			d[i].szfile = si[i].filecnt;
			d[i].mszfile = d[i].szfile + 
					zonefree("file_zone");
			d[i].szproc = proctbl(i);

			/* record system table sizes */

			d[i].szlckr = si[i].reccnt;
			d[i].mszlckr = d[i].szlckr;

			/* record maximum sizes of system tables */

			d[i].mszproc = tbl[i].v_proc;

			/* get time stamp */
			if (i == 0) {
				d->ts = time ((time_t *) 0);
			}
			else {
				d[i].ts = d->ts;
			}
		}

		i=0;

		/* write data to data file from structure d */

		write_rec(SA_MAGIC, (char *)d, sizeof(struct sa), nsites,
			"Could not write sa record");
		write_rec(SINFO_MAGIC, (char *)si, sizeof(struct sysinfo),
			 nsites, "Could not write sysinfo record");
		write_rec(MINFO_MAGIC, (char *)mi, sizeof(struct minfo),
			nsites, "Could not write minfo record");
		write_rec(DINFO_MAGIC, (char *)di, sizeof(struct dinfo),
			nsites, "Could not write dinfo record");
		write_rec(GFX_MAGIC, (char *)gi, sizeof(struct gfxinfo),
			nsites, "Could not write gfxinfo record");
		write_rec(RMINFO_MAGIC, (char *)ri, sizeof(struct rminfo),
			nsites, "Cound not write rminfo record");
		for (i = 0, cstp = cst; i < nsites; i++) {

			/* For each site we write a CPU record */

			write_rec(CPU_MAGIC, (char *)cstp,
				 sizeof(struct sysinfo_cpu), d[i].apstate,
				"Could not write cpu record");
			cstp += d[i].apstate;
		}
		write_rec(NEODISK_MAGIC, (char *)Sgidio, sizeof(struct sgidio),
			 ndisk, "Could not write disk record");
		
#ifdef SA_DEBUG
	{
		static int	th=0;
		printf("sadc : wrote %dth time to fd = %d d.numdisks = %d\n",
		       ++th,fp,ndisk);
	}
#endif
		if(--ct > 0)
			sleep(ti);
		else {
			close(fp);
			exit(0);
		}
	}
}

/*ARGSUSED*/
static int
zonefree(char *sym)
{
#if 0
    /*
     * zone_t structure is not visible to the user level programs anymore
     * It's a kernel private data structure, and should be treated like
     * that. 
     * Anyway, the mechanism here is broken. file_zone does not give any
     * idea of the maximum number of files that can be opened. This 
     * needs to be fixed.
     */

    zone_t file_zone, *addr1;

    lseek(f, addr, 0);
    read(f, &addr1, sizeof(addr1));
    lseek(f, (long)addr1, 0);
    read(f, &file_zone, sizeof(file_zone));
    return(file_zone.zone_freesize);
#else
    return 0;
#endif
}

#include <paths.h>
#include <dirent.h>
static DIR *dd = NULL;

static int
proctbl(int x)
{
	int nactive = 0;
	struct dirent *ent;

	if (x > 0) {

		/* We don't have a way to get procs per cell yet */

		return 0;
	}

	if (!dd) {
		if ((dd = opendir(_PATH_PROCFSPI)) == NULL) {
			fprintf(stderr, "sadc: can't read process status\n");
			perrexit("");
		}
	}

	rewinddir(dd);
	while (ent = readdir(dd)) {
		if (ent->d_name[0] == '.')
			continue;
		nactive++;
	}

	return nactive;
}

void
perrexit(const char *mesg)
{
	fprintf(stderr, "sadc: ");
	perror(mesg);
	exit(2);
}


void
getgfxinfo(void)
{
	static gfxinfo_t *gfxinfo;
	int i, j, start;

	if (!gi) {
		gi = (struct gfxinfo *)calloc(sizeof(struct gfxinfo), 
			d->apstate);
		if (!gi) {
			fprintf(stderr, "Can't alloc gfxinfo\n");
			perrexit("getgfxinfo");
		}
	}
	kread("gfxinfo", (void **)&gfxinfo, sizeof(gfxinfo_t), 0);
	if (!gfxinfo) {
		return;
	}
	for (j = 0; j < nsites; j++) {
		start = j * d->apstate;
		gi[j].gswitch  = gfxinfo[start].gswitch;
		gi[j].griioctl  = gfxinfo[start].griioctl;
		gi[j].gintr  = gfxinfo[start].gintr;
		gi[j].gswapbuf  = gfxinfo[start].gswapbuf;
		gi[j].fifowait  = gfxinfo[start].fifowait;
		gi[j].fifonowait  = gfxinfo[start].fifonowait;
		for (i=1; i< d->apstate; i++) {
			gi[j].gswitch  += gfxinfo[start + i].gswitch;
			gi[j].griioctl  += gfxinfo[start + i].griioctl;
			gi[j].gintr  += gfxinfo[start + i].gintr;
			gi[j].gswapbuf  += gfxinfo[start + i].gswapbuf;
			gi[j].fifowait  += gfxinfo[start + i].fifowait;
			gi[j].fifonowait  += gfxinfo[start + i].fifonowait;
		}
	}
}
	
	
/*
 * ksaread - read info from kernel (rather than using kmem)
 */
void
ksaread(void)
{
	static long *vn_vnumber = (long *)0;
	static int *vn_epoch = (int *)0;
	static int *vn_nfree = (int *)0;
	static int *min_file_pages = (int *)0;
	int i, tcpus;
	sgt_cookie_t ck;

	for (i = 0, tcpus = 0; i < nsites; i++) {

		/* how many processors on-line */

		d[i].apstate = cstat[i].info.si_num;
		tcpus += d[i].apstate;

	}
	if (ncpus != tcpus) {
		cst = realloc(cst, (sizeof(struct sysinfo_cpu) * tcpus));
		if (!cst) {
			perrexit("");
		}
		ncpus = tcpus;
	}

	if (nsites == 1) {
		if (sysmp(MP_SAGET,MPSA_SINFO,si,sizeof(struct sysinfo)))
			perrexit("");
		if (sysmp(MP_SAGET,MPSA_MINFO,mi,sizeof(struct minfo)))
			perrexit("");
		if (sysmp(MP_SAGET,MPSA_RMINFO,ri,sizeof(struct rminfo)))
			perrexit("");
		if (sysmp(MP_SAGET,MPSA_DINFO,di,sizeof(struct dinfo)))
			perrexit("");
		if (sysmp(MP_SAGET,MPSA_SERR,err,sizeof(struct syserr)))
			perrexit("");
		for (i = 0; i < d->apstate; i++) {

			/* The cpu stats are at the front of sysinfo */

			if (sysmp(MP_SAGET1, MPSA_SINFO, &cst[i],
					 sizeof(struct sysinfo_cpu), i) < 0) {
				continue;
			}
		}
	}
	else {

		/* Get data for each cell */

#ifdef NOTYET
		/* The performace of this looks terrible.  It might be better
		 * to consolidate all this info into one structure to minimize
		 * the number of sysget calls.
		 */
#endif

		SGT_COOKIE_INIT(&ck);
		if (sysget(SGT_SINFO, (char *)si, sysinfo_sz * nsites,
			 SGT_READ, &ck) < 0) {
			perrexit("SGT_SINFO failed");
		}
		SGT_COOKIE_INIT(&ck);
		if (sysget(SGT_MINFO, (char *)mi, (int)sizeof(struct minfo) * nsites,
			 SGT_READ, &ck) < 0) {
			perrexit("SGT_MINFO failed");
		}
		SGT_COOKIE_INIT(&ck);
		if (sysget(SGT_RMINFO, (char *)ri, (int)sizeof(struct rminfo) *
			 nsites, SGT_READ, &ck) < 0) {
			perrexit("SGT_RMINFO failed");
		}
		SGT_COOKIE_INIT(&ck);
		if (sysget(SGT_DINFO, (char *)di, (int)sizeof(struct dinfo) * nsites,
			 SGT_READ, &ck) < 0) {
			perrexit("SGT_DINFO failed");
		}
		SGT_COOKIE_INIT(&ck);
		if (sysget(SGT_SERR, (char *)err, (int)sizeof(struct syserr) *
			 nsites, SGT_READ, &ck) < 0) {
			perrexit("SGT_SERR failed");
		}
		SGT_COOKIE_INIT(&ck);
		if (sysget(SGT_SINFO_CPU, (char *)cst,
			 	(int)sizeof(struct sysinfo_cpu) * ncpus, 
				SGT_READ | SGT_CPUS, &ck) < 0) {
			perrexit("SGT_SINFO_CPU failed");
		}
	}

	kread("vn_vnumber", (void **)&vn_vnumber, sizeof(vn_vnumber), 1);
	kread("vn_epoch", (void **)&vn_epoch, sizeof(vn_epoch), 1);
	kread("vn_nfree", (void **)&vn_nfree, sizeof(vn_nfree), 1);
	kread("min_file_pages", (void **)&min_file_pages,
		 sizeof(min_file_pages), 1);

	for (i = 0; i < nsites; i++) {
		d[i].inodeovf = (long)err[i].inodeovf;
		d[i].fileovf = (long)err[i].fileovf; 
		d[i].procovf = (long)err[i].procovf; 
		d[i].mszinode = (int)(vn_vnumber[i] - vn_epoch[i]);
		d[i].szinode = d[i].mszinode - vn_nfree[i];

		/* translate from clicks to disk blocks */

		si[i].bswapin=(unsigned)(ctob(si[i].bswapin)/NBPSCTR);  /*BSUZE to NBPSCTR*/
		si[i].bswapout=(unsigned)(ctob(si[i].bswapout)/NBPSCTR);/*BSIZE to NBPSCTR*/

		/* Fix availsmem to take into account min_file_pages cutoff */

		ri[i].availsmem -= min_file_pages[i];
	}

	getgfxinfo();
}

#include <sys/iograph.h>

#ifdef SA_DEBUG
void
sadc_iotime_print(struct sgidio *ds)
{

	printf("ds->sdio_lname = %s \n",ds->sdio_lname);


	printf("io_ops=%d,io_misc=%d,io_qcnt=%d,io_unlog=%d,io_bcnt=%d\n"
	       ",io_resp=%ld,io_act=%ld,io_wops=%d,io_wbcnt=%d\n",
	       ds->sdio_iotim.ios.io_ops,
	       ds->sdio_iotim.ios.io_misc,
	       ds->sdio_iotim.ios.io_qcnt,
	       ds->sdio_iotim.ios.io_unlog,
	       ds->sdio_iotim.io_bcnt,
	       ds->sdio_iotim.io_resp,
	       ds->sdio_iotim.io_act,
	       ds->sdio_iotim.io_wops,
	       ds->sdio_iotim.io_wbcnt);
	
}
#endif /* SA_DEBUG */

/*
 * read an iotime struct for each drive and store its counters and name in d
 */
void
sgi_disk_read(void)
{
	register int c;
	struct disk_iostat_info *ds;

	c = ndisk;
	
	/*
	 * Allocate the table, if needed.
	 */
	if (!Sgidio) {
		Sgidio = (struct sgidio *)calloc(1, c * sizeof(struct sgidio));
		if (!Sgidio) {
			fprintf(stderr, "No memory for sgidio table (%d)\n",
				c);
			perrexit("");
		}
	}

	for (i = 0, ds = Ndiosetup; (i < c) && ds; i++, ds = ds->next) {
		strncpy(SGIDIO_DNAME(Sgidio[i]), ds->diskname, SDIO_NAMESIZE);
		SGIDIO_DNAME(Sgidio[i])[SDIO_NAMESIZE-1] = '\0';
		dkiotime_neoget(ds->devicename, &(Sgidio[i].sdio_iotim));
#ifdef SA_DEBUG
		sadc_iotime_print(&Sgidio[i]);
#endif
	}

	return;
}

/*
 * This routine sets up the initial dio data structures
 */
/*ARGSUSED*/
void
sgi_dio_setup(int count)
{
	/*
	 * Allocate space for the first NDEVS elements.
	 */
	AllocdNdiosetup = (struct disk_iostat_info *)
				calloc(1, NDEVS * sizeof(struct disk_iostat_info));
	if (!AllocdNdiosetup) {
		fprintf(stderr, "No memory for disk_iostat_info table (%d)\n",
			ndisk);
		perrexit("");
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
struct disk_iostat_info *
sgi_dio_alloc()
{
	static int count;
	struct disk_iostat_info *ds;

#ifdef SA_DEBUG
{
	static	int call_num = 0;
	printf("sgi_dio_alloc called %dth time\n",++call_num);
}
#endif /* SA_DEBUG */
	/*
	 * Check to see if we're past the end of allocated elements.
	 */
	if (NdiosetupIndex >= NDEVS) {
		/*
		 * We need to allocate a new chunk of elements.
		 */
		count++;
		sgi_dio_setup(count);	
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
	 * Increment the # disks counter.
	 */
	ndisk++;

	return ds;	
}

void
write_rec(char *id, char *buf, int recsize, int nrec, char *errmessage)
{
	struct rec_header header;
	int bufsize = recsize * nrec;
	
	strncpy(header.magic, SAR_MAGIC, SAR_MAGIC_LEN);
	strncpy(header.id, id, SAR_MAGIC_LEN);
	header.recsize = recsize;
	header.nrec = nrec;
	if (write(fp, &header, sizeof(header)) != sizeof(header)) {
		perrexit("Can't write rec header");
	}
	if (write(fp, buf, bufsize) != bufsize) {
		perrexit(errmessage);
	}
}

void
kread(char *sym, void **buf, int len, int required)
{
	sgt_cookie_t ck;
	sgt_info_t info;
	int i;
	int flags;

	for (i = 0; i < num_knames; i++) {
		if (!strcmp(sym, knames[i].name)) {
			break;
		}
	}
	if (i == num_knames) {
		fprintf(stderr, "kread: %s is unknown\n", sym);
		perrexit("");
	}
	
	if (knames[i].count < 0) {

		/* Find out how many of these exist */

		SGT_COOKIE_INIT(&ck);
		SGT_COOKIE_SET_KSYM(&ck, sym);
		if (sysget(SGT_KSYM, (char *)&info, sizeof(info), SGT_INFO,
				 &ck) < 0) {
			if (!required && errno == ENOENT) {

				/* sym not configured so skip it */

				knames[i].count = 0;
				return;
			}
			fprintf(stderr, "sgt_sread: SGT_INFO of %s failed\n",
				sym);
			perrexit("sysget");
		}
		knames[i].count = info.si_num;
		*buf = (void *)calloc(len, info.si_num);
		if (!*buf) {
			fprintf(stderr, "kread: can't alloc space for %s\n",
				sym);
			perrexit("");
		}
	}
	else if (!knames[i].count) {

		/* Not configured */

		return;
	}
			
	flags = SGT_READ;
	SGT_COOKIE_INIT(&ck);
	SGT_COOKIE_SET_KSYM(&ck, sym);
	if (sysget(SGT_KSYM, *buf, len * knames[i].count, flags, &ck) < 0) {
		fprintf(stderr, "kread: SGT_READ for %s failed\n", sym);
		perrexit("sysget");
	}
}

void
sgi_sys_setup(void)
{
	sgt_cookie_t ck;
	sgt_info_t info;

	SGT_COOKIE_INIT(&ck);
	if (sysget(SGT_SINFO, (char *)&info, sizeof(info), SGT_INFO, &ck) < 0) {
		perrexit("SGT_SINFO failed");
	}
	nsites = info.si_num;
	d = (struct sa *)calloc(sizeof(struct sa), nsites);
	if (!d) {
		perrexit("Can't alloc space for sa");
	}

	/* Allocate space for structures that are cpu related. We make
	 * a special effort for sysinfo to use the size returned by
	 * the kernel instead of the one we are compiled with.
	 */

	cstat = (sgt_stat_t *)calloc(sizeof(sgt_stat_t), nsites);
	if (!cstat) {
		perrexit("Can't alloc space for cstat");
	}
	if (nsites > 1) {
		SGT_COOKIE_INIT(&ck);
		if (sysget(SGT_SINFO, (char *)cstat,
				 (int)sizeof(sgt_stat_t) * nsites,
				 SGT_STAT | SGT_CPUS, &ck) < 0) {
			perrexit("SGT_STAT failed");
		}
	}
	else {
		cstat[0].info.si_num = (int)sysmp(MP_NPROCS);
	}

	SGT_COOKIE_INIT(&ck);
	if (nsites > 1) {
		SGT_COOKIE_SET_CELL(&ck, SC_MYCELL);
	}
	if (sysget(SGT_SINFO, (char *)&info, sizeof(info), SGT_INFO, &ck) < 0) {
		perrexit("SGT_SINFO failed");
	}
	sysinfo_sz = info.si_size;

	si = (struct sysinfo *)calloc(sysinfo_sz, nsites);
	if (!si) {
		perrexit("Can't alloc space for sysinfo");
	}
	mi = (struct minfo *)calloc(sizeof(struct minfo), nsites);
	if (!mi) {
		perrexit("Can't alloc space for minfo");
	}
	di = (struct dinfo *)calloc(sizeof(struct dinfo), nsites);
	if (!di) {
		perrexit("Can't alloc space for dinfo");
	}
	err = (struct syserr *)calloc(sizeof(struct syserr), nsites);
	if (!err) {
		perrexit("Can't alloc space for err");
	}
	ri = (struct rminfo *)calloc(sizeof(struct rminfo), nsites);
	if (!ri) {
		perrexit("Can't alloc space for rminfo");
	}
}
