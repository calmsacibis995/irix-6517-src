/***********************************************************************\
*	File:		flashprom.c					*
*									*
*	Contains routines for erasing and programming the IO4 Flash 	*
*	EPROM. The Flash EPROM is an Intel 28F020. 			*
*									*
\***********************************************************************/

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/epc.h>
#include <sys/EVEREST/promhdr.h>
#include <arcs/io.h>
#include <libsc.h>
#include <libsk.h>

#define DELAY us_delay 

#define QUARTER		(256 * 1024)
#define SPLIT 		(512 * 1024)
#define MEG 		(1024 * 1024)

#define LOW_PROM 	((Adapter << LWIN_PADAPSHIFT) + EPC_LWIN_LOPROM)
#define HIGH_PROM	((Adapter << LWIN_PADAPSHIFT) + EPC_LWIN_HIPROM)

#define READ_FLASH(_x) \
    (((_x) < SPLIT) ? (load_lwin_half(Region, LOW_PROM + (_x))) : \
		      (load_lwin_half(Region, HIGH_PROM + (_x) - SPLIT)))

#define WRITE_FLASH(_x, _y) \
    (((_x) < SPLIT) ? 	\
	(store_lwin_half(Region, LOW_PROM + (_x), (_y))) : \
	(store_lwin_half(Region, HIGH_PROM + (_x) - SPLIT, (_y))))


#define MAX_ERASE_RETRIES	3000
#define MAX_PROGRAM_RETRIES	50	

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

#define DELAY_6_US	12	
#define DELAY_10_US	14
#define DELAY_10_MS	11000

extern unsigned short load_lwin_half(unsigned int, unsigned int);
extern void store_lwin_half(unsigned int, unsigned int, unsigned short);

static void reset_flashprom(void);
static int read_flashprom(char*, evpromhdr_t*, seginfo_t*, char*);
static void program_all_flashproms(evpromhdr_t*, seginfo_t*, unsigned short*);
static int program_board(int, evpromhdr_t*, seginfo_t*, unsigned short*);
static int program_half(unsigned int, unsigned int);
static int erase_flashprom(void);
static int write_block(unsigned int, unsigned short*, unsigned int);


/* Define the region and adapter of the EPC whose flash proms should
 * be programmed.  These are variable so this code can work on practically
 * any kind of IO4 board. 
 */
static unsigned int Region  = 1;
static unsigned int Adapter = 1;
static unsigned int EraseFlag = 0;
static unsigned int FlashPromSize;


/*
 * flash reads the contents of the NVRAM from a binary file and
 * writes the data directly into the flash prom.  It is a little
 * dangerous since if some kind of error occurs the flash prom
 * can be corrupted.
 */

int
flash_cmd(int argc, char **argv)
{
    evpromhdr_t promhdr;
    seginfo_t   seginfo;
    unsigned board; 
    unsigned boardlist = 0;
    unsigned short *buffer = (unsigned short *)FLASHBUF_BASE;
    char filename[100];
    int foundfile = 0;
    unsigned int i;

    /* Check arguments */
    if (argc < 2)
	return 1;

    bzero(filename, 100);
    EraseFlag = 0;
    FlashPromSize = (MEG);
 
    for (i = 1; i < argc; i++) {
	if (argv[i][0] == '-') {
	    switch (argv[i][1]) {
	      case 's':
		/* Make sure that there's another arg */
		if (++i == argc)
		    return 1;

		if (*atobu(argv[i], &board)) {
		    printf("Error: slot parameter must be a number.\n");
		    return 1;
		}

		if (board >= EV_MAX_SLOTS) {
		    printf("slot must be between 0 and %d.\n", EV_MAX_SLOTS);
		    return 0;
		}

		boardlist |= (1 << board);
		break;

	      case 'e':
		EraseFlag = 1;
		break;


	      case 'T':
		FlashPromSize = (1024 * 1024);
		break;

	      case 't':
		FlashPromSize = MEG/2;
		break;

	      default:
		printf("Unrecognized flag: '%s'\n", argv[i]);
		return 1; 
	    }
	
	} else {
	    if (!foundfile) {
		strncpy(filename, argv[i], 99);
		i++;
		foundfile = 1;
	    } else {
		printf("Error: already specified filename '%s'.\n", filename);
		return 1;	
	    }
	    break;
	}
    }

    /* Make sure that all the args were parsed */
    if (i != argc)
	return 1;

    /* Complain if no file name was specified */
    if (!foundfile && !EraseFlag) {
	printf("Error: no filename specified\n");
	return 1;
    }

    /* Read in the flashprom binary file if we're not erasing */
    if (!EraseFlag)
	if (read_flashprom(filename, &promhdr, &seginfo, (char*) buffer) == -1)
	    return 0;

    /* Now program the selected boards */
    if (boardlist) {
	for (i = 0; i < EV_MAX_SLOTS; i++) {
	    if (boardlist & (1 << i)) 
		program_board(i, &promhdr, &seginfo, buffer);
	}
    } else {
	program_all_flashproms(&promhdr, &seginfo, buffer); 
    }

    return 0;
}

/*
 * static void dump_promhdr
 *      Dump the information in the evpromhdr_t structure.
 */
static void
dump_promhdr(evpromhdr_t *php)
{

    printf("Evpromhdr structure (not used by IP21) contains:\n");
    printf("  Magic ==     0x%x\n", php->prom_magic);
    printf("  Checksum ==  0x%x\n", php->prom_cksum);
    printf("  Startaddr == 0x%x\n", php->prom_startaddr);
    printf("  Length ==    0x%x\n", php->prom_length);
    printf("  Entry ==     0x%x\n", php->prom_entry);
    printf("  Version ==   0x%x\n", php->prom_version);

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
    ULONG fd, cnt;
    long err;
    int numsegs;
    unsigned short *data;
    char *curbufptr;
    int numbytes;
    unsigned int cksum, cksum2;
    unsigned int i;

    /* Open the file */
    if ((err = Open(file_name, OpenReadOnly, &fd)) != 0) {
	printf("cannot open flash prom source file: ");
	perror(err, file_name);
	return -1;
    }

    /* Read in the header and make sure it looks kosher before we
     * fry the flash prom.
     */
    if (err = Read(fd, ph, sizeof(evpromhdr_t), &cnt)) { 
	printf("Could not read prom header structure\n");
	Close(fd);
	return -1;
    }

    if (cnt != sizeof(evpromhdr_t)) {
	printf("Incorrect prom header structure size.\n");
	Close(fd);
	return -1;
    }

    if (ph->prom_magic != PROM_MAGIC && ph->prom_magic != PROM_NEWMAGIC) {
	printf("Invalid PROM magic number: 0x%x\n", ph->prom_magic);
	Close(fd);
	return -1;
    }

    if (ph->prom_length > FlashPromSize) {
	printf("Flash data is too big and won't fit in Flash PROM.\n");
	Close(fd);
	return -1;
    }

    /* If this binary file is in the new prom format, we read in the
     * segment table as well and alter the prom magic number so that
     * the transfer from the IP19 PROM won't fail.  If it isn't a
     * new prom, we fake a segment table so that the low-level code
     * won't be able to tell the difference.
     */
    if (ph->prom_magic == PROM_NEWMAGIC) {
	uint length = 0;
	int i;
	ph->prom_magic = PROM_MAGIC;
	err = Read(fd, si, sizeof(seginfo_t), &cnt);
	if (err) {
	    printf("Could not read segment info table.\n");
	    Close(fd);
	    return -1;
	} 
	if (cnt != sizeof(seginfo_t)) {
	    printf("Incorrect seg info table size.\n");
	    Close(fd);
	    return -1;
	}
	if (si->si_magic != SI_MAGIC) {
	    printf("Incorrect seg info magic number.\n");
	    Close(fd);
	    return -1;
	}
	numsegs = si->si_numsegs;

	for (i = 0; i < numsegs; i++)
	    length += si->si_segs[i].seg_length;

	if (length > FlashPromSize) {
	    printf("Flash data is too big and won't fit in Flash PROM.\n");
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
	numsegs = si->si_numsegs;
    }	

    /* Read in the Flash EPROM contents */
    curbufptr = (char *) buffer;

    for (i = 0; i < numsegs; i++) { 
	promseg_t *seg;
	char *start = curbufptr;
	cksum = cksum2 = 0;

	seg = &(si->si_segs[i]);
	numbytes = seg->seg_length;
	printf("  Reading segment %d (%d bytes)", i, numbytes);
	curbufptr = ((char *) buffer + seg->seg_offset - PROMDATA_OFFSET);
	while (numbytes) {
	    unsigned int count = ((numbytes < 200000) ? numbytes : 200000);

	    if (err = Read(fd, (CHAR*) curbufptr, count, &cnt)) {
		printf("Error occurred while reading prom.\n");
		Close(fd);
		return -1;
	    }
     
	    if (cnt == 0) {  
		printf("Unexpected EOF\n");
		Close(fd);
		return -1;
	    }

	    curbufptr += cnt;
	    numbytes  -= cnt;
	}
	putchar('\n');

	/* Checksum the buffer and make sure it seems okay */
	numbytes = seg->seg_length;
	data = (unsigned short *) start;

	/* Must use "> 0" since the number may be odd. */
	while (numbytes > 0) {
	    cksum += *data++;
	    numbytes -= 2;
	} 
	cksum2 = seg->seg_cksum;
	if (cksum != cksum2) {
	    printf("   Checksum error (%x %x).\n", cksum, cksum2);
	    Close(fd);
	    return -1;
	}

    }    /* end for loop */

    Close(fd);
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
	brd = &(EVCFGINFO->ecfg_board[slot]);

	/* Skip over this board if it isn't an IO board or if
	 * the board is disabled.
	 */
	if ((brd->eb_type & EVCLASS_MASK) != EVCLASS_IO || !brd->eb_enabled)
	    continue;

	program_board(slot, ph, si, buffer);
    }
}


/*
 * program_board()
 *	Programs a single IO4 board.
 */

static int 
program_board(int slot, evpromhdr_t *ph, seginfo_t *si,
	      unsigned short *buffer)
{
    evbrdinfo_t *brd = BOARD(slot);
    unsigned padap;
    unsigned int base, numbytes;
    unsigned int total_length = 0;
    int last_seg = 0;
    int i;

    /* Make sure this is a legitimate board.  Complain if not */
    if ((brd->eb_type & EVCLASS_MASK) != EVCLASS_IO) {
	printf("Slot %d does not contain an IO board.\n", slot);
	return -1;
    }

    if ( ! brd->eb_enabled) {
	printf("The IO4 in slot %d is not enabled.\n", slot);
	return -1;
    }

    for (i = 0; i < si->si_numsegs; i++)
	if (si->si_segs[i].seg_offset > si->si_segs[last_seg].seg_offset)
	    last_seg = i;

    total_length = si->si_segs[last_seg].seg_offset
		   + si->si_segs[last_seg].seg_length;

    for (padap = 1; padap < IO4_MAX_PADAPS; padap++) {
	
	if (brd->eb_ioarr[padap].ioa_type != IO4_ADAP_EPC ||
	    !brd->eb_ioarr[padap].ioa_enable) 
	    continue;

	Region  = brd->eb_io.eb_winnum;
	Adapter = padap;
	printf("Programming flashproms in slot %d\n", slot);

	if (erase_flashprom() == -1) {
	    printf("Could not erase flash prom on IO4 in slot %d\n", slot);
	    continue;
	}

	/* If we're just erasing, return now */
	if (EraseFlag)
	    return 0;

	printf("    Writing PROM header to flash PROM...\n");
	if (write_block(0, (unsigned short*) ph, sizeof(evpromhdr_t)) == -1) {
	    printf("      Couldn't write header into flash prom\n");
	    continue;
	}

	printf("    Writing segment table to flash PROM...\n");
	if (write_block(SEGINFO_OFFSET, (unsigned short*) si, 
		    sizeof(seginfo_t)) == -1) {
	    printf("      Couldn't write segment table into flash prom\n");
	    continue;
	}

	printf("    Writing PROM contents to flash PROM...\n");
	if (write_block(PROMDATA_OFFSET, buffer, total_length) == -1) {
	    printf("      Couldn't write data into flash prom\n");
	    continue;
	}
    }

    reset_flashprom();

    /* Final check of prom data */
    base = PROMDATA_OFFSET;
    numbytes = total_length;
    while (numbytes) {

	if (READ_FLASH(base) != *buffer)
	    printf("At address 0x%x value1 0x%x != value2 0x%x\n", base,
		   READ_FLASH(base), *buffer);
	base += 2;
	buffer++;
	numbytes -= 2;
    }
 	
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

        if (flashval == value)
            return 0;

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

    printf("Could not program halfword at 0x%x: wrote 0x%x, read back 0x%x\n",
           prom_addr, value, READ_FLASH(prom_addr));
    return -1;
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
    unsigned int curr_index = 0;	/* EPROM location being checked */
    unsigned int erased;

    reset_flashprom();

    /* Check to see if the PROM has already been erased */
    printf("    Checking for empty flash prom...\n");
    for (i = 0; i < FlashPromSize; i += 2) {
	if (READ_FLASH(i) != 0xffff) 
	    break;
    }

    if (i == FlashPromSize) 
	return 0;
	
    /* First we erase the Flash EPROM */
    printf("    Zeroing out the flash eprom...\n");
    for (i = 0; i < FlashPromSize; i += 2) {
	if (program_half(i, 0) == -1) {
	    printf("      Couldn't zero halfword %d\n", i);
	    return -1;
	}
    }

    /* Now we repeatedly try to erase all of the data in the flash prom.
     * If we find a word which hasn't been cleared, we re-erase and continue
     * from that point.
     */
    printf("    Erasing flash eprom...\n");
erase:
    WRITE_FLASH(curr_index, SETUP_ERASE);
    WRITE_FLASH(curr_index, ERASE);
    DELAY(DELAY_10_MS);
    erased = 1;
 
    /* Now verify that the Flash PROM was actually erased */
    while (curr_index < FlashPromSize) {

	/* If we've hit a flash prom boundary and we haven't done an
	 * erase cycle recently we jump back up and force an erase
	 * cycle to occur. 
	 */
	if (!erased && ((curr_index % QUARTER) == 0)) 
	    goto erase;
	else 
	    erased = 0;
 
	WRITE_FLASH(curr_index, ERASE_VERIFY);
	DELAY(DELAY_6_US);

	if ((READ_FLASH(curr_index) != 0xffff) && 
	    (retries < MAX_ERASE_RETRIES)) {
	    retries++;
	    goto erase;
	} else if (retries >= 5000) {
	    return -1;
	}

	curr_index += 2;
    }

    return 0;
}


static int
write_block(unsigned int offset, unsigned short *data, unsigned int length)
{
    /* Check for alignment */
    if (offset & 0x1) {
	printf("      Error: offset not halfword aligned\n");
	return -1;
    }

    if ((unsigned long) data & 0x1) {
	printf("      Error: data not halfword aligned\n");
	return -1;
    }

    if (length & 0x1) {
	printf("      Error: uneven length\n");
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

void
reset_flashprom(void)
{
    WRITE_FLASH(0, RESET);
    WRITE_FLASH(0, RESET);
    WRITE_FLASH(QUARTER, RESET);
    WRITE_FLASH(QUARTER, RESET);
    WRITE_FLASH(SPLIT, RESET);
    WRITE_FLASH(SPLIT, RESET);
    WRITE_FLASH(QUARTER * 3, RESET);
    WRITE_FLASH(QUARTER * 3, RESET);
}


/*
 * copy_prom(__scunsigned_t src, __scunsigned_t dest, uint numbytes, int window)
 *	Copies the requested number of bytes from the
 *	flash eprom to the specified memory address.
 *	As the copy is performed, a checksum is calculated
 *	and is returned on completion.  
 */

uint
copy_prom(register __int64_t src,
	  register void * dstaddr, 
          __uint32_t numbytes,
	  int window)
{
    register __scunsigned_t offset = (EVCFGINFO->ecfg_epcioa<<LWIN_PADAPSHIFT) + src;
    register uint cksum = 0;
    register uint numtocopy;
    register __int64_t dest = (__int64_t)dstaddr;

    while (numbytes) {

	/* We can't allow the transfer to cross the boundary between
	 * Flash PROM windows 0 and 1, so we break up the transfer into
	 * two pieces if necessary and select the proper window.
	 */
	if (src < SPLIT) {
	    numtocopy = ((numbytes < (SPLIT - src)) ? numbytes : (SPLIT - src));
	    offset += EPC_LWIN_LOPROM;
	} else {
	    numtocopy = numbytes;
	    offset += EPC_LWIN_HIPROM - SPLIT;
	}

	cksum += load_lwin_half_store_mult(window, offset, dest, numtocopy);
	dest += numtocopy;
	offset += numtocopy;
	src += numtocopy;
	numbytes -= numtocopy;
    }

    return cksum;
}

