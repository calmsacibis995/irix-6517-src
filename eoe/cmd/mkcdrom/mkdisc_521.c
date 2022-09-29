/*
 * mkdisc.c
 *
 * Make a WORM disc using a Philips CDD 521 Compact Disc Recoder
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <dslib.h>
#include <fcntl.h>
#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/schedctl.h>
#include "mkdisc.h"

#define min(a,b) ((a)<(b)?(a):(b))

static int debug;

/*
 * Store dsp in here so signal handler can see it if needed
 */

static dsreq_t *sig_dsp;

struct timespec thistm, lasttm;	/* timestamp write commands */

void
byteprint(caddr_t sarg, int n, int nperline, int space)
{
	int   i, x;
	char  *sp = (space) ? " ": "";
	unsigned char *s = (unsigned char *)sarg;

#define hex(x) "0123456789ABCDEF" [ (x) & 0xF ]

	for(i=0;i<n;i++)  {
		x = s[i];
		fprintf(stderr,((i%4==3)?"%c%c%s%s":"%c%c%s"),
			hex(x>>4), hex(x), sp, sp);
		if ( i%nperline == (nperline - 1) )
			fprintf(stderr,"\n");
	}
	if ( space )
		fprintf(stderr,"\n");
#undef hex
}

displayerr (dsreq_t *dsp)
{
    caddr_t sp = SENSEBUF(dsp);

    if(clock_gettime(CLOCK_SGI_CYCLE, &thistm)) {
	perror("can't get clock for duration time stamp");
	thistm.tv_sec = 0;
	thistm.tv_nsec = 0;
    }
       
    if (SENSESENT(dsp)) {	/* decode sense data returned */
	fprintf(stderr, "sense key %x - %s\n", SENSEKEY(sp),
	ds_ctostr( (unsigned long)SENSEKEY(sp), sensekeytab));
	byteprint(SENSEBUF(dsp), SENSESENT(dsp), 16, 1);
    }
	fprintf(stderr, "SCSI status=%x, devscsi return=%x", STATUS(dsp),
		RET(dsp));
	if(RET(dsp))
		fprintf(stderr, " (%s)", ds_vtostr((uint)RET(dsp),dsrtnametab));
	fprintf(stderr, "\n");
	fprintf(stderr, "datalen=%x, datasent=%x\n", DATALEN(dsp),
		DATASENT(dsp));
	fprintf(stderr,"Failing command was: %s: ",
		ds_vtostr((uint)(CMDBUF(dsp))[0], cmdnametab));
	byteprint(CMDBUF(dsp),CMDLEN(dsp),16,1);
    if(thistm.tv_sec || thistm.tv_nsec) 
	fprintf(stderr, "Command took %u.%.9lu seconds\n",
		thistm.tv_sec - lasttm.tv_sec,
		thistm.tv_nsec - lasttm.tv_nsec);
}

/*
 * A version of read that works the same for std input as files
 */

read_data (int fd, void *buf, long n)
{
    char *ptr = buf;
    int num_read = 0, this_time;

    while (num_read < n && 0 < (this_time = read(fd, ptr, n - num_read))) {
	num_read += this_time;
	ptr += this_time;
    }
    return (num_read);
}


/*
 * Catch signals; abort writing
 */

void
sig_abort (int sig)
{
    fprintf(stderr, "got signal, rezeroing worm...exit...\n");
    rezero(sig_dsp);
    exit(sig);
}


/*
 * rezerounit- issue group 0 rezero command (0x01)
*/

rezero (dsreq_t *dsp)
{
  fillg0cmd(dsp,FIXPROTO CMDBUF(dsp),REZERO,0,0,0,0,0);
  filldsreq(dsp, 0, 0, DSRQ_READ | DSRQ_SENSE);
  return(doscsireq(getfd(dsp), dsp));
}


/*
 * stopstart- issue group 0 stop/start command (0x1b)
*/

stopstart (dsreq_t *dsp, long str)
{
  fillg0cmd(dsp,FIXPROTO CMDBUF(dsp),STOPSTART,0,0,0,B1(str),0);
  /* ASYNXFR, to force async scsi mode; the drive does not do well
   * with sync negotiations */
  filldsreq(dsp, 0, 0, DSRQ_READ | DSRQ_SENSE | DSRQ_ASYNXFR);
  return(doscsireq(getfd(dsp), dsp));
}


/*
 * Medium prevent/allow removal - group 0 command (0x1b)
*/

mediumremoval (dsreq_t *dsp, long prv)
{
  fillg0cmd(dsp,FIXPROTO CMDBUF(dsp),MEDIUMLOCK,0,0,0,B1(prv&0x01),0);
  filldsreq(dsp, 0, 0, DSRQ_READ | DSRQ_SENSE);
  return(doscsireq(getfd(dsp), dsp));
}


/*
 * Medium load/unload - group 1 command (0xe7)
*/

mediumload (dsreq_t *dsp, long imm, long mlu)
{
  fillg1cmd(dsp,FIXPROTO CMDBUF(dsp),MEDIUMLOAD,B1(imm&0x01),0,0,0,0,0,0,
	B1(mlu&0x01),0);
  filldsreq(dsp, 0, 0, DSRQ_READ | DSRQ_SENSE);
  return(doscsireq(getfd(dsp), dsp));
}


/*
 * Flush cache - group 1 command (0x35)
 */

flushcache (dsreq_t *dsp, u_char *dbuf, long count)
{
    int ret;
    fillg1cmd(dsp,FIXPROTO CMDBUF(dsp),FLUSHCACHE,0,0,0,0,0,0,0,0,0);
    filldsreq(dsp, dbuf, count, DSRQ_WRITE | DSRQ_SENSE);
    TIME(dsp) = 240 * 1000; /* Set 4 minute timeout */
    (void)clock_gettime(CLOCK_SGI_CYCLE, &lasttm);
    ret = doscsireq(getfd(dsp), dsp);
    if(debug>1 && !ret) { /* if debug && not going to be printed in displayerr */
	if(!clock_gettime(CLOCK_SGI_CYCLE, &thistm))
	    fprintf(stderr, "Took %u.%.9lu seconds\n",
		thistm.tv_sec - lasttm.tv_sec,
		thistm.tv_nsec - lasttm.tv_nsec);
    }
    return ret;
}


/*
 * Recover group 1 command (0xec)
 */

recover (dsreq_t *dsp, u_char *dbuf, long count)
{
    fillg1cmd(dsp,FIXPROTO CMDBUF(dsp),RECOVER,0,0,0,0,0,0,0,0,0);
    filldsreq(dsp, dbuf, count, DSRQ_READ | DSRQ_SENSE);
    TIME(dsp) = 120 * 1000; /* Set 2 minute timeout */
    return(doscsireq(getfd(dsp), dsp));
}


/*
 * Fixation group 1 command (0xe9)
 */

fixation (dsreq_t *dsp, u_char *dbuf, long count, long imm, long onp, long toc)
{
    int ret;
    fillg1cmd(dsp,FIXPROTO CMDBUF(dsp),FIXATION,B1(imm),0,0,0,0,0,0,(toc|onp),0);
    filldsreq(dsp, dbuf, count, DSRQ_WRITE | DSRQ_SENSE);
    TIME(dsp) = 240 * 1000; /* Set 4 minute timeout */
    (void)clock_gettime(CLOCK_SGI_CYCLE, &lasttm);
    ret = doscsireq(getfd(dsp), dsp);
    if(debug>1 && !ret) { /* if debug && not going to be printed in displayerr */
	if(!clock_gettime(CLOCK_SGI_CYCLE, &thistm))
	    fprintf(stderr, "Took %u.%.9lu seconds\n",
		thistm.tv_sec - lasttm.tv_sec,
		thistm.tv_nsec - lasttm.tv_nsec);
    }
    return ret;
}



/*
 * Write group 1 command (0x2a)
 */
writegroup1 (dsreq_t *dsp, u_char *dbuf, long datalen, long lba)
{
    int ret;
    fillg1cmd(dsp,FIXPROTO CMDBUF(dsp),WRITE1,0,0,0,0,0,0,B2(DTOB(datalen)),0);
    filldsreq(dsp, dbuf, datalen, DSRQ_WRITE | DSRQ_SENSE);
    TIME(dsp) = 120 * 1000; /* Set 2 minute timeout */
    (void)clock_gettime(CLOCK_SGI_CYCLE, &lasttm);
    ret = doscsireq(getfd(dsp), dsp);
    if(debug>1 && !ret) { /* if debug && not going to be printed in displayerr */
	if(!clock_gettime(CLOCK_SGI_CYCLE, &thistm))
	    fprintf(stderr, "Took %u.%.9lu seconds\n",
		thistm.tv_sec - lasttm.tv_sec,
		thistm.tv_nsec - lasttm.tv_nsec);
    }
    return ret;
}


/*
 * Read group 0 command (0x08)
 */

readgroup0 (dsreq_t *dsp, u_char *dbuf, long datalen, long lba)
{
    fillg0cmd(dsp,FIXPROTO CMDBUF(dsp),READ0,B3(lba),B1(DTOB(datalen)),0);
    filldsreq(dsp, dbuf, datalen, DSRQ_READ | DSRQ_SENSE);
    return(doscsireq(getfd(dsp), dsp));
}



/*
 * Read group 1 command (0x28)
 */

readgroup1 (dsreq_t *dsp, u_char *dbuf, long datalen, long lba)
{
    fillg1cmd(dsp,FIXPROTO CMDBUF(dsp),READ1,0,B4(lba),0,B2(DTOB(datalen)),0);
    filldsreq(dsp, dbuf, datalen, DSRQ_READ | DSRQ_SENSE);
    return(doscsireq(getfd(dsp), dsp));
}


/*
 * Verify group 1 command (0x2f)
 */

verifygroup1 (dsreq_t *dsp, u_char *dbuf, long datalen, long lba)
{
    fillg1cmd(dsp,FIXPROTO CMDBUF(dsp),VERIFY1,(BVY_MEDIUM<<3),B4(lba),0,B2(DTOB(datalen)),0);
    filldsreq(dsp, dbuf, datalen, DSRQ_READ | DSRQ_SENSE);
    TIME(dsp) = 7200 * 1000; /* Set 120 minute timeout */
    return(doscsireq(getfd(dsp), dsp));
}


/*
 * Seek group 0 command (0x0b)
 */

seekgroup0 (dsreq_t *dsp, u_char *dbuf, long datalen, long lba)
{
    fillg0cmd(dsp,FIXPROTO CMDBUF(dsp),SEEK0,B3(lba),0,B1(DTOB(datalen)));
    filldsreq(dsp, dbuf, datalen, DSRQ_READ | DSRQ_SENSE);
    return(doscsireq(getfd(dsp), dsp));
}


/*
 * Seek group 1 command (0x2b)
 */

seekgroup1 (dsreq_t *dsp, u_char *dbuf, long datalen, long lba)
{
    fillg1cmd(dsp,FIXPROTO CMDBUF(dsp),SEEK1,0,B4(lba),0,B2(DTOB(datalen)),0);
    filldsreq(dsp, dbuf, datalen, DSRQ_READ | DSRQ_SENSE);
    return(doscsireq(getfd(dsp), dsp));
}


/*
 * Read Track Information group 1 command (0xe5)
 */

readtrkinfo (dsreq_t *dsp, trkinfo_t *dbuf, int count) 
{
    int trknum = 1;	/* only one track written for software image builds */

    fillg1cmd(dsp,FIXPROTO CMDBUF(dsp),READTRACK,B4(0),B1(trknum),B2(0),B1(count),0);
    filldsreq(dsp, (u_char *)dbuf, count, DSRQ_READ | DSRQ_SENSE);
    if (doscsireq(getfd(dsp), dsp)) {
        fprintf(stderr, "readtrkinfo fails\n");
        displayerr(dsp);
	return -1;
    }
    printf("track info:: blen: 0x%x, ntrks: 0x%x, saddr: 0x%x, trklen: %d,\n\t trkstmode: 0x%x, dmode: 0x%x, fblks: 0x%x\n",
       dbuf->blen, dbuf->ntrks, V4(dbuf->saddr), V4(dbuf->trklen), dbuf->trkstmode, dbuf->dmode, V4(dbuf->fblks));
   return 0;
}


/*
 * Read Disc Information group 1 command (0x43)
 */

readdiscinfo (dsreq_t *dsp, discinfo_t *dbuf, int count)
{
    int trknum = 1;     /* only one track written for software image builds */

    fillg1cmd(dsp,FIXPROTO CMDBUF(dsp),READDISC,0,B4(0),B1(trknum),B2(count),SESSION);
    filldsreq(dsp, (u_char *)dbuf, count, DSRQ_READ | DSRQ_SENSE);
    if (doscsireq(getfd(dsp), dsp)) {
        fprintf(stderr, "readdiscinfo fails\n");
        displayerr(dsp);
	return -1;
    }
    printf("Disc info:: toclen: 0x%x, firsttrk: 0x%x, lasttrk: 0x%x\n",
       V2(dbuf->toclen), dbuf->firsttrk, dbuf->lasttrk);
    return 0;
}



/*
 * Reserve track group 1 command (0xe4)
 */

reservetrack (dsreq_t *dsp, u_char *dbuf, long datalen)
{
    fillg1cmd(dsp,FIXPROTO CMDBUF(dsp),RESERVETRACK,0,0,0,0,B4(DTOB(datalen)),0);
    filldsreq(dsp, dbuf, datalen, DSRQ_WRITE | DSRQ_SENSE);
    return(doscsireq(getfd(dsp), dsp));
}


/*
 * Write track group 1 command (0xe6)
 */

writetrack (dsreq_t *dsp, u_char *dbuf, long datalen, long trk, long amode, long mix)
{
    int ret;
    fillg1cmd(dsp,FIXPROTO CMDBUF(dsp),WRITETRACK,0,0,0,0,
	B1(trk),B1(amode),B2(DTOB(datalen)),B1(mix));
    filldsreq(dsp, dbuf, datalen, DSRQ_WRITE | DSRQ_SENSE);
    TIME(dsp) = 120 * 1000; /* Set 2 minute timeout */
    (void)clock_gettime(CLOCK_SGI_CYCLE, &lasttm);
    ret = doscsireq(getfd(dsp), dsp);
    if(debug>1 && !ret) { /* if debug && not going to be printed in displayerr */
	if(!clock_gettime(CLOCK_SGI_CYCLE, &thistm))
	    fprintf(stderr, "Took %u.%.9lu seconds\n",
		thistm.tv_sec - lasttm.tv_sec,
		thistm.tv_nsec - lasttm.tv_nsec);
    }
    return ret;
}



/*
 * First writable address command (0xe2)
 */

firstwrite (dsreq_t *dsp, firstwrite_t *dbuf, int count, long trk)
{
    fillg1cmd(dsp,FIXPROTO CMDBUF(dsp),FIRSTWRITE,0,B1(trk),MODE1,0,0,0,0,B1(count),0);
    filldsreq(dsp, FIXPROTO(dbuf), count, DSRQ_WRITE | DSRQ_SENSE);
    TIME(dsp) = 120 * 1000; /* Set 2 minute timeout */
    return(doscsireq(getfd(dsp), dsp));
}



/*
 * Send mode sense command (0x1a)
 */

modesense (dsreq_t *dsp, modesense_t *dbuf, int count)
{
    if (modesense1a(dsp, (caddr_t) dbuf, count, 0, 0, 0) != -1) {
      printf("sense:: dlen: 0x%x, mtype: 0x%x, appcode: 0x%x, bdl: 0x%x\n",
	 dbuf->sdl, dbuf->mtype, dbuf->hac, dbuf->bdl);
      return 0;
    }
    return -1;
}


/*
 * Send inquiry command using default format (0x12)
 */

inquiry (dsreq_t *dsp, inquiry_t *dbuf, int count)
{
    if (inquiry12(dsp, (caddr_t) dbuf, count, 0) != -1) {
      printf("inquiry:: dtype: 0x%x, dtypeq: 0x%x, version: 0x%x, rdf: 0x%x,\n \
	al: 0x%x, vid: %-.4s, pid: %-.10s, level: %-.4s, date: %-.8s\n",
	dbuf->pdt, dbuf->dtq, dbuf->ver, dbuf->rdf, dbuf->al, dbuf->vid,
	dbuf->pid, dbuf->level, dbuf->date);
      return 0;
    }
    return -1;
}


/*
 * Send inquiry command using alternate format (0x12)
 */

inquiry_long (dsreq_t *dsp, inquirylong_t *dbuf, int count)
{
    if (inquiry12(dsp, (caddr_t) dbuf, count, (char)0x01) != -1) {
      printf("inquiry long:: dtype: 0x%x, dtypeq: 0x%x, version: 0x%x, \
         rdf: 0x%x, al: 0x%x, dcpv: %0x%x, dt: 0x%x, dtr: 0x%x, hrn: 0x%x, \
         smrn: 0x%x, dsn: 0x%x, ltv: %0x%x, stv: 0x%x, mnd: 0x%x, los: 0x%x, \
         scid: %-.36s, cs: 0x%x\n", dbuf->pdt, dbuf->dtq, dbuf->ver, dbuf->rdf,
	 dbuf->al, dbuf->dcpv, dbuf->dt, dbuf->dtr, dbuf->hrn, dbuf->smrn, 
	 dbuf->dsn, dbuf->ltv, dbuf->stv, dbuf->mnd, dbuf->los, dbuf->scid, 
	 dbuf->cu);
      return 0;
    }
    return -1;
}


readcd (dsreq_t *dsp, u_char *dbuf, int img, int imgsz, int dbuflen)
{
    int nblk = 0, bytes_left = imgsz, count, bcount;

    fprintf(stderr, "readcd:: %d bytes\n", bytes_left);
    while (bytes_left) {
        count = bytes_left > dbuflen ? dbuflen : bytes_left;
        bcount = (bytes_left < dbuflen) ? (bytes_left-(bytes_left%CACHE_SIZE))+CACHE_SIZE : count;
        bytes_left -= count;
        if (readgroup1(dsp, dbuf, bcount, nblk)) {
          fprintf(stderr, "read failed with %d remaining bytes of %d (blk %d)\n", bytes_left, imgsz, nblk);
          displayerr(dsp);
          break;
	}
        if (write(img, dbuf, count) < 0) {
	  perror("write");
	  break;
	}
	if (debug) write(STDERR_FILENO, ".", 1);
        nblk += DTOB(bcount);
    }
    printf("done\n");
    return 0;
}


writecd (dsreq_t *dsp, u_char *dbuf, int img, int imgsz, int dbuflen)
{
    int nblk = 0, bytes_left = imgsz, count, bcount;

	ds_default_timeout = 60;	/* 10 definitely not enough */

    if (schedctl(NDPRI, getpid(), NDPHIMAX+5))
	perror("couldn't set non-degrading priority, may not work");

    fprintf(stderr, "write cdrom:: %d bytes\n", bytes_left);

    if(clock_gettime(CLOCK_SGI_CYCLE, &lasttm))
	    perror("can't get initial clock value for debugging");

    if(writetrack(dsp, dbuf, NEWTRACK, NEWTRACK, MODE1, NOMIX)) {
	fprintf(stderr, "write newtrack cmd failed\n");
	displayerr(dsp);
    }

    while (bytes_left) {
        count = bytes_left > dbuflen ? dbuflen : bytes_left;
	if (count != read_data(img, dbuf, count)) {  /* Unrecoverable */
            perror("writecd");
            return -1;
        }
        bcount = (bytes_left < dbuflen) ? (bytes_left-(bytes_left%CACHE_SIZE))+CACHE_SIZE : count;
        bytes_left -= count;

        if (writegroup1(dsp, dbuf, bcount, nblk)) {
	  fprintf(stderr, "write failed with %d remaining bytes of %d (blk %d)\n", bytes_left, imgsz, nblk);
	  displayerr(dsp);
	  break;
	}
	
	if (debug==1) write(STDERR_FILENO, ".", 1);
	nblk += DTOB(bcount)+1;
    }

    fprintf(stderr, "flushing cache...");
    /* bcount was used here as the 3rd parameter, but the CD-R takes
     * no data (which makes sense for it's mode of oepration; bcount
     * was more or less random here, anyway....  (same for fixation()
     * below).  Olson, 3/96 */
    if (flushcache(dsp, dbuf, 0)) {
	fprintf(stderr, "flushcache fails\n");
	displayerr(dsp);
    }

    fprintf(stderr, "fixating...");
    if(fixation(dsp, dbuf, 0, IMMCLEAR, ONPCLEAR, TOCCDROM)) {
	fprintf(stderr, "fixate (write TOC) failed\n");
	displayerr(dsp);
    }

    fprintf(stderr, "\nCompleted creation of CD with no errors\n");
    return 0;
}


verifycd (dsreq_t *dsp, u_char *dbuf, int img, int imgsz, int dbuflen)
{
    int nblk = 0, bytes_left = imgsz, count, bcount;

    fprintf(stderr, "verifycd:: %d bytes\n", bytes_left);
    while (bytes_left) {
        count = bytes_left > dbuflen ? dbuflen : bytes_left;
        bcount = (bytes_left < dbuflen) ? (bytes_left-(bytes_left%CACHE_SIZE))+CACHE_SIZE : count;
        bytes_left -= count;
        if (verifygroup1(dsp, dbuf, bcount, nblk)) {
          fprintf(stderr, "verify failed with %d remaining bytes of %d (blk %d)\n", bytes_left, imgsz, nblk);
          displayerr(dsp);
          break;
        }
        if (debug) write(STDERR_FILENO, ".", 1);
        nblk += DTOB(bcount);
    }
    printf("done\n");
  return 0;
}


/*
 * mkdisc
 *
 *  Description:
 *      Make a WORM disc using the Philips CDD 521 unit corresponding
 *      to the devscsi device dev.  The data on the disc comes from
 *      the file named by image.
 *
 *	NOTE: only the 'w' command (write) has had much testing/use/debugging
 *
 *  Parameters:
 *	cmd	    command type, [v]erify, [r]ead, [w]rite.
 *      dev         name of devscsi device to use.
 *      image       name of file containing image for WORM.
 *      retry       maximum number of times to retry commands.
 *      dsize       size of data stream.  This ignored unless image.
 *                  is NULL, in which case the data is read from stdin.
 *      verbose     if non-zero, debbugging output, 1 = verbose, 2 = full debug.
 *
 *  Returns:
 *      0 if successful, non-zero otherwise.
 */

mkdisc (char cmd, char *dev, char *image, int retry, int dsize, int verbose)
{
    dsreq_t *dsp;
    struct stat st;
    modesense_t mode;
    trkinfo_t trkinfo;
    discinfo_t discinfo;
    inquiry_t inquire;
    inquirylong_t inquirelong;
    u_char *dbuf;
    int delay = 1;
    int img, sz, syspagesz;
    int dbuflen = G0_BSIZE * CACHE_SIZE; /* 64KB (32 blocks) seems to work */
					 /* well with 1.02 firmware */

    if ((syspagesz = getpagesize()) <= 0)
	syspagesz = 0x1000;

    debug = verbose;
    if (debug == 2) dsdebug = 1;
    if (!(dsp = dsopen(dev, O_RDWR))) {
	perror(dev);
	return -1;
    }
    sig_dsp = dsp;  /* for signal handler */

    if (image) {
        img = open(image, (cmd == 'r') ? O_WRONLY : O_RDONLY);
        if (img == -1 || -1 == fstat(img, &st)) {
            perror(image);
            return -1;
        }
        if (st.st_rdev > -1) sz = (st.st_size/2)*1024;	/* block device */
        else sz = st.st_size;				/* plain file */
    }
    else {
        img = (cmd == 'r') ? STDOUT_FILENO : STDIN_FILENO;
        sz = dsize;
    }

    if (!(dbuf = (u_char *) malloc(dbuflen+(syspagesz-1)))) {
	fprintf(stderr, "Out of memory\n");
	return -1;
    }
    /* align to page boundary for slightly more efficient i/o
     * and cache flushing */
    dbuf += syspagesz - ((uint)dbuf&(syspagesz-1));

    /*
     * Retry because
     *  (1) the device could have been just turned on and
     *  (2) it's flaky and sometimes needs to be poked a few times
     */

	/****  skipping testunitready, other stuff works, but this fails! 3/96 
    while (retry-- && testunitready00(dsp) < 0) {
	printf("waiting for ready...\n");
	delay *= 2;
	if(delay > 8) delay = 8;
	sleep(delay);
    }
    if (retry <= 0) fprintf(stderr, "warning: WORM never became ready!\n");
	****/

    rewind01(dsp);
    stopstart(dsp, SPINUP);
    modesense(dsp, &mode, sizeof(mode));
    inquiry(dsp, &inquire, sizeof(inquire));
	fflush(stdout);	/* flush inquiry, etc. out, in case to file */
    switch (cmd) {
	case 'r': return(readcd(dsp, dbuf, img, sz, dbuflen)); break;
	case 'w': return(writecd(dsp, dbuf, img, sz, dbuflen)); break;
	case 'v': return(verifycd(dsp, dbuf, img, sz, dbuflen)); break;
	case 't': return(readtrkinfo(dsp, &trkinfo, sizeof(trkinfo))); break;
	case 'd': return(readdiscinfo(dsp, &discinfo, sizeof(discinfo))); break;
	default:  break;
    }
    close(img);
    dsclose(dsp);
    return 0;
}
