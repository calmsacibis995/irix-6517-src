
#include <stdio.h>
#include <sys/param.h>  /* for types.h and BBSIZE */
#include <sys/dkio.h>

#include "mkfp.h"
#include "macLibrary.h"

extern	int	debug_flag;

#define DEV_BSIZE    BBSIZE
#define VP(x)        ((struct volume_header *)x)
#define DP(x)        (&VP(x)->vh_dp)


/*  unfortunately, the time to format isn't a linear function of
    anything in particular; this is purely empirical.  We print
    the length of time because people used to ESDI format often
    think the drive is hung, since nothing is printed.  Then they
    reset the system, and of course the drive is hosed until they
    re-format it, usually after calling the hotline.
    Now that we support different block sizes, adjust for that also.
*/

int
do_scsiformat(FPINFO * fpinfop)
{
    int retval = E_NONE; 
    unsigned blksize=0, minit;

    if(scsi_readcapacity(fpinfop, &blksize) == -1 || !blksize) {
        /* make a reasonable guess... Happens sometimes when
           the drive was only partially formatted before being
           reset (or an earlier format failed) for some reason.  */
        minit = 15;
        goto doit;
    }

    /* allow for different block sizes when computing time.
     * we are real close to overflowing 32 bits if we
     * aren't careful, so divide first. */
    blksize = (blksize/DEV_BSIZE) * DP(&fpinfop->vh)->dp_secbytes;

    if(blksize < 1000)    /* 48 tpi floppies */
        minit = 1;
    else if(blksize < 3000)    /* 96 tpi floppies and most 3.5" 1:30 */
        minit = 2;
    else if(blksize < 41000) {   /* floptical drives 22:50 */
        minit = (int) ((41000.0 / 3000.0) * 1.5 + 0.9);
    }

doit:
    fprintf(stderr, " mkfp: Format will take approximately %u minute%s ...\n",
                                             minit, minit>1 ? "s" : "");

    if (ioctl(fpinfop->devfd, DIOCFORMAT, 0) < 0)
        retval = E_FORMAT;

    return(retval);
}



scsi_readcapacity(FPINFO * fpinfop, unsigned *addr)
{
    if (ioctl(fpinfop->devfd, DIOCREADCAPACITY, addr) < 0)
        return -1;

    if (debug_flag)
	printf("scsi.c:scsi_readcapacity: capacity = %d\n", *addr);

    return 0;
}


/* format the floppy */
int
do_smfdformat(FPINFO * fpinfop)
{
    return(do_scsiformat(fpinfop));
}

