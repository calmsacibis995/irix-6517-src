/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)sa:sar.c	1.25" */
#ident	"$Revision: 1.71 $"
/*	sar.c 1.25 of 11/27/85	*/
/*
	sar.c - It generates a report either
		from an input data file or
		by invoking sadc to read system activity counters 
		at the specified intervals.
	usage: sar [-ubdycwaqvmprtgIUMR] [-o file] t [n]    or
	       sar [-ubdycwaqvmprtgIUMR][-s hh:mm][-e hh:mm][-i ss][-f file]
	       sar [-C cellid ]
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/param.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <signal.h>
#include <wait.h>
#include <fcntl.h>
#include <sys/flock.h>
#include <tzfile.h>
#include <sys/stat.h>
#include <sys/sysmp.h>

#include "sa.h"

struct sa n_sa,o_sa,a_sa,b_sa;
struct sysinfo n_si, o_si, a_si;
struct minfo n_mi, o_mi, a_mi;
struct dinfo n_di, o_di, a_di;
struct rminfo n_ri, o_ri, a_ri;
struct gfxinfo n_gi, o_gi, a_gi;
struct sysinfo_cpu *n_cst, *o_cst, *a_cst;
struct sgidio *Nsgidio, *Osgidio, *Asgidio;
size_t	Sgidiosz;
int	dskcnt, odskcnt;
int	no_pcpu_data;
struct infomap infomap;
struct tm *curt,args,arge;
struct utsname uname_data;
int	unix_restart;
off_t	rec_start;
int	tmp_bufsize;
char	*tmp_buf;
int	sflg, eflg, iflg, oflg, fflg, Cflg, Tflg, Fflg;
float	Isyscall, Isysread, Isyswrite, Isysexec, Ireadch, Iwritech;
float	Osyscall, Osysread, Osyswrite, Osysexec, Oreadch, Owritech;
float 	Lsyscall, Lsysread, Lsyswrite, Lsysexec, Lreadch, Lwritech;
int	realtime;
int	passno;
int	t=0;
int	n=0;
int	rcnt=0;
int	recno=0;
double 	magic = 4.294967296e9;
int	j,i;
int	no_tab_flg;
char	options[30],fopt[30];
char	cc;
double	tdiff;
double	tot_tdiff;
int	*cst_tdiff;
int	*cst_twait;
float	strttime, etime, isec;
int	fin, fout, childid;
int	pipedes[2];
char	arg1[10], arg2[10];

int	strdmp();
void	sgi_cst_setup(void), display_totals(void);
void	resync(void);
size_t	sar_read(char *, size_t);
void	read_rec (char *, struct rec_header *, int, int, char *);
int	read_data(char *, void *, int, int *, int);
void	write_rec(char *, void *, int, int, char *);
void	write_log_recs(void);
void	read_infomap(void);

int	badsa(void);
int	baddsk(void);
void	tsttab(void);

int	read_header(char *id, char *id2, struct rec_header *header, int cell);
int	read_cpu_data(void);
int	read_disk_data(void);

void prpass(void);
void prttim(void);
void prthdg(void);
void prtopt(void);
void prtavg(void);
void pillarg(void);
void sgidpr(int);
void sgidapr(int);
void pmsgexit(const char *);


off_t	lpos, log_start;
int	nsites, cellid;
int	sar_debug;
extern  int optind;
extern  char *optarg;

#define BUFSIZE	4096
#define KB	1000
#define SINGLE_CELL(id) 	(nsites > 1 && id >= 0 && Cflg)  
#define SUM_CELLS(id)		(nsites > 1 && id == 0 && !Cflg)

#define SAR_DEBUG(s)						\
{								\
	if (sar_debug) {					\
		printf s;						\
	}							\
}

/*
 * Unfortunately the struct sysinfo fields "cpu" and "wait" are declared as
 * "time_t" which is a signed integer.  This means that those fields wrap to
 * negative numbers when a system has been up for a long time.  This in turn
 * causes all sorts of strange ``negative time'' output that no one likes.  As
 * a result we declare a macro here which casts the difference between two
 * time_t values into an unsigned integer of the same size.  This works fine
 * even when the fields or a sum of them wrap around 0 because of the basic
 * math properties.  There are a few cases where it's okay not to do this
 * casting -- when the result is simply going to be tossed into an integral
 * type of the same size for instance -- but generally its just safer to
 * always use this macro.
 */
#define TUNSIGNED	unsigned
#define TDIFF(x, y)	((TUNSIGNED)(x) - (TUNSIGNED)(y))

int
main (int argc, char *const argv[])
{
	char    flnm[50], ofile[50];
	char	ccc;
	time_t  temp;
	int	jj=0;
	char	*cp;
	float	convtm();
	char 	*sadc_path = "/usr/lib/sa/sadc";

/*      process options with arguments and pack options 
	without arguments  */
	while ((i= getopt(argc,argv,
			"uybdvcwaqmprtghDFIAUMTRo:s:e:i:f:C:")) != EOF)
		switch(ccc = i){
		case 'o':
			oflg++;
			sprintf(ofile,"%s",optarg);
			break;
		case 's':
			if (sscanf(optarg,"%d:%d:%d",
			&args.tm_hour, &args.tm_min, &args.tm_sec) < 1)
				pillarg();
			else {
				sflg++,
				strttime = args.tm_hour*3600.0 +
					args.tm_min*60.0 +
					args.tm_sec;
			}
			break;
		case 'e':
			if(sscanf(optarg,"%d:%d:%d",
			&arge.tm_hour, &arge.tm_min, &arge.tm_sec) < 1)
				pillarg();
			else {
				eflg++;
				etime = arge.tm_hour*3600.0 +
					arge.tm_min*60.0 +
					arge.tm_sec;
			}
			break;
		case 'i':
			if(sscanf(optarg,"%f",&isec) < 1)
				pillarg();
			else{
			if (isec > 0.0)
				iflg++;
			}
			break;
		case 'f':
			fflg++;
			sprintf(flnm,"%s",optarg);
			break;
		case 'C':
			Cflg++;
			cellid = atoi(optarg);
			break;
		case 'T':
			Tflg++;
			break;
		case 'F':
			Fflg++;
			break;
		case '?':
			fprintf(stderr,"usage: sar [-ubdycwaqvmprtghDFIAUMTR][-o file] t [n]\n");
			fprintf(stderr,"       sar [-ubdycwaqvmprtghDFIAUMTR][-s hh:mm][-e hh:mm][-i ss][-f file]\n");
			fprintf(stderr,"       sar [-C cellid] ... \n");
			exit(2);
			break;
		default:
			strncat (options,&ccc,1);
			break;
		}

	/*   Are starting and ending times consistent?  */
	if ((sflg) && (eflg) && (etime <= strttime))
		pmsgexit("etime <= strttime");

	/*   Determine if t and n arguments are given,
	 *   and whether to run in real time or from a file
	 */
	switch(argc - optind) {
	case 0:		/*   Get input data from file   */
		if(fflg == 0) {
			temp = time((time_t *) 0);
			curt = localtime(&temp);
			sprintf(flnm, "/var/adm/sa/sa%.2d", curt->tm_mday);
		}
		if((fin = open(flnm, O_RDONLY)) == -1) {
			fprintf(stderr, "sar: Can't open %s\n", flnm);
			exit(1);
		}
		lseek(fin, 0L, SEEK_SET);
		break;
	case 1:		/*   Real time data; one cycle   */
		realtime++;
		t = atoi(argv[optind]);
		n = 2;
		break;
	case 2:		/*   Real time data; specified cycles   */
	default:
		realtime++;
		t = atoi(argv[optind]);
		n = 1 + atoi(argv[optind+1]);
		break;
	}

	cp = getenv("SAR_DEBUG");
	if (cp) {
		sar_debug = atoi(cp);
	}

	cp = getenv("SAR_SADC_PATH");
	if (cp) {
		sadc_path = cp;
	}

	/*	"u" is default option to display cpu utilization   */
	if(strlen(options) == 0)
		strcpy(options, "u");
	/*    'A' means all data options   */
	if(strchr(options, 'A') != NULL)
		strcpy(options, "udqbwcayvmprtghUMR");

	if(realtime) {
	/*	Get input data from sadc via pipe   */
		if((t <= 0) || (n < 2))
			pmsgexit("args t & n <= 0");
		sprintf(arg1,"%d", t);
		sprintf(arg2,"%d", n);
		if (pipe(pipedes) == -1)
			perrexit("can't create pipe");
#ifdef SA_DEBUG
		printf("sar : calling sadc arg1=%s,arg2=%s\n",arg1,arg2);
#endif
		if ((childid = fork()) == 0){	/*  child:   */
			close(1);       /*  shift pipedes[write] to stdout  */
			dup(pipedes[1]);
			close(pipedes[0]);	/* close unused output */
			if (execlp (sadc_path, sadc_path, arg1, arg2, 0) == -1)
				perrexit(sadc_path);
		}		/*   parent:   */
		fin = pipedes[0];
		close(pipedes[1]);	/*   Close unused output   */
	}

	if(oflg) {
		if(strcmp(ofile, flnm) == 0)
			pmsgexit("ofile same as ffile");
		fout = creat(ofile, 0644);
	}

	/*
	 * read the header record and compute record size
	 */

	read_infomap();

	if(oflg) {
		write_rec(INFO_MAGIC, (char *)&infomap, sizeof(infomap),
			1, "Error writing infomap");
#ifdef NOTYET
		write_rec(SCNAME_MAGIC, (char *)syscallname, MAXSCNAME,
			MAXSYSENT, "Error writing syscallname");
		write_rec(LIDNAME_MAGIC, (char *)lid_name, MAXLIDNAME,
			MAXLID, "Error writing lid_name");
#endif
	}

	if(realtime) {
		/*   Make single pass, processing all options   */
		strcpy(fopt, options);
		passno++;
		prpass();
		kill(childid, 2);
		wait((int *) 0);
	}
	else if (Tflg) {
		/*   Make single pass, processing all options   */
		strcpy(fopt, options);
		passno++;
		prpass();
	}
	else {
		/*   Make multiple passes, one for each option   */
		while(strlen(strncpy(fopt,&options[jj++],1)) > 0) {
			lseek(fin, log_start, SEEK_SET);
			passno++;
			prpass();
		}
	}
	exit(0);
	/*NOTREACHED*/
}

/*****************************************************/

/*	Read records from input, classify, and decide on printing   */
void
prpass(void)
{
	int	lines=0;
	float tnext=0;
	float trec;
	int kk, rval;
	char *cp;
	struct revname *revp;
	struct  rec_header header;
	int reconfig = 0;

	uname_data = infomap.uname;  /* Reset so restart messages are correct */

	rcnt = 0;
	recno = 0;

	if(sflg)	tnext = strttime;

	while((rval = read_header(SA_MAGIC, NULL, &header, cellid))) {
		if (rval < 0) {

			/* Had to re-sync, try again */

			continue;
		}
		unix_restart = 0;

		if (nsites && header.nrec != nsites) {

			/* Number of cells has been reconfigured */

			reconfig++;
		}
		nsites = header.nrec;
		read_rec((char *)&n_sa, &header, sizeof(struct sa), cellid,
			"Can't read sa data");

		/*
                 * Check for valid data in datafile
                 */
                if (badsa()) {
                        printf("Incompatible sa data found, resyncing\n");
                        continue;
                }

		if (n_sa.apstate == CPUDUMMYTIME) {

			/*
                         * This dummy record signifies system restart. New
                         * initial values of counters follow in next sa record
                         */

                        unix_restart++;
		}
		else {

			/*
                         * These records follow the normal sa record.
                         */

                        rec_start = lseek(fin, (off_t)0, SEEK_CUR);

			if (read_data(SINFO_MAGIC, &n_si, sizeof(n_si), 0,
					cellid) < 0) {
				continue;
			}
			if (read_data(MINFO_MAGIC, &n_mi, sizeof(n_mi), 0,
					cellid) < 0) {
				continue;
			}
			if (read_data(DINFO_MAGIC, &n_di, sizeof(n_di), 0,
					cellid) < 0) {
				continue;
			}
			if (read_data(GFX_MAGIC, &n_gi, sizeof(n_gi), 0,
					cellid) < 0) {
				continue;
			}
			if (read_data(RMINFO_MAGIC, &n_ri, sizeof(n_ri), 0,
					cellid) < 0) {
				continue;
			}
			if (read_cpu_data() < 0) {
				continue;
			}
			if (read_disk_data() < 0) {
				continue;
			}
		}

		curt = localtime(&n_sa.ts);
		trec =    curt->tm_hour * 3600.0
			+ curt->tm_min * 60.0
			+ curt->tm_sec;
		if((recno == 0) && (trec < strttime))
			continue;
		if((eflg) && (trec > etime))
			break;
		if((oflg) && (passno == 1)) {
			write_log_recs();
		}
		if(recno == 0) {
			if(passno == 1) {
				printf("\n%s %s %s %s %s    %.2d/%.2d/%.2d\n",
					infomap.uname.sysname,
					infomap.uname.nodename,
					infomap.uname.release,
					infomap.uname.version,
					infomap.uname.machine,
					curt->tm_mon + 1,
					curt->tm_mday,
					curt->tm_year % 100);
				if (nsites > 1) {
					if (Cflg) 
						printf("\nCells: %d Cell# %d\n",
							nsites, cellid);
					else 
					  	printf("\nCells: %d\n",
							nsites);
				}
			}
			if (!Tflg) {
				prthdg();
			}
			recno = 1;
			if((iflg) && (tnext == 0))
				tnext = trec;
		}



		if (unix_restart) {

			/*  This dummy record signifies system restart
			 *  New initial values of counters follow in next record
			 */

			prttim();
			revp = (struct revname *)&n_sa;
			if (strncmp(uname_data.release, revp->release, 9) ||
			    strncmp(uname_data.version, revp->version, 9)) {

				/* Display new unix version since it changed */

				strcpy(uname_data.release, revp->release);
				strcpy(uname_data.version, revp->version);
				printf("\tunix restarts %s %s\n",
					uname_data.release, uname_data.version);
			}
			else {
				printf("\tunix restarts\n");
			}
			recno = 1;
			continue;
		}
		if((iflg) && (trec < tnext))
			continue;
		if(recno++ > 1) {
			int ii;

			/* Check for cell reconfig */

			if (reconfig) {
				prttim();
				printf("\tNumber of cells changed to %d,"
					" continuing...\n", nsites);
				reconfig = 0;
				goto ageit;
			}

			tdiff = TDIFF(n_si.cpu[0], o_si.cpu[0])
			  + TDIFF(n_si.cpu[1], o_si.cpu[1])
			  + TDIFF(n_si.cpu[2], o_si.cpu[2])
			  + TDIFF(n_si.cpu[3], o_si.cpu[3])
			  + TDIFF(n_si.cpu[4], o_si.cpu[4])
			  + TDIFF(n_si.cpu[5], o_si.cpu[5]);

                        for (ii = 0; ii < (n_sa.apstate ? n_sa.apstate : 1);
						 ii++) {

                                /* Save per-cpu tdiff/twait */

                                cst_tdiff[ii] = 0;
                                for (kk = 0; kk < 6; kk++) {
                                        cst_tdiff[ii] +=
					    TDIFF(n_cst[ii].cpu[kk],
						  o_cst[ii].cpu[kk]);
                                }
                                cst_twait[ii] = 0;
                                for (kk = 0; kk < 5; kk++) {
                                        cst_twait[ii] +=
					    TDIFF(n_cst[ii].wait[kk],
						  o_cst[ii].wait[kk]);
                                }
                        }

			/* divide by # active processors - we get already
			 * summed times from sadc
			 */
			tot_tdiff = tdiff;
			tdiff /= (n_sa.apstate ? n_sa.apstate : 1);
			if(tdiff <= 0) 
				continue;
			
			if (!Tflg) {
				prtopt();	/*  Print a line of data  */
			}
			rcnt++;
			lines++;
		}
ageit:
		if (!Tflg || recno == 2) {
			o_sa = n_sa;		/*  Age the data	*/
			o_si = n_si;
			o_mi = n_mi;
			o_di = n_di;
			o_gi = n_gi;
			o_ri = n_ri;
			cp = (char *)Osgidio;
			Osgidio = Nsgidio;
			Nsgidio = (struct sgidio *)cp;
			cp = (char *)o_cst;
			o_cst = n_cst;
			n_cst = (struct sysinfo_cpu *)cp;

			if(isec > 0)
				while(tnext <= trec)
					tnext += isec;
		}
	}

	if (Tflg) {
		display_totals();
		return;
	}

	if(lines > 1)
		prtavg();
	n_sa = o_sa = a_sa = b_sa;           /*  Zero out the accumulators   */
	bzero(&a_si, sizeof(a_si));
	bzero(&a_mi, sizeof(a_mi));
	bzero(&a_di, sizeof(a_di));
	bzero(&a_gi, sizeof(a_gi));
	bzero(&a_ri, sizeof(a_ri));
	bzero(Asgidio, Sgidiosz);
	bzero(a_cst, n_sa.apstate * sizeof(struct sysinfo_cpu));
}

/************************************************************/

/*      print time label routine	*/
void
prttim(void)
{
	curt = localtime(&n_sa.ts);
	printf("%.2d:%.2d:%.2d",
		curt->tm_hour,
		curt->tm_min,
		curt->tm_sec);
	no_tab_flg = 1;
}

/***********************************************************/

/*      test if 8-spaces to be added routine    */
void
tsttab(void)
{
	if (no_tab_flg == 0) 
		printf("        ");
	else
		no_tab_flg = 0;
}

/************************************************************/

/*      print report heading routine    */
void
prthdg(void)
{
	int	jj=0;
	char	ccc;

	printf("\n");
	prttim();
	while((ccc = fopt[jj++]) != NULL)
	switch(ccc){
	case 'u':
		tsttab();
		printf(" %5s %5s %5s %5s %5s %5s %5s %5s %5s %5s %5s\n",
			"%usr",
			"%sys",
			"%intr",
			"%wio",
			"%idle",
			"%sbrk",
			"%wfs",
			"%wswp",
			"%wphy",
			"%wgsw",
			"%wfif"
			);
		break;
	case 'U':
		tsttab();
		printf("  %5s %5s %5s %5s%5s %5s %5s %5s %5s %5s %5s %5s\n",
			"CPU",
			"%usr",
			"%sys",
			"%intr",
			"%wio",
			"%idle",
			"%sbrk",
			"%wfs",
			"%wswp",
			"%wphy",
			"%wgsw",
			"%wfif"
			);
		break;
	case 'y':
		tsttab();
		printf(" %7s %7s %7s %7s %7s %7s\n",
			"rawch/s",
			"canch/s",
			"outch/s",
			"rcvin/s",
			"xmtin/s",
			"mdmin/s");
		break;
	case 'b':
		tsttab();
		printf(" %7s %7s %6s %7s %7s %7s %6s %7s %7s\n",
			"bread/s",
			"lread/s",
			"%rcach",
			"bwrit/s",
			"lwrit/s",
			"wcncl/s",
			"%wcach",
			"pread/s",
			"pwrit/s");
		break;
	case 'D':
	case 'd':
		tsttab();
		if (Fflg)
			printf(" %5s %6s %6s %7s %6s %7s %7s %7s %s\n",
				"%busy",
				"avque",
				"r+w/s",
				"blks/s",
				"w/s",
				"wblks/s",
				"avwait",
				"avserv",
				"device");
		else
			printf(" %10s %5s %6s %6s %7s %6s %7s %7s %7s\n",
				"device",
				"%busy",
				"avque",
				"r+w/s",
				"blks/s",
				"w/s",
				"wblks/s",
				"avwait",
				"avserv");
		break;
	case 'v':
		tsttab();
		printf(" %12s %12s %12s %12s\n",
			"proc-sz   ov",
			"inod-sz   ov",
			"file-sz   ov",
			"lock-sz   ov");
		break;
	case 'c':
		tsttab();
		printf(" %7s %7s %7s %7s %7s %7s %7s\n",
			"scall/s",
			"sread/s",
			"swrit/s",
			"fork/s",
			"exec/s",
			"rchar/s",
			"wchar/s");
		break;
	case 'w':
		tsttab();
		printf(" %7s %7s %7s %7s %7s %7s %7s\n",
			"swpin/s",
			"bswin/s",
			"swpot/s",
			"bswot/s",
			"pswot/s",
			"pswch/s",
			"kswch/s");
		break;
	case 'a':
		tsttab();
		printf(" %7s %7s %7s\n",
			"iget/s",
			"namei/s",
			"dirbk/s");
		break;
	case 'q':
		tsttab();
		printf(" %7s %7s %7s %7s %7s %7s\n",
			"runq-sz",
			"%runocc",
			"swpq-sz",
			"%swpocc",
			"wioq-sz",
			"%wioocc");
		break;
	case 'm':
		tsttab();
		printf(" %7s %7s\n",
			"msg/s",
			"sema/s");
		break;
	case 'r':
		tsttab();
		printf(" %7s %7s %7s\n",
			"freemem",
			"freeswp",
			"vswap");
		break;
	case 'R':
		tsttab();
		printf(" %7s %7s %7s %7s %7s %7s %7s %7s\n",
			"physmem",
			"kernel",
			"user",
			"fsctl",
			"fsdelwr",
			"fsdata",
			"freedat",
			"empty");
		break;
	case 'h':
		tsttab();
		printf("  %7s %7s %7s %7s %7s\n",
			"heapmem",
			"overhd",
			"unused",
			"alloc/s",
			"free/s");
		break;
	case 'S':
		tsttab();
		printf("%11s %9s %9s %9s %9s\n",
			"serv/lo-hi",
			"request",
			"request",
			"server",
			"server");
		tsttab();
		printf("%6s %d %1s %d %9s %9s %9s %9s\n",
			"",
			n_sa.minserve,
			"-",
			n_sa.maxserve,
			"%busy",
			"avg lgth",
			"%avail",
			"avg avail");
		break;
	case 't':
		tsttab();
		printf(" %7s %7s %7s %7s %7s %7s %7s %7s %7s\n",
			"tflt/s",
			"rflt/s",
			"vmwrp/s",
			"sync/s",
			"flush/s",
			"idwrp/s",
			"idget/s",
			"idprg/s",
			"vmprg/s");
		break;

	case 'p':
		tsttab();
		printf(" %7s %7s %7s %7s %7s %7s %7s %7s %6s\n",
			"vflt/s",
			"dfill/s",
			"cache/s",
			"pgswp/s",
			"pgfil/s",
			"pflt/s",
			"cpyw/s",
			"steal/s",
			"rclm/s");
		break;
	case 'g':
		tsttab();
		printf(" %7s %7s %7s %7s %7s\n",
			"gcxsw/s",
			"ginpt/s",
			"gintr/s",
			"fintr/s",
			"swpbf/s");
		break;
	case 'I':
		tsttab();
		printf(" %9s %9s\n",
			"intr/s",
			"vmeintr/s");
		break;
	case 'M':
		tsttab();
		printf(" %7s %7s\n",
			"Msnt/s",
			"Mrcv/s");
		break;
	}
	if(jj > 2)	printf("\n");
	no_tab_flg = 1;
}

/**********************************************************/

/*      print options routine   */
void
prtopt(void)
{
	int ii, kk;
	int jj=0;
	int twait;
	int tot_twait;
	char	ccc;
	int np = 1;			/* # active processors */
	np = (n_sa.apstate ? n_sa.apstate : 1); /* # active processors */

	prttim();
	for(ii=0;ii<6;ii++) {
		a_si.cpu[ii] += TDIFF(n_si.cpu[ii], o_si.cpu[ii]);
		for (kk = 0; kk < np; kk++) {
                        a_cst[kk].cpu[ii] +=
			    TDIFF(n_cst[kk].cpu[ii], o_cst[kk].cpu[ii]);
                }
	}
	twait = 0;
	for(ii=0;ii<5;ii++) {
		a_si.wait[ii] += TDIFF(n_si.wait[ii], o_si.wait[ii]);
		twait += TDIFF(n_si.wait[ii], o_si.wait[ii]);
                for (kk = 0; kk < np; kk++) {
                        a_cst[kk].wait[ii] +=
			    TDIFF(n_cst[kk].wait[ii], o_cst[kk].wait[ii]);
                }
	}
	tot_twait = twait;
	twait /= np;

	while((ccc = fopt[jj++]) != NULL)
	switch(ccc){
	case 'u':
		tsttab();
		printf(" %5.0f %5.0f %5.0f %5.0f %5.0f %5.0f %5.0f %5.0f %5.0f %5.0f %5.0f\n",
		(double)TDIFF(n_si.cpu[1], o_si.cpu[1])/tot_tdiff * 100.0,
		(double)TDIFF(n_si.cpu[2], o_si.cpu[2])/tot_tdiff * 100.0,
		(double)TDIFF(n_si.cpu[5], o_si.cpu[5])/tot_tdiff * 100.0,
		(double)TDIFF(n_si.cpu[3], o_si.cpu[3])/tot_tdiff * 100.0,
		(double)TDIFF(n_si.cpu[0], o_si.cpu[0])/tot_tdiff * 100.0,
		(double)TDIFF(n_si.cpu[4], o_si.cpu[4])/tot_tdiff * 100.0,
		(twait == 0) ? (0.0) :
		    (double)TDIFF(n_si.wait[0], o_si.wait[0])/tot_twait * 100.0,
		(twait == 0) ? (0.0) :
		    (double)TDIFF(n_si.wait[1], o_si.wait[1])/tot_twait * 100.0,
		(twait == 0) ? (0.0) :
		    (double)TDIFF(n_si.wait[2], o_si.wait[2])/tot_twait * 100.0,
		(twait == 0) ? (0.0) :
		    (double)TDIFF(n_si.wait[3], o_si.wait[3])/tot_twait * 100.0,
		(twait == 0) ? (0.0) :
		    (double)TDIFF(n_si.wait[4], o_si.wait[4])/tot_twait * 100.0
		);
		break;
	case 'U':
		tsttab();
		if (no_pcpu_data) {
			printf(" no per-cpu data, continuing...\n");
			break;
		}
		for (ii = 0; ii < n_sa.apstate; ii++) {
			if (ii) printf("        ");
			printf(" %5d %5.0f %5.0f %5.0f %5.0f %5.0f %5.0f %5.0f %5.0f %5.0f %5.0f %5.0f\n", ii,
			(double)TDIFF(n_cst[ii].cpu[1], o_cst[ii].cpu[1]) / 
				cst_tdiff[ii] * 100.0,
			(double)TDIFF(n_cst[ii].cpu[2], o_cst[ii].cpu[2]) / 
				cst_tdiff[ii] * 100.0,
			(double)TDIFF(n_cst[ii].cpu[5], o_cst[ii].cpu[5]) /
				cst_tdiff[ii] * 100.0,
			(double)TDIFF(n_cst[ii].cpu[3], o_cst[ii].cpu[3]) / 
				cst_tdiff[ii] * 100.0,
			(double)TDIFF(n_cst[ii].cpu[0], o_cst[ii].cpu[0]) / 
				cst_tdiff[ii] * 100.0,
			(double)TDIFF(n_cst[ii].cpu[4], o_cst[ii].cpu[4]) / 
				cst_tdiff[ii] * 100.0,
			(cst_twait[ii] == 0) ? (0.0) :
			    (double)TDIFF(n_cst[ii].wait[0], o_cst[ii].wait[0]) /
				cst_twait[ii] * 100.0,
			(cst_twait[ii] == 0) ? (0.0) :
			    (double)TDIFF(n_cst[ii].wait[1], o_cst[ii].wait[1]) /
				cst_twait[ii] * 100.0,
			(cst_twait[ii] == 0) ? (0.0) :
			    (double)TDIFF(n_cst[ii].wait[2], o_cst[ii].wait[2]) /
				cst_twait[ii] * 100.0,
			(cst_twait[ii] == 0) ? (0.0) :
			    (double)TDIFF(n_cst[ii].wait[3], o_cst[ii].wait[3]) /
				cst_twait[ii] * 100.0,
			(cst_twait[ii] == 0) ? (0.0) :
			    (double)TDIFF(n_cst[ii].wait[4], o_cst[ii].wait[4]) /
				cst_twait[ii] * 100.0);
		}
		break;
	case 'y':
		tsttab();
		printf(" %7.0f %7.0f %7.0f %7.0f %7.0f %7.0f\n",
		(float)(n_si.rawch - o_si.rawch)/tdiff * HZ,
		(float)(n_si.canch - o_si.canch)/tdiff * HZ,
		(float)(n_si.outch - o_si.outch)/tdiff * HZ,
		(float)(n_si.rcvint - o_si.rcvint)/tdiff * HZ,
		(float)(n_si.xmtint - o_si.xmtint)/tdiff * HZ,
		(float)(n_si.mdmint - o_si.mdmint)/tdiff * HZ);

		a_si.rawch += n_si.rawch - o_si.rawch;
		a_si.canch += n_si.canch - o_si.canch;
		a_si.outch += n_si.outch - o_si.outch;
		a_si.rcvint += n_si.rcvint - o_si.rcvint;
		a_si.xmtint += n_si.xmtint - o_si.xmtint;
		a_si.mdmint += n_si.mdmint - o_si.mdmint;
		break;
	case 'b':
		{
		float bread = n_si.bread - o_si.bread;
		float lread = n_si.lread - o_si.lread;
		float bwrit = n_si.bwrite - o_si.bwrite;
		float lwrit = n_si.lwrite - o_si.lwrite;

		tsttab();
		printf(" %7.0f %7.0f", bread / tdiff * HZ, lread / tdiff * HZ);
		if (lread == 0.0)
			printf("   idle");
		else if (lread - bread < 0.0)
			printf(" %6.0f", 0.0);
		else
			printf(" %6.0f", (lread - bread) / lread * 100.0);

		printf(" %7.0f %7.0f", bwrit / tdiff * HZ, lwrit / tdiff * HZ);
		printf(" %7.0f",
			(float)(n_si.wcancel - o_si.wcancel)/tdiff * HZ);
		if (lwrit == 0.0)
			printf("   idle");
		else if (lwrit - bwrit < 0.0)
			printf(" %6.0f", 0.0);
		else
			printf(" %6.0f", (lwrit - bwrit) / lwrit * 100.0);

		printf(" %7.0f %7.0f\n",
			(float)(n_si.phread - o_si.phread)/tdiff * HZ,
			(float)(n_si.phwrite - o_si.phwrite)/tdiff * HZ);

		a_si.bread += n_si.bread - o_si.bread;
		a_si.bwrite += n_si.bwrite - o_si.bwrite;
		a_si.lread += n_si.lread - o_si.lread;
		a_si.lwrite += n_si.lwrite - o_si.lwrite;
		a_si.wcancel += n_si.wcancel - o_si.wcancel;
		a_si.phread += n_si.phread - o_si.phread;
		a_si.phwrite += n_si.phwrite - o_si.phwrite;
		break;
		}
	case 'D':
	case 'd':
		sgidpr(ccc == 'D');
		break;
	case 'v':
		tsttab();
		printf(" %4d/%-4d%3ld %4d/%-4d%3ld %4d/%-4d%3ld %4d/%-4d%3ld\n",
			n_sa.szproc, n_sa.mszproc, (n_sa.procovf - o_sa.procovf),
			n_sa.szinode, n_sa.mszinode, (n_sa.inodeovf - o_sa.inodeovf),
			n_sa.szfile, n_sa.mszfile, (n_sa.fileovf - o_sa.fileovf),
			n_sa.szlckr, n_sa.mszlckr, (n_sa.lckrovf - o_sa.lckrovf));
		break;
	case 'c':
		tsttab();
		printf(" %7.0f %7.0f %7.0f %7.2f %7.2f %7.0f %7.0f\n",
			(float)(n_si.syscall - o_si.syscall)/tdiff *HZ,
			(float)(n_si.sysread - o_si.sysread)/tdiff *HZ,
			(float)(n_si.syswrite - o_si.syswrite)/tdiff *HZ,
			(float)(n_si.sysfork - o_si.sysfork)/tdiff *HZ,
			(float)(n_si.sysexec - o_si.sysexec)/tdiff *HZ,
			(float)(n_si.readch - o_si.readch)/tdiff * HZ,
			(float)(n_si.writech - o_si.writech)/tdiff * HZ);

		a_si.syscall += n_si.syscall - o_si.syscall;
		a_si.sysread += n_si.sysread - o_si.sysread;
		a_si.syswrite += n_si.syswrite - o_si.syswrite;
		a_si.sysfork += n_si.sysfork - o_si.sysfork;
		a_si.sysexec += n_si.sysexec - o_si.sysexec;
		a_si.readch += n_si.readch - o_si.readch;
		a_si.writech += n_si.writech - o_si.writech;
		break;
	case 'w':
		tsttab();
		printf(" %7.2f %7.1f %7.2f %7.1f %7.2f %7.0f %7.0f\n",
			(float)(n_si.swapin - o_si.swapin)/tdiff * HZ,
			(float)(n_si.bswapin -o_si.bswapin)/tdiff * HZ,
			(float)(n_si.swapout - o_si.swapout)/tdiff * HZ,
			(float)(n_si.bswapout - o_si.bswapout)/tdiff * HZ,
			(float)(n_si.pswapout - o_si.pswapout)/tdiff * HZ,
			(float)(n_si.pswitch - o_si.pswitch)/tdiff * HZ,
			(float)(n_si.kswitch - o_si.kswitch)/tdiff * HZ);

		a_si.swapin += n_si.swapin - o_si.swapin;
		a_si.swapout += n_si.swapout - o_si.swapout;
		a_si.bswapin += n_si.bswapin - o_si.bswapin;
		a_si.bswapout += n_si.bswapout - o_si.bswapout;
		a_si.pswapout += n_si.pswapout - o_si.pswapout;
		a_si.pswitch += n_si.pswitch - o_si.pswitch;
		a_si.kswitch += n_si.kswitch - o_si.kswitch;
		break;
	case 'a':
		tsttab();
		printf(" %7.0f %7.0f %7.0f\n",
			(float)(n_si.iget - o_si.iget)/tdiff * HZ,
			(float)(n_si.namei - o_si.namei)/tdiff * HZ,
			(float)(n_si.dirblk - o_si.dirblk)/tdiff * HZ);

		a_si.iget += n_si.iget - o_si.iget;
		a_si.namei += n_si.namei - o_si.namei;
		a_si.dirblk += n_si.dirblk - o_si.dirblk;
		break;
	case 'S':
		tsttab();
		printf("%11.0f %9.1f %9.0f %9.1f %9.0f\n",
			(float)(n_di.nservers - o_di.nservers)/tdiff * HZ,
			(((n_di.rcv_occ - o_di.rcv_occ)/tdiff * HZ * 100.0) > 100) ? 100.0 :
				(float)(n_di.rcv_occ - o_di.rcv_occ)/tdiff * HZ * 100.0,
			(n_di.rcv_occ - o_di.rcv_occ <= 0) ? 0.0 :
				((n_di.rcv_que - o_di.rcv_que <= 0) ? 0.0 :
				(float)(n_di.rcv_que - o_di.rcv_que)/
					(float)(n_di.rcv_occ - o_di.rcv_occ)),
			(((n_di.srv_occ - o_di.srv_occ)/tdiff * HZ * 100.0) > 100) ? 100.0 :
				(float)(n_di.srv_occ - o_di.srv_occ)/tdiff * HZ * 100.0,
			(n_di.srv_occ - o_di.srv_occ == 0) ? 0.0 :
				(float)(n_di.srv_que - o_di.srv_que)/
					(float)(n_di.srv_occ - o_di.srv_occ));
 


		a_di.nservers += n_di.nservers - o_di.nservers;
		a_di.rcv_occ += n_di.rcv_occ - o_di.rcv_occ;
		a_di.rcv_que += n_di.rcv_que - o_di.rcv_que;
		a_di.srv_occ += n_di.srv_occ - o_di.srv_occ;
		a_di.srv_que += n_di.srv_que - o_di.srv_que;
		break;
	
	case 'q':
		tsttab();
		if ((n_si.runocc - o_si.runocc) == 0)
			printf(" %7s %7s", "  ", "  ");
		else {
			printf(" %7.1f %7.0f",
			(float)(n_si.runque -o_si.runque)/
				(float)(n_si.runocc - o_si.runocc),
			(float)(n_si.runocc -o_si.runocc)/tdiff *HZ *100.0);
			a_si.runque += n_si.runque - o_si.runque;
			a_si.runocc += n_si.runocc - o_si.runocc;
		}
		if ((n_si.swpocc - o_si.swpocc) == 0)
			printf(" %7s %7s","  ","  ");
		else {
			printf(" %7.1f %7.0f",
			(float)(n_si.swpque -o_si.swpque)/
				(float)(n_si.swpocc - o_si.swpocc),
			(float)(n_si.swpocc -o_si.swpocc)/tdiff *HZ *100.0);
			a_si.swpque += n_si.swpque - o_si.swpque;
			a_si.swpocc += n_si.swpocc - o_si.swpocc;

		}
		if ((n_si.wioocc - o_si.wioocc) == 0)
			printf(" %7s %7s\n", "  ", "  ");
		else {
			printf(" %7.1f %7.0f\n",
			(float)(n_si.wioque - o_si.wioque) /
				(float)(n_si.wioocc - o_si.wioocc),
			(float)(n_si.wioocc -o_si.wioocc)/tdiff *100.0);
			a_si.wioque += n_si.wioque - o_si.wioque;
			a_si.wioocc += n_si.wioocc - o_si.wioocc;
		}
		break;
	case 'm':
		tsttab();
		printf(" %7.2f %7.2f\n",
			(float)(n_si.msg - o_si.msg)/tdiff * HZ,
			(float)(n_si.sema - o_si.sema)/tdiff * HZ);

		a_si.msg += n_si.msg - o_si.msg;
		a_si.sema += n_si.sema - o_si.sema;
		break;

/* new freemem for 3b2 */


	case 'r':
 	{
		unsigned long k0, k1, x;

		tsttab();
		k1 = (n_mi.freemem[1] - o_mi.freemem[1]);
		if (n_mi.freemem[0] >= o_mi.freemem[0]) {
			k0 = n_mi.freemem[0] - o_mi.freemem[0]; 
		}
		else
		{ 	k0 = 1 + (~(o_mi.freemem[0] - n_mi.freemem[0]));
			k1--; 
		}
		printf(" %7.0f %7.0f %7.0f\n",
			((double)k0 + magic * (double)k1)/tdiff,
			(double)n_mi.freeswap,
			(double)n_ri.availsmem);

			x = a_mi.freemem[0];
			a_mi.freemem[0] += k0;
			a_mi.freemem[1] += k1;
			if ( x > a_mi.freemem[0])
				a_mi.freemem[1]++;
			a_mi.freeswap += n_mi.freeswap;
			a_ri.availsmem += n_ri.availsmem;
		break;
 	}

	case 'R':
 	{
		tsttab();
		printf(" %7.0f %7.0f %7.0f %7.0f %7.0f %7.0f %7.0f %7.0f\n",
			(float)n_ri.physmem,
			(float)(n_ri.physmem - (n_ri.availrmem + n_ri.bufmem)),
			(float)(n_ri.availrmem - (n_ri.freemem + 
				n_ri.chunkpages + n_ri.dpages)),
			(float)(n_ri.bufmem),
			(float)(n_ri.dchunkpages + n_ri.dpages),
			(float)(n_ri.chunkpages - n_ri.dchunkpages),
			(float)(n_ri.freemem - n_ri.emptymem),
			(float)(n_ri.emptymem));

		a_ri.physmem += n_ri.physmem;
		a_ri.availrmem += n_ri.availrmem;
		a_ri.freemem += n_ri.freemem;
		a_ri.chunkpages += n_ri.chunkpages;
		a_ri.dpages += n_ri.dpages;
		a_ri.bufmem += n_ri.bufmem;
		a_ri.dchunkpages += n_ri.dchunkpages;
		a_ri.emptymem += n_ri.emptymem;
		break;
	}

	case 'h':
 	{
		tsttab();
		printf("  %7.0f %7.0f %7.0f %7.0f %7.0f\n",
			(float)n_mi.heapmem,
			(float)n_mi.hovhd,
			(float)n_mi.hunused,
			(float)(n_mi.halloc - o_mi.halloc) / tdiff * HZ,
			(float)(n_mi.hfree - o_mi.hfree) / tdiff * HZ);

			a_mi.heapmem += n_mi.heapmem;
			a_mi.hovhd += n_mi.hovhd;
			a_mi.hunused += n_mi.hunused;
			a_mi.halloc += n_mi.halloc - o_mi.halloc;
			a_mi.hfree += n_mi.hfree - o_mi.hfree;
		break;
 	}

	case 't':
		tsttab();
		printf(" %7.2f %7.2f %7.2f %7.2f %7.2f %7.2f %7.2f %7.2f %7.2f\n",
			(float)(n_mi.tfault - o_mi.tfault) / tdiff * HZ,
			(float)(n_mi.rfault - o_mi.rfault) / tdiff * HZ,
			(float)(n_mi.tvirt - o_mi.tvirt) / tdiff * HZ,
			(float)(n_mi.tlbsync - o_mi.tlbsync) / tdiff * HZ,
			(float)(n_mi.tlbflush - o_mi.tlbflush) / tdiff * HZ,
			(float)(n_mi.twrap - o_mi.twrap) / tdiff * HZ,
			(float)(n_mi.tlbpids - o_mi.tlbpids) / tdiff * HZ,
			(float)(n_mi.tdirt - o_mi.tdirt) / tdiff * HZ,
			(float)(n_mi.tphys - o_mi.tphys) / tdiff * HZ);
		a_mi.tfault += n_mi.tfault - o_mi.tfault;
		a_mi.rfault += n_mi.rfault - o_mi.rfault;
		a_mi.tlbsync += n_mi.tlbsync - o_mi.tlbsync;
		a_mi.tlbflush += n_mi.tlbflush - o_mi.tlbflush;
		a_mi.tlbpids += n_mi.tlbpids - o_mi.tlbpids;
		a_mi.twrap += n_mi.twrap - o_mi.twrap;
		a_mi.tdirt += n_mi.tdirt - o_mi.tdirt;
		a_mi.tphys += n_mi.tphys - o_mi.tphys;
		a_mi.tvirt += n_mi.tvirt - o_mi.tvirt;
		break;

	case 'p':
		tsttab();
		printf(" %7.2f %7.2f %7.2f %7.2f %7.2f %7.2f %7.2f %7.2f %6.2f\n",
			(float)(n_mi.vfault - o_mi.vfault) / tdiff * HZ,
			(float)(n_mi.demand - o_mi.demand) / tdiff * HZ,
			(float)(n_mi.cache - o_mi.cache) / tdiff * HZ,
			(float)(n_mi.swap - o_mi.swap) / tdiff * HZ,
			(float)(n_mi.file - o_mi.file) / tdiff * HZ,
			(float)(n_mi.pfault - o_mi.pfault) / tdiff * HZ,
			(float)(n_mi.cw - o_mi.cw) / tdiff * HZ,
			(float)(n_mi.steal - o_mi.steal) / tdiff * HZ,
			(float)(n_mi.freedpgs - o_mi.freedpgs) / tdiff * HZ);
		a_mi.vfault += n_mi.vfault - o_mi.vfault;
		a_mi.demand += n_mi.demand - o_mi.demand;
		a_mi.cache += n_mi.cache - o_mi.cache;
		a_mi.swap += n_mi.swap - o_mi.swap;
		a_mi.file += n_mi.file - o_mi.file;
		a_mi.pfault += n_mi.pfault - o_mi.pfault;
		a_mi.cw += n_mi.cw - o_mi.cw;
		a_mi.steal += n_mi.steal - o_mi.steal;
		a_mi.freedpgs += n_mi.freedpgs - o_mi.freedpgs;
		break;
	case 'g':
		tsttab();
		printf(" %7.0f %7.0f %7.0f %7.0f %7.0f\n",
			(float)(n_gi.gswitch - o_gi.gswitch) / tdiff * HZ,
			(float)(n_gi.griioctl - o_gi.griioctl) / tdiff * HZ,
			(float)(n_gi.gintr - o_gi.gintr) / tdiff * HZ,
			(float)(n_gi.fifowait - o_gi.fifowait +
				n_gi.fifonowait - o_gi.fifonowait
				) / tdiff * HZ,
			(float)(n_gi.gswapbuf - o_gi.gswapbuf) / tdiff * HZ);
		a_gi.gswitch += n_gi.gswitch - o_gi.gswitch;
		a_gi.griioctl += n_gi.griioctl - o_gi.griioctl;
		a_gi.gswapbuf += n_gi.gswapbuf - o_gi.gswapbuf;
		a_gi.gintr += n_gi.gintr - o_gi.gintr;
		a_gi.fifowait += n_gi.fifowait - o_gi.fifowait;
		a_gi.fifonowait += n_gi.fifonowait - o_gi.fifonowait;
		break;
	case 'I':
		tsttab();
		printf("   %7.0f   %7.0f\n",
			(float)(n_si.intr_svcd - o_si.intr_svcd) / tdiff * HZ,
			(float)(n_si.vmeintr_svcd - o_si.vmeintr_svcd) / tdiff * HZ);
		a_si.intr_svcd += n_si.intr_svcd - o_si.intr_svcd;
		a_si.vmeintr_svcd += n_si.vmeintr_svcd - o_si.vmeintr_svcd;
		break;
	case 'M':
		tsttab();
		printf(" %7.0f %7.0f\n",
			(float)(n_si.mesgsnt - o_si.mesgsnt) / tdiff * HZ,
			(float)(n_si.mesgrcv - o_si.mesgrcv) / tdiff * HZ);
			a_si.mesgsnt += n_si.mesgsnt - o_si.mesgsnt;
			a_si.mesgrcv += n_si.mesgrcv - o_si.mesgrcv;
		break;
	}
}

/**********************************************************/

/*      print average routine  */
void
prtavg(void)
{
	int kk;
	int ii, jj=0;
	uint64_t twait;
	char ccc;
	int np;			/* # active processors */

	np = (n_sa.apstate ? n_sa.apstate : 1);

	tdiff = (TUNSIGNED)(a_si.cpu[0] + a_si.cpu[1] + a_si.cpu[2] + a_si.cpu[3] + a_si.cpu[4] + a_si.cpu[5]);
	tdiff /= np;
	twait = (TUNSIGNED)(a_si.wait[0] + a_si.wait[1] + a_si.wait[2] + a_si.wait[3] + a_si.wait[4]);
	twait /= np;
	if (tdiff <= 0.0)
		return;

	for (ii = 0; ii < np; ii++) {
		cst_tdiff[ii] = 0;
		for (kk = 0; kk < 6; kk++) {
			cst_tdiff[ii] += (TUNSIGNED)(a_cst[ii].cpu[kk]);
		}
		cst_twait[ii] = 0;
		for (kk = 0; kk < 5; kk++) {
			cst_twait[ii] += (TUNSIGNED)(a_cst[ii].wait[kk]);
		}
	}

	while((ccc = fopt[jj++]) != NULL)
	switch(ccc){
	case 'u':
		printf("Average  %5.0f %5.0f %5.0f %5.0f %5.0f %5.0f %5.0f %5.0f %5.0f %5.0f %5.0f\n",
		(double)(TUNSIGNED)a_si.cpu[1]/(np*tdiff) * 100.0,
		(double)(TUNSIGNED)a_si.cpu[2]/(np*tdiff) * 100.0,
		(double)(TUNSIGNED)a_si.cpu[5]/(np*tdiff) * 100.0,
		(double)(TUNSIGNED)a_si.cpu[3]/(np*tdiff) * 100.0,
		(double)(TUNSIGNED)a_si.cpu[0]/(np*tdiff) * 100.0,
		(double)(TUNSIGNED)a_si.cpu[4]/(np*tdiff) * 100.0,
		(twait == 0) ? (0.0) : (double)(TUNSIGNED)a_si.wait[0]/(double)(np*twait) * 100.0,
		(twait == 0) ? (0.0) : (double)(TUNSIGNED)a_si.wait[1]/(double)(np*twait) * 100.0,
		(twait == 0) ? (0.0) : (double)(TUNSIGNED)a_si.wait[2]/(double)(np*twait) * 100.0,
		(twait == 0) ? (0.0) : (double)(TUNSIGNED)a_si.wait[3]/(double)(np*twait) * 100.0,
		(twait == 0) ? (0.0) : (double)(TUNSIGNED)a_si.wait[4]/(double)(np*twait) * 100.0
		);
		break;
	case 'U':
		if (no_pcpu_data) {
			printf("Average  no per-cpu data, continuing...\n");
			break;
		}
		for (kk = 0; kk < n_sa.apstate; kk++) {
			if (!kk) printf("Average ");
			else printf("        ");
			printf(" %5d %5.0f %5.0f %5.0f %5.0f %5.0f %5.0f %5.0f %5.0f %5.0f %5.0f %5.0f\n", kk,
			(double)(TUNSIGNED)a_cst[kk].cpu[1]/cst_tdiff[kk] * 100.0,
			(double)(TUNSIGNED)a_cst[kk].cpu[2]/cst_tdiff[kk] * 100.0,
			(double)(TUNSIGNED)a_cst[kk].cpu[5]/cst_tdiff[kk] * 100.0,
			(double)(TUNSIGNED)a_cst[kk].cpu[3]/cst_tdiff[kk] * 100.0,
			(double)(TUNSIGNED)a_cst[kk].cpu[0]/cst_tdiff[kk] * 100.0,
			(double)(TUNSIGNED)a_cst[kk].cpu[4]/cst_tdiff[kk] * 100.0,
			(cst_twait[kk] == 0) ? (0.0) : 
			    (double)(TUNSIGNED)a_cst[kk].wait[0] / cst_twait[kk] * 100.0,
			(cst_twait[kk] == 0) ? (0.0) : 
			    (double)(TUNSIGNED)a_cst[kk].wait[1] / cst_twait[kk] * 100.0,
			(cst_twait[kk] == 0) ? (0.0) :
			    (double)(TUNSIGNED)a_cst[kk].wait[2] / cst_twait[kk] * 100.0,
			(cst_twait[kk] == 0) ? (0.0) : 
			    (double)(TUNSIGNED)a_cst[kk].wait[3] / cst_twait[kk] * 100.0,
			(cst_twait[kk] == 0) ? (0.0) :
                            (double)(TUNSIGNED)a_cst[kk].wait[4] / cst_twait[kk] * 100.0);
		}
		break;
	case 'y':
		printf("Average  %7.0f %7.0f %7.0f %7.0f %7.0f %7.0f\n",
			(float)a_si.rawch/tdiff *HZ,
			(float)a_si.canch/tdiff *HZ,
			(float)a_si.outch/tdiff *HZ,
			(float)a_si.rcvint/tdiff *HZ,
			(float)a_si.xmtint/tdiff *HZ,
			(float)a_si.mdmint/tdiff *HZ);
		break;
	case 'b':
		printf("Average  %7.0f %7.0f", 
			(float)a_si.bread/tdiff *HZ,
			(float)a_si.lread/tdiff *HZ);
		if (a_si.lread == 0) printf("   idle");
		else
		    printf(" %6.0f", 
			((float)a_si.lread - (float)a_si.bread)/
				(float)(a_si.lread) * 100.0);
		printf(" %7.0f %7.0f", 
			(float)a_si.bwrite/tdiff *HZ,
			(float)a_si.lwrite/tdiff *HZ);
		printf(" %7.0f",
			(float)a_si.wcancel/tdiff *HZ);
		if (a_si.lwrite == 0) printf("   idle");
		else
		    printf(" %6.0f", 
			((float)a_si.lwrite - (float)a_si.bwrite)/
				(float)(a_si.lwrite) * 100.0);
		printf(" %7.0f %7.0f\n", 
			(float)a_si.phread/tdiff *HZ,
			(float)a_si.phwrite/tdiff *HZ);
		break;
	case 'D':
	case 'd':
		printf("Average");
		no_tab_flg = 1;
		sgidapr(ccc == 'D');
		break;
	case 'v':
		break;
	case 'c':
		printf("Average  %7.0f %7.0f %7.0f %7.2f %7.2f %7.0f %7.0f\n",
			(float)a_si.syscall/tdiff *HZ,
			(float)a_si.sysread/tdiff *HZ,
			(float)a_si.syswrite/tdiff *HZ,
			(float)a_si.sysfork/tdiff *HZ,
			(float)a_si.sysexec/tdiff *HZ,
			(float)a_si.readch/tdiff * HZ,
			(float)a_si.writech/tdiff * HZ);
		break;
	case 'w':
		printf("Average  %7.2f %7.1f %7.2f %7.1f %7.1f %7.0f %7.0f\n",
			(float)a_si.swapin/tdiff * HZ,
			(float)a_si.bswapin /tdiff * HZ,
			(float)a_si.swapout/tdiff * HZ,
			(float)a_si.bswapout/tdiff * HZ,
			(float)a_si.pswapout/tdiff * HZ,
			(float)a_si.pswitch/tdiff * HZ,
			(float)a_si.kswitch/tdiff * HZ);
		break;
	case 'a':
		printf("Average  %7.0f %7.0f %7.0f\n",
			(float)a_si.iget/tdiff * HZ,
			(float)a_si.namei/tdiff * HZ,
			(float)a_si.dirblk/tdiff * HZ);
		break;
	case 'S':
		printf("Average %11.0f %9.0f %9.0f %9.0f %9.0f\n",
			(float)a_di.nservers / tdiff * HZ,
			((a_di.rcv_occ / tdiff * HZ * 100.0) > 100) ? 100.0 :
				(float)a_di.rcv_occ / tdiff * HZ * 100.0,
			(a_di.rcv_occ == 0) ? 0.0 :
				(float)a_di.rcv_que / (float)a_di.rcv_occ,
			((a_di.srv_occ / tdiff * HZ * 100.0) > 100) ? 100.0 :
				(float)a_di.srv_occ / tdiff * HZ * 100.0,
			(a_di.srv_occ == 0 ) ? 0.0 :
				(float)a_di.srv_que / (float)a_di.srv_occ );
		break;
	case 'q':
		if (a_si.runocc == 0)
			printf("Average  %7s %7s ","  ","  ");
		else {
			printf("Average  %7.1f %7.0f ",
			(float)a_si.runque /
				(float)a_si.runocc,
			(float)a_si.runocc /tdiff *HZ *100.0);
		}
		if (a_si.swpocc == 0)
			printf("%7s %7s","  ","  ");
		else {
			printf("%7.1f %7.0f",
			(float)a_si.swpque/
				(float)a_si.swpocc,
			(float)a_si.swpocc/tdiff *HZ *100.0);

		}
		if (a_si.wioocc == 0)
			printf(" %7s %7s\n","  ", "  ");
		else {
			printf(" %7.1f %7.0f\n",
			(float)a_si.wioque /
				(float)a_si.wioocc,
			(float)a_si.wioocc/tdiff *100.0);
		}
		break;
	case 'm':
		printf("Average  %7.2f %7.2f\n",
			(float)a_si.msg/tdiff * HZ,
			(float)a_si.sema/tdiff * HZ);
		break;

	case 'r':
		printf("Average  %7.0f",
			(double)(a_mi.freemem[0] + magic * a_mi.freemem[1])/tdiff);
		if (rcnt > 0)
			printf(" %7.0f %7.0f\n",(float)(a_mi.freeswap) / rcnt,
				(float)(a_ri.availsmem) / rcnt);
		else
			printf("\n");
		break;

	case 'R':
		if (!rcnt) {
			printf("Average\n");
			break;
		}
		printf("Average  %7.0f %7.0f %7.0f %7.0f %7.0f %7.0f %7.0f %7.0f\n",
			(float)a_ri.physmem / rcnt,
			(float)(a_ri.physmem - (a_ri.availrmem + a_ri.bufmem)) /
					rcnt,
			(float)((a_ri.availrmem - (a_ri.freemem + 
				a_ri.chunkpages + a_ri.dpages)) / rcnt),
			(float)(a_ri.bufmem) /rcnt,
			(float)(a_ri.dchunkpages + a_ri.dpages) /rcnt,
			(float)(a_ri.chunkpages - a_ri.dchunkpages) /rcnt,
			(float)(a_ri.freemem - a_ri.emptymem) /rcnt,
			(float)(a_ri.emptymem) /rcnt);
		break;

	case 'h':
	{
		if (rcnt > 0)
			printf("Average   %7.0f %7.0f %7.0f",
				(float)(a_mi.heapmem) / rcnt,
				(float)(a_mi.hovhd) / rcnt,
				(float)(a_mi.hunused) / rcnt);
		else
			printf("Average   ??????? ??????? ???????");

		printf(" %7.0f %7.0f\n",
			(float)(a_mi.halloc / tdiff * HZ),
			(float)(a_mi.hfree / tdiff * HZ));


		break;
	}

	case 't':
		printf("Average  %7.2f %7.2f %7.2f %7.2f %7.2f %7.2f %7.2f %7.2f %7.2f\n",
			(float)(a_mi.tfault / tdiff * HZ),
			(float)(a_mi.rfault / tdiff * HZ),
			(float)(a_mi.tvirt / tdiff * HZ),
			(float)(a_mi.tlbsync / tdiff * HZ),
			(float)(a_mi.tlbflush / tdiff * HZ),
			(float)(a_mi.twrap / tdiff * HZ),
			(float)(a_mi.tlbpids / tdiff * HZ),
			(float)(a_mi.tdirt / tdiff * HZ),
			(float)(a_mi.tphys / tdiff * HZ));
		break;

	case 'p':
		printf("Average  %7.2f %7.2f %7.2f %7.2f %7.2f %7.2f %7.2f %7.2f %6.2f\n",
			(float)(a_mi.vfault / tdiff * HZ),
			(float)(a_mi.demand / tdiff * HZ),
			(float)(a_mi.cache / tdiff * HZ),
			(float)(a_mi.swap / tdiff * HZ),
			(float)(a_mi.file / tdiff * HZ),
			(float)(a_mi.pfault / tdiff * HZ),
			(float)(a_mi.cw / tdiff * HZ),
			(float)(a_mi.steal / tdiff * HZ),
			(float)(a_mi.freedpgs / tdiff * HZ));
		break;
	case 'g':
		printf("Average  %7.0f %7.0f %7.0f %7.0f %7.0f\n",
			(float)(a_gi.gswitch / tdiff * HZ),
			(float)(a_gi.griioctl / tdiff * HZ),
			(float)(a_gi.gintr / tdiff * HZ),
			(float)((a_gi.fifowait + a_gi.fifonowait)/tdiff * HZ),
			(float)(a_gi.gswapbuf / tdiff * HZ));
		break;
	case 'I':
		printf("Average    %7.0f   %7.0f\n",
			(float)(a_si.intr_svcd / tdiff * HZ),
			(float)(a_si.vmeintr_svcd / tdiff * HZ));
		break;
	case 'M':
		printf("Average  %7.0f %7.0f\n",
			(float)(a_si.mesgsnt / tdiff * HZ),
			(float)(a_si.mesgrcv / tdiff * HZ));
		break;
	}
}

/**********************************************************/

/*      error exit routines  */
void
pillarg(void)
{
	fprintf(stderr,"%s -- illegal argument for option  %c\n",
		optarg,cc);
	exit(1);
}

void
perrexit(const char *mesg)
{
	char buf[180];

	sprintf(buf, "sar: %s", mesg);
	perror(buf);
	exit(1);
}

void
pmsgexit(const char *s)
{
	fprintf(stderr, "%s\n", s);
	exit(1);
}

/*************************************************************************/

/* new sgi print routines */

void
sgidpr(int Dflg)
{
	int i;
	unsigned long reqs;
	unsigned long blocks;
	unsigned long active;
	unsigned long response;
	unsigned long wreqs;
	unsigned long wblocks;
	unsigned long curqueue;
	unsigned long queuesum;
	unsigned long oldqueue;
	int pr=0;
	float busy, avque, rw_s, blk_s, w_s, wblk_s, avwait, avserv;

	if (Tflg)
	{
	    if (Fflg)
	    {
		printf("\n ----------------- DISK STATISTICS ------------------\n\n");
		printf(" busy avque     reads     blocks     writes    blocks   avwait avserv device\n");
		printf(" ---- ------ ---------- ---------- ---------- ---------- ----- ----- ------------\n");
	    }
	    else
	    {
		printf("\n ----------------- DISK STATISTICS ------------------\n\n");
		printf("    device  busy avque     reads     blocks     writes    blocks   avwait avserv\n");
		printf(" ---------- ---- ------ ---------- ---------- ---------- ---------- ----- -----\n");
	    }
	}

	for (i = 0; i < dskcnt; i++)
	{
		reqs = SGIDIO_COUNT(Nsgidio[i]) - SGIDIO_COUNT(Osgidio[i]);
		blocks = SGIDIO_BCNT(Nsgidio[i]) - SGIDIO_BCNT(Osgidio[i]);
		active = SGIDIO_ACT(Nsgidio[i]) - SGIDIO_ACT(Osgidio[i]);
		response = SGIDIO_RESP(Nsgidio[i]) - SGIDIO_RESP(Osgidio[i]);
		wreqs = SGIDIO_WCOUNT(Nsgidio[i]) - SGIDIO_WCOUNT(Osgidio[i]);
		wblocks = SGIDIO_WBCNT(Nsgidio[i]) - SGIDIO_WBCNT(Osgidio[i]);
		curqueue = SGIDIO_QCNT(Nsgidio[i]);
		oldqueue = SGIDIO_QCNT(Osgidio[i]);
		queuesum = SGIDIO_QCUM(Nsgidio[i]) - SGIDIO_QCUM(Osgidio[i]);

	/* Now some fudge factors. With the new multiple-active command
	   controllers (like 4201, 754) we have to infer the drive active
	   times rather indirectly, since we don't know what's actually
	   going on at the drive level any more. There can be quantization
	   errors in this inferred time, and we have to ensure that these
	   don't give rise to obviously absurd displays, like more than
	   100% active or negative wait times! */

		if (active > response) active = response;

		if (oldqueue > 0 && reqs < oldqueue)
		{
			busy = 100.0;
			Nsgidio[i].save_busy = 0.0;
		}
		else
		{
			busy = (float) active / tdiff * 100.0;
			busy += Osgidio[i].save_busy;
			if (curqueue == 0 && reqs == 0) /* nothing going on */
				busy = 0.0;
			Nsgidio[i].save_busy = 0.0;
			if (busy > 100.0)
			{
				Nsgidio[i].save_busy += (busy - 100.0);
				busy = 100.0;
			}
		}
		avque = (reqs) ?
			(float) queuesum / (float) reqs : 
			(float) curqueue;
		rw_s = (float)reqs / tdiff * HZ;
		blk_s = (float)blocks / tdiff * HZ;
		w_s = (float)wreqs / tdiff * HZ;
		wblk_s = (float)wblocks / tdiff * HZ;
		avwait = (reqs) ? 
			 ((float) (response - active) / (float) reqs) / HZ * 1000. :
			 0;
		avserv = (reqs) ? 
			 ((float) active / (float) reqs) / HZ * 1000. :
			 0;

		SGIDIO_COUNT(Asgidio[i]) += reqs;
		SGIDIO_BCNT(Asgidio[i]) += blocks;
		SGIDIO_ACT(Asgidio[i]) += active;
		SGIDIO_RESP(Asgidio[i]) += response;
		SGIDIO_WCOUNT(Asgidio[i]) += wreqs;
		SGIDIO_WBCNT(Asgidio[i]) += wblocks;
		SGIDIO_QCNT(Asgidio[i]) = (unsigned int)curqueue;
		SGIDIO_QCUM(Asgidio[i]) += queuesum;

		if (Dflg && (busy == 0.0))
			continue;
		pr++;
		if (!Tflg) {
			tsttab();
		}

		if (!Fflg)
			printf("%11s", SGIDIO_DNAME(Nsgidio[i]));
		if (Tflg)
			printf("%4.0f%% %6.1f %10ld %10ld %10ld %10ld %5.1f %5.1f",
				busy, avque, (reqs - wreqs), (blocks - wblocks),
				wreqs, wblocks, avwait, avserv);
		else
			printf("%6.0f %6.1f %6.1f %7.0f %6.1f %7.0f %7.1f %7.1f",
			       busy, avque, rw_s, blk_s, w_s, wblk_s, avwait, avserv);
		if (Fflg)
			printf(" %s\n", SGIDIO_DNAME(Nsgidio[i]));
		else
			printf("\n");
	}
	if (pr == 0) {
		tsttab();
		printf(" disks idle\n");
	}
}

void
sgidapr(int Dflg)
{
	int i;
	unsigned long reqs;
	unsigned long blocks;
	unsigned long active;
	unsigned long response;
	unsigned long wreqs;
	unsigned long wblocks;
	unsigned long curqueue;
	unsigned long queuesum;
	float busy, avque, rw_s, blk_s, w_s, wblk_s, avwait, avserv;
	int pr=0;

	for (i = 0; i < odskcnt; i++)
	{
		reqs = SGIDIO_COUNT(Asgidio[i]); 
		blocks = SGIDIO_BCNT(Asgidio[i]);
		active = SGIDIO_ACT(Asgidio[i]); 
		response = SGIDIO_RESP(Asgidio[i]);
		wreqs = SGIDIO_WCOUNT(Asgidio[i]); 
		wblocks = SGIDIO_WBCNT(Asgidio[i]);
		curqueue = SGIDIO_QCNT(Asgidio[i]);
		queuesum = SGIDIO_QCUM(Asgidio[i]);

		if (reqs == 0 && curqueue > 0 && SGIDIO_QCNT(Osgidio[i]) > 0)
			busy = 100.0;
		else
			busy = (float)active / tdiff * 100.0;
		if (busy > 100.0)
			busy = 100.0;
		avque = (reqs) ?
			(float) queuesum / (float) reqs : 
			(float) curqueue;
		rw_s = (float)reqs / tdiff * HZ;
		blk_s = (float)blocks / tdiff * HZ;
		w_s = (float)wreqs / tdiff * HZ;
		wblk_s = (float)wblocks / tdiff * HZ;
		avwait = (reqs) ? 
			 ((float) (response - active) / (float) reqs) /
			 HZ * 1000. :
			 0;
		avserv = (reqs) ? 
			 ((float) active / (float) reqs ) / HZ * 1000. :
			 0;

		if (Dflg && (busy == 0.0))
			continue;
		if (pr) printf("       ");
		pr++;
		printf(" ");
		if (!Fflg)
			printf("%11s", SGIDIO_DNAME(Nsgidio[i]));
		printf("%6.0f %6.1f %6.1f %7.0f %6.1f %7.0f %7.1f %7.1f",
			busy, avque, rw_s, blk_s, w_s, wblk_s, avwait, avserv);
		if (Fflg)
			printf(" %s\n", SGIDIO_DNAME(Nsgidio[i]));
		else
			printf("\n");
	}
	if (pr == 0)
		printf("    no disk activity\n");
}

/*
 * Setup the dio pointers.  Assumes the n_sa.numdisks contains the number
 * of devices.
 */
void
sgi_dio_setup(void)
{
	Sgidiosz = dskcnt * sizeof(struct sgidio);
	if (Sgidiosz == 0) return;
	if (dskcnt == odskcnt) return;

#ifdef SA_DEBUG
{
	static int	call_num = 0;
	printf("sgi_dio_setup called %dth time: Sgidiosz=%ld numdisks=%d\n",
	       ++call_num, Sgidiosz, dskcnt);
	if(call_num > 1)
		abort();
}
#endif /* SA_DEBUG */
	Nsgidio = (struct sgidio *)realloc(Nsgidio, Sgidiosz);
	Osgidio = (struct sgidio *)realloc(Osgidio, Sgidiosz);
	Asgidio = (struct sgidio *)realloc(Asgidio, Sgidiosz);
	
	if (!Nsgidio || !Osgidio || !Asgidio) {
		fprintf(stderr, "No memory for sgidio table (%ld)\n",
			Sgidiosz);
		exit(2);
	}
	
	return;
}

/*
 * Setup the cst pointers.  Assumes the n_sa.apstate contains the number
 * of cpus.
 */
void
sgi_cst_setup(void)
{
	size_t size;

	if (o_sa.apstate == n_sa.apstate) return;

	SAR_DEBUG(("cst setup: apstate = %d\n", n_sa.apstate));

#ifdef SA_DEBUG
{
	static int	call_num = 0;
	printf("sgi_cst_setup called %dth time: apstate=%d\n",
	       ++call_num,n_sa.apstate);
	if(call_num > 1)
		abort();
}
#endif /* SA_DEBUG */

	size = n_sa.apstate * sizeof(struct sysinfo_cpu);
	n_cst = (struct sysinfo_cpu *)realloc(n_cst, size);
	o_cst = (struct sysinfo_cpu *)realloc(o_cst, size);
	a_cst = (struct sysinfo_cpu *)realloc(a_cst, size);
	cst_tdiff = (int *)realloc(cst_tdiff, n_sa.apstate * sizeof(int));
	cst_twait = (int *)realloc(cst_twait, n_sa.apstate * sizeof(int));
	
	if (!n_cst || !o_cst || !a_cst || !cst_tdiff || !cst_twait) {
		fprintf(stderr, "No memory for cst table (%lu)\n",
			n_sa.apstate * sizeof(struct sysinfo_cpu));
		exit(2);
	}
	
	return;
}

size_t
sar_read(char *buf, size_t size)
{
	ssize_t bytes;
	size_t bytecnt = 0;

	while (size > 0) {
		if ((bytes = (read(fin, buf, size))) > 0) {
			size -= bytes;
			buf  += bytes;
			bytecnt += bytes;
		} else if (bytes == 0) {
			break;
		} else if (bytes < 0) {
			perrexit("sar_read failed");
		}
	}
	return(bytecnt);
}

void
resync(void)
{
	off_t offset2 = 0;
	char buffer[BUFSIZE];
	size_t bytes, idx;

	printf("  Invalid sar data, resyncing\n");
	
        /*
         * Back up to the beginning of previous record.
         * Search for the next magic word, looking byte by byte.
         */

	lseek(fin, lpos, SEEK_SET);

        for (;;) {
                idx = 0;
                bytes = sar_read(buffer, BUFSIZE);
                if (bytes == 0) {
			printf("  EOF reached\n");
			exit(1);
		}
                while (idx < bytes) {
			if (!strncmp(&buffer[idx++], SAR_MAGIC,
					SAR_MAGIC_LEN)) {
				offset2 = (off_t)(-bytes + idx - 1);
				lseek(fin, offset2, SEEK_CUR);
				return;
                        }
                }
        }
}

int
read_header(char *id, char *id2, struct rec_header *header, int cell)
{
	off_t	cpos;
	int	checked = 0;
	int	rval = 0;
	
	/*
	 * Read a record header structure from the log.  If it is not
	 * the one we expected (i.e. the magic field did not match) 
	 * search the log until:
	 * 1) we find it
	 * 2) the next sa record is reached, or
	 * 3) the end of the log is reached.
	 * If the next sa record was reached we try one more time by
	 * going back to the last sa record to see if we skipped over
	 * it earlier.
	 * We return 0 if we did not find it, -1 if we had to resync,
	 * and 1 if we found it.
	 */

        
	cpos = lseek(fin, (off_t)0, SEEK_CUR);
	SAR_DEBUG(("At position: %10lld looking for header: %s\n", cpos, id));
	for (;;) {
		if (sar_read((char *)header, sizeof(struct rec_header)) !=
				sizeof(struct rec_header)) {
			break;
		}

		SAR_DEBUG(("header: id %s size: %d nrec: %d\n", header->id,
			header->recsize, header->nrec));

		if (strncmp(header->magic, SAR_MAGIC, SAR_MAGIC_LEN)) {
		
			/* Didn't find a valid record header */

			if (realtime) {
				
				/* Give up, sadc doesn't match */

				break;
			}
			
			/* Attemp re-sync to next valid record header */

			resync();
			return(-1);
		}

		/*
		 * Valid header. Remember start of last valid record for
		 * resync.
		 */

		lpos = lseek(fin, (off_t)0, SEEK_CUR);

		if (!strncmp(header->id, id, SAR_MAGIC_LEN))
			rval = 1;
		else if (id2 != NULL && !strncmp(header->id, id2, SAR_MAGIC_LEN))
			rval = 2;

		if (!rval) {

			/*
			   Didn't find the one we expected. If this the
			   sa record then we hit the start of the next
			   record sequence. 
		 	 */

			if (realtime) {
				
				/* Give up, sadc doesn't match */

				break;
			}
			if (strncmp(header->id, SA_MAGIC, SAR_MAGIC_LEN) == 0) {

				/*
				 * Hit the the next record sequence. Go back
				 * once more to check if we skipped over it
				 * earlier in the sequence.
				 */

				if (checked) {
					break;
				}
				else {
					lseek(fin, rec_start, SEEK_SET);
					checked++;
					continue;
				}
			}
			else {

				/* Skip over this record and try the next */

				lseek(fin, header->nrec * header->recsize, 
					SEEK_CUR);
			}
		}
		else if (cell >= (int)header->nrec) {

			/* This record is not from a cell system or the
			 * cell specified is invalid.
			 */

			printf("cell %d not present, continuing...\n", cell);
			lseek(fin, header->nrec * header->recsize,
				SEEK_CUR);
		}
		else {
			/* Found it */

			return rval;
		}
	}

	/* Did not find it */

	if (!realtime) {

		/* Go back to where we started */

		lseek(fin, cpos, SEEK_SET);
	}
	return(0);
}

void
sum_rec(struct rec_header *header_p, char *buf, char *curtmp, size_t recsize)
{
	int *accum = (int *)buf;
	int *from = (int *)curtmp;
	size_t n = recsize / sizeof(int);
	time_t ts = 0;

	/* Sum fields of the record in curtmp with the record in buf.
	 * It is assumed they are composed of 'int' fields. For the SA
	 * rec we have to careful since it can't always be summed together.
	 */

	if (!strcmp(header_p->id, SA_MAGIC)) {
		if (((struct sa *)buf)->apstate == CPUDUMMYTIME) {

			/* This is the reboot record. Don't sum it */

			return;
		}
		ts = ((struct sa *)buf)->ts;	/* Save the timestamp */
	}
	while (n--) {
		*accum++ += *from++;
	}
	if (ts) {

		/* Put back the timestamp */

		((struct sa *)buf)->ts = ts;
	}
}
		
void
read_rec(char *buf, struct rec_header *header_p, int expected_recsize,
	 int cell, char *errmessage)
{
	char *curbuf, *curtmp;
	int x, recsize, nrec;

	lpos = lseek(fin, (off_t)0, SEEK_CUR);	/* remember where we started */

	SAR_DEBUG(("read rec: %s expected sz: %d sz: %d\n", header_p->id,
		expected_recsize, header_p->recsize));

	if (SINGLE_CELL(cell)) {
		
		/* Get just the record for the specified cell. We keep that
		 * one record and then skip over the rest.
		 */

		if (cell >= header_p->nrec) {
			fprintf(stderr, "cellid too large\n");
			pmsgexit(errmessage);
		}
		SAR_DEBUG(("read rec: skipping to cell at: %lld\n",
			lpos + cell * header_p->recsize));
	}
	
	nrec = header_p->nrec;
	if (SUM_CELLS(cell)) {
		bzero(buf, expected_recsize);
	}

	if (header_p->recsize == expected_recsize && !SUM_CELLS(cell) &&
			!SINGLE_CELL(cell)) {

		/* Everything should match. Just read in the way it is */

		if (sar_read(buf, header_p->recsize * nrec) !=
				 header_p->recsize * nrec) {
			pmsgexit(errmessage);
		}
	}
	else {

		/*
		 * Structure in log does not match current one. We will
		 * have to copy each sub-record and hope that only the
		 * end of the structure has been modified.  If this
		 * is a log from a cell system we may need to sum the
		 * records together or just pick the record from the
		 * specified cell.
		 */

		if (tmp_bufsize < MAX(header_p->recsize, expected_recsize) *
				 nrec) {

			/* Need to increase the tmp buffer we have been using */

			tmp_bufsize = MAX(header_p->recsize, expected_recsize) *
				nrec;
			if ((tmp_buf = (char *)realloc(tmp_buf,
					 tmp_bufsize)) == NULL) {
				fprintf(stderr, "tmp_buf: Not enough space\n");
				pmsgexit(errmessage);
			}
		}
		if (sar_read(tmp_buf, header_p->recsize * nrec) !=
				 header_p->recsize * nrec) {
			pmsgexit(errmessage);
		}

		/*
		 * Copy the log sub-records into the structure
		 * starting at the top.  Copy the smaller of the
		 * two record sizes.  If the log sub-records are
		 * smaller the end of the record will be left blank.
		 * In either case we assume this data is not essential
		 * for now.  Each record type will have to be checked
		 * for validity afterward.
		 */
		
		recsize = MIN(expected_recsize, header_p->recsize);
		curtmp = tmp_buf;
		curbuf = buf;
		for (x = 0; x < nrec; x++) {
			if (SUM_CELLS(cell)) {

				/* Sum the records together */

				sum_rec(header_p, buf, curtmp, recsize);
				curtmp += header_p->recsize;
			}
			else if (SINGLE_CELL(cell)) {
				if (x != cell) {

					/* This is not the cell we want */

					curtmp += header_p->recsize;
					continue;
				}
				SAR_DEBUG(("read rec: found cell rec\n"));
				memcpy(curbuf, curtmp, recsize);
			}
			else {

				/* Just copy the data */

				memcpy(curbuf, curtmp, recsize);
				curtmp += header_p->recsize;
				curbuf += expected_recsize;
			}
		}
	}
}

void
write_rec(char *id, void *buf, int recsize, int nrec, char *errmessage)
{
        struct rec_header header;
	int bufsize = recsize * nrec;

        strncpy(header.magic, SAR_MAGIC, SAR_MAGIC_LEN);
        strncpy(header.id, id, SAR_MAGIC_LEN);
        header.recsize = recsize;
        header.nrec = nrec;
        if (write(fout, &header, sizeof(header)) != sizeof(header)) {
                pmsgexit("Can't write rec header");
        }
        if (write(fout, buf, bufsize) != bufsize) {
                pmsgexit(errmessage);
        }
}

void
write_log_recs(void)
{

	write_rec(SA_MAGIC, (char *)&n_sa, sizeof(struct sa), 1,
		"Can't write sa record");
	if (unix_restart) {
		return;
	}
	write_rec(SINFO_MAGIC, &n_si, sizeof(n_si), 1,
		"Can't write sinfo record");
	write_rec(MINFO_MAGIC, &n_mi, sizeof(n_mi), 1,
		"Can't write minfo record");
	write_rec(DINFO_MAGIC, &n_di, sizeof(n_di), 1,
		"Can't write dinfo record");
	write_rec(GFX_MAGIC, &n_gi, sizeof(n_gi), 1,
		"Can't write gfxinfo record");
	write_rec(RMINFO_MAGIC, &n_ri, sizeof(n_ri), 1,
		"Can't write rminfo record");
	write_rec(CPU_MAGIC, (char *)n_cst, sizeof(struct sysinfo_cpu),
		n_sa.apstate, "Can't write cpu record");
	write_rec(NEODISK_MAGIC, (char *)Nsgidio, sizeof(struct sgidio),
		 dskcnt, "Can't write disk record");
}

void
read_infomap(void)
{

	struct rec_header header;

	if (read_header(INFO_MAGIC, NULL, &header, -1) <= 0) {
		pmsgexit("Can't read infomap");
	}

	read_rec((char *)&infomap, &header, sizeof(infomap), -1,
		"Can't read infomap");

	if ((infomap.long_sz && infomap.long_sz != sizeof(long)) ||
			(!infomap.long_sz && infomap.bootrt)) {

		/* A size mis-match may have occurred.  bootrt is currently
		 * not used and is a long so a value in there could indicate
		 * a problem also.
		 */

		fprintf(stderr, 
			"data wrong format or corrupted, expected %lu bit\n",
			sizeof(long) * 8);
		pmsgexit("sar failed");
	}

#ifdef NOTYET
	/* Get syscall names */

	if (read_header(SCNAME_MAGIC, NULL, &header) <= 0) {
		pmsgexit("Can't read syscall names");
	}
	read_rec((char *)syscallname, &header,
		 MAXSCNAME,"Can't read syscall names");

	/* Get lid names */

	if (read_header(LIDNAME_MAGIC, NULL, &header) <= 0) {
		pmsgexit("Can't read lid names");
	}
	read_rec((char *)lid_name, &header,
		 MAXLIDNAME, "Can't read lid names");
#endif
	log_start = lseek(fin, (off_t)0, SEEK_CUR);
}

int
read_data(char *id, void *buf, int size, int *invalid_p, int cell)
{
	struct rec_header header;
	int rval;
	char errmessage[40];

	if (invalid_p) {
		*invalid_p = 0;
	}
	rval = read_header(id, NULL, &header, cell);
	if (rval < 0) {
		return(rval);	/* had to re-sync */
	}
	else if (!rval) {

		/* Can't find header record */

		if (realtime) {
			sprintf(errmessage, "sadc mismatch, no record %s", id);
			perrexit(errmessage);
		}
		if (invalid_p) {
			*invalid_p = 1;
		}
		return(rval);
	}
	sprintf(errmessage, "Can't read record %s", id);
	read_rec((char *)buf, &header, size, cell, errmessage);
	return(1);
}

int
read_cpu_data(void)
{
	int i, rval = 0;
	struct sysinfo_cpu *buf;
	int prev_nrec = 0;
	static struct sysinfo_cpu *tmp_buf = (struct sysinfo_cpu *)0;
	struct rec_header header;
	

	/* Since the per-cpu data is an array and each cell's may be of a
	 * different size there is a record per cell.  
	 */

	sgi_cst_setup();
	no_pcpu_data = 0;
	for (i = 0; i < nsites; i++) {
		rval = read_header(CPU_MAGIC, NULL, &header, -1);
		if (rval < 0) {
			printf("Incompatible cpu data found, resyncing\n");
			return(-1);
		}
		if (header.nrec == 0) {

			/* No per-cpu data for now */

			no_pcpu_data = 1;
			return(0);
		}
		if (SUM_CELLS(cellid) && prev_nrec != 0) {

			/* n_sa.apstate is the sum of all cpus so as we
			 * we read each cell we append the data.
			 */

			buf += prev_nrec;
		}
		else if (SINGLE_CELL(cellid) && i != cellid) {

			/* We are going to read data from another cell that
			 * we are not interested in so it just goes into
			 * a tmp buffer.
			 */

			tmp_buf = (struct sysinfo_cpu *)realloc(tmp_buf,
				 sizeof(struct sysinfo_cpu) * header.nrec);
			if (!tmp_buf) {
				fprintf(stderr, "Can't allocate tmp_buf\n");
				pmsgexit("read_cpu_data");
			}
			buf = tmp_buf;
		}
		else {
			buf = n_cst;
		}
		read_rec((char *)buf, &header, sizeof(struct sysinfo_cpu), -1,
			"Could not read cpu record");
		prev_nrec = header.nrec;
	}
	return(0);
}

int
read_disk_data(void)
{
	struct rec_header header;
	int rval;

	rval = read_header(NEODISK_MAGIC, DISK_MAGIC, &header, -1);
	if (rval < 0) {
		return(rval);	/* had to re-sync */
	}
	else if (!rval) {
		/* Can't find disk header record */
		return(rval);
	}
	odskcnt = dskcnt;
	dskcnt = header.nrec;
	Sgidiosz = dskcnt * sizeof(struct sgidio);

	if (dskcnt != odskcnt) {

		Nsgidio = (struct sgidio *)realloc(Nsgidio, Sgidiosz);
		Osgidio = (struct sgidio *)realloc(Osgidio, Sgidiosz);
		Asgidio = (struct sgidio *)realloc(Asgidio, Sgidiosz);

		if (!Nsgidio || !Osgidio || !Asgidio) {
			fprintf(stderr, "No memory for sgidio table (%ld)\n",
				Sgidiosz);
			exit(2);
		}
	}

	if (rval == 1)
		read_rec((char *)Nsgidio, &header, sizeof(struct sgidio), -1,
			"Could not read disk record");
	else /* rval == 2 */ {
		/*
		 * This sar file has disk data in the old format.
		 * To make things simple for the display routine,
		 * we will convert it to the new format.  This means
		 * copying the data to the proper location.  The new
		 * data is more than twice as large as the old, so
		 * it's safe to do a lowest-to-highest copy.
		 */
		int i;
		struct oldsgidio *olddio = (struct oldsgidio *) Nsgidio;

		read_rec((char *)Nsgidio, &header, sizeof(struct oldsgidio), -1,
			"Could not read disk record");
		i = dskcnt - 1;
		for (i = dskcnt - 1; i >= 0; i--) {
			Nsgidio[i].save_busy = olddio[i].save_busy;
			Nsgidio[i].sdio_iotim = olddio[i].sdio_iotim;
			memcpy(SGIDIO_DNAME(Nsgidio[i]),
			       SGIDIO_OLD_DNAME(olddio[i]),
			       sizeof(SGIDIO_OLD_DNAME(olddio[i])));
		}
	}

	return(1);
}


/*
 * Verify sa data structure.
 */
int
badsa(void)
{
	if (realtime) return(0);
#ifdef NOTYET
	if (n_sa.szinode < 0 || n_sa.szfile < 0 || n_sa.szproc < 0) return(1);
	if (n_sa.mszinode < 0 || n_sa.mszfile < 0 || n_sa.mszproc < 0) return(1);
	if (n_sa.ts < 0) return(1);
#endif

	return(0);
}

void
print_cpu_totals(void)
{

	float total_cpu_stats[CPU_STATES][2];
	float total_secs = 0.0;
	float wait_io, wait_swap, wait_raw, wait_gfxc, wait_gfxf;
	int i,j;
	static struct {
	char *title;
	int  index;
	} cpu_stats_id[] = {
	      {"User mode", CPU_USER},
	      {"Kernel mode", CPU_KERNEL},
	      {"Idle", CPU_IDLE},
	      {"Waiting for I/O",CPU_WAIT},
	      {"SXBRK", CPU_SXBRK},
	      {"Intr", CPU_INTR}};

	for (i = 0; i < CPU_STATES; i++) {
		total_cpu_stats[i][0] = 0.0;
	}

	if (no_pcpu_data) {
		for (i = 0; i < CPU_STATES; i++) {

			/* Determine total seconds used from sysinfo */

			total_cpu_stats[i][0] =
			    (float)TDIFF(n_si.cpu[i], o_si.cpu[i]) / HZ;
			total_secs += total_cpu_stats[i][0];
		}

		for (i = 0; i < CPU_STATES; i++) {

			/* Determine percent utilization */

			total_cpu_stats[i][1] = 100.0 * (1 - (total_secs -
				total_cpu_stats[i][0]) / total_secs);
		}
	}

	printf("\n ----------------- CPU STATISTICS ------------------\n");

	if (no_pcpu_data) {
		printf("\n (%d CPUs: No per-cpu data available)\n", 
			n_sa.apstate);
	}
	else {
		printf("\n CPU     User           Kernel           Idle          Wait I/O\n");
		printf(" ---- ----------      ----------      ----------      ---------- \n");
	}

	for (i = 0; i < n_sa.apstate && !no_pcpu_data; i++) {
		printf("%5d %10.2f %3.0f%% %10.2f %3.0f%% %10.2f %3.0f%% %10.2f %3.0f%%\n", i,
			(float)TDIFF(n_cst[i].cpu[CPU_USER], 
				     o_cst[i].cpu[CPU_USER]) / HZ,
			(float)TDIFF(n_cst[i].cpu[CPU_USER],
				     o_cst[i].cpu[CPU_USER]) / 
					(float)cst_tdiff[i] * 100.0,
			(float)TDIFF(n_cst[i].cpu[CPU_KERNEL], 
				     o_cst[i].cpu[CPU_KERNEL]) / HZ,
			(float)TDIFF(n_cst[i].cpu[CPU_KERNEL],
				     o_cst[i].cpu[CPU_KERNEL]) / 
					(float)cst_tdiff[i] * 100.0,
			(float)TDIFF(n_cst[i].cpu[CPU_IDLE], 
				     o_cst[i].cpu[CPU_IDLE]) / HZ,
			(float)TDIFF(n_cst[i].cpu[CPU_IDLE],
				     o_cst[i].cpu[CPU_IDLE]) / 
					(float)cst_tdiff[i] * 100.0,
			(float)TDIFF(n_cst[i].cpu[CPU_WAIT], 
				     o_cst[i].cpu[CPU_WAIT]) / HZ,
			(float)TDIFF(n_cst[i].cpu[CPU_WAIT],
				     o_cst[i].cpu[CPU_WAIT]) / 
					(float)cst_tdiff[i] * 100.0);
			for (j = 0; j < CPU_STATES; j++) {

				/* Accum total seconds */

				total_cpu_stats[j][0] +=
				    (float)TDIFF(n_cst[i].cpu[j],
						 o_cst[i].cpu[j]) / HZ;
			}
			total_secs += ((float)cst_tdiff[i] / HZ);
	}
	if (!no_pcpu_data) {
		for (j = 0; j < CPU_STATES; j++) {
			/* Determine percent utilization */

			total_cpu_stats[j][1] = 100.0 * (1 -
				 (total_secs - total_cpu_stats[j][0]) / 
					total_secs);
		}
		printf(" ---- ----------      ----------      ----------      ---------- \n");
		printf("%5s %10.2f %3.0f%% %10.2f %3.0f%% %10.2f %3.0f%% %10.2f %3.0f%%\n", "",
			total_cpu_stats[CPU_USER][0], 
			total_cpu_stats[CPU_USER][1], 
			total_cpu_stats[CPU_KERNEL][0], 
			total_cpu_stats[CPU_KERNEL][1], 
			total_cpu_stats[CPU_IDLE][0], 
			total_cpu_stats[CPU_IDLE][1], 
			total_cpu_stats[CPU_WAIT][0], 
			total_cpu_stats[CPU_WAIT][1]);
	}

	printf("\n Total                   (secs)\n");
	printf(" --------------- --------------\n"); 
	for (i = 0; i < CPU_STATES; i++) {
		printf(" %-15s %14.2f %5.1f%%\n", cpu_stats_id[i].title,
			total_cpu_stats[cpu_stats_id[i].index][0],
			total_cpu_stats[cpu_stats_id[i].index][1]);
	}

	printf("\n");
	printf("  I/O Wait Type    Interval secs \n");
	printf(" ----------------  ------------- \n");

	wait_io = (float)TDIFF(n_si.wait[W_IO], o_si.wait[W_IO]) / HZ;
	wait_swap = (float)TDIFF(n_si.wait[W_SWAP], o_si.wait[W_SWAP]) / HZ;
	wait_raw = (float)TDIFF(n_si.wait[W_PIO], o_si.wait[W_PIO]) / HZ;
	wait_gfxc = (float)TDIFF(n_si.wait[W_GFXC], o_si.wait[W_GFXC]) / HZ;
	wait_gfxf = (float)TDIFF(n_si.wait[W_GFXF], o_si.wait[W_GFXF]) / HZ;

	printf(" Standard          %13.2f %5.1f%%\n", wait_io,
		wait_io / total_secs * 100.0);
	printf(" Swap              %13.2f %5.1f%%\n", wait_swap,
		wait_swap / total_secs * 100.0);
	printf(" Raw               %13.2f %5.1f%%\n", wait_raw,
		wait_raw / total_secs * 100.0);
	printf(" GFXC              %13.2f %5.1f%%\n", wait_gfxc,
		wait_gfxc / total_secs * 100.0);
	printf(" GFXF              %13.2f %5.1f%%\n", wait_gfxf,
		wait_gfxf / total_secs * 100.0);
}

void
print_buf_totals(void)
{

	unsigned breads, lreads, bwrites, lwrites, phreads, phwrites;
	float r_cached, w_cached;

	breads = n_si.bread - o_si.bread;
	lreads = n_si.lread - o_si.lread;
	bwrites = n_si.bwrite - o_si.bwrite;
	lwrites = n_si.lwrite - o_si.lwrite;
	phreads = n_si.phread - o_si.phread;
	phwrites = n_si.phwrite - o_si.phwrite;

	if (lreads > 0) {
		r_cached = (float)(lreads - breads) / (float)lreads * 100.0;
	}
	else r_cached = 0.0;
	if (lwrites > 0) {
		w_cached = (float)(lwrites - bwrites) / (float)lwrites * 100.0;
	}
	else w_cached = 0.0;

	printf("\n --------------------- BUFFER STATISTICS -------------\n");
	printf("\n               Logical     Block      Raw     %%Cache\n");
	printf("             ---------- ---------- ---------- ------\n");
	printf(" Reads       %10u %10u %10u %6.1f\n", lreads, breads,
	       phreads, r_cached);
	printf(" Writes      %10u %10u %10u %6.1f\n", lwrites, bwrites,
		phwrites, w_cached);
}

void
print_call_totals(void)
{
	unsigned int writech, readch;

	writech = n_si.writech - o_si.writech;
	readch = n_si.readch - o_si.readch;

	printf("\n ----------------- SYSTEM CALL STATISTICS ----------------\n\n");
	printf("                         Count     Per Second\n");
	printf("                     ------------ ------------\n");
	printf(" System Calls        %12u %12.2f\n", (n_si.syscall - 
		o_si.syscall), ((float)(n_si.syscall - o_si.syscall) /  
		(float)tdiff * (float)HZ));
	printf(" Reads               %12u %12.2f\n", (n_si.sysread - 
		o_si.sysread), ((float)(n_si.sysread - o_si.sysread) /   
		(float)tdiff * (float)HZ));
	printf(" KBytes read         %12u %12.2f\n", readch / KB,
		(float)readch / KB / (float)tdiff * (float)HZ);
	printf(" Writes              %12u %12.2f\n", (n_si.syswrite - 
		o_si.syswrite), ((float)(n_si.syswrite - o_si.syswrite) / 
		(float)tdiff * (float)HZ));
	printf(" KBytes written      %12u %12.2f\n", writech / KB,
		(float)writech / KB / (float)tdiff * (float)HZ);
	printf(" Fork's              %12u %12.2f\n", (n_si.sysfork - 
		o_si.sysfork), ((float)(n_si.sysfork - o_si.sysfork) / 
		(float)tdiff * (float)HZ));
	printf(" Exec's              %12u %12.2f\n", (n_si.sysexec - 
		o_si.sysexec), ((float)(n_si.sysexec - o_si.sysexec) / 
		(float)tdiff * (float)HZ));
}

void
print_file_totals(void)
{
	printf("\n ----------------- SYSTEM FILE STATISTICS ----------------\n\n");
	printf("                         Count     Per Second\n");
	printf("                     ------------ ------------\n");
	printf(" Iget's              %12u %12.2f\n", (n_si.iget - 
		o_si.iget), ((float)(n_si.iget - o_si.iget) / 
		(float)tdiff * (float)HZ));
	printf(" Namei's             %12u %12.2f\n", (n_si.namei - 
		o_si.namei), ((float)(n_si.namei - o_si.namei) / 
		(float)tdiff * (float)HZ));
	printf(" Directory blk reads %12u %12.2f\n", (n_si.dirblk - 
		o_si.dirblk), ((float)(n_si.dirblk - o_si.dirblk) / 
		(float)tdiff * (float)HZ));

}

void
print_swap_totals(void)
{
	printf("\n ----------------- SYSTEM SWAP STATISTICS ----------------\n\n");
	printf("                         Count     Per Second\n");
	printf("                     ------------ ------------\n");
	printf(" Swap-ins            %12u %12.2f\n", (n_si.swapin - 
		o_si.swapin), ((float)(n_si.swapin - o_si.swapin) / 
		(float)tdiff * (float)HZ));
	printf(" Swap-outs           %12u %12.2f\n", (n_si.swapout - 
		o_si.swapout), ((float)(n_si.swapout - o_si.swapout) / 
		(float)tdiff * (float)HZ));
	printf(" Process switches    %12u %12.2f\n", (n_si.pswitch - 
		o_si.pswitch), ((float)(n_si.pswitch - o_si.pswitch) / 
		(float)tdiff * (float)HZ));
	printf(" Kernel switches     %12u %12.2f\n", (n_si.kswitch - 
		o_si.kswitch), ((float)(n_si.kswitch - o_si.kswitch) / 
		(float)tdiff * (float)HZ));
}

void
print_mesg_totals(void)
{
	printf("\n ----------------- SYSTEM MESSAGE STATISTICS ----------------\n\n");
	printf("                         Count     Per Second\n");
	printf("                     ------------ ------------\n");
	printf(" IDL Messages snt    %12u %12.2f\n", (n_si.mesgsnt - 
		o_si.mesgsnt), ((float)(n_si.mesgsnt - o_si.mesgsnt) / 
		(float)tdiff * (float)HZ));
	printf(" IDL Messages rcv    %12u %12.2f\n", (n_si.mesgrcv - 
		o_si.mesgrcv), ((float)(n_si.mesgrcv - o_si.mesgrcv) / 
		(float)tdiff * (float)HZ));
}

void
print_tlb_totals(void)
{
	printf("\n ----------------- SYSTEM TLB STATISTICS ----------------\n\n");
	printf("                         Count     Per Second\n");
	printf("                     ------------ ------------\n");
	printf(" Double miss fault   %12u %12.2f\n", (n_mi.tfault - 
		o_mi.tfault), ((float)(n_mi.tfault - o_mi.tfault) / 
		(float)tdiff * (float)HZ));
	printf(" Reference Bit fault %12u %12.2f\n", (n_mi.rfault - 
		o_mi.rfault), ((float)(n_mi.rfault - o_mi.rfault) / 
		(float)tdiff * (float)HZ));
	printf(" Virt. Addr (tvirt)  %12u %12.2f\n", (n_mi.tvirt - 
		o_mi.tvirt), ((float)(n_mi.tvirt - o_mi.tvirt) / 
		(float)tdiff * (float)HZ));
	printf(" Sync                %12u %12.2f\n", (n_mi.tlbsync - 
		o_mi.tlbsync), ((float)(n_mi.tlbsync - o_mi.tlbsync) / 
		(float)tdiff * (float)HZ));
	printf(" Flush               %12u %12.2f\n", (n_mi.tlbflush - 
		o_mi.tlbflush), ((float)(n_mi.tlbflush - o_mi.tlbflush) / 
		(float)tdiff * (float)HZ));
	printf(" PID wrap            %12u %12.2f\n", (n_mi.twrap - 
		o_mi.twrap), ((float)(n_mi.twrap - o_mi.twrap) / 
		(float)tdiff * (float)HZ));
	printf(" New PID             %12u %12.2f\n", (n_mi.tlbpids - 
		o_mi.tlbpids), ((float)(n_mi.tlbpids - o_mi.tlbpids) / 
		(float)tdiff * (float)HZ));
	printf(" Dirty               %12u %12.2f\n", (n_mi.tdirt - 
		o_mi.tdirt), ((float)(n_mi.tdirt - o_mi.tdirt) / 
		(float)tdiff * (float)HZ));
	printf(" Virt/Phys invalid   %12u %12.2f\n", (n_mi.tphys - 
		o_mi.tphys), ((float)(n_mi.tphys - o_mi.tphys) / 
		(float)tdiff * (float)HZ));
}

void
print_page_totals(void)
{
	printf("\n ----------------- SYSTEM PAGE STATISTICS ----------------\n\n");
	printf("                         Count     Per Second\n");
	printf("                     ------------ ------------\n");
	printf(" Translation fault   %12u %12.2f\n", (n_mi.vfault - 
		o_mi.vfault), ((float)(n_mi.vfault - o_mi.vfault) / 
		(float)tdiff * (float)HZ));
	printf(" Demand zero/fill    %12u %12.2f\n", (n_mi.demand - 
		o_mi.demand), ((float)(n_mi.demand - o_mi.demand) / 
		(float)tdiff * (float)HZ));
	printf(" From cache          %12u %12.2f\n", (n_mi.cache - 
		o_mi.cache), ((float)(n_mi.cache - o_mi.cache) / 
		(float)tdiff * (float)HZ));
	printf(" From swap           %12u %12.2f\n", (n_mi.swap - 
		o_mi.swap), ((float)(n_mi.swap - o_mi.swap) / 
		(float)tdiff * (float)HZ));
	printf(" From file           %12u %12.2f\n", (n_mi.file - 
		o_mi.file), ((float)(n_mi.file - o_mi.file) / 
		(float)tdiff * (float)HZ));
	printf(" Protection fault    %12u %12.2f\n", (n_mi.pfault - 
		o_mi.pfault), ((float)(n_mi.pfault - o_mi.pfault) / 
		(float)tdiff * (float)HZ));
	printf(" Copy-on-write copy  %12u %12.2f\n", (n_mi.cw - 
		o_mi.cw), ((float)(n_mi.cw - o_mi.cw) / 
		(float)tdiff * (float)HZ));
	printf(" Copy-on-write steal %12u %12.2f\n", (n_mi.steal - 
		o_mi.steal), ((float)(n_mi.steal - o_mi.steal) / 
		(float)tdiff * (float)HZ));
	printf(" Reclaimed           %12u %12.2f\n", (n_mi.freedpgs - 
		o_mi.freedpgs), ((float)(n_mi.freedpgs - o_mi.freedpgs) / 
		(float)tdiff * (float)HZ));
}

void
display_totals(void)
{
	char opt;
	int opt_index = 0;

	curt = localtime(&o_sa.ts);
	printf("\n Log Started           %.2d/%.2d/%.2d %.2d:%.2d:%.2d\n", 
		curt->tm_mon + 1, curt->tm_mday, curt->tm_year % 100,
		curt->tm_hour, curt->tm_min, curt->tm_sec);
	curt = localtime(&n_sa.ts);
	printf(" Log Ended             %.2d/%.2d/%.2d %.2d:%.2d:%.2d\n", 
		curt->tm_mon + 1, curt->tm_mday, curt->tm_year % 100,
		curt->tm_hour, curt->tm_min, curt->tm_sec);
	printf(" Log Interval (secs)   %17.2f\n", ((float)tdiff /
		(float)HZ)); 
	printf(" Samples               %17d\n", recno);
	printf(" Avg. Sample Interval  %17.0f\n", tdiff / HZ / (recno - 1));

	while ((opt = options[opt_index++]) != NULL) {
		switch(opt) {
		case 'U':
			if (options[0] == 'u') {

				/* Already did cpu totals for 'A' option */

				break;
			}
		case 'u':
			print_cpu_totals();
			break;
		case 'b':
			print_buf_totals();
			break;
		case 'c':
			print_call_totals();
			break;
		case 'a':
			print_file_totals();
			break;
		case 'w':
			print_swap_totals();
			break;
		case 'M':
			print_mesg_totals();
			break;
		case 't':
			print_tlb_totals();
			break;
		case 'p':
			print_page_totals();
			break;
		case 'D':
		case 'd':
			sgidpr(opt == 'D');
			break;
		default:
			printf("\noption '%c' totals not supported yet\n", opt);
			break;
		}
	}
}
