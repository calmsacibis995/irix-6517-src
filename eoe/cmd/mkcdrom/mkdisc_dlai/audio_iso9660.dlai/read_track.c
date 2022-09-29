/* reads the TOC */

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
#define READ_TOC	0xe7
#define READ_CUE	0xe9
#define READ_BLKLIM	0x05
#define MODE_SELECT	0x15
#define READ_START	0xe3
#define READ_CONTINUE	0xe4

/* buffer */
#define BUFLEN 4096
char buf[BUFLEN];
#define BBUFLEN 524288
char bbuf[BBUFLEN];
struct dsreq ds;
struct dsreq *dsp = &ds;
static int verbose=0;
static int flag_900=0;	/* sony CDW-900E */
static int data_disk=0;	/* a data disk */
struct  cue_point
	{
	int	min;
	int	sec;
	int	frame;
	} track_start[100], track_end[100], lead_out;

/* get returned length */
#define	DATA_LENGTH(b0,b1,b2,b3) ( (unsigned char)(b0)<<24 | (unsigned char)(b1)<<16 | (unsigned char)(b2)<<8 | (unsigned char)(b3))
#define bcd_to_i(x) ((((unsigned char)x)>>4)*10 + x&0xf)

#define ds_ctrlname "/dev/scsi/sc0d7l0"

err_exit(char *s)
{
fprintf(stderr, "Error: %s\n",s);
exit(1);
}


main(int argc, char **argv)
{
unsigned long i, j;
int track=1, last_track;
FILE *fo;
for (i=1; i< argc; i++)
 {
 switch(argv[i][0])
  {
  case 'v': verbose++; break;
  case '0': case '1': case '2': case '3': case '4':
  case '5': case '6': case '7': case '8': case '9':
	sscanf("%d", argv[i], &track);
	break;
  default:
	err_exit("Usage: read_track [v] track#");
  }
 }
dsp = dsopen(ds_ctrlname, O_RDONLY);
testunitready00(dsp);
if (STATUS( dsp ) != STA_GOOD) {
	err_exit("unit not ready");
	}
inquiry12(dsp, buf, 36L, 0);
if (STATUS( dsp ) != STA_GOOD) {
	err_exit("cant inquire");
	}
printf("device: %28s\n", buf+8);
if (strncmp(buf+16,"CDW-900E",8)==0) flag_900=1;
/* get read block limits */
fillg0cmd(dsp,CMDBUF(dsp),READ_BLKLIM, 0,0,0,0,0);
filldsreq(dsp, buf, BUFLEN, DSRQ_READ|DSRQ_SENSE);
doscsireq(getfd(dsp), dsp);
if (STATUS( dsp ) != STA_GOOD) {
	err_exit("cant read block limits");
	}
i = DATA_LENGTH(0, buf[1],buf[2],buf[3]);
j= DATA_LENGTH(0, 0, buf[4],buf[5]);
printf("read block length min: %d  max: %d\n", j, i);
/* read toc */
fillg1cmd(dsp,CMDBUF(dsp),READ_TOC,0,0,0,0,0,0,B2(BUFLEN),
	0);
filldsreq(dsp, buf, BUFLEN, DSRQ_READ|DSRQ_SENSE);
doscsireq(getfd(dsp), dsp);
if (STATUS( dsp ) != STA_GOOD) {
	err_exit("cant read toc");
	}
i = DATA_LENGTH(buf[0], buf[1],buf[2],buf[3]);
if (verbose) printf("toc length %ld\n",i);
for (j=4; j<i+4; j+=6)
	{
	if (verbose) printf("%x %x %x %x %x %x\n",buf[j],buf[j+1],buf[j+2],buf[j+3],buf[j+4],buf[j+5]);
	switch (buf[j+1])
		{
		case 0xa0:
			printf("1st track # %x\n", buf[j+3]);
			break;
		case 0xa1:
			printf("Last track # %x, disk type: %s\n", buf[j+3],
			(((buf[j]>>4)&4)==4)?"DATA":"AUDIO");
			last_track= bcd_to_i(buf[j+3]);
			data_disk=(((buf[j]>>4)&4)==4);
			break;
		case 0xa2:
			printf("Lead out Min: %x  Sec: %x  Frame: %x\n",
			 buf[j+3],buf[j+4],buf[j+5]);
			lead_out.min= bcd_to_i(buf[j+3]);
			lead_out.sec= bcd_to_i(buf[j+4]);
			lead_out.frame = bcd_to_i(buf[j+5]);
			break;
		default:
			printf("Track # %x, type: %s, Start Min: %x  Sec: %x  Frame: %x\n",
				buf[j+1], (((buf[j]>>4)&4)==4)?"DATA":"AUDIO",
				buf[j+3],buf[j+4],buf[j+5]);
			track_start[bcd_to_i(buf[j+1])].min= bcd_to_i(buf[j+3]);
			track_start[bcd_to_i(buf[j+1])].sec= bcd_to_i(buf[j+4]);
			track_start[bcd_to_i(buf[j+1])].frame= bcd_to_i(buf[j+5]);

			break;
		}
	}
/* calculate end points */
for(i=1; i<last_track; i++)
	 {
	 track_end[i] = track_start[i+1];
	 }
track_end[last_track]=lead_out;

#if 0
/* now set up to read at double speed */
if (flag_900)
 {
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
	err_exit("cant set double speed");
	}
 }
#endif
 /* start read at beginning of track */
 if (verbose)
	{
	fprintf(stderr,"Attempting read %s starting at min: %d sec: %d frame: %d\n",
		data_disk?"data":"audio", track_start[track].min, track_start[track].sec,
		track_start[track].frame);
	}
#if 0
 fillg1cmd(dsp,CMDBUF(dsp),READ_START,0,0,data_disk?1:1,
	track_start[track].min, track_start[track].sec,
	track_start[track].frame, 0, 0,0);
#else
 fillg1cmd(dsp,CMDBUF(dsp),READ_START,0,0,data_disk?2:1,
	0, 0,
	0, 0, 0,0);
#endif
 filldsreq(dsp, buf, 0, DSRQ_READ|DSRQ_SENSE);
 TIME(dsp)=120 * 1000;
 doscsireq(getfd(dsp), dsp);
 if (STATUS( dsp ) != STA_GOOD) {
	err_exit("cant read start");
	}
 /* read 1 buffers worth */
 fillg1cmd(dsp,CMDBUF(dsp),READ_CONTINUE,B3(BBUFLEN),
	0,0,0,0,0,0);
 filldsreq(dsp, bbuf, BBUFLEN, DSRQ_READ|DSRQ_SENSE);
 doscsireq(getfd(dsp), dsp);
 if (STATUS( dsp ) != STA_GOOD) {
	err_exit("cant read continue");
	}
 /* dump the buffer */
 fo = fopen("out","wb");
 fwrite(bbuf, sizeof(char), BBUFLEN, fo);
 fclose(fo);
/* read cue */
#if 0
if (flag_900)
 {
	/* disk info, sheet 0 */
 fillg1cmd(dsp,CMDBUF(dsp),READ_CUE,0,0,0,0,0,0,get_b1(BUFLEN), get_b0(BUFLEN), 
	0);
 filldsreq(dsp, buf, BUFLEN, DSRQ_READ|DSRQ_SENSE);
 doscsireq(getfd(dsp), dsp);
 if (STATUS( dsp ) != STA_GOOD) {
	err_exit("cant read cue sheet 0");
	}
 i = DATA_LENGTH(buf[4], buf[5],buf[6],buf[7]);
 if (verbose) printf("cue sheet 0 length %ld\n",i);
for (j=8; j<i+8; j+=8)
	{
	if (verbose) printf("%x %x %x %x %x %x %d %d\n",buf[j],buf[j+1],buf[j+2],buf[j+3],buf[j+4],buf[j+5], buf[j+6],buf[j+7]);
	}
	/* master cue, sheet 1 */
 fillg1cmd(dsp,CMDBUF(dsp),READ_CUE,0,0,0,1,0,0,get_b1(BUFLEN), get_b0(BUFLEN), 
	0);
 filldsreq(dsp, buf, BUFLEN, DSRQ_READ|DSRQ_SENSE);
 doscsireq(getfd(dsp), dsp);
 if (STATUS( dsp ) != STA_GOOD) {
	err_exit("cant read cue sheet 1");
	}
 i = DATA_LENGTH(buf[4], buf[5],buf[6],buf[7]);
 if (verbose) printf("cue sheet 1 length %ld\n",i);
for (j=8; j<i+8; j+=8)
	{
	if (verbose) printf("%x %x %x %x %x %x %d %d\n",buf[j],buf[j+1],buf[j+2],buf[j+3],buf[j+4],buf[j+5], buf[j+6],buf[j+7]);
	}
 }
#endif

dsclose(dsp);
}
