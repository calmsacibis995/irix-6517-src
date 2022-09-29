/**************************************************************************
 *									  *
 *	 	Copyright (C) 1986-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded	instructions, statements, and computer programs	 contain  *
 *  unpublished	 proprietary  information of Silicon Graphics, Inc., and  *
 *  are	protected by Federal copyright law.  They  may	not be disclosed  *
 *  to	third  parties	or copied or duplicated	in any form, in	whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * this is the subset of flashio functionality for the speed racer chassis
 * we are able to update the flash prom while running unix, and that is
 * the use of this command.
 *
 * the design is such that we actually have two sets of code loaded into
 * the flash prom, a small, 2 segment "recovery prom", and a large, 13
 * segment "flash prom".  the recovery prom is for re-programming the
 * "flash prom" segments in case of a failed programming attempt.
 *
 * as for programming the segments, we've decided that there are two
 * basic ways of programming the two sets of code ...
 *	<normal>	programs just the "flash prom"
 *	-R		programs both the "recovery" and "flash" areas
 */

/*
 * documented SN0 options
 *
 * -f/-F:  [ignored] SN0 used -f/-F to allow operator to load some 
 *         configuration parameters at runtime ... overriding values 
 *         in the current prom or the prom image.  we have no need for
 *         such support.
 * -i/-c/-n [ignored] RACER has just the main system board to be flashed
 * -m # -s # [ignored] ditto
 * -p:     pull image from the named directory, not default
 * -P:     use provided file as the image, don't bother to look for it
 * -S:     [ignored] only one prom, so can't parallelize  :)
 * -d:     dump prom header from file.  works with -p & -P, or pulls it
 *         from the default location.
 * -D:     dumps the currently loaded prom to a file.  file gets dumped
 *         into the default directory unless -p is specified, then the
 *         file is dumped into that directory.  if -P is specified, then
 *         dump into the specified file.  [would have been so much nicer
 *         if they'd just 'cat /hw/.../flash > file']
 * -V:     Verifies prom: just dumps some information out of it.
 *         RACER will print out the header information of the currently
 *         loaded prom in response to this option.
 * -v:     verbose ... print lots of messages.
 * -o:     override, load this value regardless of current level
 *
 * new options to handle the PDS section
 * -L:     dump flash PROM logging information (put in by ide, etc)
 * -N:	   nvram/sgikopts mode.  get/set PROM environment variables
 *         that are stored in the flash
 * -E:     use the verbose "x=y" style for -N variables.
 */

#define	USAGE	\
"flash [-d] [-D] [-V] [-v] [-o] [ [-P file] | [-p DIR] ]\n"	\
    "  Flash the IP30 system prom from the appropriate file in directory\n" \
    "  or directly from the file specified with -P filename\n" \
    "    -d print prom header information from file to stdout\n" \
    "    -D dump current prom contents to file\n" \
    "    -V print information about current prom\n" \
    "    -v verbose, print messages to stdout as operation progresses\n" \
    "    -o force override, load image even if older than current\n" \
    "    -P filename, load image from provided filename\n" \
    "    -p dirname, look in directory dirname from image to load\n" \
"flash -L\n"	\
    "  Transfer logged information from Flash PROM into syslog\n" \
"flash -N [-E] [variable [value]]\n"	\
    "  Get or set PROM environment variables that reside in the Flash PROM\n" \
    "    -E get values in a verbose manner (i.e. variable=value)\n"


#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <malloc.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/RACER/sflash.h>
#include <syslog.h>
#include <sys/syssgi.h>

#define STD_PROM_SIZE		(SFLASH_FPROM_SIZE)
#define STD_PROM_OFF		SFLASH_SEG_OFF(SFLASH_FPROM_SEG)

#define RECOV_PROM_SIZE		(SFLASH_RPROM_SIZE)
#define RECOV_PROM_OFF		SFLASH_SEG_OFF(SFLASH_RPROM_SEG)
#define RECOV_PROM_HDR_OFF	(RECOV_PROM_OFF + SFLASH_RPROM_HDR_OFF)

#define MAX_PROM_SIZE		(STD_PROM_SIZE + RECOV_PROM_SIZE)

#define	FLASHDEV		"/hw/node/xtalk/15/flash"

#define	DEFAULT_DIR		"/usr/cpu/firmware"
#define	DEFAULT_FILE		"IP30prom.bin"

/*
 * ATTENTION:
 * this set of reserved variables needs to be kept synchronized
 * between the nvram command, this flash command, and the set
 * of variables defined to be resident in the flash PROM in the
 * file irix/kern/sys/RACER/IP30nvram.h
 *
 * these variables are specially defined to be allowed to be set
 * into the flash PROM, even if they did not exist in the past.
 */
char * ip30ReservedVars[] = 	{ "OSLoadPartition",	"OSLoader",
				  "OSLoadFilename",	"SystemPartition",
				  "OSLoadOptions",	"fastfan",
				  NULL
				};

#ifndef TRUE
#define	TRUE	1
#define FALSE	0
#endif	/* !defined TRUE */

/* "globals" structure */
typedef struct racerGlobals_s {
	int fd;					/* flash prom fd */
	int verbose;				/* verbosity */
	char * appname;				/* application name */
	volatile uint16_t * FlashPromBase;	/* base mmap'd address */
	volatile uint16_t * pdsBegin;		/* base address of PDS */
	volatile uint16_t * pdsBase;		/* address of first PDS entry*/
	void * pdsEnd;				/* first bad address past PDS*/
} racerGlob_t;


/* prototypes */
void racer_Usage(void);
int racer_MapFlash( racerGlob_t*, int, int* );
int racer_GetFlashVersion( racerGlob_t*, flash_header_t * ); 
int racer_GetRpromVersion( racerGlob_t*, flash_header_t * ); 
int racer_ReadFlashFile(char *filename,flash_header_t*,flash_header_t*);
void racer_CompareFlash( racerGlob_t*, int *, int *);
int racer_CompareRprom( racerGlob_t* );
int racer_FlashProm( racerGlob_t*, int ); 
int racer_FlashErase(volatile uint16_t *addr);
int racer_CheckStatus(volatile uint16_t *addr);
int racer_WriteHWord(volatile uint16_t *addr, uint16_t w);
int racer_VerifyPart( racerGlob_t *, int * );
uint16_t in_cksum(uint8_t *data, int len, uint32_t  sum);
int racer_HdrOk(flash_header_t *h);
int racer_PdsHdrOk(flash_pds_hdr_t *h);
void racer_sigAlarm();
int racer_VerifyProm(racerGlob_t*, int rprom);
int racer_DumpVersion( racerGlob_t* );
int racer_initPDS( racerGlob_t* );
int racer_pds_setValue( racerGlob_t * racer, char *arg1, char *arg2 );
int racer_pds_getValue( racerGlob_t * racer, char * arg1, int nvram_verbose );
int racer_pds_dumpValues( racerGlob_t * racer );
int racer_pds_dumpLog( racerGlob_t * racer );
int racer_is_reserved( const char *var, char *const *reserved);
int racer_CopyFlash(racerGlob_t* racer);
int pds_compress( racerGlob_t * pds );
int pds_writeentry(flash_pds_ent_t * freespace, char * string);

/* globals */
int racer_alert = 0;			/* did alarm clock bing? */
char * buffer;				/* buffer for data to load */

/*
 * speed racer flash support
 *
 * as of this writing, the only flash that is able to be programmed
 * on the speed racer platform is the main system board flash ... the
 * one that the cpu fetches instructions from when starting up
 *
 * this chunk of code has the functionality to
 *   a) flash new executable code into the flash prom
 *   b) access and modify PROM environment variables stored in the flash
 *   c) dump logging information stored in the flash
 *
 * in the case of the PROM environment variables, this command will
 * be invoked as a backend to the nvram/sgikopt command, so in the case
 * that "verbose" is not turned on, be absolutely quiet.
 */
int
flashio_racer(int argc, char **argv) 
{
	int c;				/* step through command line opts */
	int rc;				/* function return code */
	int force = 0; 			/* flash with older software */
	int rprom = 0;			/* flash a Recovery Prom */
	int version = 0;		/* print current version info */
	int show_file_header = 0;	/* show file header information */
	int dump_prom = 0;		/* dump current contents to a file */
	int diff_rprom;			/* file and current are different */
	int diff_fprom;			/* file and current are different */
	char * srcfile;			/* for -P option */
	char * srcdir = DEFAULT_DIR;	/* for -p option */
	char filename[PATH_MAX];	/* filename to compare/load */
	struct sigaction sigact;	/* set of signals to catch */
	flash_header_t rheader, fheader;/* file version of the headers */
	racerGlob_t racer;		/* variables used throughout racer */
	int error = 0;			/* errno carrier on func failures */
	int flashing = 0;		/* -P or -p specified? */
	int nmode = 0;			/* number of different modes selected */
	int map_readonly = TRUE;	/* map the flash readonly */

	/* log dumping, nvram support */
	int logdump = 0;		/* dump logging information */
	int nvram = 0;			/* nvram mode */
	int nvram_verbose = 0;		/* verbose nvram mode */
	char * arg1 = NULL;		/* nvram: optional cmd line arg1 */
	char * arg2 = NULL;		/* nvram: optional cmd line arg2 */

	/*
	 * initialize the racerGlob structure
	 */
	racer.verbose = 0;			/* quiet */
	racer.FlashPromBase = (volatile uint16_t *)-1;	/* un-initialized */
	racer.pdsBegin = (volatile uint16_t *)-1;	/* un-initialized */
	racer.pdsBase = (volatile uint16_t *)-1;	/* un-initialized */
	racer.pdsEnd = (void*) -1;			/* un-initialized */
	racer.appname = "flash";		/* prefix for all messages */

	/*
	 * setup an alarm clock handler.  we use the alarm clock 
	 * to ensure that we don't wait too long for any one step
	 * of programming the flash
	 */
	sigemptyset( &sigact.sa_mask );		/* catch just SIGALRM */
	sigact.sa_flags = 0;
	sigact.sa_handler = racer_sigAlarm;
	sigaction( SIGALRM, &sigact, NULL );

	/*
	 * parse the command line arguments
	 * see above for the valid command line layout
	 */
	while ((c = getopt(argc, argv, "vVoP:dDcfFimnp:sSLNEK")) != EOF) {
		switch (c) {
			case 'd':
				show_file_header = 1;
				break;
			case 'D':
				dump_prom = 1;
				break;
			case 'P':
				flashing = 1;
				srcfile = optarg;
				break;
			case 'v':
				racer.verbose += 1;
				break;
			case 'o':
				force = 1;
				break;
			case 'V':
				version = 1;
				break;
			case 'p':
				flashing = 1;
				srcdir = optarg;
				break;

			case 'L':
				logdump = 1;
				break;
			case 'N':
				nvram = 1;
				break;
			case 'E':
				nvram_verbose = 1;
				break;

			case 'c':
			case 'f':
			case 'F':
			case 'i':
			case 'm':
			case 'n':
			case 's':
			case 'S':
				/* silently ignored SN0 options */
				break;
			default:
				racer_Usage();
				break;
		}
	}

	/*
	 * check that we're doing only one major function:
	 *   flashing the prom, handling nvram, or dumping logs
	 */
	nmode = 0;
	if (show_file_header) nmode++;
	if (dump_prom) nmode++;
	if (version) nmode++;
	if (logdump) nmode++;
	if (nvram) nmode++;

	/*
	 * if more than one of the major functions, or if
	 * we had an option to specify a flash filename, then
	 * we're trying to do too many things ...
	 */
	if ((nmode > 1) || (flashing && (nvram || logdump))) {
		fprintf(stderr,"%s: more than one function specified\n",
								racer.appname);
		racer_Usage();
	}

	/*
	 * pell off the up-to-two additional parameters that
	 * are used by the nvram/sgikopts processing
	 *	arg1 is the prom environment variable to get/set
	 *	arg2 is the value to set that variable to
	 */
	if (optind < argc) {
		arg1 = argv[optind++];
		if (optind < argc) {
			arg2 = argv[optind];
		}
	}

	/*
	 * at this point we should know whether we're going to need
	 * to map the flash in readonly or read/write mode.
	 */
	if ((nvram && (NULL == arg2)) || (version) || 
				(dump_prom) || (show_file_header)) {
		map_readonly = TRUE;
	}
	else {
		map_readonly = FALSE;
	}

	/*
	 * do some common initialization, the minimum that is needed
	 * for the version command to work.
	 */
	if (racer_MapFlash(&racer, map_readonly, &error)) {
		exit( EPERM );
	}

	/*
	 * if it looks like we're going to write to the part, then
	 * we'd better check to make sure that it's the part we're
	 * expecting!
	 */
	if (FALSE == map_readonly) {
		if (racer_VerifyPart( &racer, &error )) {
			exit( ENODEV );
		}
	}

	/*
	 * short circuit if we're going to just dump the current
	 * information out of the prom
	 */
	if (version) {
		racer_DumpVersion( &racer );
		return 0;
	}

	/*
	 * determine the (basic) filename that we're going to access.
	 * it's based on -P/-p/DIR and defaults  (yeck)
	 * (note we get the name here, but it's each option's job to
	 * check the name vs. the operation to be performed)
	 */
	if (srcfile != NULL) {
		strncpy(filename, srcfile, PATH_MAX);
	}
	else if (srcdir != NULL) {
		int spaceleft;
		strncpy(filename, srcdir, PATH_MAX);
		spaceleft = (PATH_MAX - strlen(filename)) - 2;
		if (spaceleft > 0) {
			strcat( filename, "/" );
			strncat( filename, DEFAULT_FILE, spaceleft );
		}
	}

	/*
	 * if we're actually dumping the current prom contents to a
	 * file, use either the filename provided, or the default filename
	 * with ".dump" appended.
	 */
	if (dump_prom) {
		int fd, rc;
		if (srcfile == NULL) {
			int spaceleft;
			spaceleft = PATH_MAX - strlen(filename) - 1;
			if (spaceleft > 0) strncat(filename,".dump",spaceleft);
		}

		if (racer.verbose) {
			printf("Dumping current Prom to file %s\n",filename);
		}

		fd = open( filename, O_WRONLY | O_CREAT | O_TRUNC, 0600 );
		if (fd < 0) {
			fprintf(stderr,"Failed to open output file %s\n",
								filename);
			perror("open");
			return 0;
		}

		rc = write(fd, (char *)racer.FlashPromBase, SFLASH_MAX_SIZE);
		if (rc != SFLASH_MAX_SIZE) {
			fprintf(stderr,"Dump failed, wrote %d bytes "
				"(%d expected)\n",rc,SFLASH_MAX_SIZE);
			perror("write");
			return 0;
		}
		return 0;
	}

	/*
	 * do the PDS region initialization here, but only do that
	 * if we're going to be using the PDS (ie dumping logging
	 * information or tweaking PROM variables)
	 */
	if ((logdump) || (nvram)) {
		if (racer_initPDS(&racer)) {
			syslog(LOG_INFO, "flash: PDS segment is invalid\n");
			exit( ENODEV );
		}
	}
	
	/*
	 * if we're dumping the logging information, take that
	 * branch here ...
	 */
	if (logdump) {
		return racer_pds_dumpLog( &racer );
	}

	/*
	 * and handle the nvram variables here too ...
	 */
	if (nvram) {
		if (NULL == arg1) {
			/* dump all the values */
			rc = racer_pds_dumpValues( &racer );
		}
		else if (NULL == arg2) {
			/* dump just arg1 value */
			rc = racer_pds_getValue( &racer, arg1, nvram_verbose );
		}
		else {
			/* set arg1 to arg2 */
			rc = racer_pds_setValue( &racer, arg1, arg2 );
		}

		return rc;
	}

	/*
	 * malloc the largest size buffer that we might need, and
	 * fill it with -1 values (so any region not programmed contains
	 * the "erased" value for the hardware)
	 */
	if ((buffer = (char *) malloc(MAX_PROM_SIZE)) == NULL) {
		fprintf(stderr, "flash: failed to malloc "
				"internal memory buffer\n");
		perror("malloc");
		return 1;
	}
	memset( buffer, -1, MAX_PROM_SIZE );

	if (racer_ReadFlashFile(filename,&rheader,&fheader)) 
		exit(1);

	/*
	 * show information about the file we just loaded
	 */
	if (show_file_header) {
		if (racer.verbose) {
			printf("PROM header contents (file %s):\n"
			       "  Magic:   0x%x\n"
			       "  Version: %d.%d\n"
			       "  Length:  %d bytes\n", filename,
					fheader.magic, (fheader.version >> 8),
					(fheader.version & 0xff), 
					fheader.datalen);

			printf("Recovery PROM contents:\n"
			       "  Magic:   0x%x\n"
			       "  Version: %d.%d\n"
			       "  Length:  %d bytes\n",
					rheader.magic, (rheader.version >> 8),
					(rheader.version & 0xff), 
					rheader.datalen);
		}

		return 0;
	}

	/*
	 * if the contents of the file we loaded are newer than the
	 * contents of the currently loaded flash, load the new bits.
	 */
	racer_CompareFlash( &racer, &diff_rprom, &diff_fprom);

	if ((diff_rprom < 0) || (diff_fprom < 0) || force) {
		/*
		 * are we going to flash the rprom too?
		 * (only program rprom if it is different from loaded version)
		 */
		openlog("flash", 0, LOG_USER);

		if ((diff_rprom < 0) || (force && diff_rprom)) {
			rprom = 1;
		}

		/* dump current flash prom contents to foo.O */
		racer_CopyFlash( &racer );

		if (racer.verbose)
			printf("flash: Programming %s from file %s\n",
				((rprom)?"RPROM and PROM":"PROM"), filename);

		if (racer_FlashProm(&racer, rprom)) {
			fprintf(stderr,"flash: PROM PROGRAMMING FAILED\n");
			fprintf(stderr,"flash: re-attempt programming "
							"is suggested\n");
			syslog(LOG_CRIT,"IP30 PROM programming failed\n");
			closelog();
			return -1;
		} else {
			if (rprom) {
				syslog(LOG_INFO,"IP30 RPROM version "
					"%d.%d loaded.\n", 
					(rheader.version >> 8),
                                        (rheader.version & 0xff));
			}

			syslog(LOG_INFO,"IP30 PROM version "
				"%d.%d loaded.\n", (fheader.version >> 8),
                                        (fheader.version & 0xff));
			if (racer.verbose) {
				printf("flash: %s programming "
					"completed successfully\n",
					((rprom)?"RPROM and PROM":"PROM"));
			}
		}

		closelog();
	} else {
		if (racer.verbose) {
		    printf("flash: PROM programming canceled.\n");
		    printf("flash: currently loaded PROM is %s %s\n",
			((diff_fprom > 0)?"newer than":"the same as"),filename);
		    printf("flash: currently loaded RPROM is %s %s\n",
			((diff_rprom > 0)?"newer than":"the same as"),filename);
		}
	}
	
	return 0;
}


void 
racer_Usage(void) 
{
	fprintf(stderr, USAGE );
	exit(1);
}

/*
 * racer_MapFlash
 *
 * mmap's the flash PROM device into the address space of this
 * process.  takes as an argument whether to map the PROM in a
 * read/write or readonly mode.
 *
 * on success return 0
 * on error returns -1 with error set to the errno of the failure
 */

int
racer_MapFlash( racerGlob_t * globals, int map_readonly, int * error )
{
	int open_options;		/* file open readonly/read-write */
	int map_options;		/* file map readonly/read-write */

	/*
	 * setup whether we're opening this device readonly or read-write 
	 */
	if (map_readonly == TRUE) {
		open_options = O_RDONLY;
		map_options = PROT_READ;
	}
	else {
		open_options = O_RDWR;
		map_options = PROT_READ | PROT_WRITE;
	}

	if ((globals->fd = open(FLASHDEV, open_options)) < 0) {
		*error = errno;
		if (globals->verbose)
			fprintf(stderr, "flash: unable to open %s,"
					" errno %d\n", FLASHDEV, errno);
		return -1;
	}

	globals->FlashPromBase = (volatile uint16_t *) 
		mmap(0, SFLASH_MAX_SIZE, map_options, MAP_SHARED, 
							globals->fd, 0);
	
	if (globals->FlashPromBase == (volatile uint16_t *) -1) {
		*error = errno;
		if (globals->verbose)
			perror("flash: mmap failed");
		return -1;
	}

	return 0;
}


int 
racer_GetFlashVersion( racerGlob_t * racer, flash_header_t * fheader )
{
	flash_header_t *fh = (flash_header_t*)
				((char*)racer->FlashPromBase+STD_PROM_OFF);
	int hdr_ok = racer_HdrOk(fh);

	if (hdr_ok) {
		if (fheader != NULL) {
			*fheader = *fh;
		}
	}
	else {
		if (racer->verbose)
			printf("flash: PROM has invalid header checksum\n");
	}

	return ((hdr_ok) ? fh->version : 0);
}

int 
racer_GetRpromVersion(racerGlob_t * racer, flash_header_t * fheader) 
{
	flash_header_t *fh = (flash_header_t*)
			((char*)racer->FlashPromBase+RECOV_PROM_HDR_OFF);
	int hdr_ok = racer_HdrOk(fh);

	if (hdr_ok) {
		if (fheader != NULL) {
			*fheader = *fh;
		}
	}
	else {
		if (racer->verbose)
			printf("flash: Recovery PROM has invalid "
							"header checksum\n");
	}

	return ((hdr_ok) ? fh->version : 0);
}




/*
 * racer_ReadFlashFile
 *
 * load an image to flash out of the provided filename.
 * the file must be a combined rprom+fprom image file.
 */
int
racer_ReadFlashFile(char *filename,
		flash_header_t *rheader, flash_header_t * fheader)
{
	int fd;
	off_t offset;			/* seek location for magic */
	flash_header_t fh;

	if ((fd = open(filename, O_RDONLY)) < 0) {
		fprintf(stderr, "flash: failed to open PROM binary %s\n",
								filename);
		exit(1);
	}

	/*
	 * if we're loading an rprom, check that there are both an
	 * rprom and fprom portion.
	 */
	/* rprom has a flash_header_t at the end of the data */
	offset = RECOV_PROM_HDR_OFF;
	(void) lseek(fd, offset, SEEK_SET);
	if ((read(fd, (void *)&fh, sizeof(fh)) != sizeof(fh)) || 
							!racer_HdrOk(&fh)) {
		fprintf(stderr,"flash: %s is not an IP30prom "
			"(no Recovery PROM)\n",filename);
		return -1;
	}

	if (rheader != NULL) *rheader = fh;	/* structure copy */

	/* rprom magic is good ... now check fprom. it is next */
	offset = STD_PROM_OFF;
	(void) lseek(fd, offset, SEEK_SET);
	if ((read(fd, (void *)&fh, sizeof(fh)) != sizeof(fh)) || 
							!racer_HdrOk(&fh)) {
		fprintf(stderr,"flash: %s is not an IP30prom "
			"(no PROM image)\n",filename);
		return -1;
	}

	if (fheader != NULL) *fheader = fh;	/* structure copy */

	(void) lseek(fd, 0, SEEK_SET);	/* set fd offset */

	/* fd is pointing to the start of the data to load into buffer */
	if (read(fd, buffer, MAX_PROM_SIZE) < 0) {
		fprintf(stderr, "flash: failed to read prom binary\n");
		perror("flash: read");
		exit(1);
	}

	close(fd);

	return 0;	
}


/*
 * racer_CompareFlash
 *
 * compares the fprom portion of the prom to be downloaded to
 * the currently programmed fprom portion.
 *
 * this code does not check the recovery portion versions ...
 *
 * returns:
 *	0	current and to-be-loaded are the same version
 *	1	to-be-loaded is older (lower version) than current
 *     -1	to-be-loaded is newer (higher version) than current
 */
void 
racer_CompareFlash(racerGlob_t* racer, int * rprom, int * fprom)
{
	int current_fprom = racer_GetFlashVersion( racer, NULL );
	int current_rprom = racer_GetRpromVersion( racer, NULL );
	flash_header_t *fh;

	fh = (flash_header_t *)(buffer + STD_PROM_OFF);

	if (fh->version < current_fprom) *fprom = 1;
	else if (fh->version == current_fprom) *fprom = 0;
	else *fprom = -1;

	fh = (flash_header_t *)(buffer + RECOV_PROM_HDR_OFF);

	if (fh->version < current_rprom) *rprom = 1;
	else if (fh->version == current_rprom) *rprom = 0;
	else *rprom = -1;
}

/*
 * racer_CompareRprom
 *
 * compares the rprom portion of the prom to be downloaded to
 * the currently programmed rprom portion.
 *
 * returns:
 *	0	current and to-be-loaded are the same version
 *	1	to-be-loaded is older (lower version) than current
 *     -1	to-be-loaded is newer (higher version) than current
 */
int 
racer_CompareRprom( racerGlob_t* racer)
{
	int current = racer_GetRpromVersion( racer, NULL );
	flash_header_t *fh;

	fh = (flash_header_t *)((char *)buffer + RECOV_PROM_HDR_OFF);

	if (fh->version == current)
		return 0;
	if (fh->version < current)
		return 1;

	return -1;
}

/*
 * racer_UpdateHeader
 *
 * Update the flash header timestamp and with some information
 * from the previous header.
 */
void
racer_UpdateHeader(racerGlob_t * racer, int is_rprom, flash_header_t *nfh)
{
	time_t ut;
	struct tm *gmt;
	int nvvar;			/* which variable to get */

	ut = time(NULL);
	gmt = gmtime(&ut);

	nvvar = (is_rprom) ? SFL_INCGET_RPROM : SFL_INCGET_FPROM;
	nfh->nflashed = ioctl(racer->fd, nvvar);

	nfh->timestamp.Year    = gmt->tm_year + 1900;
	nfh->timestamp.Month   = gmt->tm_mon + 1;
	nfh->timestamp.Day     = gmt->tm_mday;
	nfh->timestamp.Hour    = gmt->tm_hour;
	nfh->timestamp.Minutes = gmt->tm_min;
	nfh->timestamp.Seconds = gmt->tm_sec;
	nfh->timestamp.Milliseconds = 0;

	nfh->hcksum = 0;
	nfh->hcksum = ~in_cksum((void *)nfh, sizeof(*nfh), 0);
}

int 
racer_FlashProm(racerGlob_t*racer, int rprom) 
{
	int i = 0;			/* # of shorts already written */
	int err = 0;			/* errors detected */
	uint16_t *buf = (uint16_t *)buffer;
	struct sigaction sigact;	/* set of signals to block */
	volatile uint16_t *fprom_addr;	/* base of fprom programming */
	volatile uint16_t *rprom_addr;	/* base of rprom programming */

#if 0
	flash_header_t *fh;		/* used during header updates */
#endif

	if (rprom) {
#if 0
		fh = (flash_header_t *)((char*)racer->FlashPromBase + 
							RECOV_PROM_HDR_OFF);
#endif

		racer_UpdateHeader(racer, 1, (flash_header_t *)
					((char*)buffer + RECOV_PROM_HDR_OFF));
	}

#if 0
	fh = (flash_header_t *)((char*)racer->FlashPromBase + STD_PROM_OFF);
#endif

	racer_UpdateHeader(racer, 0, (flash_header_t *)
					((char*)buffer + STD_PROM_OFF));

	/*
	 * we do not want anyone interrupting us while we're
	 * actually programming the flash ... we're kinda in a
	 * critical section of code that could hose the system
	 * if it is not completed.
	 */
	sigemptyset( &sigact.sa_mask );
	sigact.sa_flags = 0;
	sigact.sa_handler = SIG_IGN;

	sigaction( SIGHUP, &sigact, NULL );
	sigaction( SIGINT, &sigact, NULL );
	sigaction( SIGQUIT, &sigact, NULL );
	sigaction( SIGTERM, &sigact, NULL );

	/*
	 * step 1, erase the first segment of the FPROM, where the
	 * FPROM header lives.  we do this to guarantee that the FPROM
	 * header will be invalid in case we somehow are interrupted.
	 */
	fprom_addr = (volatile uint16_t *)
				((char*)racer->FlashPromBase + STD_PROM_OFF);
	racer_FlashErase( fprom_addr );

	/*
	 * step 2, optionally program the rprom.  RPROM header is
	 * at the end of the RPROM, so we erase all segments of the
	 * RPROM, and then program from low address to high.
	 */
	if (rprom) {
		buf = (uint16_t *)buffer;
		rprom_addr = (volatile uint16_t *)
				((char*)racer->FlashPromBase + RECOV_PROM_OFF);
		for (i=0; i<SFLASH_RPROM_NSEGS; i++) {
			racer_FlashErase( (uint16_t*) 
				(((char*)rprom_addr) + (i * SFLASH_SEG_SIZE)));
		}

		for (i=0; i<(SFLASH_RPROM_NSEGS*SFLASH_SEG_SIZE); i++) {
			if (racer_WriteHWord(rprom_addr++, *buf++)) {
				err = 1;
				break;
			}
		}
	}

	/*
	 * step 3, program the fprom code segment. for each segment
	 * to be programmed erase it.  then we'll program from the
	 * high addresses down to the low address ... and that way
	 * we'll update the FPROM header last (because it's all the
	 * way at the start of the FPROM area).
	 */
	for (i=0; i<SFLASH_FPROM_NSEGS; i++) {
		racer_FlashErase( (uint16_t *)
				((char*)fprom_addr + (i * SFLASH_SEG_SIZE)) );
	}

	buf = (uint16_t *) ((char *)buffer + 
			(SFLASH_RPROM_NSEGS * SFLASH_SEG_SIZE) +
			(SFLASH_FPROM_NSEGS * SFLASH_SEG_SIZE));

	fprom_addr = (volatile uint16_t *)
			((char *)racer->FlashPromBase + STD_PROM_OFF +
			(SFLASH_FPROM_NSEGS * SFLASH_SEG_SIZE));


	/*
	** XXX -- currently writes the whole region ... really should
	** write only the region needed (shorten load time too)
	*/
	for (i=0; i<(SFLASH_FPROM_NSEGS*SFLASH_SEG_SIZE); i+=sizeof(int16_t)){
		if (racer_WriteHWord(--fprom_addr, *(--buf))) {
			err = 1;
			break;
		}
	}
	
	/*
	 * reinstate the default signal handlers for the
	 * set of signals that we ignored above
	 */
	sigemptyset( &sigact.sa_mask );
	sigact.sa_flags = 0;
	sigact.sa_handler = SIG_DFL;

	sigaction( SIGHUP, &sigact, NULL );
	sigaction( SIGINT, &sigact, NULL );
	sigaction( SIGQUIT, &sigact, NULL );
	sigaction( SIGTERM, &sigact, NULL );
	

	/*
	 * Need to verify in the flash for both Rprom and Fprom:
	 * (1) headers, racer_HdrOk()
	 * (2) the data cksum 
	 */
	if (!err) {
		err = racer_VerifyProm( racer, rprom );
	}

	return err;
}


int
racer_FlashErase(volatile uint16_t *addr) 
{
	int retval;

	*addr = SFLASH_CMD_ERASE;
	*addr = SFLASH_CMD_ERASE_CONFIRM;

	retval = racer_CheckStatus(addr);

	*addr = SFLASH_CMD_READ;

	return retval;
}

int
racer_CheckStatus(volatile uint16_t *addr) 
{
	volatile uint16_t csr;	
	volatile uint16_t val = 0;
		
	*addr = SFLASH_CMD_STATUS_READ;
	alarm(20);
	while (!(*addr & SFLASH_STATUS_READY) && !racer_alert) 
		;

	alarm(0);

	racer_alert = 0;
	*addr = SFLASH_CMD_STATUS_READ;
	csr = *addr & 0xff;
	if (csr != SFLASH_STATUS_READY)
		return 1;
	return 0;
}


int
racer_WriteHWord(volatile uint16_t *addr, uint16_t w) 
{
	int retval;

	*addr = SFLASH_CMD_WRITE;
	*addr = w;

	retval = racer_CheckStatus(addr);

	*addr = SFLASH_CMD_READ;

	return retval;
}

/*
 * racer_VerifyPart
 *
 * this routine verifies that the memory at the location we have
 * is really one of those parts that we know about ... if it isn't,
 * we're going to have a hard time writing to it.
 *
 * this routine is called only if we think we might write to the
 * flash part  (not called for readonly stuff)
 *
 * this routine returns 0 on success
 * on failure, -1 is returned along with an errno-style value in "error"
 */
int
racer_VerifyPart( racerGlob_t * glob, int * error )
{
	volatile uint16_t *addr = glob->FlashPromBase;
	unsigned short mfg, dev;

	*addr = SFLASH_CMD_ID_READ;
	mfg = addr[0];
	dev = addr[1];
	*addr = SFLASH_CMD_READ;

	if (mfg == SFLASH_MFG_ID && dev == SFLASH_DEV_ID)
		return 0;

	if (glob->verbose) {
		fprintf(stderr,"%s: Unrecognized flash PROM device\n",
			glob->appname);
	}

	syslog(LOG_INFO,"%s: Unrecognized flash PROM hardware device\n",
			glob->appname);

	*error = ENXIO;
	return -1;
}

/*
** checksum_addto
** this is the algorithm used to create the data region checksums
*/
static void
checksum_addto(uint32_t * checksum, char value)
{
	/* Calculate checksum (sum -r algorithm) */
	if (*checksum & 01)
		*checksum = (*checksum >> 1) + 0x8000;
	else
		*checksum >>= 1;

	*checksum += value;
	*checksum &= 0xFFFF;
}

uint16_t
in_cksum(uint8_t *data, int len, uint32_t  sum)
{
	if ((int)data & 1) {
	    sum += *data;
	    data++;
	    len--;
	}
	while (len > 1) {
	    sum  += *((uint16_t *)data);
	    len  -= 2;
	    data += 2;
	}
	if (len) {
	    sum = *data;
	}
	sum = (sum & 0xffff) + (sum >> 16);
	sum = (sum & 0xffff) + (sum >> 16);
	return (uint16_t)sum;
}

/*
 * racer_HdrOk
 *
 * return 1 if the header checksum works out,
 * 0 if the header checksum does not match the data (failure)
 */
int
racer_HdrOk(flash_header_t *h)
{
	if (h->magic == FLASH_PROM_MAGIC &&
	    (0xffff & ~in_cksum((void *)h, sizeof(*h), 0)) == 0)
	    return 1;
	return 0;
}

/*
 * racer_PdsHdrOk
 *
 * return 1 if the PDS header checksum works out,
 * 0 if the header checksum doesn't match the data (failure)
 */
int
racer_PdsHdrOk(flash_pds_hdr_t *h)
{
	if (h->magic == FPDS_HDR_MAGIC &&
		(0xffff & ~in_cksum((void *)h, sizeof(*h), 0)) == 0)
		return 1;
	return 0;
}


/*
 * SIGALRM timeout handler
 *
 * note that we're using POSIX semantics, so we don't need to
 * reinstall the handler  :)
 */
void 
racer_sigAlarm() 
{
	racer_alert = 1;
}


/*
 * verify that the part was flashed correctly.  this checks the
 * header checksums, and also steps through the flashed memory to
 * check the data checksum
 */
int
racer_VerifyProm(racerGlob_t* racer, int rprom)
{
	int i;				/* counter */
	int err = 0;			/* error encountered ? */
	flash_header_t *r_hdr;		/* rprom header */
	flash_header_t *f_hdr;		/* fprom header */
	uint32_t checksum;		/* checksum accumulator */
	volatile uint8_t *byteaddr;	/* address to read for checksum */

	/*
	 * Need to verify in the flash for both Rprom and Fprom:
	 * (1) headers, racer_HdrOk()
	 * (2) the data cksum 
	 */
	r_hdr = (flash_header_t *) ((char*)racer->FlashPromBase +
						RECOV_PROM_HDR_OFF);
	f_hdr = (flash_header_t *) ((char*)racer->FlashPromBase + STD_PROM_OFF);
	if (!racer_HdrOk(r_hdr) || !racer_HdrOk(f_hdr)) {
		fprintf(stderr,"WARNING: flash: headers are invalid. FLASH"
			" PROM is corrupt\n");
		return 1;
	}

	/*
	 * now verify the rprom data section checksum.
	 * rprom data begins at offset zero, and stretches to the length
	 * value stored in the rprom header
	 */
	if (rprom) {
		checksum = 0;
		byteaddr = (volatile uint8_t *) 
				((char*)racer->FlashPromBase + RECOV_PROM_OFF);
		for (i=0; i<r_hdr->datalen; i++) {
			checksum_addto( &checksum, *byteaddr++ );
		}

		if (checksum != r_hdr->dcksum) {
			fprintf(stderr,"WARNING: flash: recovery prom "
				"data region checksum mismatch.\n"
				"FLASH PROM is corrupt\n");
			err = 1;
		}
	}

	/*
	 * and now verify the fprom data section checksum.
	 * fprom data begins at offset STD_PROM_OFF + FLASH_HEADER_SIZE
	 * and run for datalen bytes from fprom header
	 */
	checksum = 0;
	byteaddr = (volatile uint8_t *) ((char*)racer->FlashPromBase +
					STD_PROM_OFF + FLASH_HEADER_SIZE);

	for (i=0; i<f_hdr->datalen; i++) {
		checksum_addto( &checksum, *byteaddr++ );
	}

	if (checksum != f_hdr->dcksum) {
		fprintf(stderr,"WARNING: flash prom data region "
			"checksum mismatch.\nFLASH PROM is corrupt\n");
		err = 1;
	}

	return err;
}

int
racer_DumpVersion( racerGlob_t * racer )
{
	flash_header_t header;
	int rvers;
	int fvers = racer_GetFlashVersion(racer, &header);

	if (fvers > 0) {
	    printf("PROM header information (presently loaded)\n"
		"  Magic:      0x%x\n"
		"  Version:    %d.%d\n"
		"  Programmed: %d/%02d/%4d %02d:%02d:%02d GMT\n"
		"  Length:     %d bytes\n"
		"  Reloads:    %d\n",
			header.magic,
			(header.version >> 8),(header.version & 0xff),
			header.timestamp.Month, header.timestamp.Day,
			header.timestamp.Year, header.timestamp.Hour,
			header.timestamp.Minutes, 
			header.timestamp.Seconds,
			header.datalen, header.nflashed);
	}
	
	rvers = racer_GetRpromVersion(racer, &header);
	if (rvers > 0) {
	    printf("Recovery PROM header information "
						"(presently loaded)\n"
		"  Magic:      0x%x\n"
		"  Version:    %d.%d\n"
		"  Programmed: %d/%02d/%4d %02d:%02d:%02d GMT\n"
		"  Length:     %d bytes\n"
		"  Reloads:    %d\n",
			header.magic,
			(header.version >> 8),(header.version & 0xff),
			header.timestamp.Month, header.timestamp.Day,
			header.timestamp.Year, header.timestamp.Hour,
			header.timestamp.Minutes,
			header.timestamp.Seconds, 
			header.datalen, header.nflashed);
	}

	return 0;
}

/*
 * racer_initPDS
 *
 * initialize the Persistent Data Storage area variables in
 * the racerGlob_t structure
 *
 * returns 0 on success, -1 on failure (ie bogus flash PDS)
 */
int
racer_initPDS( racerGlob_t * racer )
{
	racer->pdsBegin = (volatile uint16_t*) ((char*)racer->FlashPromBase
				+ SFLASH_SEG_OFF(SFLASH_PDS_SEG));

	racer->pdsEnd = (void *) ((char*)racer->FlashPromBase
				+ SFLASH_SEG_OFF(SFLASH_PDS_SEG)
				+ (SFLASH_PDS_NSEGS * SFLASH_SEG_SIZE));

	if (!racer_PdsHdrOk((flash_pds_hdr_t*)racer->pdsBegin)) return -1;

	racer->pdsBase = (uint16_t *)(((char*)racer->pdsBegin) 
				+ sizeof(flash_pds_hdr_t));

	return 0;
}

/*
 * racer_pds_getValue
 *
 * goes looking for string in the pds section.  if it is found,
 * prints the current value, with how the value is printed being
 * based on the verbose and koptsmode parameters
 */
int
racer_pds_getValue( racerGlob_t * pds, char * string, int verbose )
{
	char * ptr;
	int found = 0;			/* did we find the entry? */
	flash_pds_ent_t * p;
	char tmpstring[FPDS_ENT_MAX_DATA_HLEN * sizeof(int16_t)];

	strcpy(tmpstring, string);
	strcat(tmpstring, "=");

	p = (flash_pds_ent_t *)(pds->pdsBase);

	while ((p->pds_type_len != FPDS_TYPELEN_FREE) && 
					(p < (flash_pds_ent_t *) pds->pdsEnd)) {
		if ((p->u.type_len & FPDS_ENT_ENV_TYPE) && (p->valid)) {
			if (strncasecmp(tmpstring, (char *)p->data, 
						strlen(tmpstring)) == 0) {
				found = 1;
				if (verbose) {
					printf("%s\n", (char *) p->data);
				}
				else {
					ptr = strchr((char*)p->data, '=');
					if (*ptr) ptr++;
					printf("%s\n",ptr);
				}
				break;
			}
		}
		FPDS_NEXT_ENT(p);
	}

	if (found)
		return 0;
	else
		return EINVAL;
}

/*
** pds_invalidate
**
** takes a pds entry and invalidates it.
*/ 
void
pds_invalidate(flash_pds_ent_t * p)
{
	if ((p->valid != FPDS_ENT_VALID) ||
				(p->pds_type_len == FPDS_TYPELEN_FREE)) {
		/* shouldn't happen */
		return;
	}
	else {
		racer_WriteHWord(&(p->valid), 0);
	}
}

/*
 * racer_pds_setValue
 *
 * finds the param string in the pds and sets the value to "value"
 * if string does not exist, fail.  if the PDS is too full, then we
 * need to suck everything out, erase the PDS segment, and write 
 * the valid stuff back in.
 */
int
racer_pds_setValue( racerGlob_t * pds, char * param, char * value )
{
	int rc;				/* sub-function return values */
	int len = 0;			/* length of new entry */
	int found = 0;			/* did we find it? */
	flash_pds_ent_t * p;
	char tmpstring[FPDS_ENT_MAX_DATA_HLEN * sizeof(int16_t)];

	/*
	** first check that the new string fits into the
	** restricted space for a prom variable ...
	** param + '=' + value + '\0'
	*/
	if ((strlen(param) + strlen(value) + 2) > 
				(FPDS_ENT_MAX_DATA_HLEN * sizeof(int16_t))) {
		return E2BIG;
	}

	/* 
	** find the string in the set of current variables.
	** if found, invalidate it
	*/
	strcpy(tmpstring, param);
	strcat(tmpstring, "=");

	p = (flash_pds_ent_t *)(pds->pdsBase);

	while ((p->pds_type_len != FPDS_TYPELEN_FREE) && 
					(p < (flash_pds_ent_t *) pds->pdsEnd)) {
		if ((p->u.type_len & FPDS_ENT_ENV_TYPE) && (p->valid)) {
			if (strncasecmp(tmpstring, (char *)p->data, 
						strlen(tmpstring)) == 0) {
				found = 1;
				pds_invalidate(p);
				break;
			}
		}
		FPDS_NEXT_ENT(p);
	}

	/*
	** if we didn't find the string, then we need to bail
	** (we don't allow them to create new variables at the
	** unix level).
	**
	** if we found it, but the second parameter is null, then
	** just unsetting the variable is enough ...
	**
	** 493692: there are a few PROM variables that were moved
	**	   from the Dallas part into the Flash, and they
	**	   don't need to have been present in the Flash
	**	   for us to set them (there are default values
	**	   compiled into the PROM).
	*/
	if ( !found && !racer_is_reserved(param, ip30ReservedVars) ) {
		return -1;
	}

	if ((value == NULL) || (*value == '\0')) {
		return 0;
	}

	strcpy(tmpstring, param);
	strcat(tmpstring, "=");
	strcat(tmpstring, value);

	/*
	** find some empty space and add a new entry ... 
	** if it will fit.
	**
	** 493692: start back at the beginning of PDS
	*/
	p = (flash_pds_ent_t *)(pds->pdsBase);
	while ((p->pds_type_len != FPDS_TYPELEN_FREE) && 
					(p < (flash_pds_ent_t *) pds->pdsEnd)) {
		FPDS_NEXT_ENT(p);
	}

	/* this entry is no more than this size ... */
	len = strlen(tmpstring) + 2 + sizeof(flash_pds_ent_t);
	if (((char*)p + len) >= (char *) pds->pdsEnd) {
		rc = pds_compress( pds );
		if (rc < 0) return rc;

		/*
		** need to find a pointer to some available space
		*/
		p = (flash_pds_ent_t *)(pds->pdsBase);
		while ((p->pds_type_len != FPDS_TYPELEN_FREE) && 
					(p < (flash_pds_ent_t *) pds->pdsEnd)) {
			FPDS_NEXT_ENT(p);
		}

		/*
		** now just check and make sure that the PDS isn't
		** 100% full
		*/
		if (((char*)p + len) >= (char*)pds->pdsEnd) {
			/* we're full, so fail  :(  */
			return ENOSPC;
		}
	}

	/*
	** if we're here, p points to a free entry of the PDS that
	** is at least large enough to put our entry into ... so go
	** and put this entry there ...
	*/
	rc = pds_writeentry(p, tmpstring);

	/*
	** 493692: for each variable in the reserved set that we
	** 	   successfully write, call syssgi() to set that
	**	   variable in the kernel environment table.
	*/
	if ((0 == rc) && racer_is_reserved(param, ip30ReservedVars)) {
		(void) syssgi(SGI_SETKOPT, param, value);
	}

	return rc;
}

/*
 * racer_pds_dumpLog
 *
 * steps through each entry of the PDS and checks if it is a "LOG"
 * type entry.  if it is, we extract that entry, and dump it into
 * SYSLOG.  mark the entry "consumed".
 *
 * each entry is timestamped with a heart counter value.  the first
 * log of each session will be a TYPE0 log and will contain the time
 * of day and heart counter incrementing rate so that later logs can
 * map to the time of the next log entry.  we should output that time
 * with each entry
 *
 * TYPE1 is a variable length string value
 * TYPE2 is a set of four 64bit integers
 *
 * >>> any unrecognized type will be handled as a TYPE2 (4 integers)
 */
int
racer_pds_dumpLog(racerGlob_t * pds)
{
	flash_pds_ent_t * p;		/* used to step through and extract */
	flash_pds_log_data_t log;	/* a log entry */
	TIMEINFO timeinfo;		/* to timestamp entries */
	struct tm tm_time;		/* tm format of TIMEINFO */
	int latestTicks;		/* heart ticks value for timeinfo */
	int timeMult;			/* time conversion multiplier */
	int timeDiv;			/* time conversion divisor */
	int log_type;			/* what kind of log is it? */
	int latestInitd = 0;		/* if no type0, then no times */

	int convTicks;			/* n ticks between latest and entry */
	struct tm * gmt;		/* time_t for time output */
	time_t secs;			/* converted seconds */
	int msecs;			/* milliseconds */
	int usecs;			/* microseconds */
	time_t latestTT;		/* latest time as a time_t */
	char tstring[128];		/* string representing time */

	/*
	** setup the syslog handler stuff
	*/
	openlog("flash", 0, LOG_USER);

	p = (flash_pds_ent_t *)(pds->pdsBase);
	while ((p->pds_type_len != FPDS_TYPELEN_FREE) && 
					(p < (flash_pds_ent_t *) pds->pdsEnd)) {
		if ((p->u.type_len & FPDS_ENT_LOG_TYPE) && (p->valid)) {
			bcopy((void *) p->data, &log,
			      sizeof(flash_pds_log_data_t));
			log_type = log.log_hdr >> PDS_LOG_TYPE_SHFT;
			switch( log_type ) {
			    case PDS_LOG_TYPE0:
				timeinfo = log.u.init.tod;	/* copy */
				latestTicks = log.log_hdr & PDS_LOG_TICK_MASK;
				timeMult = log.u.init.tick_mult;
				timeDiv = log.u.init.tick_div;
				syslog(LOG_INFO,"Log Init: %d/%d/%d"
					" %02d:%02d:%02d GMT\n",
					timeinfo.Month, timeinfo.Day,
					timeinfo.Year,
					timeinfo.Hour, timeinfo.Minutes,
					timeinfo.Seconds );

				/*
				** setup later conversions
				*/
				tm_time.tm_sec = timeinfo.Seconds;
				tm_time.tm_min = timeinfo.Minutes;
				tm_time.tm_hour = timeinfo.Hour;
				tm_time.tm_mday = timeinfo.Day;
				tm_time.tm_mon = timeinfo.Month - 1;
				tm_time.tm_year = timeinfo.Year - 1900;
				tm_time.tm_isdst = 0;
				latestInitd = 1;
			
				break;

			    case PDS_LOG_TYPE1:
			    case PDS_LOG_TYPE2:
			    default:
				convTicks = log.log_hdr - latestTicks;
				usecs = (convTicks * timeMult) / timeDiv;
				msecs = usecs / 1000;
				secs = msecs / 1000;
				msecs %= 1000;
				if (!latestInitd) {
				    strcpy(tstring, "<unknown time>");
				}
				else {
				    latestTT = mktime(&tm_time);
				    latestTT -= timezone;
				    if (latestTT == -1) {
					strcpy(tstring, "<invalid time>");
				    }
				    else {
					secs += latestTT;
					gmt = gmtime(&secs);
					sprintf(tstring,"%d/%d/%d "
						"%02d:%02d:%02d.%03d GMT",
						gmt->tm_mon + 1,
                                		gmt->tm_mday,
                                		gmt->tm_year + 1900,
                                		gmt->tm_hour,
                                		gmt->tm_min,
                                		gmt->tm_sec,
						msecs );
				    }
				}

				if (PDS_LOG_TYPE1 == log_type) {
				    syslog(LOG_INFO,
					"%s: %s\n",tstring,log.u.c);
				}
				else if (PDS_LOG_TYPE2 == log_type) {
				    syslog(LOG_INFO,
					"%s: 0x%llx 0x%llx 0x%llx 0x%llx\n",
					tstring,
					log.u.d[0], log.u.d[1],
					log.u.d[2], log.u.d[3] );
				}
				else {
				    syslog(LOG_INFO,
					"%s: 0x%x 0x%x 0x%x 0x%x\n",
					tstring,
					log.u.d[0], log.u.d[1],
					log.u.d[2], log.u.d[3] );
				}
				break;
			}
			pds_invalidate(p);	/* consumed */
		}
		FPDS_NEXT_ENT(p);
	}

	/*
	** we're done with SYSLOG processing
	*/
	closelog();

	return 0;
}

/*
 * loadKoptTable()
 *
 * this static routine steps through all the entries in the kopts
 * table and loads them into an allocated array.  We then scan this
 * array for each entry in the nvram, in the hopes of making each
 * entry appear only once
 *
 * FAILURE: on failures, I'm gonna say that the occurrence of 
 * duplicated variables is small, and isn't going to mess someone
 * up who is looking at the variables.  So, worst case, don't
 * do the duplicate vars checking.
 */

#define MAX_KOPT_VARS	256

static void
loadKoptTable( char *** koptTable )
{
	int i;
	char ** koptEntry;
	char buf[SGI_NVSTRSIZE];
	char nam[SGI_NVSTRSIZE];

	*koptTable = (char **)malloc( MAX_KOPT_VARS * sizeof(char *) );
	koptEntry = *koptTable;

	for (i=0; i<MAX_KOPT_VARS; i++) {
		sprintf(nam, "%d", i);
		if (syssgi(SGI_GETNVRAM, nam, buf) < 0) {
			*koptEntry = NULL;
			break;
		}
		*koptEntry++ = strdup( nam );
	}
}

/*
 * noDupKopt()
 * 
 * this static routine steps through all the entries in the kopts
 * table to make sure that we don't output entries that live in both
 * the kopts table and the flash pds, twice
 */
static int
noDupKopt( char ** koptTable, char * pdsEnv )
{
	int i;
	char * equalPtr;		/* pointer to '=' in env */
	char pds[SGI_NVSTRSIZE];

	/*
	 * make ourselves a null terminated copy of just the
	 * variable name.  if there isn't an '=' sign, then the
	 * variable name is too large to fit into the kopt table,
	 * so just say that it's unique
	 */
	strncpy(pds, pdsEnv, SGI_NVSTRSIZE);
	pds[SGI_NVSTRSIZE-1] = '\0';
	equalPtr = strchr( pds, '=' );
	if (NULL == equalPtr) {
		return 1;
	}
	else {
		*equalPtr = '\0';	/* null that equals */
	}

	i=0;
	while ((i<MAX_KOPT_VARS) && (koptTable[i])) {
		/* if exact match (including null), then the same */
		if (!strcmp(koptTable[i], pds)) return 0;
		i++;
	}

	return 1;
}

/*
 * racer_pds_dumpValues
 *
 * steps through each entry of the PDS, and if it is a valid variable
 * entry, print out the variable and value
 */
int
racer_pds_dumpValues(racerGlob_t * pds)
{
	char ** koptTable;
	flash_pds_ent_t * p;

	/* check header */
	p = (flash_pds_ent_t *)(pds->pdsBase);

	loadKoptTable( &koptTable );

	while ((p->pds_type_len != FPDS_TYPELEN_FREE) && 
					(p < (flash_pds_ent_t *) pds->pdsEnd)) {
		if ((p->u.type_len & FPDS_ENT_ENV_TYPE) && (p->valid)) {
			if (noDupKopt(koptTable, (char*)p->data)) {
				printf("%s\n",(char *)p->data);
			}
		}
		FPDS_NEXT_ENT(p);
	}

	return 0;
}

/*
** pds_writeentry
**
** this little guy takes a pointer to some empty space, and creates
** a pds environment variable type entry with the string that's passed
** in, and puts it there.
**
** return codes are either
**   a) 0 for success
**   b) EIO for failure
*/
int
pds_writeentry(flash_pds_ent_t * freespace, char * string)
{
	int i;				/* counter */
	int datalen;			/* int16_t's of data */
	int entrylen;			/* int16_t's of pds_ent struct */
	uint16_t * free16;		/* int16_t pointer to freespace */
	uint16_t * newbuf16;		/* int16_t pointer to newbuf */
	flash_pds_ent_t * newbuf;

	if (freespace->pds_type_len != FPDS_TYPELEN_FREE) return -1;

	/*
	** allocate a new flash_pds_entry that is large enough to
	** actually hold the control and data
	*/
	newbuf = (flash_pds_ent_t *)
			malloc(strlen(string) + 2 + sizeof(flash_pds_ent_t));

	if (NULL == newbuf) return EIO;
	
	/*
	** round the data length up to the next int16_t, and don't
	** forget to include a spot for the NULL char
	*/
	datalen = (strlen(string) + 2) / sizeof(int16_t);

	/*
	** the actual entry length is the sizeof the flash_pds_ent_t +
	** the datalen, minus 1 because there's a data[1] in the
	** flash_pds_ent_t structure
	*/
	entrylen = datalen + (sizeof(flash_pds_ent_t) / sizeof(int16_t))
				- (sizeof(newbuf->data) / sizeof(int16_t));

	newbuf->pds_type_len = datalen & FPDS_ENT_HLEN_MASK;
	newbuf->pds_type_len |= FPDS_ENT_ENV_TYPE;
	newbuf->valid = FPDS_ENT_VALID;
	strcpy((char*)newbuf->data, string);

	free16 = (uint16_t *)freespace;
	newbuf16 = (uint16_t *)newbuf;
	for (i=0; i<entrylen; i++) {
		racer_WriteHWord( free16++, *(newbuf16++));
	}

	return 0;
}

/*
** pds_compress
**
** coalesce all the valid information in the flash pds section
*/ 
int
pds_compress( racerGlob_t * pds )
{
	int i;				/* counter */
	int nHWord = 0;			/* number of int16's in memory */
	flash_pds_ent_t * p;		/* pointer to an entry to look at */
	flash_pds_hdr_t * hdr;		/* PDS header info */
	uint16_t * memBuffer;		/* in memory version of (new) PDS */
	uint16_t * memBufferPtr;	/* pointer to memory copy */
	uint16_t * validEntry;		/* the entry data to insert */
	uint16_t * writeAddr;		/* write back into flash address */
	int hdrsizeHW;			/* flash_pds_ent header size */
	int erases;			/* times this segment was erased */

	time_t ut;			/* current time for updating flash */
	struct tm *gmt;			/* current time as GMT */

	/*
	** the basic idea here it that the flash pds data segment
	** is now too full to accept the new entry that we'd like
	** to add, and we now need to do is get rid of all the 
	** cruft that's no longer valid
	** 
	** the way that this block of code works, we first setup
	** a chunk of memory that is the size of the flash PDS
	** segment, then copy all the still valid entries into 
	** that chunk of memory, erase the hardware, and then
	** copy out of memory into the hardware.
	*/

	hdrsizeHW = (sizeof(flash_pds_ent_t) - sizeof(p->data)) / 
							sizeof(uint16_t);

	memBuffer = (uint16_t *)malloc(SFLASH_PDS_NSEGS * SFLASH_SEG_SIZE);
	if (NULL == memBuffer) return -1;
	memBufferPtr = (uint16_t*)((char*)memBuffer + sizeof(flash_pds_hdr_t));

	/*
	** get the number of times that this part has been erased
	** before (also increment that in a single step)
	*/
	erases = ioctl(pds->fd, SFL_INCGET_PDS);

	/*
	** our flash part allows us to pull bits from ones to zeros 
	** when writing to the part, so start with every bit set
	*/
	memset(memBuffer, 0xff, (SFLASH_PDS_NSEGS * SFLASH_SEG_SIZE) );

	/*
	** initialize to read point to the start of the existing PDS
	*/
	p = (flash_pds_ent_t *)(pds->pdsBase);

	/*
	** initialize the new PDS header structure
	*/
	hdr = (flash_pds_hdr_t *)memBuffer;
	hdr->magic = FPDS_HDR_MAGIC;
	hdr->erases = erases;
	
	ut = time(NULL);
	gmt = gmtime(&ut);
	hdr->last_flashed.Year    = gmt->tm_year + 1900;
	hdr->last_flashed.Month   = gmt->tm_mon + 1;
	hdr->last_flashed.Day     = gmt->tm_mday;
	hdr->last_flashed.Hour    = gmt->tm_hour;
	hdr->last_flashed.Minutes = gmt->tm_min;
	hdr->last_flashed.Seconds = gmt->tm_sec;
	hdr->last_flashed.Milliseconds = 0;

	hdr->cksum = 0;
	hdr->cksum = ~in_cksum((void *)hdr, sizeof(*hdr), 0);

	nHWord = sizeof(flash_pds_hdr_t) / sizeof(uint16_t);

	/*
	** do the fun stuff of copying anything that is valid out
	** of the PDS and into the new memory copy
	*/
	while ((p->pds_type_len != FPDS_TYPELEN_FREE) && 
					(p < (flash_pds_ent_t *) pds->pdsEnd)) {
		if (p->valid) {
			validEntry = (uint16_t *)p;
			for (i=0; i<(p->u.typelen.hlen + hdrsizeHW); i++) {
				*memBufferPtr++ = *validEntry++;
				nHWord++;
			}
		}
		FPDS_NEXT_ENT(p);
	}

	/*
	** erase the flash PDS segment(s)
	*/
	for (i=0; i<SFLASH_PDS_NSEGS; i++) {
		uint16_t * eraseAddr;

		eraseAddr = (uint16_t*)((char*)pds->pdsBegin + 
							(i*SFLASH_SEG_SIZE));
		racer_FlashErase(eraseAddr);
	}

	/*
	** and now copy all the data that we wrote into memory 
	** into the flash, one half-word at a time, backwards
	** (so that if we're interrupted the pds will be invalid)
	*/
	writeAddr = (uint16_t *)pds->pdsBegin + nHWord - 1;
	memBufferPtr = (uint16_t *)memBuffer + nHWord - 1;
	for (i=0; i<nHWord; i++) {
		racer_WriteHWord( writeAddr--, *(memBufferPtr--) );
	}

	openlog("flash", 0, LOG_USER);
	syslog(LOG_INFO, "IP30 PDS segment compressed successfully.");
	closelog();

	return 0;
}


/*
 * racer_is_reserved
 *
 * check to see if the variable passed in as "var" is one of
 * special "reserved" set allows us to set the variable, even
 * if it was not previously included in the flash PDS.
 *
 * return values:
 *	0	var is not in the set of strings in reserved
 *	1	var is in reserved
 */
int
racer_is_reserved( const char *var, char *const *reserved) 
{
	if (!var || !reserved) return 0;

	while (*reserved) {
		if (!strcmp(var, *reserved)) {
			return 1;
		}
		reserved++;
	}

	return 0;
}



/*
 * racer_CopyFlash
 *
 * 448808: squirrel away a copy of the current PROM contents
 * 	   in a known place.  that good place is 
 *	   /usr/cpu/firmware/IP30prom.bin.O
 */
int racer_CopyFlash(racerGlob_t* racer)
{
	int fd;
	char filename[PATH_MAX];	/* filename to dump to */

	strcpy(filename, DEFAULT_DIR);
	strcat(filename, "/");
	strcat(filename, DEFAULT_FILE);
	strcat(filename, ".O");

	(void) unlink(filename);
	
	if (racer->verbose) {
		printf("flash: Dumping current Prom to file %s\n",filename);
	}

	fd = open( filename, O_WRONLY | O_CREAT | O_TRUNC, 0600 );
	if (fd < 0) {
		return 1;
	}

	(void) write(fd, (char *)racer->FlashPromBase, SFLASH_MAX_SIZE);
	return 0;
}
