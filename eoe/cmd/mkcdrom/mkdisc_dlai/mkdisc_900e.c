/*
 * mkdisc.c
 *
 * Make a WORM disc using a Sony CDW-E1 or CDW-900E encoding unit
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <dslib.h>
#include <fcntl.h>
#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <signal.h>

#define CDROM_BLOCKSIZE 2048
#define WSTART 0xe0
#define WCONT 0xe1
#define RBLIMIT 0x05
#define DISCONTINUE 0xe2
#define MODE_SELECT     0x15


#define FIXPROTO (uchar_t *)

#define INTTOTWODIGITBCD( i )      ((((i) / 10) << 4) | ((i) % 10))
#define TWODIGITBCDTOINT( x )	(((x)/16)*10 + ((x)%16))
#define CDEW1_SENSELEN 22
#define PRINTSENSE(dsp) { \
    int i;\
    if (STATUS(dsp) == STA_CHECK) {\
	for (i = 0; i <= CDEW1_SENSELEN; i++)\
	    fprintf(stderr, "%02x ", SENSEBUF(dsp)[i]);\
	fprintf(stderr, "\n");\
    }}
static int flag_900=0;  /* sony CDW-900E */

/*
 * The numbers here need to be in BCD
 */
typedef struct {
    unsigned char control, tno, index, dform, zero, min, sec, frame;
} ind_t;

/*
 * Here we make very limited use of the capabilities of the CDW-E1.
 * It can lay down up to 99 tracks.  The purpose of this program
 * is to transfer filesystems to disc, so it only writes one track
 * per disc.
 */
typedef struct {
    ind_t leadin, gap, data, leadout;
} default_cuesheet_t;

/*
 * Store dsp in here so signal handler can see it if needed
 */
static dsreq_t *sig_dsp;

/*
 * Send the cue sheet to the writer
 *
 * Note that this assumes cue sheet is less than 64K
 */
static void
write_start(dsreq_t *dsp, void *cue, size_t sz)
{
    fillg1cmd(dsp, FIXPROTO CMDBUF(dsp), WSTART,
     0x10, B2(sz), 0, 0, 0, 0, 0, 0x00);
    filldsreq(dsp, (unsigned char *)cue, sz, DSRQ_WRITE | DSRQ_SENSE);
    TIME(dsp) = 120 * 1000; /* Set 2 minute timeout */
    doscsireq(getfd(dsp), dsp);
}

/*
 * A version of read that works the same for std input as files
 */
static int
read_data(int fd, void *buf, int n)
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
 * Tell the writer to stop
 */
static void
abort_writing(dsreq_t *dsp)
{
    fillg1cmd(dsp, FIXPROTO CMDBUF(dsp), DISCONTINUE,
     0, 0, 0, 0, 0, 0, 0, 0, 0);
    doscsireq(getfd(dsp), dsp);
}

/*
 * Catch signals; abort writing
 */

static void
sig_abort(int sig)
{
    abort_writing(sig_dsp);
    exit(sig);
}

/*
 * Send data to the writer
 */
static void
write_continue(dsreq_t *dsp, unsigned char *dbuf, int count)
{
    fillg1cmd(dsp, FIXPROTO CMDBUF(dsp), WCONT,
     B3(count), 0, 0, 0, 0, 0, 0);
    filldsreq(dsp, dbuf, count, DSRQ_WRITE | DSRQ_SENSE );
    TIME(dsp) = 7200 * 1000; /* Set 120 minute timeout */
    doscsireq(getfd(dsp), dsp);
}

/*
 *  int
 *  mkdisc(char *dev, char *image)
 *
 *  Description:
 *      Make a WORM disc using the Sony CDW-E1 ecoding unit corresponding
 *      to the devscsi device dev.  The data on the disc comes from
 *      the file named by image.
 *
 *  Parameters:
 *      dev         name of devscsi device to use
 *      image       name of file containing image for WORM
 *      max_retries maximum number of times to retry commands
 *      data_size   size of data stream.  This ignored unless image
 *                  is NULL, in which case the data is read from stdin.
 *      verbose     if non-zero, more debbugging output
 *
 *  Returns:
 *      0 if successful, non-zero otherwise
 */
int
mkdisc(char *dev, char *image, int max_retries, int data_size, int verbose,
	int pretend_flag, char *cuesheet_file, int audio_flag)
{
    dsreq_t *dsp;
    int img;
    default_cuesheet_t default_cuesheet;
    unsigned char *user_cuesheet;
    size_t user_cuesheet_size;
    struct stat st;
    int frames, min, sec, frame, blocks; 
    int bytes_left, count, dbuflen, slop=0, sz;
    unsigned char *dbuf;
    char maxblklen[6];
#define BUFLEN 2048
    char buf[BUFLEN];
    int dotcount, dot_total;

    dsp = dsopen(dev, O_RDWR);

    if (!dsp) {
	perror(dev);
	return 1;
    }

    /*
     * Retry because
     *  (1) the device could have been just turned on and
     *  (2) it's flaky and sometimes needs to be poked a few times
     */
    do {
	testunitready00(dsp);
	if (verbose && STATUS(dsp) != STA_GOOD) {
	    fprintf(stderr, "Device not ready\n");
	    PRINTSENSE(dsp);
	}
    } while (max_retries-- && STATUS(dsp) == STA_CHECK);

    /* see if it is a CDW-900E, if so we will set doublespeed write */
    inquiry12(dsp, buf, 36L, 0);
    if (STATUS( dsp ) != STA_GOOD) {
	err_exit("cant inquire");
	}
    if (verbose) printf("device: %28s\n", buf+8);
    if (strncmp(buf+16,"CDW-900E",8)==0) flag_900=1;

    if (flag_900)
     {
     /* set double speed read and writing */
 	fillg0cmd(dsp,CMDBUF(dsp),MODE_SELECT,0x10,0,0,12 ,0);
 	buf[0]=buf[1]=buf[2]=buf[3]=0;
 	buf[4]=0x21;
 	buf[5]=6;
 	buf[6]=0;
 	buf[7]=1;
 	buf[8]=buf[9]=buf[10]=buf[11]=0;
 	filldsreq(dsp, buf, BUFLEN, DSRQ_WRITE|DSRQ_SENSE);
 	doscsireq(getfd(dsp), dsp);
 	if (STATUS( dsp ) != STA_GOOD) {
		err_exit("cant set double speed read");
		}
 	fillg0cmd(dsp,CMDBUF(dsp),MODE_SELECT,0x10,0,0,12 ,0);
 	buf[0]=buf[1]=buf[2]=buf[3]=0;
 	buf[4]=0x20;
 	buf[5]=6;
 	buf[6]=0;
 	buf[7]=(pretend_flag?3:1);
 	buf[8]=buf[9]=buf[10]=buf[11]=0;
 	filldsreq(dsp, buf, BUFLEN, DSRQ_WRITE|DSRQ_SENSE);
 	doscsireq(getfd(dsp), dsp);
 	if (STATUS( dsp ) != STA_GOOD) {
		err_exit("cant set double speed write");
		}
     }

    /*
     * Figure out the maximum transfer length for WCONT and malloc
     * a buffer of that size
     */
    fillg0cmd(dsp, FIXPROTO CMDBUF(dsp), RBLIMIT, 0, 0, 0, 0, 0);
    filldsreq(dsp, (unsigned char *)maxblklen, sizeof maxblklen,
     DSRQ_READ | DSRQ_SENSE);
    doscsireq(getfd(dsp), dsp);

    dbuflen = (maxblklen[1] << 16 | maxblklen[2] << 8 | maxblklen[3]);

    dbuf = malloc(dbuflen);
    if (!dbuf) {
	fprintf(stderr, "Out of memory\n");
	return 1;
    }

    if (image) {
	img = open(image, O_RDONLY);

	if (img == -1 || -1 == fstat(img, &st)) {
	    perror(image);
	    return 1;
	}
	sz = st.st_size;
    }
    else {
	img = STDIN_FILENO;
	sz = data_size;
    }

if (!cuesheet_file) {
    /*
     * The lead in track
     */
    default_cuesheet.leadin.control = 0x41;
    default_cuesheet.leadin.tno = 0;
    default_cuesheet.leadin.index = 0;
    default_cuesheet.leadin.dform = 0;
    default_cuesheet.leadin.zero = 0;
    default_cuesheet.leadin.min = 0;
    default_cuesheet.leadin.sec = 0;
    default_cuesheet.leadin.frame = 0;

    /*
     * Gap between lead in track and first track (actually part
     * of the first track)
     */
    default_cuesheet.gap.control = 0x41;
    default_cuesheet.gap.tno = 1;
    default_cuesheet.gap.index = 0;
    default_cuesheet.gap.dform = 0x10;
    default_cuesheet.gap.zero = 0;
    default_cuesheet.gap.min = 0;
    default_cuesheet.gap.sec = 0;
    default_cuesheet.gap.frame = 0;

    /*
     * The data
     */
    default_cuesheet.data.control = 0x41; /* 0x61 == Data track, copy permitted */
    default_cuesheet.data.tno = 1;
    default_cuesheet.data.index = 1;
    default_cuesheet.data.dform = 0x11; /* 0x11 == generate header/ECC/EDC,
				   use data we send */
    default_cuesheet.data.zero = 0;
    default_cuesheet.data.min = 0;
    default_cuesheet.data.sec = 2;
    default_cuesheet.data.frame = 0;

    /*
     * Calculate the frame offset of the lead out track.  This will
     * be the number of frames in the data area + 150 frames for
     * the 2 second pause at the beginning of the data track
     * (There are 75 frames/second).
     */
    frames = (sz + CDROM_BLOCKSIZE - 1) / CDROM_BLOCKSIZE + 150;

    /*
     * The lead out track
     */
    default_cuesheet.leadout.control = 0x41;
    default_cuesheet.leadout.tno = 0xaa;
    default_cuesheet.leadout.index = 1;
    default_cuesheet.leadout.dform = 0;
    default_cuesheet.leadout.zero = 0;
    min = (frames / 75) / 60;
    default_cuesheet.leadout.min = INTTOTWODIGITBCD(min);
    sec = (frames / 75) % 60;
    default_cuesheet.leadout.sec = INTTOTWODIGITBCD(sec);
    frame = frames % 75;
    default_cuesheet.leadout.frame = INTTOTWODIGITBCD(frame);
    }
else
    {
	/* read the cuesheet_file for the cuesheet */
	struct stat statbuf;
	FILE *f;
	if (stat(cuesheet_file, &statbuf))
	 {
	 err_exit("cant read cuesheet");
	 }
	user_cuesheet=malloc(statbuf.st_size);
	if (( f=fopen(cuesheet_file,"rb")) ==NULL) err_exit("cant open cuesheet");
	if ((user_cuesheet_size = fread(user_cuesheet, 1, statbuf.st_size, f)) != statbuf.st_size)
	 err_exit("cant read cuesheet");
	fclose(f);
	/* get the leadout time */
	if (user_cuesheet[user_cuesheet_size-7]!=0xaa) {
	 err_exit("cuesheet file has bad lead-out");
	}
	min = TWODIGITBCDTOINT(user_cuesheet[user_cuesheet_size-3]);
	sec = TWODIGITBCDTOINT(user_cuesheet[user_cuesheet_size-2]);
	frame = TWODIGITBCDTOINT(user_cuesheet[user_cuesheet_size-1]);
    }

    sig_dsp = dsp;
    signal(SIGINT, sig_abort);
    signal(SIGHUP, sig_abort);
    signal(SIGTERM, sig_abort);

    /*
     * Send the cue sheet
     *
     * Retry because the device is flaky
     */
    do {
	if (cuesheet_file) 
	 {
	 write_start(dsp, user_cuesheet, user_cuesheet_size);
	 }
	else
	 {
	 write_start(dsp, &default_cuesheet, sizeof default_cuesheet);
	 }
	if (verbose && STATUS(dsp) == STA_CHECK) {
	    PRINTSENSE(dsp);
	    fprintf(stderr, "Not ready -- retrying\n");
	}
    } while (max_retries-- && STATUS(dsp) == STA_CHECK);

    if (STATUS(dsp) != STA_GOOD) {
	fprintf(stderr, "Error sending cue sheet\n");
	if (verbose)
	    PRINTSENSE(dsp);
	return 1;
    }

    if (!audio_flag) {
     /*
     * We need to add this much at the end to make
     * sure that the amount of data we send is a multiple
     * of CDROM_BLOCKSIZE; otherwise, the writer will think
     * the last frame is only part full.
     */
     slop = (CDROM_BLOCKSIZE - sz % CDROM_BLOCKSIZE) % CDROM_BLOCKSIZE;
    }
    bytes_left = sz;

    printf("%02d:%02d:%02d of %s to disc\n", min, sec, frame, audio_flag?"audio":"data");
    dot_total=(bytes_left + CDROM_BLOCKSIZE - 1) / dbuflen;
    printf("%d dots until disc is done:\n", dot_total);

    dotcount = 0;
    while (bytes_left) {
	putchar('.');
	fflush(stdout);
	dotcount++;
	if (dotcount%50==0) printf("[%d%s]\n", dotcount*100/dot_total, "%");
	count = bytes_left > dbuflen ? dbuflen : bytes_left;

	/* Unrecoverable: this trashes the disc */
	if (count != read_data(img, dbuf, count)) {
	    perror(image);
	    abort_writing(dsp);
	    return 1;
	}

	bytes_left -= count;

	if (!bytes_left)
	    bzero(dbuf + count, CDROM_BLOCKSIZE - count % CDROM_BLOCKSIZE);
	
	write_continue(dsp, dbuf, count);

	if (!bytes_left && STATUS(dsp) == STA_CHECK &&
	 SENSEKEY(SENSEBUF(dsp)) == 0 &&
	 ADDSENSECODE(SENSEBUF(dsp)) == 0x80)
	    printf("\nDisc complete");
	else if (STATUS(dsp) != STA_GOOD) {
	    fprintf(stderr, "Error sending data\n");
	    if (verbose)
		PRINTSENSE(dsp);
	    abort_writing(dsp);
	    return 1;
	}
    }

    /*
     * Add some 0 bytes at the end to make it an even multiple
     * of blocksize
     */
    if (slop) {
	unsigned char buf[CDROM_BLOCKSIZE];

	bzero(buf, sizeof buf);
	write_continue(dsp, buf, slop);

	if (STATUS(dsp) == STA_CHECK &&
	 SENSEKEY(SENSEBUF(dsp)) == 0 &&
	 ADDSENSECODE(SENSEBUF(dsp)) == 0x80)
	    printf("\nDisc complete");
	else {
	    fprintf(stderr, "Error sending data\n");
	    if (verbose)
		PRINTSENSE(dsp);
	    return 1;
	}
    }

    putchar('\n');
    return 0;
}
