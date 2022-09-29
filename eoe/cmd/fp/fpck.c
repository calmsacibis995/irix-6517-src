/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1993 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.18 $"

/*
 * fpck.c --
 *
 * floppy disk FAT(DOS) and HFS(MAC) file system checker.
 *
 */

#include <bstring.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <ctype.h> 
#include <fcntl.h> 
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/buf.h>
#include <sys/iobuf.h>
#include <sys/mkdev.h>
#include <sys/stat.h> 
#include <sys/sema.h>
#include <sys/scsi.h>
#include <sys/dkio.h>

#include "smfd.h" 
#include "fpck.h"
#include "dosfs.h"
#include "macLibrary.h"
#include <sys/dksc.h>

FILE  *logfd = NULL;

int	debug_flag   = 0;
int	verbose_flag = 1;

extern char *smfd_partname(int);
extern void badsig(int, int, struct sigcontext *);
extern int valid_vh(struct volume_header *); /* defined in libdisk */
extern int vhchksum(struct volume_header *); /* defined in libdisk */
extern int remmedinfo(int fd, uint *status);

static void gothup(int);
static void devsetup(FPINFO *);
static int  checkarg(FPINFO *, int, char **);
static void vlabelinspect(FPINFO *, char *);
static BOOL (* vlabelchkfunc[MAX_TYPE])(FPINFO *, char *);
static void init_labelfunc(void);
void exit_func(void);


static  int  correctflag = 0;
static  u_int partition = 0;

/* ----- fpck globals ----- */

char *signames[] = {
    "",        /* place holder */
    "HUP",     /* hangup */
    "INT",     /* interrupt (rubout) */
    "QUIT",    /* quit (ASCII FS) */
    "ILL",     /* illegal instruction (not reset when caught)*/
    "TRAP",    /* trace trap (not reset when caught) */
    "IOT",     /* IOT instruction */
    "EMT",     /* EMT instruction */
    "FPE",     /* floating point exception */
    "KILL",    /* kill (cannot be caught or ignored) */
    "BUS",     /* bus error */
    "SEGV",    /* segmentation violation */
    "SYS",     /* bad argument to system call */
    "PIPE",    /* write on a pipe with no one to read it */
    "ALRM",    /* alarm clock */
    "TERM",    /* software termination signal from kill */
    "USR1",    /* user defined signal 1 */
    "USR2",    /* user defined signal 2 */
    "CLD",     /* death of a child */
    "PWR",     /* power-fail restart */
    "STOP",    /* sendable stop signal not from tty */
    "TSTP",    /* stop signal from tty */
    "POLL",    /* pollable event occured */
    "IO",      /* input/output possible signal */
    "URG",     /* urgent condition on IO channel */
    "WINCH",   /* window size changes */
    "VTALRM",  /* virtual time alarm */
    "PROF",    /* profiling alarm */
    "CONT",    /* continue a stopped process */
    "TTIN",    /* to readers pgrp upon background tty read */
    "TTOU",    /* like TTIN for output if (tp->t_local&LTOSTOP) */
};



main(int argc, char ** argv)
{
    extern char *progname;

    FPINFO fpinfo;
    struct DsDevice *hfs_fd;
    int retval = E_NONE;
    int partitioned = 0;

    progname = "fpck";

    memset(&fpinfo, '\0', sizeof(FPINFO));

    if (checkarg(&fpinfo, argc, argv) != E_NONE)
        exit(1);

    devsetup(&fpinfo);

    if (fpinfo.filesystype == MAC_HFS) {

        /* create a fake DsDevice to
            please the DIT mac lib codes */
        hfs_fd = (struct DsDevice *) newDsDevice(&fpinfo);
        retval = hfsCk((void *) hfs_fd, correctflag);
        freeDsDevice ((void **) &hfs_fd);

    } else if (fpinfo.filesystype == DOS_FAT) {
        dfs_t        dosfs;

	if (fatChkvh(&fpinfo) == E_NONE)
		partitioned = 0;
	else
		partitioned = 1;

	if (partitioned == 0) {
		if (partition) {
			fpWarn(
    "Partition set to %d, & the media appears to be non-partitioned!",
								    partition);
		}
	} else {

	    if (partition == 0) {
		partition = 1;

		fpWarn("the media appears partitioned, assuming partition 1.");
	    }
	}

        retval = fatGetvh(&fpinfo, partition, &dosfs);

	if (debug_flag)
	    fpMessage("fpck.c:main: fatGetvh returned %d", retval);	

        if(retval == E_NONE)
            BeginScanLoop(&dosfs, correctflag);
        else if (retval == E_READ)
            fpError("can't read DOS file system information, the disk is not safe to use!");
        else if (retval == E_UNKNOWN) {

            fpError("Unknown file system type");

	} else if (retval == E_RANGE) {

	    fpError("Illegal partition # %d.", partition);

	    if (partitioned) {
		/*
		 * Dump the "known" partition information.
		 */
		fpPart_read_disk(&fpinfo);
	    }
	}

	if (   partitioned
	    && (debug_flag || retval == E_NOTSUPPORTED)) {
	    /*
	     * Dump the "known" partition information.
	     */
	    fpPart_read_disk(&fpinfo);
	}
    }

    return 0;
}    



static int
checkarg(FPINFO * fpinfop, int xargc, char ** xargv)
{
    extern  int  optind;
    extern  char *optarg;
    extern  void getoptreset(void);

    char * ftypep;
    int    c;
    int    lfd;
    int    devlen;
    int    findex;
    int    maxlabelsz = MAX_STR;
    char   vlabel[MAX_STR];
    int    vdsize;
    char   *logfilenamep = NULL;
    char   *filesystemp = NULL;

    /*
     * check switchs on cmd line for file system
     * type selection and defined volume label.
     */
    optind = 1;
    optarg = NULL;
    optopt = 0;
    while ((c = getopt(xargc, xargv, "cdp:t:v")) != -1) {
        switch(c) {
#ifdef LATER
            case 'l':  /* specify the log file */
                
	       logfilenamep = optarg;

               if (((lfd = open(logfilenamep, O_WRONLY|O_CREAT|O_SYNC|O_APPEND,
                     0664)) == -1) || ((logfd = fdopen(lfd, "w")) == NULL)) {

                   fpSysError("Couldn't open log file %s", logfilenamep);
                   goto leave;
               }
               break;
#endif /* LATER */

            case 'p':   /* specify the file system partition */
		partition = atoi(optarg);
		break;

            case 't':   /* specify the file system type */

	        filesystemp = optarg;

                if (strcasecmp(filesystemp, "hfs")== 0 || 
                    strcasecmp(filesystemp, "mac")== 0)
                    fpinfop->filesystype = MAC_HFS;
                else if (strcasecmp(filesystemp, "dos")== 0 || 
                         strcasecmp(filesystemp, "fat")== 0)
                    fpinfop->filesystype = DOS_FAT;
                else {
                    fpError("The file system type <%s> is not supported",
								filesystemp);
                    goto usage;
                }
                break;

            case 'c':  
                /* this flag to correct the fat system 
                   not just to report the error */
                correctflag=1;
                break;

            case 'd':		/* Increase the debugging	*/
		debug_flag++;
		break;

            case 'v':		/* Increase the verbosity	*/
		verbose_flag++;
		break;

            default:
                fpError("unrecognized option %c", c);
               /* Fall through */
            case '?':   /* usage message */
usage:
                fprintf(stderr, "Usage: fpck -t dos|mac [-c] [-d] [-p part] [-v] <dev>\n");
leave:
                return(E_UNKNOWN);
        }
    }

    if (xargc == 1)
	goto usage;

    /* the first argument should be device name */ 
    if (optind)
        strcpy(&fpinfop->dev[0], xargv[optind]);
    else
        goto usage;

    if (filesystemp == NULL) {
	fpError("The file system type [-t dos|fat|mac|hfs] was not provided");
        goto usage;
    }

    return(E_NONE);
}

int sector_size(int fd)
{
#define	SENSE_LEN_ADD	(8+4+2)

    int				i, maxd, pgnum;
    int				secsize;
    u_char			pglengths[ALL+1];
    u_char			*d;
    struct dk_ioctl_data	sense;
    struct mode_sense_data	sense_data;


    d = (u_char *)&sense_data;

    /*
     * 1st, try to read ALL pages, so's we can figure out
     * whether the device actually has page 3 (device format).
     */
    bzero(pglengths, sizeof(pglengths));

    pglengths[ALL] = sizeof(sense_data) - (1 + SENSE_LEN_ADD);

    sense.i_addr = (void *)&sense_data;
    sense.i_len  = pglengths[ALL] + SENSE_LEN_ADD;
    sense.i_page = ALL|CURRENT;

				/* only one byte! (don't want modulo) */
    if (sense.i_len > 0xff)
	sense.i_len = 0xff;

    bzero(&sense_data, sizeof sense_data);

    if (ioctl(fd, DIOCSENSE, &sense) < 0) {
	if (debug_flag)
	    printf("fpck.c:sector_size: bad 1st ioctl, secsz=512\n");

	return 512;
    }

    /*
     * sense_len doesn't include itself;
     * set cd->pglengths[ALL] for completeness
     */
    pglengths[ALL] = maxd = sense_data.sense_len + 1;

    /*
     * Scan through the pages to determine which
     * are actually supported.
     */
    if (sense_data.bd_len > 7)
	i = 4 + sense_data.bd_len;	/* skip header and block desc. */
    else
	i = 4;				/* skip just the header */

    while (i < maxd) {
	pgnum = d[i] & ALL;

	pglengths[pgnum] = d[i+1];

	i += pglengths[pgnum] + 2;	/* +2 for header */
    }

    if (debug_flag > 2) {
	for (i = 0; i < (ALL+1); i++)
                        printf("fpck.c:sector_size:.. pg[0x%x]: length/%d\n",
                                                        i, pglengths[i]);
    }

    if (pglengths[3] == 0) {
	if (debug_flag)
		printf("fpck.c:sector_size: page 3 missing, secsz=512\n");

	return 512;
    }


    sense.i_addr = (void *) &sense_data;
    sense.i_len = sizeof sense_data;
    sense.i_page = 3;

    bzero(&sense_data, sizeof sense_data);

    if (ioctl(fd, DIOCSENSE, &sense) != 0) {
	if (debug_flag)
		printf("fpck.c:sector_size: bad 2nd ioctl, secsz=512\n");

	return 512;			/* default sector size */
    }

    if (sense_data.bd_len < 8) {
	if (debug_flag)
		printf("fpck.c:sector_size: bad bd_len, secsz=512\n");

	return 512;
    }

    secsize = (sense_data.block_descrip[5] << 16 |
		    sense_data.block_descrip[6] << 8 |
		    sense_data.block_descrip[7]);
    if (debug_flag)
	    printf("fpck.c:sector_size: secsz=%d\n", secsize);

    return secsize ? secsize : 512;
}

unsigned int
howmany_sectors(int fd, int secsize)
{
    unsigned int n_blocks;

    if (ioctl(fd, DIOCREADCAPACITY, &n_blocks) != 0)
    {
	struct stat status;

	if (fstat(fd, &status) != 0)
	{   perror("fpck fstat");
	    exit(1);
	}
	n_blocks = status.st_size / secsize; /* round down */

	if (debug_flag)
		printf(
	"fpck.c:howmany_sectors: calc capacity = %d, stsz/%lld, secsz/%d\n",
				n_blocks, status.st_size, secsize);
    } else if (debug_flag) {
	printf("fpck.c:howmany_sectors: capacity = %d\n", n_blocks);
    }
    return n_blocks;
}



static void
size_device(FPINFO *fpinfop)
{
    struct device_parameters *dp = &fpinfop->vh.vh_dp;
    int secsize = sector_size(fpinfop->devfd);


    fpinfop->n_sectors = howmany_sectors(fpinfop->devfd, secsize);

    if (debug_flag > 1) {
	printf("fpck.c:size_device[%d]: fpinfop/0x%x fp->n_sectors = %d\n",
					__LINE__, fpinfop, fpinfop->n_sectors);
    }

    bzero(&fpinfop->vh, sizeof(struct volume_header));

    if (   ioctl(fpinfop->devfd, DIOCGETVH, &fpinfop->vh) == -1
	|| valid_vh(&fpinfop->vh)
	|| dp->dp_secbytes == 0
	|| dp->dp_drivecap != fpinfop->n_sectors) {

	if (debug_flag > 1 && dp->dp_drivecap != fpinfop->n_sectors) {
	    printf("fpck.c:size_device: n_sectors != drivecap! %d/%d\n",
				fpinfop->n_sectors, dp->dp_drivecap);
	}

	/*
	 *  Produce a fake volume header and install it in the driver.
	 */
	bzero(&fpinfop->vh, sizeof(struct volume_header));

	fpinfop->vh.vh_magic = VHMAGIC;

	dp->dp_secbytes = secsize;
	dp->dp_drivecap = fpinfop->vh.vh_pt[10].pt_nblks = fpinfop->n_sectors;

	fpinfop->vh.vh_csum = -vhchksum(&fpinfop->vh);

			/* Don't care if ioctl works.  Might not be a disk. */
	(void) ioctl(fpinfop->devfd, DIOCSETVH, &fpinfop->vh);

    } else {

	fpinfop->n_sectors = dp->dp_drivecap;	/* drivers always fill in */

	if (debug_flag > 1) {
		printf("fpck.c:size_device[%d]: fpinfop/0x%x drivecap/%d\n",
						__LINE__,
						fpinfop, fpinfop->n_sectors);
	}
    }
}


/*
 * using args if present, open a drive and get its label info.
 *    usage:  fx [devname [drivetype]]
 * first, get args if any.  if a drive type is specified, save it
 * for later.
 *
 * open the device.  do a quick controller check.  
 * Clear out any label info from a previous drive, get new label info.
 * then, we are finally ready to run the main menu.
 */
static void
devsetup(FPINFO * fpinfop)
{
    uint    status;
    struct stat    sb; 
    int        devminor;


    /* (void)setintr(-1); */ /* make the process killable */

    /* after we get the drive name, but before any i/o */

    sigset(SIGHUP, gothup);

    /* Catch and report other signals; generally divide by 0, or
       dereferencing a pointer that hasn't yet been set.  Better
       than just dumping core, but not as good as fixing the (MANY)
       places in the code where not enough error checking has
                                                       been done.  */

    sigset(SIGTRAP, badsig);
    sigset(SIGIOT, badsig);
    sigset(SIGILL, badsig);
    sigset(SIGFPE, badsig);
    sigset(SIGEMT, badsig);
    sigset(SIGSEGV, badsig);
    sigset(SIGBUS, badsig);

    if ((fpinfop->devfd = open(fpinfop->dev,  O_RDWR | O_NDELAY)) == -1) {
        fpSysError("can't open device");
        exit(1);
    } else if(chkmounts(fpinfop->dev)) {
        fpError("this disk appears to have mounted filesystems.\n"
               "\t Don't do anything destructive, unless you are sure\n"
               "\t nothing is really mounted on this disk.");
        exit(1);
    } else {
        if ((ioctl(fpinfop->devfd, SMFDMEDIA, &status) == 0) ||
            remmedinfo(fpinfop->devfd, &status) ) {
	    if (status & SMFDMEDIA_WRITE_PROT) {
		close(fpinfop->devfd);
		fpError("disk is write protected!");
		exit(1);
	    }
	}
	if (fstat(fpinfop->devfd, &sb) < 0) {
	    close(fpinfop->devfd);
	    fpError("The file %s does not exist!", fpinfop->dev);
	    exit(1);
	}
	if (fstat(fpinfop->devfd, &sb) < 0) {
	    close(fpinfop->devfd);
	    fpError("The file %s does not exist!", fpinfop->dev);
	    exit(1);
	} else {
	    devminor = minor(sb.st_rdev);
	    fpinfop->mediatype = smfd_type(devminor);
	}
    }

    memset((void *)&fpinfop->vh, '\0', sizeof(struct volume_header));

    size_device(fpinfop);
}


static void
gothup(int sig)
{
    exit(1);
}


#define MAX_BAD_SIGS 15

void
badsig(int signo, int flag, struct sigcontext *cont)
{
    static sigcnt;

    printf("At pc 0x%llx, got unexpected signal ", cont->sc_pc);
    if(signo>0 && signo < NSIG)
        printf("SIG%s\n", signames[signo]);
    else
        printf("# %d\n", signo);

    sigrelse(signo);    /* so we can take signal again */
    if(++sigcnt > MAX_BAD_SIGS) {
        /* in case we are in a loop getting sigsegv, etc */
        printf("too many unexpected signals (%d), bye\n", MAX_BAD_SIGS);
        exit(-1);
    }
}



/*
 * make the current level interruptible or not according to flag.
 *       1 - interruptible
 *       0 - non-interruptible
 *      -1 - killable
 * return previous flag value.  ioctl returns
 *       1 - non-interruptible
 *       0 - killable
 *      xx - interruptible
 * Non-ARCS version in standalone.c
 */

/*
int
setintr(int flag)
{
    SIGNALHANDLER oldintr;

    if( flag == 0 )
        oldintr = Signal(SIGINT, SIGIgnore);

    else if( flag > 0 )
        oldintr = Signal(SIGINT, (SIGNALHANDLER)mpop);

    else oldintr = Signal(SIGINT, SIGDefault);

    return oldintr==SIGDefault ?-1 : oldintr==SIGIgnore ?0 :1;
}
*/


#ifdef LATER
void
exit_func(void)
{
    (void)setintr(1);
    if(driveopen) {
        if(formatrequired &&
        no("Geometry has changed, and a format is required\n"
               "Exit anyway"))
            mpop();
            /* after check, since the read  
           may fail if a format is required */
            optupdate();
    }
    exit (0);
}
#endif /* LATER */

 /*-------- fAllocInfoCell -------------------------------------

   create the file allocation information block and link
   it to the list.

  ---------------------------------------------------------------*/

FALLOCINFO *
fAllocInfoCell(char * name, CLUSTERLIST ** listheadpp, int clustercnt,
                         FALLOCINFO ** finfoheadpp, FALLOCINFO * parentdirp,
             CLUSTERLIST ** crosslinklistheadpp)
{
    int crosslinkflag = 0;  /* only need first cross link cluster number */
    int index = 0;
    FALLOCINFO  * curfbinfop;
    CLUSTERLIST * curclusterp;
    CLUSTERLIST * tmpclusterp;

    /* construct the file allocation infomation structure */
    curfbinfop = (FALLOCINFO *) safemalloc(sizeof(FALLOCINFO));
    curfbinfop->name      = name;
    curfbinfop->blknum    = clustercnt;
    curfbinfop->blkv      = safemalloc(sizeof(int)*clustercnt);
    curfbinfop->parentdir = parentdirp;
    curclusterp = * listheadpp;

    /* construct the file cluster infomation block based on the cluster
       list of the file, and also release the cluster list memory */
    while (index < clustercnt && curclusterp) {
        curfbinfop->blkv[index++] = curclusterp->clusterno;
    tmpclusterp = curclusterp->nextcluster; /* save the list */

    if (curclusterp->status == E_CROSSLINK && !crosslinkflag) {
            /* link the cluster into the crosslinklist link list */
        curclusterp->nextcluster = NULL; /* clean up the structure */
            if (* crosslinklistheadpp == NULL)
                * crosslinklistheadpp = curclusterp;
            else {
                curclusterp->nextcluster = * crosslinklistheadpp;
                * crosslinklistheadpp = curclusterp;
            }
        crosslinkflag = 1;  /* forget the following information */
    } else  /* just release the structure */
            free((char *) curclusterp);

        curclusterp = tmpclusterp;
    }
    * listheadpp = (CLUSTERLIST *)NULL;

    /* link the info block into the link list */
    curfbinfop->nextfile = * finfoheadpp;
    * finfoheadpp = curfbinfop;

    return(curfbinfop);
}


 /*-------- linkClusterCells -------------------------------------

   the function handle the link list.

  ---------------------------------------------------------------*/

void
linkClusterCells(CLUSTERLIST ** headpp, CLUSTERLIST * targetp)
{
    CLUSTERLIST * curp;

    curp = lastClusterCell(* headpp);
    if (curp) 
        curp->nextcluster = targetp;
    else 
        * headpp = targetp;
}


 /*-------- countClusterCell -------------------------------------

  ---------------------------------------------------------------*/

int
countClusterCell(CLUSTERLIST * headp)
{
    int           count = 0;
    CLUSTERLIST * curp;

    curp = headp;
    while (curp) { 
        count++;
        curp = curp->nextcluster;
    }

    return(count);
}


 /*-------- lastClusterCell -------------------------------------

   return the last cluster info block in the link list.

  ---------------------------------------------------------------*/

CLUSTERLIST *
lastClusterCell(CLUSTERLIST * headp)
{
    CLUSTERLIST * curp = NULL;

    /* sanity checking */ 
    if (headp == (CLUSTERLIST *) NULL)
        return(curp);
    else
       curp = headp;

    while (curp->nextcluster)
        curp = curp->nextcluster;

    return(curp);
}

 /*-------- getUnixPath -----------------------------------------

   allocate a string which show the full path name of the file.

  ---------------------------------------------------------------*/

char * 
getUnixPath(FALLOCINFO * targetinfop, char * name)
{
    char   fpath1[256];
    char   fpath2[256];
    char * fullpath;
    FALLOCINFO * curinfop;

    /* handle the case of file entry point error, which
       there is no file allocation information block can use */
    if (name)
        strcpy(fpath1, name);
    else
        strcpy(fpath1, targetinfop->name);

    curinfop = targetinfop->parentdir;
    while(curinfop) {
        sprintf(fpath2, "%s/%s", curinfop->name, fpath1);
    strcpy(fpath1, fpath2);
    curinfop = curinfop->parentdir;
    }
    sprintf(fpath2, "/%s", fpath1);

    fullpath = safemalloc(strlen(fpath2));
    strcpy(fullpath, fpath2);

    return(fullpath);
}

 /*-------- selfLinked -------------------------------------

   the function is used to see whether the target cluster
   is self linked with it's own data cluster chain.

  ----------------------------------------------------------*/

int
selfLinked(CLUSTERLIST * fheaderp, int tcluster)
{
    CLUSTERLIST * tmpclusterp;

    tmpclusterp = fheaderp;
    while(tmpclusterp)
        if (tmpclusterp->clusterno == tcluster)
            return(E_CROSSLINK);
        else
            tmpclusterp = tmpclusterp->nextcluster;

    return(E_NONE);
}


 /*-------- searchCrossLinked ------------------------------------

   the function is used to see whether the target cluster
   is cross linked with the any file in the file list.

  ----------------------------------------------------------*/

FALLOCINFO *
searchCrossLinked(FALLOCINFO * fblkheadp, CLUSTERLIST * tclusterp)
{
    FALLOCINFO * tmpfinfop;
    int          tcluster = tclusterp->clusterno;

    tmpfinfop = fblkheadp;
    while(tmpfinfop)
        if (isMemberCluster(tmpfinfop, tcluster) == E_NOTFOUND)
            tmpfinfop = tmpfinfop->nextfile;
        else
            return(tmpfinfop);

    return((FALLOCINFO *)NULL);
}


/*-------- isMemberCluster -------------------------------------

   the function is used to see whether the target cluster
   is cross linked with the specify file data chain.

  --------------------------------------------------------------*/

int
isMemberCluster(FALLOCINFO * finfop, int tcluster)
{
    int i;
   
    for (i = 0; i < finfop->blknum; i++)
        if (finfop->blkv[i] == tcluster)
            return(E_NONE);

    return(E_NOTFOUND);
}


/*--------  releaseFalloc ---------------------------

   release the allocated memory for falloc list

  ---------------------------------------------------*/

void
releaseFalloc(FALLOCINFO ** finfoheadpp)
{
    FALLOCINFO * curp;

    while (* finfoheadpp) {
        curp = * finfoheadpp;
        * finfoheadpp = (* finfoheadpp)->nextfile;
        if (curp->blkv)
            free(curp->blkv);
        if (curp->name)
            free(curp->name);
        free((char *)curp);
    }
}
