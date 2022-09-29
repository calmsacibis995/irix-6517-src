/*
 *=============================================================================
 * mkfp.c
 *
 * There are two parts to this disk formatting utility:
 * (1) Part which is responsible for placing partitions on media other than
 *     floppies and flopticals (for DOS file systems).
 * (2) Part which is responsible for formatting floppies and flopticals.
 *
 * The first task is to determine which device to use, and which 
 * target file system type the floppy will be formatted into.
 *
 * The next task is to determine and examine the specified volume
 * label based on the individual file system.
 *
 * Do the low level format to the floppy disk, and call the
 * corresponding file system initialization function to setup
 * the disk as the data disk (not bootable) of the target system. 
 *=============================================================================
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
#include <sys/smfd.h> 
#include <sys/sema.h>
#include <sys/scsi.h>
#include <sys/dkio.h>
#include <sys/major.h>

#include <sys/conf.h>
#include <invent.h>
#include <sys/iograph.h>

#include "fp.h"
#include "mkfp.h"
#include "smfd.h"
#include <sys/dksc.h>

#include "dosfs.h"
#include "macSG.h"
#include "macLibrary.h"
#include "misc.h"

#define DEBUGLABEL
#undef  DEBUGLABEL 

int	debug_flag   = 0;
int	verbose_flag = 1;
int	file_sys_flag = 0;
FILE  *logfd = NULL;

extern char 	*smfd_partname(int);
extern void 	badsig(int, int, struct sigcontext *);
extern int 	remmedinfo(int fd, uint *status);
extern char 	*spaceskip(char * strp);

/*
 *---------------------------------------------------------------------------
 * External functions that constitute the DOS partition table format module.
 *---------------------------------------------------------------------------
 */
extern	int	scsi_is_floppy(u_int capacity);
extern  int     scsi_query(void);
extern  void    part_read_disk(void);
extern  void    part_incore_init(void);
extern  void    part_data_sync(void);
extern  int     part_create(u_long *dsize, u_long *daddr);
extern  int     part_entire_create(u_long *dsize, u_long *daddr);
extern  void    dos_mkfs(u_long daddr, u_long dsize, char *label);

/*
 *----------------------------------------------------------------------------
 * External global variables.
 *----------------------------------------------------------------------------
 */
extern	int     fd;
extern	char    *disk_device;
extern	u_int   heads;          /* Disk Geometry */
extern	u_int   sectors;        /* Disk Geometry */
extern	u_int   cylinders;      /* Disk Geometry */
extern	u_int   capacity;       /* Disk capacity in sectors */


/*
 *-----------------------------------------------------------------------------
 * Function prototypes.
 *-----------------------------------------------------------------------------
 */
int	main(int, char **);
static int  	floppy_main(int , char **);
static int	is_smfd_device(int fd);
static void 	gothup(int);
static int  	devSetup(FPINFO *);
static int 	size_device(FPINFO *);
static void  	process_args0(int, char **);
static int  	process_args1(int, char **);
static int  	process_args2(FPINFO *, int, char **);
static void 	vlabelinspect(FPINFO *, char *);
static BOOL 	(*vlabelchkfunc[MAX_TYPE])(FPINFO *, char *);
static void 	init_labelfunc(void);
static int      extract_part_info(char *pstr, u_long *dsize, char *dlabel);
static void	print_error(int ecode);

static int 	expertmode = 0;
static int 	retcode = 0;
static int 	fat12flag = 0;


char *signames[] = {
    "",         /* place holder */
    "HUP",      /* hangup */
    "INT",      /* interrupt (rubout) */
    "QUIT",     /* quit (ASCII FS) */
    "ILL",      /* illegal instruction (not reset when caught)*/
    "TRAP",     /* trace trap (not reset when caught) */
    "IOT",      /* IOT instruction */
    "EMT",      /* EMT instruction */
    "FPE",      /* floating point exception */
    "KILL",     /* kill (cannot be caught or ignored) */
    "BUS",      /* bus error */
    "SEGV",     /* segmentation violation */
    "SYS",      /* bad argument to system call */
    "PIPE",     /* write on a pipe with no one to read it */
    "ALRM",     /* alarm clock */
    "TERM",     /* software termination signal from kill */
    "USR1",     /* user defined signal 1 */
    "USR2",     /* user defined signal 2 */
    "CLD",      /* death of a child */
    "PWR",      /* power-fail restart */
    "STOP",     /* sendable stop signal not from tty */
    "TSTP",     /* stop signal from tty */
    "POLL",     /* pollable event occured */
    "IO",       /* input/output possible signal */
    "URG",      /* urgent condition on IO channel */
    "WINCH",    /* window size changes */
    "VTALRM",   /* virtual time alarm */
    "PROF",     /* profiling alarm */
    "CONT",     /* continue a stopped process */
    "TTIN",     /* to readers pgrp upon background tty read */
    "TTOU",     /* like TTIN for output if (tp->t_local&LTOSTOP) */
};

int	main(int argc, char **argv)
{
	extern char *progname;

	int	ret;

	progname = "mkfp";

	process_args0(argc, argv);	/* Catch the "-d" option	*/

        /*
	 * process_args1() takes a peek at the args as well as the
	 * type of device. If the device type is floppy/floptical
 	 * then, the request is serviced at this point. If not,
	 * process_args1() has already serviced request.
         */
        ret = process_args1(argc, argv);

	if (debug_flag)
	    printf("mkfp.c:main; process_args1 returned %d\n", ret);

	switch (ret){
	case MKFP_NONE:
		/* Formatting done */
		return (0);	
	case MKFP_FLOPPY:
		if (debug_flag)
		    printf("mkfp.c:main; calling floppy_main\n");

		/* Formatting floppy/floptical */
		(void) floppy_main(argc, argv);
		return (0);
	default:
		/* Error */
		print_error(ret);
		exit (ret);
        }
        return (0);

}

static void
process_args0(int xargc, char ** xargv)
{
    char   *ftypep;
    char   *strtmp;
    char   *logfilenamep;
    char   *filesystemp;
    char   *vdiskszstrp;
    int    c, lfd;
    int    vdsize;
    int    retval = E_NONE;
    char   vlabel[MAX_STR];

    extern  int  optind;
    extern  char *optarg;
    extern  void getoptreset(void);

    optind = 1;
    optarg = NULL;
    optopt = 0;

    while ((c = getopt(xargc, xargv, "xsv:dynt:p:")) != -1){
        switch(c){
        case 'y':
        case 'p':
        case 's':
        case 't':   
        case 'v':  
        case 'x':
		break;
        case 'd':
		debug_flag++;
                break;
        case 'n':
		if (debug_flag == 0)
			debug_flag = 1;
                break;
        default:
        case '?':   
               /* usage message */
	    gerror("Incorrect command line arguments");
	    break;
        }
   }
}

static	int	floppy_main(int xargc, char **xargv)
{
    FPINFO fpinfo;
    struct DsDevice *fd;
    int retval = E_NONE;

    memset(&fpinfo, '\0', sizeof(FPINFO));

    init_labelfunc();

    if ((retval = process_args2(&fpinfo, xargc, xargv)) != E_NONE)
        exit(retval);

#ifdef DEBUGLABEL
    printf("label=%s\n", fpinfo.volumelabel);
#else
    
    if ((retval = devSetup(&fpinfo)) != E_NONE)
        exit(retval);

    /* Do the low level format */
    if (expertmode && do_smfdformat(&fpinfo) != E_NONE)  {
        if (!retcode)
            fpSysError("fail in performing low level format");
        exit(MKFP_LOWFMT_F);
    }

    if (!fpinfo.filesystype) {
        exit(MKFP_LOWFMT_S);
    }

    /* write file system informations to the floppy */
    if (fpinfo.filesystype == MAC_HFS) {
        /* create a fake DsDevice to please the DIT mac lib codes */
        fd = (struct DsDevice *) newDsDevice(&fpinfo);
        retval = dm_format((void *) fd, fpinfo.volumelabel, 0);
        freeDsDevice ((void **) &fd);
        
    } else if (fpinfo.filesystype == DOS_FAT) {
        /* intitialize the format function for supported media */
        dos_formatfunc_init(fat12flag);
        retval = dos_format(&fpinfo);
    }

    if (retval == E_NONE) {
        if (!retcode)
            fpMessage(" mkfp: Format completed");
        exit(MKFP_NONE);
    } else if (retval == E_WRITE) {
        if (expertmode) {
            if (!retcode)
                fpError("bad sector has been detected, the disk is not safe to use!");
            exit(MKFP_BADFLOP);
        } else {
            if (!retcode)
                fpError("bad sector has been detected, use mkfp -x");
            exit(MKFP_BADSEC);
	}
    } else if (retval == E_MEMORY) {
        if (!retcode)
            fpError("out of memory");
        exit(MKFP_MEMORY);
    } else if (retval == E_NOTSUPPORTED) {
        if (!retcode)
            fpError("floppy type is not supported!");
        exit(MKFP_TYPE);
    } else if (retval == E_PROTECTION) {
	if (!retcode)
	    fpError("disk is write protected");
	exit(MKFP_WPROT);
    }
#endif /* DEBUGLABEL */
    return (0);
}    

static	int process_args2(FPINFO * fpinfop, int xargc, char ** xargv)
{
    char   *ftypep;
    char   *strtmp;
    char   *logfilenamep;
    char   *filesystemp;
    char   *vdiskszstrp;
    int    c, lfd;
    int    vdsize;
    int    retval = E_NONE;
    char   vlabel[MAX_STR];

    extern  int  optind;
    extern  char *optarg;
    extern  void getoptreset(void);

    file_sys_flag = 0;
    strcpy(vlabel, "Untitled");

    optind = 1;
    optarg = NULL;
    optopt = 0;
    while ((c = getopt(xargc, xargv, "xsv:dynt:p:")) != -1){
        switch(c){
        case 'n':
		gerror("-n option disallowed for floppies/flopticals");
		goto usage;
        case 'y':
                break;
        case 'p':
		if (*optarg != ':')
			goto usage;
                strncpy(vlabel, ++optarg, MAX_STR);
		if (!file_sys_flag){
			gerror("Must specify target file system");
			goto usage;
		}
                if (((*vlabelchkfunc[fpinfop->filesystype])
                     (fpinfop, spaceskip(vlabel))) != E_NONE){
			printf(" mkfp: label = %s\n", vlabel);
                        if (!retcode)
                                fpError("Invalid volume label");
                        return (MKFP_VLABEL);
                }
		break;
        case 't':   
                /* Specify the file system type */
		file_sys_flag = 1;
                filesystemp = optarg;
                if (!strcmp(filesystemp, "dos") || 
                    !strcmp(filesystemp, "fat"))
                        fpinfop->filesystype = DOS_FAT;
                else if (!strcmp(filesystemp, "mac") ||
                         !strcmp(filesystemp, "hfs"))
                        fpinfop->filesystype = MAC_HFS;
                else goto usage;
                break;
        case 'x':
                /* Do the low level format */
                expertmode = 1;
                break;
        case 'd':
				/* debug_flag handled in process_args0	*/
                break;
        case 's':
                /* This flag to use the 12bit fat system in floptical disk */
                fat12flag = 1;
                break;
        case 'v':  
                /* Specify the virtual disk size */
                vdiskszstrp = optarg;
                strtmp = vdiskszstrp;
                while (*strtmp){
                    if (!isdigit(*strtmp++))
                        goto usage;
                }
                vdsize = atoi(vdiskszstrp);
                break;
        default:
        case '?':   
               /* usage message */
usage:
                if (!retcode)
                    gerror("Incorrect command line arguments");
leave:
                return(MKFP_USAGE);
        }
   }
   if (!file_sys_flag){
	gerror("Target file system not specified");
	return (MKFP_USAGE);	
   }
   if (!xargv[optind]){
	gerror("Target device name not specified");
	return (MKFP_USAGE);
   }
   disk_device = xargv[optind];
   strcpy(&fpinfop->dev[0], disk_device);
   return(retval);
}

extern int valid_vh(struct volume_header *); 
extern int vhchksum(struct volume_header *);

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
	    printf("mkfp.c:sector_size: bad 1st ioctl, secsz=512\n");

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
                        printf("mkfp.c:sector_size:.. pg[0x%x]: length/%d\n",
                                                        i, pglengths[i]);
    }

    if (pglengths[3] == 0) {
	if (debug_flag)
		printf("mkfp.c:sector_size: page 3 missing, secsz=512\n");

	return 512;
    }


    sense.i_addr = (void *) &sense_data;
    sense.i_len = sizeof sense_data;
    sense.i_page = 3;

    bzero(&sense_data, sizeof sense_data);

    if (ioctl(fd, DIOCSENSE, &sense) != 0) {
	if (debug_flag)
		printf("mkfp.c:sector_size: bad 2nd ioctl, secsz=512\n");

	return 512;			/* default sector size */
    }

    if (sense_data.bd_len < 8) {
	if (debug_flag)
		printf("mkfp.c:sector_size: bad bd_len, secsz=512\n");

	return 512;
    }

    secsize = (sense_data.block_descrip[5] << 16 |
		    sense_data.block_descrip[6] << 8 |
		    sense_data.block_descrip[7]);
    if (debug_flag)
	    printf("mkfp.c:sector_size: secsz=%d\n", secsize);

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
	{   perror("mkfp fstat");
	    exit(MKFP_GENERIC);
	}
	n_blocks = status.st_size / secsize; /* round down */

	if (debug_flag)
		printf(
	"mkfp.c:howmany_sectors: calc capacity = %d, stsz/%lld, secsz/%d\n",
				n_blocks, status.st_size, secsize);
    } else if (debug_flag) {
	printf("mkfp.c:howmany_sectors: capacity = %d\n", n_blocks);
    }
    return n_blocks;
}

/*
 * is_smfd_device()
 * This routine takes a file descriptor associated with a raw
 * device, and returns 1 if it's associated with the smfd driver.
 */
static int
is_smfd_device(int fd)
{
    char          devname[MAXDEVNAME];
    int           len;

    len = MAXDEVNAME;
    fdes_to_drivername(fd, devname, &len);
    if (!strncmp(devname, "smfd", 4))
	return (1);
    return (0);
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
static int
devSetup(FPINFO * fpinfop)
{
    int           retval = E_NONE;
    uint          status;
    struct stat   sb;
    int           devminor;
    int           is_a_floppy;

    /* after we get the drive name, but before any i/o */
    sigset(SIGHUP, gothup);

    /* Catch and report other signals; generally divide by 0, or
       dereferencing a pointer that hasn't yet been set.  Better
       than just dumping core, but not as good as fixing the (MANY)
       places in the code where not enough error checking has been done. */

    sigset(SIGTRAP, badsig);
    sigset(SIGIOT, badsig);
    sigset(SIGILL, badsig);
    sigset(SIGFPE, badsig);
    sigset(SIGEMT, badsig);
    sigset(SIGSEGV, badsig);
    sigset(SIGBUS, badsig);

    if ((fpinfop->devfd = open(fpinfop->dev,  O_RDWR | O_NDELAY)) == -1) {
        if (!retcode)
            fpSysError("can't open the device");
        return(MKFP_OPENDEV);
    } else if(chkmounts(fpinfop->dev)) {
        if (!retcode)
            fpError("this disk appears to have mounted filesystems");
        close(fpinfop->devfd);
        return(MKFP_MOUNTED);
    } else {
        is_a_floppy = 1;

        if (ioctl(fpinfop->devfd, SMFDMEDIA, &status) != 0) {
            /* XXX check dksc driver for write protect */
            remmedinfo( fpinfop->devfd, &status );
            status = SMFDMEDIA_READY;
            is_a_floppy = 0;
        }
        if (status & SMFDMEDIA_WRITE_PROT) {
            close(fpinfop->devfd);
            if (!retcode)
                fpError("device is write protected!");
            return(MKFP_WPROT);
        }
        if (fstat(fpinfop->devfd, &sb) < 0) {
            close(fpinfop->devfd);
            if (!retcode)
                fpError("the specified device does not exist!");
            return(MKFP_DEVEXIST);
        } else if (is_smfd_device(fpinfop->devfd)){
           int lcapacity;
           if (ioctl(fpinfop->devfd, DIOCREADCAPACITY, &lcapacity) != 0)
               fpinfop->mediatype = FD_FLOP_GENERIC;
           else if (lcapacity == 720)
               fpinfop->mediatype = FD_FLOP;
           else if (lcapacity == 2400)
               fpinfop->mediatype = FD_FLOP_AT;
           else if (lcapacity == 1440)
               fpinfop->mediatype = FD_FLOP_35LO;
           else if (lcapacity == 2880)
               fpinfop->mediatype = FD_FLOP_35;
           else if (lcapacity == 40662)
               fpinfop->mediatype = FD_FLOP_35_20M;
           else
               fpinfop->mediatype = FD_FLOP_GENERIC;

		if (debug_flag)
		    printf("mkfp.c:devSetup: mediatype = %d, lcapacity = %d\n",
					fpinfop->mediatype, lcapacity);
        }
    }

    retval = size_device(fpinfop);
    if (retval != E_NONE)
        return retval;

    /* run a simple diagnostic test to see if the floppy
       is a raw media, if it's uninitialized then go to
       make sure the program is running in expert mode   */

    /* Skip the diagnostic if we have a fake volume header. */
    if (!expertmode && is_a_floppy &&
        fpinfop->vh.vh_dp.dp_drivecap == fpinfop->n_sectors) {
        unsigned char * sectorbufp;
        int i, j, sectorspercyl, sectorloc, readstatus = 0;

        /* check the accessibility of the specified sector */
        sectorbufp = safemalloc(fpinfop->vh.vh_dp.dp_secbytes);
        sectorspercyl = 32;     /* arbitrary; same as fx */

        for (i = 0; i < 2; i++)
            for (j = 0; j < 2; j++) {
                sectorloc = ((fpinfop->vh.vh_dp.dp_drivecap*i/2) +
                             (sectorspercyl * j / 2)) * fpinfop->vh.vh_dp.dp_secbytes;

                /* move head to point to the specified location */
                lseek(fpinfop->devfd, sectorloc, SEEK_SET);
                if (rfpblockread(fpinfop->devfd, sectorbufp,
                    fpinfop->vh.vh_dp.dp_secbytes) != E_NONE)
                    readstatus |= 0x1;
                else
                    readstatus |= 0x2;
            }
        free(sectorbufp);

        if (readstatus != 0x02) {
            close(fpinfop->devfd);
            if (readstatus == 0x01) {
                if (!retcode)
                    fpError("media not formatted, use mkfp -x");
                return(MKFP_UNINITFP);
            } else if (readstatus == 0x03) {
                if (!retcode)
                    fpError("bad sector has been detected, use mkfp -x");
                return(MKFP_BADSEC);
            }
        }
    }
    return(retval);
}

static int
size_device(FPINFO *fpinfop)
{
    struct device_parameters *dp = &fpinfop->vh.vh_dp;
    int rc;
    int secsize = sector_size(fpinfop->devfd);

    fpinfop->n_sectors = howmany_sectors(fpinfop->devfd, secsize);

    if (debug_flag > 1) {
	printf("mkfp.c:size_device[%d]: fpinfop/0x%x fp->n_sectors = %d\n",
					__LINE__, fpinfop, fpinfop->n_sectors);
    }

    bzero(&fpinfop->vh, sizeof(struct volume_header));

    if (   ioctl(fpinfop->devfd, DIOCGETVH, &fpinfop->vh) == -1
	|| valid_vh(&fpinfop->vh)
	|| dp->dp_secbytes == 0
	|| dp->dp_drivecap != fpinfop->n_sectors) {

	if (debug_flag > 1 && dp->dp_drivecap != fpinfop->n_sectors) {
	    printf("mkfp.c:size_device: n_sectors != drivecap! %d/%d\n",
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
    }
/*
 *  The below else was "else if (dp->dp_secs != 32769)", but since
 *  there were changes in dvh.h that removed this
 *  member (dp_secs) of the device_parameters struct.  It has been
 *  removed.
 */
    else
    {
        fpinfop->n_sectors = dp->dp_drivecap;   /* driver always fills in */

	if (debug_flag > 1) {
	    printf("mkfp.c:size_device[%d]: fpinfop/0x%x fp->n_sectors = %d\n",
					__LINE__, fpinfop, fpinfop->n_sectors);
	}

    }
    return E_NONE;
}

static void
gothup(int sig)
{
 /*   logmsg("mkfp terminating on hangup\n"); */
    exit(MKFP_GENERIC);
}


#define MAX_BAD_SIGS 15

void
badsig(int signo, int flag, struct sigcontext *cont)
{
    static sigcnt;

    fprintf(stderr,"At pc 0x%llx, got unexpected signal ", cont->sc_pc);
    if(signo>0 && signo < NSIG)
        fprintf(stderr,"SIG%s\n", signames[signo]);
    else
        fprintf(stderr,"# %d\n", signo);
    sigrelse(signo);    /* so we can take signal again */
    if(++sigcnt > MAX_BAD_SIGS) {
        /* in case we are in a loop getting sigsegv, etc */
        fprintf(stderr,"too many unexpected signals (%d), bye\n", MAX_BAD_SIGS);
        exit(MKFP_GENERIC);
    }
}

/*
 *-----------------------------------------------------------------------------
 * process_args1()
 * This routine processes the command line arguments once and sees if:
 * (a) It's a DOS format request and
 * (b) It's a device that's not a floppy or floptical.
 * If the above two conditions are met, then we decide to place one or more
 * Type 6 DOS partitions, as well as a partition table in the first sector.
 * If the above two conditions are not met, then we decide simply to return,
 * and the request is serviced elsewhere.
 *-----------------------------------------------------------------------------
 */
int     process_args1(int xargc, char **xargv)
{
        int     c, ecode;
        int     error, indx;
        char    *file_sys;

        extern  int  optind;
        extern  char *optarg;
	extern	void getoptreset(void);
        
        int     pflg;
        int     pnum;
        pstr_t  ptbl[20];
        
        u_long  dsize;
	u_long	daddr;
	char	*dname;
        char    dlabel[20];

        disk_device = xargv[xargc-1];
        if (strncmp(disk_device, "/dev/rdsk", 9))
		Throw(1, ecode, MKFP_FLOPPY, is_floppy);

        error = scsi_query();

	Throw(error, ecode, MKFP_SCSI_QUERY, is_error);
	Throw(scsi_is_floppy(capacity), ecode, MKFP_FLOPPY, is_floppy);

        part_incore_init();

        pnum = 0;
        pflg = P_INIT;
	file_sys_flag = 0;

	optind = 1;
    	optarg = NULL;
    	optopt = 0;
        while ((c = getopt(xargc, xargv, "xsv:dynt:p:")) != -1){
                switch (c){
		case 'x':
			Throw(1, ecode, MKFP_NO_FMT, is_error);
			break;
		case 's':
			break;
		case 'v':
			/* Request to partition virtual disk */
			Throw(1, ecode, MKFP_FLOPPY, is_floppy);
			break;
		case 'd':
				/* debug_flag handled in process_args0	*/
			break;
		case 'y':
			verbose_flag = 0;
			break;
                case 'n':
				/* debug_flag handled in process_args0	*/
			Throw(pflg != P_INIT, ecode, MKFP_USAGE, is_error);
                        pflg = P_VIEW;
                        break;
                case 't':
                        file_sys = optarg;
			file_sys_flag = 1;
                        if (!strcmp(file_sys, "dos") ||
                            !strcmp(file_sys, "fat")){
                                file_sys_flag = DOS;
                        }
                        else if (!strcmp(file_sys, "hfs") ||
                                 !strcmp(file_sys, "mac")){
                                file_sys_flag = HFS;
                        }
                        break;
                case 'p':
			if (*optarg == ':'){
				/* Create entire partition over media */
				Throw(pflg != P_INIT, 
			              ecode, MKFP_USAGE, is_error);
				pflg = P_ENTIRE;
				strcpy(dlabel, ++optarg);
			}
			else {
				/* Create individual partition */
			  	Throw(pflg != P_INIT && pflg != P_SEVERAL,
				      ecode, MKFP_USAGE, is_error);
				pflg = P_SEVERAL;
				extract_part_info(optarg, &dsize, dlabel);
                        	ptbl[pnum].psize  = dsize;
                        	ptbl[pnum].plabel = strdup(dlabel);
                        	pnum++;
			}
			break;
		case '?':
		default:
			Throw(1, ecode, MKFP_USAGE, is_error);
			break;
                }
        }
        /*
         * Check if: File system type is specified.
         * Check if: File system type specified is HFS.
         */
        Throw(file_sys_flag == HFS, ecode, MKFP_FLOPPY, is_floppy);
	Throw(!file_sys_flag, ecode, MKFP_NO_FS, is_error);
        /*
         * Print summary of request specified on command line.
         */
        switch (pflg){
        case P_VIEW:
		/* Option to view existing DOS partitions */
                printf(" mkfp: Viewing existing partitions\n");
                break;
	case P_INIT:
		/* Option to create one partition over entire media */
		strcpy(dlabel, "Untitled");	
        case P_ENTIRE:
		/* Option to create one partition over entire media */
		printf(" mkfp: partition index = %d\n", 1);
                printf(" mkfp: partition size  = entire media\n");
                printf(" mkfp: partition label = %s\n", dlabel);
                break;
        case P_SEVERAL:
		/* Option to create several partitions over media */
                printf(" mkfp: Creating individual partitions on media\n");
                for (indx = 0; indx < pnum; indx++){
		   dsize = ptbl[indx].psize;
		   dname = ptbl[indx].plabel;
                   printf(" \n");
                   if (indx >= 3)
                         printf(" mkfp: partition index = %d\n", indx+2);
                   else  printf(" mkfp: partition index = %d\n", indx+1);
                   printf(" mkfp: partition size  = %s\n", ground(dsize));
                   printf(" mkfp: partition label = %s\n", ptbl[indx].plabel);
                }
		printf("\n");
        }
        disk_device = xargv[optind];
        printf(" mkfp: Device          = %s\n", disk_device);
action:
        switch (pflg){
        case P_VIEW:
                part_read_disk();
                break;
	case P_INIT:
        case P_ENTIRE:
		if (verbose_flag){
		   printf(" mkfp: All existing data on media will be lost\n");
        	   printf(" mkfp: continue ? (y/n) ");
        	   if ((c = getchar()) != 'y')
			exit (0);
		}
                error = part_entire_create(&dsize, &daddr);
		Throw(error, ecode, MKFP_PART_CREATE, is_error);
                dos_mkfs(daddr, dsize, dlabel);
                part_data_sync();
                break;
        case P_SEVERAL:
		if (verbose_flag){
		   printf(" mkfp: All existing data on media will be lost\n");
        	   printf(" mkfp: continue ? (y/n) ");
        	   if ((c = getchar()) != 'y')
		   	exit (0);
		}
                for (indx = 0; indx < pnum; indx++){
                   dsize = ptbl[indx].psize;
                   error = part_create(&dsize, &daddr);
		   Throw(error, ecode, MKFP_PART_CREATE, is_error);
                   dos_mkfs(daddr, dsize, ptbl[indx].plabel);
                }
                part_data_sync();

        }
	close (fd);
        return (MKFP_NONE);
is_floppy:
	/* Control is transferred to this point if */
	/* The format request is either:  */
	/* (1) A floppy/floptical request */
	/* (2) A HFS request              */
	/* (3) A virtual disk format request */
	close (fd);
	return (MKFP_FLOPPY);
is_error:
        return (ecode);
}

static void 	init_labelfunc(void)
{
    vlabelchkfunc[MAC_HFS] = mac_volabel;
    vlabelchkfunc[DOS_FAT] = dos_volabel;
}

/*
 *-----------------------------------------------------------------------------
 * extract_part_info()
 * This routine parses strings of the form: size[K|M]:label
 *-----------------------------------------------------------------------------
 */
int     extract_part_info(char *pstr, u_long *dsize, char *dlabel)
{
        int     units = 1;
        char    size_str[20];
        char    *t1, *t2;

        for (t1 = pstr, t2 = size_str; *t1 != '\0' && *t1 != ':'; t1++, t2++)
                *t2 = *t1;
        *t2 = '\0';

        t1 = strchr(pstr, ':');
        if (!t1)
                strcpy(dlabel, "Untitled");
        else    strcpy(dlabel, t1+1);

        if (size_str[strlen(size_str)-1] == 'k' ||              
            size_str[strlen(size_str)-1] == 'K'){         
                size_str[strlen(size_str)-1] = '\0';
                units = 1024;
                *dsize = atoi(size_str)*units;
        }               
        else if (size_str[strlen(size_str)-1] == 'm' ||         
                 size_str[strlen(size_str)-1] == 'M'){          
                units = 1024*1024;
                size_str[strlen(size_str)-1] = '\0';
                *dsize = atoi(size_str)*units;
        }
        else    *dsize = atoi(size_str);
        return (0);
}

/*
 *-----------------------------------------------------------------------------
 * print_error()
 *-----------------------------------------------------------------------------
 */
void	print_error(int ecode)
{
	switch (ecode){
	case MKFP_USAGE:
		gerror("Command line");
		break;
	case MKFP_SCSI_QUERY:
		gerror("SCSI Query");
		break;
	case MKFP_PART_CREATE:
		gerror("Partition creation");
		break;
	case MKFP_NO_FS:
		gerror("Target file system not specified");
		break;
	case MKFP_NO_FMT:
		gerror("Low level formatting for this media disallowed");
		break;
	}
	return;
}

