#include <sys/types.h>
#include <sys/stat.h>
#include <sys/lock.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <invent.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dslib.h>
#include <limits.h>


#define MAXDRIVES 2048 /* allow for up to 2048 drives, which is silly, but ...*/

struct drvlist {
	int type;
	int adap;
	int drive;
};

struct drvlist drivelist[MAXDRIVES];
int nextdrive;	/* index into drivelist for appending */

int dohalt, warndownload, verbose;

FILE *out = stdout;

typedef	struct
{
	unchar	pqt:3;	/* peripheral qual type */
	unchar	pdt:5;	/* peripheral device type */
	unchar	rmb:1,	/* removable media bit */
		dtq:7;	/* device type qualifier */
	unchar	iso:2,	/* ISO version */
		ecma:3,	/* ECMA version */
		ansi:3;	/* ANSI version */
	unchar	aenc:1,	/* async event notification supported */
		trmiop:1,	/* device supports 'terminate io process msg */
		res0:2,	/* reserved */
		respfmt:3;	/* SCSI 1, CCS, SCSI 2 inq data format */
	unchar	ailen;	/* additional inquiry length */	
	unchar	res1;	/* reserved */
	unchar	res2;	/* reserved */
	unchar	reladr:1,	/* supports relative addressing (linked cmds) */
		wide32:1,	/* supports 32 bit wide SCSI bus */
		wide16:1,	/* supports 16 bit wide SCSI bus */
		synch:1,	/* supports synch mode */
		link:1,	/* supports linked commands */
		res3:1,	/* reserved */
		cmdq:1,	/* supports cmd queuing */
		softre:1;	/* supports soft reset */
	const char	vid[8];	/* vendor ID */
	const char	pid[16];	/* product ID */
	const char	prl[4];	/* product revision level*/
	unchar	vendsp[20];	/* vendor specific; typically firmware info */
	unchar	res4[40];	/* reserved for scsi 3, etc. */
	unchar  vendsp2[6];	/* more vendor specific */
	unchar  ibmdate[5];	/* YYDDD year/julian date for ibm ultrastar */
	unchar  vendsp3[32];	/* more vendor specific */
	/* more vendor specific information may follow */
} inqdata;

int inqbuf[sizeof(inqdata)/sizeof(int)];
inqdata *inq;

/* aenc, trmiop, reladr, wbus*, synch, linkq, softre are only valid if
 * if respfmt has the value 2 (or possibly larger values for future
 * versions of the SCSI standard). */

static char pdt_types[][16] = {
	"Disk", "Tape", "Printer", "CPU", "WORM", "CD-ROM",
	"Scanner", "Optical", "Jukebox", "Comm", "Unknown"
};
#define NPDT (sizeof pdt_types / sizeof pdt_types[0])

/* Description from IBM firmware of our new vendor specific command
 * so that we can avoid disk and filesystem errors when we spin
 * the drive down with filesystems or databases actively using the
 * drives.  This command *will* fail with firmware revs < 4343.
 *
 *   Here are the details for the vendor unique Scrub Unit command.
 *   The command descriptor block is 6 bytes long, the operation code is
 *   0xDB.  Byte 1, bit 0 is the Immed (immediate) bit and bits 1 thru 7
 *   are reserved.  Byte 2 is the delay in seconds between stopping the
 *   motor and performing the scrub.  The minimum value is 12 seconds;
 *   a value less than 12 can be specified but a 12 second delay still
 *   occurs.  Byte 3 is reserved.  Byte 4, bit 0 is the FS (force scrub)
 *   bit and bits 1 thru 7 are reserved.  Byte 5 is the standard last byte
 *   of any SCSI command descriptor block (VU, flag and link bits).
 *     If the command is received while the motor is NOT spinning, a
 *   normal Start Unit will be performed.  If the command is received
 *   when the drive has been powered on for less than 48 hours or it has
 *   been less than 48 hours since the last scrub stop and the FS bit is NOT
 *   set, good status will be returned and no stop, scrub or restart actions
 *   will be performed.  Commands received from the same initiator or other
 *   initiators while the Scrub Unit command is active will be queued until
 *   the Scrub Unit command completes.
*/
int
scrubdb(struct dsreq *dsp, int stopdelay, int immed, int force)
{
  /* really a group6 command, not g1, but both 10 byte... */
  fillg1cmd(dsp, (uchar_t*)CMDBUF(dsp), 0xdb, immed&1, stopdelay, 0, force&1,
	0, 0, 0, 0, 0);
  filldsreq(dsp, NULL, 0, DSRQ_READ|DSRQ_SENSE);
  dsp->ds_time = 1000 * (stopdelay+30);	/* should only need + 10 or 15 */
  return(doscsireq(getfd(dsp), dsp));
}

int
mytestunitready00(struct dsreq *dsp, int timeo)
{
  fillg0cmd(dsp, (uchar_t *)CMDBUF(dsp), G0_TEST, 0, 0, 0, 0, 0);
  filldsreq(dsp, 0, 0, DSRQ_READ|DSRQ_SENSE);
  dsp->ds_time = 1000 * timeo;
  return(doscsireq(getfd(dsp), dsp));
}


int
startunit1b(struct dsreq *dsp, int startstop, int immed)
{
  fillg0cmd(dsp, (uchar_t *)CMDBUF(dsp), 0x1b, 0|immed, 0, 0, (uchar_t)startstop, 0);
  filldsreq(dsp, NULL, 0, DSRQ_READ|DSRQ_SENSE);
  dsp->ds_time = 1000 * 90; /* 90 seconds */
  return(doscsireq(getfd(dsp), dsp));
}

int
myinquiry12(struct dsreq *dsp, caddr_t data, long datalen)
{
  fillg0cmd(dsp, (uchar_t *)CMDBUF(dsp), G0_INQU, 0, 0, 0, B1(datalen), 0);
  filldsreq(dsp, (uchar_t *)data, datalen, DSRQ_READ|DSRQ_SENSE);
  dsp->ds_time = 1000 * 30;
  return(doscsireq(getfd(dsp), dsp));
}

/* inquiry cmd that does vital product data as spec'ed in SCSI2 */
int
vpinquiry12( struct dsreq *dsp, caddr_t data, long datalen, int page)
{
  fillg0cmd(dsp, (uchar_t *)CMDBUF(dsp), G0_INQU, 1, page, 0,
	B1(datalen), 0);
  filldsreq(dsp, (uchar_t *)data, datalen, DSRQ_READ|DSRQ_SENSE);
  return(doscsireq(getfd(dsp), dsp));
}


void
showinqdata(const char *path, struct dsreq *dsp, inqdata *inq)
{
	int vpinqbuf[256/sizeof(int)];	/* force word aligned */
	unsigned char *vpinq;

	fprintf(out, "%s:  ", path);
	if(DATASENT(dsp) < 1) {
		fprintf(out, "No inquiry data returned\n");
		return;
	}
	fprintf(out, "%-9s", pdt_types[(inq->pdt<NPDT) ? inq->pdt:NPDT-1]);
	if (DATASENT(dsp) > 8)
		fprintf(out, "%12.8s", inq->vid);
	if (DATASENT(dsp) > 16)
		fprintf(out, "%.16s", inq->pid);
	if (DATASENT(dsp) > 32)
		fprintf(out, "%.4s", inq->prl);
	if (DATASENT(dsp) >= (offsetof(inqdata, ibmdate)+4) &&
		!strncmp(inq->vid, "SGI ", 4) &&
		!strncmp(inq->pid, "IBM  DF", 7))
		fprintf(out,"  Date: %.2s/%.3s", inq->ibmdate, inq->ibmdate+2);

	vpinq = (unsigned char *)vpinqbuf;
	if(vpinquiry12(dsp, (char *)vpinqbuf,
		sizeof(vpinqbuf)-1, 0x80) == 0 && DATASENT(dsp)>3
		&& vpinq[3]>0) {
		unsigned char *s = &vpinq[4], *s1;
		int i = vpinq[3], j;

		while(i>0 && isspace(*s)) {
			i--;
			s++;
		}
		for(j=i,s1=s; j && isalnum(*s1); s1++, j--)
			;
		if(i && i!=j) {
			*s1 = '\0';
			fprintf(out, "  Serial: %.8s" ,s);
		}
	}
	fprintf(out, "\n");
}


struct dsreq *
drvopen(struct drvlist *drv)
{
	struct dsreq *dsp;
	char dpath[64];

	sprintf(dpath, "sc%dd%dl0", drv->adap, drv->drive);
	dsp = dsopen(dpath, O_RDONLY);
	if(!dsp) {
		fprintf(out, "Error: could not reopen %s\n", dpath);
		return NULL;
	}
	return dsp;
}


char *
dname(struct drvlist *drv)
{
	static char path[64];

	sprintf(path, "sc%dd%d", drv->adap, drv->drive);
	return path;
}

void
do_scrub(struct drvlist *drv, int delay, int force)
{
	struct dsreq *dsp;
	int ret;

	if(!(dsp=drvopen(drv)))
		return;
	if(verbose>1)
		fprintf(out, "cleaning disk surface on %s\n", dname(drv));

	/* only report errors via the -d option, on this one; mostly in
	 * case it still has older firmware */
	if((ret=scrubdb(dsp, delay, 1, force)) && 
		(ret==2 || ret==8 || (ret== -1 && RET(dsp)==DSRT_AGAIN))) {
		if(ret == -1)
			sleep(2);	/* lib does it on chkcond or busy */
		/* repeat once to clear chk condition; and make it really
		 * happen.  If that's not enough, more tries probably won't
		 * help.
		*/
		(void)scrubdb(dsp, delay, 1, force);
	}

	dsclose(dsp);
}

int
unitsort(const void *a, const void *b)
{
	struct drvlist *x, *y;
	x = (struct drvlist *)a;
	y = (struct drvlist *)b;

	if(x->drive != y->drive)
		return x->drive - y->drive;
	if(x->adap != y->adap)
		return x->adap - y->adap;
	return x->type - y->type;
}

/* sort, and then spin up all the drives that we spun down back up,
 * based on unit; that is, spin up all the drives with the same ID's
 * at the same time, regardless of controller, then go on to the next
 * ID after a delay, etc.
 * With new startdb command, no need to special case the system disk
*/
void
scrub(int stagger, int stoptime, int forceit)
{
	int d, i, delay;

	qsort((void*)drivelist, (size_t)nextdrive, sizeof(drivelist[0]), unitsort);

	if(verbose)
		fprintf(out, "Starting drive surface cleaning (spindown, then up)\n");

	for(delay=i=0; i<nextdrive; ) {
		d = drivelist[i].drive;
		if(delay && stagger)
			sleep(stagger);
		while(drivelist[i].drive == d && i < nextdrive)
			do_scrub(&drivelist[i++], stoptime, forceit);
		delay = 1;
	}

#ifdef BESUREREADY
	if(verbose)
		fprintf(out, "Waiting for completion\n");

	/* Unfortunately, for the SCIP driver, the open we do here causes
	 * the SCIP to do an inquiry with a 5 second timeout, and that
	 * causes a bus reset, which sort of hoses things, or at best,
	 * is undesirable.
	*/

	/* wait for them all to become ready again */
	for(i=0; i<nextdrive; i++) {
	    struct dsreq *dsp;
	    if(dsp=drvopen(&drivelist[i])) {
		if(mytestunitready00(dsp, stoptime+30)) {
		    fprintf(out, "%s not ready after disk cleaning cycle\n",
				dname(&drivelist[i]));
		}
		else if(verbose>1)
		    fprintf(out, "%s ready after disk cleaning cycle\n",
				dname(&drivelist[i]));
		dsclose(dsp);
	    }
	}
#else /* BESUREREADY */
	/* We don't want to force the delay on system shutdown; we use
	 * -F in crontab, and what we tell people to run manually, but not
	 * at system shutdown.
	 * But we have to delay the shutdown, because xlv_shutdown runs after
	 * this patch.
	 */
	if(verbose)
		fprintf(out, "Waiting for completion\n");
	sleep(stoptime+16);	/* should be all done by then */
#endif /* BESUREREADY */
}


/* return non-zero if drive is ibm starfire, so we can spin down all
 * drives from drivelist[] with sequencing */
int
dodev(const char *path, int adap, int id)
{
	struct dsreq *dsp;
	/* int because they must be word aligned. */
	int inqbytes, isibmstar, ret;

	if((dsp = dsopen(path, O_RDONLY)) == NULL) {
		/* mostly to catch wrong permissions; i.e., not root */
		if(errno != ENXIO && errno != ENODEV)
		    fprintf(out, "Can't open %s: %s\n", path,
			strerror(errno));
		return 0;
	}

	if((ret=mytestunitready00(dsp, 120)) && 
		(ret==2 || ret==8 || (ret== -1 && RET(dsp)==DSRT_AGAIN))) {
		if(ret == -1)
			sleep(2);	/* lib does it on chkcond or busy */
		/* repeat once to clear chk condition; at this point we don't
		 * really care if ready or not */
		(void)mytestunitready00(dsp, 120);
	}

	if(myinquiry12(dsp, (caddr_t)inqbuf, sizeof inqbuf) == -1 &&
		RET(dsp) == DSRT_TIMEOUT) {
		dsclose(dsp);
		return 0;	/* do rest only if device exists/responds */
	}

	inq = (inqdata *)inqbuf;
	inqbytes = (int)DATASENT(dsp); 

	if(verbose)
		showinqdata(path, dsp, inq);

	if((dohalt) && inq->pdt) {
		/* skip halt if it isn't a disk */
		if(verbose)
			fprintf(out, "%s: %s skipped because not a disk\n",
				path, "spindown");
		dsclose(dsp);
		return 0;
	}

	isibmstar = inqbytes > 35 && !strncmp(inq->vid, "SGI ", 4) &&
			!strncmp(inq->pid, "IBM  DFHSS", 10) &&
			(*inq->prl >= '4');	/* rev 3434 fw barfs on 10 byte cmd */

	if(warndownload) {
		/* Only if we got everything we want, and the strings match. */
		if(isibmstar && (!strncmp(inq->prl, "1111", 4) ||
			 !strncmp(inq->prl, "3434", 4) ||
			 !strncmp(inq->prl, "4040", 4) ||
			 !strncmp(inq->prl, "4141", 4) ||
			 !strncmp(inq->prl, "4241", 4) ||
			 !strncmp(inq->prl, "4343", 4))) {
			 if(warndownload)
				fprintf(out, "Notice: disk dks%dd%d should have"
				    " new firmware downloaded\n", adap, id);
		}
	}

	dsclose(dsp);
	return isibmstar;
}


/*ARGSUSED1*/
int
dohinv(inventory_t *h, void *junk)
{
	char dpath[64];
	static uint64_t save_node = 0, save_port = 0;

	if (h->inv_class == INV_FCNODE) {
		save_node = ((uint64_t) h->inv_type << 32) | (uint32_t) h->inv_controller;
		save_port = ((uint64_t) h->inv_unit << 32) | (uint32_t) h->inv_state;
		return 0;
	}
	if (h->inv_class != INV_DISK || (h->inv_type != INV_SCSIDRIVE))
		goto out;
	if (save_node)
		sprintf(dpath, "%llx/lun0/c%dp%llx", save_node, h->inv_controller, save_port);
	else
		sprintf(dpath, "sc%dd%dl0", h->inv_controller, h->inv_unit);

	if(dodev(dpath, (int)h->inv_controller, (int)h->inv_unit)
		&& nextdrive < MAXDRIVES) {
		drivelist[nextdrive].type = (int)h->inv_type;
		drivelist[nextdrive].adap = (int)h->inv_controller;
		drivelist[nextdrive].drive = (int)h->inv_unit;
		nextdrive++;
	}
out:
	save_node = save_port = 0;
	return 0;
}


main(int argc, char **argv)
{
	int c, err = 0, stagger = 0, stoptime = 0, forcescrub = 0;
	extern char *optarg;
	extern int optind, opterr;

	opterr = 0;	/* handle errors ourselves. */
	while ((c = getopt(argc, argv, "vdFH:S:W")) != -1) {
		switch(c) {
		case 'v':
			verbose++;	/* show inquiry info */
			break;
		case 'd':
			dsdebug++;	/* enable debug info */
			break;
		case 'F':
			forcescrub++;	/* set bit to force drive scrub */
			break;
		case 'W':
			warndownload++;	/* warn if we should load new firmware */
			break;
		case 'H':
			/* spin the drive down, and backup with a single scsi
			 * comand.  Used even at system shutdown, since we don't
			 * no for sure that they will turn off power to the drives.
			 * we allow 0, but the firmware will treat that as 12.
			*/
			dohalt = 1;
			stoptime = atoi(optarg);
			if(stoptime < 0 || stoptime > 255) {
				stoptime = 0;
				fprintf(stderr, "Invalid stop period %s ignored\n",
					optarg);
			}
			break;
		case 'S':
			/* delay between drives, to handle cases where the
			 * powersupply can't spinup all drives at the same time */
			stagger = atoi(optarg);
			if(stagger <= 0) {
				stagger = 0;
				fprintf(stderr, "Invalid stagger period %s ignored\n",
					optarg);
			}
			break;
		default:
			err = 1;
			break;
		}
	}

	if(err || optind != argc || optind == 1)  {
		/* don't show -f option in Usage!  don't show debug either */
		fprintf(stderr, "Usage: %s [-v] [-H halttime [-S delay] [-F]] [-W]\n", argv[0]);
		return 1;
	}

	if(chdir("/dev/scsi")) {
		fprintf(stderr, "Can't cd to /dev/scsi: %s\n", strerror(errno));
		exit(1);
	}

	(void)scaninvent(dohinv, 0);

	if(dohalt && nextdrive)	/* only if any drives found */
		scrub(stagger, stoptime, forcescrub);
	return 0;
}
