/* reads the disk */

#include <stdio.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/types.h>
#include <mntent.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/smfd.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/fs/efs.h>
#include <sys/sysmacros.h>
#include <sys/stropts.h>
#include <dslib.h>
#include <errno.h>

/* operation codes */
#define READ_TOC	0x43
#define READ_CUE	0xe9
#define READ_BLKLIM	0x05
#define MODE_SELECT	0x15
#define READ_START	0xe3
#define READ_CONTINUE	0xe4
#define READ_10		0x28
#define READ_SUBCH	0x42

#define STA_RETRY       (-1)

/* buffer */
#define BUFLEN 4096
char buf[BUFLEN];
#define BBUFLEN 65536
char bbuf[BBUFLEN];
struct dsreq ds;
struct dsreq *dsp = &ds;
static int verbose=0;
struct  cue_point
	{
	int	min;
	int	sec;
	int	frame;
	int	data_flag;
	} track_start[100], track_end[100], lead_out;
int numblocks, blksize, density;

/* get returned length */
#define	DATA_LENGTH4(b0,b1,b2,b3) ( (unsigned char)(b0)<<24 | (unsigned char)(b1)<<16 | (unsigned char)(b2)<<8 | (unsigned char)(b3))
#define	DATA_LENGTH2(b0,b1) ( (unsigned char)(b0)<<8 | (unsigned char)(b1))
#define bcd_to_i(x) ((((unsigned char)(x))>>4)*10 + ((unsigned char)(x)&0xf))
#define i_to_bcd(x) ((((x)/10) << 4) + ((x)%10))

#define BLKNUM(m,s,f) ( ((m)*60 + (s))*75 + (f))

#define ds_ctrlname "/dev/cdrom"

/* stolen from mediad */
int
set_blksize(dsreq_t *dsp, int density, int size)
{
	char    params[12];
	int     retries;

	retries = 10;
	while (retries--) {
		memset( params, '\0', sizeof (params) );
		params[3] = 0x08;
		params[4] = density;
		*(int *)&params[8] = size;

		modeselect15( dsp, params, sizeof (params), 0, 0 );

		if (STATUS(dsp) == STA_GOOD)
			return (1);
		else if (STATUS(dsp) == STA_CHECK || STATUS(dsp) == STA_RETRY)
			continue;
		else
			return (-1);
	}

	return (-1);
}

void
err_exit(char *s)
{
fprintf(stderr, "Error: %s\n",s);
exit(1);
}

void
usage(char *s)
{
fprintf(stderr, "Usage: %s ", s);
fprintf(stderr, "[v] [f outfile] [t] [track#]\n");
fprintf(stderr, "  v=verbose, t=toc only, track# must be specified if not toc only\n  track#=a means entire disk, the outfile is the prefix of the filename\n");
exit(1);
}

int
read_track(int track, int numblks, int blksize, char *outfile, int breakblocks)
{
FILE *fo;
int frames_left = numblks, blks_read=0;
int incr = BBUFLEN / blksize; /* at breakblocks we set incr to 1 */
struct  cue_point msf;
int dots_printed=0;

if (outfile) fo = fopen(outfile,"wb"); else fo = stdout;
if (fo == NULL) err_exit("cant create outfile");
msf = track_start[track];

for( ; frames_left > 0 ; frames_left -= incr, blks_read+=incr)
 {
 /* read 1 buffers worth */
 if (frames_left < incr) incr = frames_left;
 if (blks_read + incr > breakblocks) incr = breakblocks - blks_read;
 if (incr < 1 ) incr = 1;
 fillg1cmd( dsp, (uchar_t *) CMDBUF( dsp ), READ_10, 0,
  i_to_bcd(msf.min), i_to_bcd(msf.sec), i_to_bcd(msf.frame),
  0, 0, B2(incr), 0x40 );
 filldsreq(dsp, bbuf, BBUFLEN, DSRQ_READ|DSRQ_SENSE);
 TIME(dsp)=120 * 1000;
 doscsireq(getfd(dsp), dsp);
 if (STATUS( dsp ) != STA_GOOD) {
	if (blks_read >= breakblocks) break;
	else err_exit("cant read");
	}
 /* dump the buffer */
 if (incr * blksize != fwrite(bbuf, sizeof(char), incr * blksize, fo))
  {
  err_exit("Error writing output file\n");
  }
 /* incr time */
 msf.frame += incr;
 while (msf.frame >= 75) { msf.sec++; msf.frame-=75;}
 while (msf.sec >= 60) { msf.min++; msf.sec-=60;}
 if (verbose) {
	putc('.', stderr);
	dots_printed++;
	if (dots_printed==60) {
	  dots_printed=0;
	  fprintf(stderr,"[%d%%]\n",(int)((blks_read*100)/numblks));
	}
 }
 }
if (outfile)  fclose(fo);
return blks_read;
}

main(int argc, char **argv)
{
unsigned long i, j;
int n;
int track=0, last_track, blks_read, breakblocks;
int all_tracks_flag = 0;
int first_track_to_read, last_track_to_read;
char *outfile=NULL;
char current_outfile[256];
int toc_only_flag=0;
for (i=1; i< argc; i++)
 {
 switch(argv[i][0])
  {
  case 'v': verbose++; break;
  case '0': case '1': case '2': case '3': case '4':
  case '5': case '6': case '7': case '8': case '9':
	sscanf(argv[i], "%d", &track);
	break;
  case 'f':
	outfile = argv[++i]; break;
  case 't':
	toc_only_flag = 1; break;
  case 'a':
	all_tracks_flag = 1; track=1; break;
  default:
	usage(argv[0]);
  }
 }
if (toc_only_flag == 0 && track == 0) 
	usage(argv[0]);
dsp = dsopen(ds_ctrlname, O_RDONLY);
if (dsp==NULL) {
	err_exit("unable to open device - check if its busy and/or permissions");
	}
testunitready00(dsp);
if (STATUS( dsp ) != STA_GOOD) {
	err_exit("unit not ready");
	}
inquiry12(dsp, buf, 36L, 0);
if (STATUS( dsp ) != STA_GOOD) {
	err_exit("cant inquire");
	}
if (verbose) printf("device: %28s\n", buf+8);
if (strncmp(buf+8,"TOSHIBA", 7)==0)
 { }
else
	fprintf(stderr,"Not a TOSHIBA Cdrom drive - may not work\n");
/* read toc */
fillg1cmd(dsp,CMDBUF(dsp),READ_TOC,2,0,0,0,0,0,B2(BUFLEN),
	0);
filldsreq(dsp, buf, BUFLEN, DSRQ_READ|DSRQ_SENSE);
doscsireq(getfd(dsp), dsp);
if (STATUS( dsp ) != STA_GOOD) {
	err_exit("cant read toc");
	}
i = DATA_LENGTH2(buf[0], buf[1]);
if (verbose && toc_only_flag) printf("toc length %ld\n",i);
if (toc_only_flag) printf("1st track %d, last track %d\n", buf[2],buf[3]);
last_track= buf[3];
for (j=4; j<=i+4-8; j+=8)
	{
	if (verbose && toc_only_flag) printf("%x %x %x %x %x %x %x %x\n",buf[j],buf[j+1],buf[j+2],buf[j+3],buf[j+4],buf[j+5], buf[j+6], buf[j+7]);
	switch (buf[j+2])
		{
#if 0
		case 0xa0:
			if (toc_only_flag) printf("1st track # %x\n", buf[j+3]);
			break;
		case 0xa1:
			if (toc_only_flag) printf("Last track # %x, disk type: %s\n", buf[j+3],
			(((buf[j]>>4)&4)==4)?"DATA":"AUDIO");
			last_track= bcd_to_i(buf[j+3]);
			break;
#endif
		case 0xaa:
			if (toc_only_flag) printf("Lead out Min: %d  Sec: %d  Frame: %d\n",
			 buf[j+5],buf[j+6],buf[j+7]);
			lead_out.min= (buf[j+5]);
			lead_out.sec= (buf[j+6]);
			lead_out.frame = (buf[j+7]);
			break;
		default:
			if (toc_only_flag) printf("Track # %d, type: %s, Start Min: %d  Sec: %d  Frame: %d\n",
				buf[j+2], (((buf[j+1]>>0)&4)==4)?"DATA":"AUDIO",
				buf[j+5],buf[j+6],buf[j+7]);
			track_start[(buf[j+2])].min= (buf[j+5]);
			track_start[(buf[j+2])].sec= (buf[j+6]);
			track_start[(buf[j+2])].frame= (buf[j+7]);
			track_start[(buf[j+2])].data_flag = (((buf[j+1]>>0)&4)==4);
			break;
		}
	}
/* now attempt to read UPC */
fillg1cmd(dsp,CMDBUF(dsp),READ_SUBCH,2,0x40,2,0,0,0,B2(24),
	0);
filldsreq(dsp, buf, 24, DSRQ_READ|DSRQ_SENSE);
doscsireq(getfd(dsp), dsp);
if (STATUS( dsp ) != STA_GOOD) {
	err_exit("cant read upc");
	}
i = DATA_LENGTH2(buf[2], buf[3]);
if (verbose && toc_only_flag) printf("upc length %ld\n",i);
if (i == 20 && buf[4]==2)
 {
 if ((buf[8]&0x80) !=0) 
	{
	if (toc_only_flag)
	 {
	 int ii;
	 printf("UPC = [");
	 for(ii=9; ii<24; ii++) printf("%d",buf[ii]);
	 printf("]\n");
	 }
	}
 else
	{
	if (toc_only_flag) printf("no UPC code\n");
	}
 }
/* now print ISRC of each track */
for(n=1; n<= last_track; n++)
 {
 fillg1cmd(dsp,CMDBUF(dsp),READ_SUBCH,2,0x40,3,0,0,(unsigned char)n,B2(24),
	0);
 filldsreq(dsp, buf, 24, DSRQ_READ|DSRQ_SENSE);
 doscsireq(getfd(dsp), dsp);
 if (STATUS( dsp ) != STA_GOOD) {
	err_exit("cant read isrc");
	}
 i = DATA_LENGTH2(buf[2], buf[3]);
 if (verbose && toc_only_flag) printf("isrc %d length %ld\n",n,i);
 if (i == 20 && buf[4]==3)
  {
  if (verbose && toc_only_flag) printf("track %d  ADR %d CONTROL %d\n",buf[6], (buf[5]>>4) & 0xf, buf[5]&0xf);
  if ((buf[8]&0x80) !=0) 
	{
	if (toc_only_flag) printf("ISRC %d = [%15s]\n",n,buf + 9);
	}
  else
	{
	if (toc_only_flag && verbose) printf("Track %d has no ISRC code\n", n);
	}
  }
 }

/* calculate end points */
for(i=1; i<last_track; i++)
	 {
	 track_end[i] = track_start[i+1];
	 }
track_end[last_track]=lead_out;

if (toc_only_flag) exit(0);

if (all_tracks_flag) {
 first_track_to_read = 1;
 last_track_to_read = last_track;
} else {
 first_track_to_read = track;
 last_track_to_read = track;
}
for (track = first_track_to_read; track <=last_track_to_read; track++) {
 /* start read at beginning of track */
 /* read till end of the track */
 numblocks = track_end[track].min - track_start[track].min;
 numblocks = numblocks * 60 + track_end[track].sec - track_start[track].sec;
 numblocks = numblocks * 75 + track_end[track].frame - track_start[track].frame;
 breakblocks = numblocks;
 blksize = (track_start[track].data_flag ? 2048 : 2352);
 density = (track_start[track].data_flag ? 0x00 : 0x82);
 if (track< last_track && track_start[track].data_flag && !track_start[track+1].data_flag)
	{
	if (verbose)
		{
		fprintf(stderr, "Warning: read may be short due to data -> audio postgap\n");
		}
	breakblocks = numblocks - (3 * 75); /* up to 3 second postgap */
	}
 if (verbose)
	{
	fprintf(stderr,"Attempting to read %d blocks of %s (%d bytes) at track %d\n starting at min: %d sec: %d frame: %d\n",
		numblocks, (track_start[track].data_flag ? "data" : "audio"),
		numblocks * blksize, track,
		track_start[track].min, track_start[track].sec,
		track_start[track].frame);
	}
/* set the density and blksize for the particular track type */
if (set_blksize( dsp, density, blksize) != 1) 
	err_exit("Couldnt set blocksize and density");

if (all_tracks_flag) {
	sprintf(current_outfile,"%s%02d",outfile,track);
} else {
	strcpy(current_outfile, outfile);
}
 
/* now read and dump */
blks_read = read_track(track,numblocks, blksize, current_outfile, breakblocks);
if (verbose && blks_read != numblocks) fprintf(stderr, "Warning: only read %d blocks\n", blks_read);
} /* for */

dsclose(dsp);
}
