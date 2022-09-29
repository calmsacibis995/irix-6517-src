/* does a scsi inquiry */

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

/* buffer */
#define BUFLEN 4096
char buf[BUFLEN];
#define BBUFLEN 524288
char bbuf[BBUFLEN];
struct dsreq ds;
struct dsreq *dsp = &ds;
static int verbose=0;

/* get returned length */
#define	DATA_LENGTH(b0,b1,b2,b3) ( (unsigned char)(b0)<<24 | (unsigned char)(b1)<<16 | (unsigned char)(b2)<<8 | (unsigned char)(b3))
#define bcd_to_i(x) ((((unsigned char)x)>>4)*10 + x&0xf)

char *ds_ctrlname = "/dev/scsi/sc0d7l0";

err_exit(char *s, char *a)
{
fprintf(stderr, "Error: %s %s\n",s, a);
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
  case '/': ds_ctrlname = argv[i]; break;
  default:
	err_exit("Usage: inquiry [v] /dev/scsi/xxx", "");
  }
 }
dsp = dsopen(ds_ctrlname, O_RDONLY);
if (dsp==NULL) { err_exit("cant open device", ds_ctrlname); }
testunitready00(dsp);
if (STATUS( dsp ) != STA_GOOD) {
	err_exit("unit not ready", ds_ctrlname);
	}
inquiry12(dsp, buf, 36L, 0);
if (STATUS( dsp ) != STA_GOOD) {
	err_exit("cant inquire", ds_ctrlname);
	}
printf("device: %s is %28s\n", ds_ctrlname, buf+8);

dsclose(dsp);
}
