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

/* buffer */
#define BUFLEN 2048
char buf[BUFLEN];
struct dsreq ds;
struct dsreq *dsp = &ds;
static int verbose=0;
static int flag_900=0;	/* sony CDW-900E */

/* get returned length */
#define	DATA_LENGTH(b0,b1,b2,b3) ( (unsigned char)(b0)<<24 | (unsigned char)(b1)<<16 | (unsigned char)(b2)<<8 | (unsigned char)(b3))

#define ds_ctrlname "/dev/scsi/sc0d7l0"

err_exit(char *s)
{
fprintf(stderr, "Error: %s\n",s);
exit(1);
}


main(int argc, char **argv)
{
unsigned long i, j;
if (argc>1 && argv[1][0]=='v') verbose++;
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
fillg1cmd(dsp,CMDBUF(dsp),READ_TOC,0,0,0,0,0,0,get_b1(BUFLEN), get_b0(BUFLEN),
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
			break;
		case 0xa2:
			printf("Lead out Min: %x  Sec: %x  Frame: %x\n",
			 buf[j+3],buf[j+4],buf[j+5]);
			break;
		default:
			printf("Track # %x, type: %s, Start Min: %x  Sec: %x  Frame: %x\n",
				buf[j+1], (((buf[j]>>4)&4)==4)?"DATA":"AUDIO",
				buf[j+3],buf[j+4],buf[j+5]);
			break;
		}
			
	}
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
