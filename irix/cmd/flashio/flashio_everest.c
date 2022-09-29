/***********************************************************************\
*	File:		flashio.c					*
*	Author:		jfk						*
*									*
*	Reprograms the IO4 flash proms.  The code in this file is	*	
*	virtually identical to the code io arcs/IO4prom/flashprom.c.	*
*	The major difference is that this code uses the flash prom	*
*	device driver (which is memory maps into its address space).	*
*									*
\***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <limits.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sbd.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <sys/syssgi.h>
#include <sys/systeminfo.h>
#include <sys/times.h>
#include <sys/EVEREST/IP19.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/promhdr.h>

#define DELAY newdelay 

#define QUARTER		(256 * 1024)
#define SPLIT 		(512 * 1024)
#define MEG		(1024 * 1024)

#define READ_FLASH(_x) \
	*((volatile unsigned short*) (FlashPromBase + (_x)))
#define WRITE_FLASH(_x, _y) \
	READ_FLASH(_x) = (unsigned short) (_y)

#define qprintf if (!Quiet) printf

#define MAX_ERASE_RETRIES	3000
#define MAX_PROGRAM_RETRIES	50	

#define EVCFGINFO_ADDR	&(evcfginfo)

/*
 * Intel programming commands.  Since we have two of these chips ganged
 * together, we have to write the commands in both the high and low bytes
 * of the halfword.
 */

#define RESET		0xffff
#define SETUP_PROGRAM	0x4040
#define PROGRAM_VERIFY	0xc0c0
#define READ		0x0000
#define SETUP_ERASE	0x2020
#define ERASE		0x2020
#define ERASE_VERIFY	0xa0a0

#define DELAY_6_US 	6	
#define DELAY_10_US	10
#define DELAY_10_MS	10000

extern unsigned short load_lwin_half(unsigned int, unsigned int);
extern void store_lwin_half(unsigned int, unsigned int, unsigned short);

static void check_system(void);
static void print_version(void);
static void reset_flashprom(void);
static int  read_flashprom(char*, evpromhdr_t*, seginfo_t*, char*);
static void program_all_flashproms(evpromhdr_t*, seginfo_t*, unsigned short*);
static int  program_board(int, evpromhdr_t*, seginfo_t*, unsigned short*);
static int  checkversions(evpromhdr_t *ph, int slot);
static int  program_half(unsigned int, unsigned int);
static int  erase_flashprom(void);
static int  write_block(unsigned int, unsigned short*, unsigned int);
static void print_version(void);
static void newdelay(int);
static int compare_prom(int, evpromhdr_t *, seginfo_t *, char*);
static void compare_all_proms(evpromhdr_t *, seginfo_t *, char*);
static void read_prom(unsigned short *buffer);
static int init_evcfg(void);
static int map_flashprom(int slot);

/* A couple of random global definitions */
static unsigned int FlashPromSize;
static volatile char *FlashPromBase;
static evcfginfo_t evcfginfo;
static int Quiet = 0;
static int Force = 0;
static int TotalLength;

volatile unsigned int prevtime, currtime; 

/*
 * main reads the contents of the NVRAM from a binary file and
 * writes the data directly into the flash prom.  It is a little
 * dangerous since if some kind of error occurs the flash prom
 * can be corrupted.
 */

void
flashio_everest(int argc, char **argv)
{
    evpromhdr_t promhdr;
    seginfo_t   seginfo;
    unsigned board; 
    unsigned boardlist = 0;
    unsigned short *buffer;
    char filename[100];
    int i;
    int errflg = 0;
    int do_print_version = 0;
    int do_flash = 0;
    int do_compare = 0;

    /* Initialize the program */
    check_system();
    bzero(filename, 100);
    FlashPromSize = MEG;
    if (init_evcfg() == -1) 
	exit(1);	

    while ((i = getopt(argc, argv, "s:THqfcv")) != EOF) {
	switch (i) {
	  case 's':
	    do_flash = 1;
	    board = atoi(optarg);
	    if (board < 1) {
		fprintf(stderr, "flash: slot value must be a number > 0.\n");
		exit(1);
	    }

	    if (board >= EV_MAX_SLOTS) {
		fprintf(stderr, "slot must be between 0 and %d.\n", 
			EV_MAX_SLOTS);
		exit(1);
	    }

	    boardlist |= (1 << board);
	    break;

	  case 'T':
	    do_flash = 1;
	    FlashPromSize = MEG;
	    break;

	  case 'H':
	    do_flash = 1;
	    FlashPromSize = SPLIT;
	    break;

	  case 'q':
	    Quiet = 1;
	    break;

	  case 'f':
	    do_flash = 1;
	    Force = 1;
	    break;

	  case 'v':
 	    do_print_version = 1;
	    break;

	  case 'c':
	    do_compare = 1;
	    break;

	  case '?':
	    errflg++;
        }
    }
  
    if (errflg) goto usage;
    if (do_print_version && do_flash) {
	fprintf(stderr, "flashio: cannot display version number and flash prom at same time\n");
	goto usage;
    }
    if (do_print_version) {
	print_version();
	exit(0);
    }

    /* Parse out the filename with appropriate error checking */
    if (optind == argc) {
	fprintf(stderr, "flashio: must specify a prom image filename\n");
	goto usage;
    } else if (optind != (argc - 1)) {
	fprintf(stderr, "flashio: only one prom image file may be specified\n");
	goto usage;
    } else {
	strcpy(filename, argv[optind]);
    }

    if ((buffer = malloc(1024 * 1024)) == (unsigned short*) 0) {
	fprintf(stderr, "cannot malloc flash buffer\n");
	exit(1);
    }
    if (read_flashprom(filename, &promhdr, &seginfo, (char*) buffer) == -1)
	exit(1);

    if (do_compare) {
	if (boardlist) {
	    for (i = 0; i < EV_MAX_SLOTS; i++) {
		if (boardlist & (1 << i)) 
		    compare_prom(i, &promhdr, &seginfo, (char *) buffer);
	    }
	} else {
	    compare_all_proms(&promhdr, &seginfo, (char *) buffer); 
	}
	exit(0);
    }

    /* Now program the selected boards */
    if (boardlist) {
	for (i = 0; i < EV_MAX_SLOTS; i++) {
	    if (boardlist & (1 << i)) 
		program_board(i, &promhdr, &seginfo, buffer);
	}
    } else {
	(void) program_all_flashproms(&promhdr, &seginfo, buffer); 
    }

    exit(0);

usage:
    fprintf(stderr, "Usage: flashio [-s slotnum] [-q] [-f] [-T] [-v] file.bin\n"
		    "\t-s slot -- Only touch specified slot\n"
		    "\t-q      -- Quiet.  Don't print informational messages\n"
		    "\t-f      -- Force the flash process\n"
		    "\t-T      -- Total.  Flash all 1 megabyte\n"
		    "\t-c      -- Compare flash proms with prom image\n"
		    "\t-v      -- Display flash prom versions\n"); 
    exit(1);
}


/* 
 * compare_prom
 *      Compares the contents of the currently mapped flash prom with the
 *      prom image.
 */

static int
compare_prom( int slot, evpromhdr_t *promhdr, seginfo_t *seginfo, char *buffer)
{
    char *prom_buffer;
    char *prom_ptr;
    int i, TotalLength;
    int last_seg = 0;
    int errors = 0;
    evbrdinfo_t *brd = &(evcfginfo.ecfg_board[slot]);

    /* Make sure this is a legitimate board.  Complain if not */
    if ((brd->eb_type & EVCLASS_MASK) != EVCLASS_IO) {
	fprintf(stderr, "flashio: slot %d does not contain an IO board.\n", 
		slot);
	return -1;
    }

    if (map_flashprom(slot) == -1)
	return -1;

    qprintf("  Comparing flashprom in slot %d to prom image...\n", slot);
    prom_buffer = malloc(FlashPromSize);
    read_prom((unsigned short *) prom_buffer);
    prom_ptr = (char *) prom_buffer;
    for (i = 0; i < sizeof(evpromhdr_t); i++) {
	if (*(((char *)promhdr) + i) != *(prom_ptr + i)) {
	    errors++;
	    fprintf(stderr,
		    "flashio: Mismatch at location 0x%x (prom header)\n"
		    "between flashprom in slot %d and prom image\n", 
		    prom_ptr + i, slot);
	    fprintf(stderr, "         burned prom = %x    file = %x\n", 
		    *(((char *)promhdr) + i), *(prom_ptr + i));
	}

    }
    prom_ptr = (char *) prom_buffer +  SEGINFO_OFFSET;
    for (i = 0; i < sizeof(seginfo_t); i++) {
	if (*(((char *)seginfo) + i) != *(prom_ptr + i)) {
	    errors++;
	    fprintf(stderr,
		    "flashio: Mismatch at location 0x%x (seg info)\n"
		    "between flashprom in slot %d and prom image\n",
		    (prom_ptr + i) - prom_buffer, slot);
	    fprintf(stderr, "         burned prom = %x    file = %x\n",
		    *(((char *)seginfo) + i), *(prom_ptr + i));
	}
    }
    for (i = 0; i < seginfo->si_numsegs; i++)
	if (seginfo->si_segs[i].seg_offset > seginfo->si_segs[last_seg].seg_offset)
	    last_seg = i;

    TotalLength = seginfo->si_segs[last_seg].seg_offset 
	+ seginfo->si_segs[last_seg].seg_length;
    prom_ptr = (char *) prom_buffer +  PROMDATA_OFFSET;
    for (i = 0; i < TotalLength; i++) {
	if (*(buffer + i) != *(prom_ptr + i)) {
	    errors++;
	    fprintf(stderr,
		    "flashio: Mismatch at location 0x%x (prom data)\n"
		    "between flashprom in slot %d and prom image\n",
		    (prom_ptr + i) - prom_buffer, slot);
	    fprintf(stderr, "         burned prom = %x    prom image = %x\n",
		    *(buffer + i), *(prom_ptr + i));
	}
    }
    if (!errors)
	qprintf("  Burned prom in slot %d matches prom image\n", slot);
    return 0;
}


/* 
 * compare_all_proms
 *      Compares the contents of each flash prom on the system with the
 *      prom image.
 */

static void
compare_all_proms(evpromhdr_t *ph, seginfo_t *si, char *buffer)
{
    int slot;
    evbrdinfo_t *brd;

    for (slot = EV_MAX_SLOTS - 1; slot >= 0; slot--) {

        /* Check to see if this is a legitimate IO4 board */
        brd = &(evcfginfo.ecfg_board[slot]);

	if (EVCLASS(brd->eb_type) == EVCLASS_IO && brd->eb_enabled) {
	    
	    if (map_flashprom(slot) == -1)
		return;

	    compare_prom(slot, ph, si, buffer);
	}
    }
}


/*
 * read_flashprom()
 *	Reads the flash prom binary data out of the specified
 *	file, checks its checksum, and then converts it into a
 *	form suitable for burning.
 */

static int
read_flashprom(char *file_name, evpromhdr_t *ph, seginfo_t *si, char *buffer)
{
    int fd, cnt;
    unsigned short *data;
    char *curbufptr;
    unsigned int numbytes;
    unsigned int cksum = 0;
    unsigned int i;

    /* Open the file */
    if ((fd = open(file_name, O_RDONLY)) < 0) {
	fprintf(stderr, "flashio: cannot open flash prom source file: %s\n", 
	        file_name);
	perror("   Reason");
	return -1;
    }

    /* Read in the header and make sure it looks kosher before we
     * fry the flash prom.
     */
    if ((cnt = read(fd, ph, sizeof(evpromhdr_t))) < 0) { 
	fprintf(stderr, "flashio: Could not read prom header structure\n");
	close(fd);
	return -1;
    }
 
    if (cnt != sizeof(evpromhdr_t)) {
	fprintf(stderr, "flashio: Incorrect prom header structure size.\n");
	close(fd);
	return -1;
    }

    if (ph->prom_magic != PROM_MAGIC && ph->prom_magic != PROM_NEWMAGIC) {
	fprintf(stderr, "flashio: Invalid PROM magic number: 0x%x\n", 
		ph->prom_magic);
	close(fd);
	return -1;
    }

    if (ph->prom_length > FlashPromSize) {
	fprintf(stderr, "flashio: flash data won't fit in Flash PROM.\n");
	close(fd);
	return -1;
    }

    /* If this binary file is in the new prom format, we read in the
     * segment table as well and alter the prom magic number so that
     * the transfer from the IP19 PROM won't fail.  If it isn't a
     * new prom, we fake a segment table so that the low-level code
     * won't be able to tell the difference.
     */
    if (ph->prom_magic == PROM_NEWMAGIC) {
	ph->prom_magic = PROM_MAGIC;
	if ((cnt = read(fd, si, sizeof(seginfo_t))) < 0) {
	    fprintf(stderr, "flashio: Could not read segment info table.\n");
	    close(fd);
	    return -1;
        } 
   
	if (cnt != sizeof(seginfo_t)) {
	    fprintf(stderr, "flashio: Incorrect seg info table size.\n");
	    close(fd);
	    return -1;
	}

	if (si->si_magic != SI_MAGIC) {
	    fprintf(stderr,
	    "SI_MAGIC == 0x%x, si->si_magic == 0x%x\n", SI_MAGIC, si->si_magic);
	    fprintf(stderr, "flashio: Incorrect seg info magic number.\n");
	    close(fd);
	    return -1;
        }
    } else {
	/* Synthesize a small segment table for data reading */
	si->si_magic   = SI_MAGIC;
	si->si_numsegs = 1;
	si->si_segs[0].seg_type = SEGTYPE_MASTER;
	si->si_segs[0].seg_cksum = ph->prom_cksum;
	si->si_segs[0].seg_length = ph->prom_length;
	si->si_segs[0].seg_offset = PROMDATA_OFFSET;
    }	

    /* Read in the Flash EPROM contents */
    curbufptr = (char *) buffer;

    for (i = 0; i < si->si_numsegs; i++) { 
	promseg_t *seg = &(si->si_segs[i]);
	char *start = curbufptr;

	cksum = 0;
	qprintf("  Reading segment %d...\n", i);
	numbytes = seg->seg_length;
	curbufptr = ((char *) buffer + seg->seg_offset - PROMDATA_OFFSET);
	while (numbytes) {
	    unsigned int count = ((numbytes < 16000) ? numbytes : 16000);
	    if ((cnt = read(fd, curbufptr, count)) < 0) {
		fprintf(stderr, "flashio: Error occurred while reading prom.\n");
		close(fd);
		return -1;
	    }
     
	    if (cnt == 0) {  
		fprintf(stderr, "flashio: Unexpected EOF\n");
		close(fd);
		return -1;
	    }

	    curbufptr += cnt;
	    numbytes  -= cnt;
	}

	/* Checksum the buffer and make sure it seems okay */
	numbytes = seg->seg_length;
	data = (unsigned short *) start;
	while (numbytes) {
	    cksum += *data++;
	    numbytes -= 2;
	} 
	if (cksum != seg->seg_cksum) {
	    fprintf(stderr, "flashio: Checksum error (0x%x, 0x%x).\n",
			cksum, seg->seg_cksum);
	    close(fd);
	    return -1;
	}
    }    /* end for loop */

    close(fd);
    return 0;
}


/* 
 * program_all_flashproms
 *	Writes the contents of the promhdr and buffer data structures
 *	into all of the flash proms in the system.
 */

static void
program_all_flashproms(evpromhdr_t *ph, seginfo_t *si, unsigned short *buffer)
{
    int slot;
    evbrdinfo_t *brd;

    for (slot = EV_MAX_SLOTS - 1; slot >= 0; slot--) {

	/* Check to see if this is a legitimate IO4 board */
	brd = &(evcfginfo.ecfg_board[slot]);

	/* Skip over this board if it isn't an IO board or if
	 * the board is disabled.
	 */
	if ((brd->eb_type & EVCLASS_MASK) != EVCLASS_IO || !brd->eb_enabled)
	    continue;

	program_board(slot, ph, si, buffer);
    }
}


/*
 * map_flashprom()
 *	Maps the IO4 flashprom for the given slot.
 */

static int
map_flashprom(int slot)
{
    static int fd = -1;
    char fpname[100];
   
    /* Close old file */
    if (fd != -1)
	close(fd);
  
    sprintf(fpname, "/dev/flash/flash%d", slot);
    if ((fd = open(fpname, O_RDWR)) < 0) {
        fprintf(stderr, "flashio: cannot open flash driver %s: ", fpname);
        perror("   Reason");
        return -1;
    }
    FlashPromBase = (volatile char*) mmap(0, 1024*1024, PROT_WRITE, MAP_SHARED,
                                          fd, 0);
    if (FlashPromBase == (volatile char*) -1) {
        perror("flashio: mmap failed");
        return -1;
    }
    return 0;
}


/*
 * program_board()
 *	Programs a single IO4 board.
 */

static int 
program_board(int slot, evpromhdr_t *ph, seginfo_t *si, 
	      unsigned short *buffer)
{
    evbrdinfo_t *brd = &(evcfginfo.ecfg_board[slot]);
    int last_seg = 0;
    int i;

    /* Make sure this is a legitimate board.  Complain if not */
    if ((brd->eb_type & EVCLASS_MASK) != EVCLASS_IO) {
	fprintf(stderr, "flashio: slot %d does not contain an IO board.\n", 
		slot);
	return -1;
    }

    if (map_flashprom(slot) == -1)
	return -1;

    if (checkversions(ph, slot) == 1)
	return 0;

    for (i = 0; i < si->si_numsegs; i++)
	if (si->si_segs[i].seg_offset > si->si_segs[last_seg].seg_offset)
	    last_seg = i;

    TotalLength = si->si_segs[last_seg].seg_offset
	+ si->si_segs[last_seg].seg_length;

    /* Make sure we can map the RTC before trashing the flashprom. */
    newdelay(-1);

    fprintf(stderr, "Programming IO4 flashproms in slot %d; please wait...\n", 
            slot);

    if (erase_flashprom() == -1) {
	qprintf("Could not erase flash prom on IO4 in slot %d\n", slot);
	return -1;
    }

    qprintf("    Writing PROM header to flash PROM...\n");
    if (write_block(0, (unsigned short*) ph, sizeof(evpromhdr_t)) == -1) 
	printf("      Couldn't write header into flash prom\n");

    qprintf("    Writing segment table to flash PROM...\n");
    if (write_block(SEGINFO_OFFSET, (unsigned short*) si, 
		    sizeof(seginfo_t)) == -1) 
	qprintf("      Couldn't write segment table into flash prom\n");

    qprintf("    Writing PROM contents to flash PROM...\n");
    if (write_block(PROMDATA_OFFSET, buffer, TotalLength) == -1) 
	qprintf("      Couldn't write data into flash prom\n");

    reset_flashprom();
    compare_prom(slot, ph, si, (char *) buffer);

    return 0;
}


int
checkversions(evpromhdr_t *ph, int slot)
{
    int oldvers;

    /* Read in the version information */
    oldvers =  READ_FLASH(offsetof(evpromhdr_t, prom_version)) << 16;
    oldvers |= READ_FLASH(offsetof(evpromhdr_t, prom_version) + 2);
  
    /* Zero is a special case */ 
    if (oldvers == 0)
	return 0;

    if ((oldvers == ph->prom_version) && !Force) {
	qprintf("IO4 flashprom in slot %d is up-to-date\n", slot);
	return 1;
    } 
   
    if ((oldvers > ph->prom_version) && !Force) {
	fprintf(stderr, 
		"WARNING: The file is older than the image which is currently\n"
		"         in the IO4 flash prom in slot %d.  For this reason,\n"
	        "         the flash proms have not been altered.  If you\n"
		"         wish to revert to a previous version of the\n"
		"         proms you must use the flashio command's -f flag.\n",
		slot
		); 
	exit(1);
    }

    /* If the image in the file is newer than what is in the proms,
     * return 0 to update.
     */
    return 0;
}
 
/*
 * program_half
 *	Program a single halfword using the quick-pulse algorithm
 *	described in the Intel documentation.
 */

static int 
program_half(unsigned int prom_addr, unsigned int value)
{
    uint retries = 0;		/* Number of retries so far */
    int program = 0x4040;
    int verify  = 0xc0c0;
    unsigned int flashval;

    for (retries = 0; retries < MAX_PROGRAM_RETRIES; retries++) {
        WRITE_FLASH(prom_addr, program);
        WRITE_FLASH(prom_addr, value);
        DELAY(DELAY_10_US);
        WRITE_FLASH(prom_addr, verify);
        DELAY(DELAY_6_US);

        flashval = READ_FLASH(prom_addr);

        if (flashval == value) {
            return 0;
	}

        program = 0xffff;
        verify  = 0xffff;

        /* If the top byte didn't program set the value to redo it */
        if ((flashval & 0xff00) != (value & 0xff00)) {
            program &= 0x40ff;
            verify  &= 0xc0ff;
        }

        /* If the bottom byte didn't program set the value to redo it */
        if ((flashval & 0x00ff) != (value & 0x00ff)) {
            program &= 0xff40;
            verify  &= 0xffc0;
        }
    }

    fprintf(stderr, "flashio: could not program 0x%x: wrote 0x%x, read 0x%x\n", 
	    prom_addr, value, READ_FLASH(prom_addr));
    return -1;
}


static	unsigned short 
erase_half(unsigned int offset, unsigned short mask)
{
    unsigned int o;
    unsigned short data;

    WRITE_FLASH(offset, SETUP_ERASE & mask);
    WRITE_FLASH(offset, ERASE & mask);
    DELAY(DELAY_10_MS);

    for (o = offset; o < offset + SPLIT; o+= 2) {
	/* Check for all bits set to '1' */
	WRITE_FLASH(o, ERASE_VERIFY & mask);
	data = READ_FLASH(o); 
	if (data != 0xffff) {
	    return(((data & 0xff00) != 0xff00 ? 0xff00 : 0) |
		   ((data & 0xff) != 0xff ? 0xff : 0));
	}
    }
    return(0);
}

/*
 * erase_flashprom
 *	Erases the entire flash EPROM. 
 */

static int
erase_flashprom(void)
{
    unsigned int i;			/* Index variable */
    unsigned int retries = 0;		/* Number of erase retries */
    unsigned short mask;		/* mask for program commands */

    reset_flashprom();

    /* Check to see if the PROM has already been erased */
    qprintf("    Checking for empty flash prom...\n");
    for (i = 0; i < FlashPromSize; i += 2) {
	if (READ_FLASH(i) != 0xffff) 
	    break;
    }

    if (i == FlashPromSize) 
	return 0;
	
    /* First we erase the Flash EPROM */
    qprintf("    Zeroing out the flash eprom...\n");
    for (i = 0; i < FlashPromSize; i += 2) {
	if (program_half(i, 0) == -1) {
	    qprintf("      Couldn't zero halfword %d\n", i);
	    /*
	     * Some flashproms were damaged by defective software.  This
	     * kludge allows those proms to work for one more release.
	     * Flashproms in IRIX 5.3 will likely be larger than 512M.
	     */
	    if ((TotalLength < SPLIT) && (FlashPromSize == MEG)) {
		printf("\007================================================="
			"==========================\n");
		printf(" WARNING: The flash PROM in this IO4 board may need"
			" replacement.\n");
		printf("         Please contact your service provider.\n");
		printf("\007================================================="
			"==========================\n");
		printf("  Trying to program the lower half only...\n");
		FlashPromSize = SPLIT;
		return erase_flashprom();
	    } else {
		return -1;
	    }
	}
    }

    /* Now we repeatedly try to erase all of the data in the flash prom.
     * If we find a word which hasn't been cleared, we start all over 
     * again
     */
    qprintf("    Erasing flash eprom...\n");

    /* Erase lower half */

    mask = 0xffff;
    retries = 0;
    while (mask = erase_half(0, mask)) {
	if (retries++ > MAX_ERASE_RETRIES) {
	    return(-1);
	}
    }

    /* Erase upper half. */

    mask = 0xffff;
    retries = 0;
    while (mask = erase_half(SPLIT, mask)) {
	if (retries++ > MAX_ERASE_RETRIES) {
	    return(-1);
	}
    }
    return 0;
}


static int
write_block(unsigned int offset, unsigned short *data, unsigned int length)
{
    /* Check for alignment */
    if (offset & 0x1) {
	fprintf(stderr, "flashio (write_block): offset not halfword aligned\n");
	return -1;
    }

    if ((unsigned long) data & 0x1) {
	fprintf(stderr, "flashio (write_block): data not halfword aligned\n");
	return -1;
    }

    if (length & 0x1) {
	fprintf(stderr, "flashio (write_block): uneven length pass\n");
	return -1;
    }

    while (length) {
	if (program_half(offset, *data) == -1) 
	    return -1; 
	length -= 2;
	offset += 2;
	data++;
    }
  
    return 0;
}

static void
read_prom(unsigned short *buffer)
{
    int i;
    for (i = 0; i < FlashPromSize; i+=2) {
	*buffer = READ_FLASH(i);
	buffer++;
    }
}


/*
 * init_evcfg initializes the local copy of the evcfginfo data
 * structure.  We play some games to insure that the macros
 * in evconfig.h continue to work.
 */

static int
init_evcfg(void)
{
    if (syssgi(SGI_GET_EVCONF, &evcfginfo, sizeof(evcfginfo)) == -1) {
	perror("flashio: syssgi call for evconf failed");
	return -1;
    }

    /* Make sure it didn't return garbage */
    if (evcfginfo.ecfg_magic != EVCFGINFO_MAGIC) {
	fprintf(stderr, "flashio: bad magic number in returned evcfg struct\n");
	return -1;
    }

    return 0;
}

#define SEC 1000000

void newdelay(int time)
{
    static volatile unsigned int *rtc;
    static int inited = 0;
    int waittime;
    int fd;
    __psint_t phys_addr;
    int cycleval;

    if ((time == -1) && !inited) {
        if ((fd = open("/dev/mmem", O_RDONLY)) < 0) {
            perror("flashio: Could not open /dev/mmem");
            exit(1);
        }

	if ((phys_addr = syssgi(SGI_QUERY_CYCLECNTR, &cycleval)) == -1) {
	     perror("flashio: Could not get RTC address");

	}

        rtc = (volatile unsigned*) mmap(0, 0x1000, PROT_READ, MAP_SHARED, fd,
                                        phys_addr);

        if (rtc == (volatile unsigned*) -1) {
            perror("flashio: mmap of RTC failed");
            exit(1);
        }
        rtc++;
	inited = 1;
	return;
    }

    if (!inited) {
	fprintf(stderr, "flashio: newdelay internal error.\n");
	exit(1);
    }

    /* Calculate amount of time to wait */
    waittime = time * 47;
    prevtime = *rtc;
    while (waittime > 0) {
        currtime = *rtc;
        if (currtime < prevtime)
            waittime -= (UINT_MAX - prevtime) + currtime;
        else
            waittime -= currtime - prevtime;

        prevtime = currtime;
    }
}


/*
 * reset the flashprom.  We have to do this multiple times for
 * each possible prom segment, because certain flash proms expect
 * reset to be done twice in a row (Hitachi 128Kx8's, for example).
 */

static void
reset_flashprom(void)
{
    WRITE_FLASH(0, RESET);
    WRITE_FLASH(0, RESET);
    WRITE_FLASH(QUARTER, RESET);
    WRITE_FLASH(QUARTER, RESET);
    WRITE_FLASH(SPLIT, RESET);
    WRITE_FLASH(SPLIT, RESET);
    WRITE_FLASH(3*QUARTER, RESET);
    WRITE_FLASH(3*QUARTER, RESET);
}


/*
 * check_system()
 *	Reads the system information and prints an error message
 *	if this system isn't an everest.
 */

static void
check_system(void)
{
    char machine[5];

    if (sysinfo(SI_MACHINE, machine, 5) == -1) {
	perror("flashio: cannot read system information structure");
	exit(1);
    }

    if (strcmp(machine, "IP19") 
	&& strcmp(machine, "IP21") 
	&& strcmp(machine, "IP25")) {
	fprintf(stderr, "Flashio is used to reprogram the IO4 flash prom on\n"
			"Challenge and Onyx machines.  It can only be run on\n"
			"those systems.\n\n");
	exit(1);
    }
}


/* 
 * print_version()
 *	Display flash prom version information for master flashprom.
 */

static void
print_version(void)
{
    int slot;
    unsigned vers;
    evbrdinfo_t *brd;

    for (slot = EV_MAX_SLOTS - 1; slot >= 0; slot--) {

        /* Check to see if this is a legitimate IO4 board */
        brd = &(evcfginfo.ecfg_board[slot]);

	if (EVCLASS(brd->eb_type) == EVCLASS_IO && brd->eb_enabled) {
	    
	    if (map_flashprom(slot) == -1)
		return;

	    vers =  READ_FLASH(offsetof(evpromhdr_t, prom_version)) << 16;
            vers |= READ_FLASH(offsetof(evpromhdr_t, prom_version) + 2);

	    printf("Slot %2d: Flashprom version is %d\n", slot, vers);
	}
    }
} 
