/***********************************************************************\
*	File:		promcvt.c					*
*									*
*	Author:		John Kraft (jfk@asd)				*
*			Plagiarized from Steve Whitney's nextract	*
*									*
*	Date:		September 26, 1992				*
*									*
*	Reads a standard SGI ELF executable, appends an EVEREST IO4 	*
*	PROM header onto it, and then writes it out in either 		*
*	straight binary or s3 hex record format.  The output serves 	*
*	as input for the PROM's various IO4 prom load routines.		*
*									*
\***********************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ar.h>
#include <scnhdr.h>
#include <filehdr.h>
#include <getopt.h>
#include <malloc.h>
#include <libelf.h>
#include <bstring.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/EVEREST/promhdr.h>
#include "s3.h"
#include "rle.h"
#include "lzw.h"

#define LOADADDR 0x80800000
#define DEFAULT_VERSION 4
#define MY_ERR 2894
#define ERROR_N_EXIT(x) {fprintf(stderr, "Error: %s.\n", x); return(MY_ERR);}

#define DEFAULT_BINARY "promcvt.bin"
#define DEFAULT_S3     "promcvt.s3"

#define VPRINTF if (Verbose) printf

unsigned int Version = DEFAULT_VERSION;
unsigned int Verbose = 0;
unsigned int Obsolete = 0;

/* Forward function declarations */
static int  add_segment(seginfo_t *, char *, uint);
static char *seg_type(uint);
static uint checksum_data(void *, uint);
static int  checksum_segment(promseg_t *);
static void init_headers(evpromhdr_t *, seginfo_t *);
static int  write_bin(seginfo_t *, evpromhdr_t *, char *);
static int  write_s3(seginfo_t *, evpromhdr_t *, char *, uint);
static void display_header(char *);
#ifdef DOING_COMPRESS
static int lzw_decompress(promseg_t *);
#endif

static int  elf32_add_segment(seginfo_t *, int, Elf *, uint);
static int  elf64_add_segment(seginfo_t *, int, Elf *, uint);

main(int argc, char **argv)
{
    int c;
    uint use_binary = 0;
    uint use_s3     = 0;
    uint errflg	    = 0;
    uint oflg       = 0;
    uint debug      = 0;
    uint masterflg  = 0;
    uint segflg	    = 0;
    uint disp_hdr   = 0;
    char *ofile_name = DEFAULT_BINARY;
    int result;
    /*REFERENCED*/
    int elf_size;
    evpromhdr_t promhdr;
    seginfo_t seginfo;

    /* Initialize the basic data structures */
    bzero(&promhdr, sizeof(promhdr));
    bzero(&seginfo, sizeof(seginfo));
    seginfo.si_magic = SI_MAGIC;

    /* Parse the arguments on the command line */
    while ((c = getopt(argc, argv, "A:OVbsv:o:R:r:T:G:M:h:")) != -1) {
	switch (c) {
	  case 'h':
	    disp_hdr = 1;
	    ofile_name = optarg;
	    break;
	  
	  case 'b':		/* Produce a binary file */
	    use_binary = 1;
	    if (use_s3) 
		errflg = 1;
	    if (! oflg) 
		ofile_name = DEFAULT_BINARY;
	    break;

	  case 's':		/* Produce an s3 file */
	    use_s3 = 1;
	    if (use_binary) 
		errflg = 1;
	    if (! oflg) 
		ofile_name = DEFAULT_S3;
	    break;

	  case 'o':		/* Specify the name of the output file */
	    oflg = 1;
	    ofile_name = optarg;
	    break;

	  case 'O':		/* Generate the obsolete format file */
	    Obsolete = 1;
	    printf("Building old-style flash prom image.\n");
	    break;

	  case 'v':		/* Set the version number */
	    Version = (uint) atoi(optarg);
	    if (Version == 0) {
		printf("Error: illegal version specified: '%s'\n", optarg);
		errflg = 1;
	    }
	    break;

	  case 'V':		/* Set verbosity on */
	    Verbose = 1;
	    break;
 
	  case '?':		/* Display help */
	    errflg = 1;
	    break;

	  case 'R':		/* R4000 BE segment */
	    elf_size = add_segment(&seginfo, optarg, SEGTYPE_R4000BE);
	    segflg = 1;
	    break;

	  case 'r':		/* R4000 LE segment */
	    break;

	  case 'T':		/* TFP BE segment */
	    elf_size = add_segment(&seginfo, optarg, SEGTYPE_TFP);
	    segflg = 1;
	    break;

	  case 'A':		/* Andes */
	    elf_size = add_segment(&seginfo, optarg, SEGTYPE_R10000);
	    segflg = 1;
	    break;

	  case 'G':		/* Graphics data segment */
	    elf_size = add_segment(&seginfo, optarg, SEGTYPE_GFX);
	    segflg = 1;
	    break;

	  case 'M':		/* Master segment */
	    elf_size = add_segment(&seginfo, optarg, SEGTYPE_MASTER);
	    masterflg = 1;
	    break;

	  default:
	    fprintf(stderr, "promcvt internal error in flag parsing.\n");
	    exit(1);
	}
    }

    if (disp_hdr) {
	if (argc > 3)
	    errflg = 1;
	else
	    display_header(ofile_name);
    }

    /* Deal with errors */
    if (!masterflg) {
	fprintf(stderr, "Error: no master segment specified\n");
	errflg = 1;
    }

    if (Obsolete && segflg) {
	fprintf(stderr, "Error: can't specify non-master segs with Obsolete\n");
	errflg = 1;
    }

    if (errflg) {
	fprintf(stderr, "Usage: %s [-V] [-s|-b] [-v version] [-o ofile] -M master_file", 
		argv[0]);
	fprintf(stderr, "[-R file | -r file | -T file | -G file]+\n");
	fprintf(stderr, "\t -V:       Switch on Verbose mode\n"); 
	fprintf(stderr, "\t -s:       Produce an s3 record format file \n");
	fprintf(stderr, "\t -b:       Produce a binary file (default)\n");
	fprintf(stderr, "\t -v vers:  Set the version number\n");
	fprintf(stderr, "\t -o ofile: Specify name of output file .o file\n");
	fprintf(stderr, "\t -R file:  Specify an R4000 BE segment .o file\n");
	fprintf(stderr, "\t -r file:  Specify an R4000 LE segment .o file\n");
	fprintf(stderr, "\t -T file:  Specify a TFP segment .o file\n");
	fprintf(stderr, "\t -A file:  Specify an R10000 segment .o file\n");
	fprintf(stderr, "\t -G file:  Specify a graphics segment .o file\n"); 
	exit(1);
    }

    /* Initialize the headers */
    init_headers(&promhdr, &seginfo);

    /* Finally, write the data out using the selected output format */
    if (use_s3)
	result = write_s3(&seginfo, &promhdr, ofile_name, debug);
    else
	result = write_bin(&seginfo, &promhdr, ofile_name);

	if (result == -1) {
	    fprintf(stderr, "%s: could not generate output file.\n", argv[0]);
	exit(1);
    }

    exit(0);
    /*NOTREACHED*/
}

/*
 * static void dump_promhdr
 *	Dump the information in the evpromhdr_t structure.
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


static void
check_data(FILE *binfile, off_t offset, promseg_t *psp)
{
    uint checksum;
    char *buffer;

    fseek(binfile, offset, SEEK_SET);
    buffer = (char *)malloc(psp->seg_length);

    printf("    Checking segment data...\n");

    if (fread(buffer, 1, psp->seg_length, binfile) != psp->seg_length) {
	printf("Can't read prom image data!\n");
	free(buffer);
	return;
    }

    checksum = checksum_data(buffer, psp->seg_length);

    if (checksum != psp->seg_cksum)
	printf("WARNING: Checksum error (stored == 0x%x, actual == 0x%x)!\n",
		psp->seg_cksum, checksum);

    psp->seg_data = (__int64_t)buffer;

    if ((psp->seg_type & SFLAG_COMPMASK) == SFLAG_LZW) {
#ifdef DOING_COMPRESS
	printf("    Checking lzw compression...\n");
	lzw_decompress(psp);
#endif
    }
    free(buffer);
}


static void
dump_segments(FILE *binfile, uint num_segs)
{
    int i;
    seginfo_t seginfo;
    promseg_t *psp = seginfo.si_segs;
    off_t current_offset;

    printf("Checking %d segment%s.\n", num_segs, num_segs == 1 ? "" : "s");

    if (fread(psp, sizeof(promseg_t), NUMSEGS, binfile) != NUMSEGS) {
	perror("promcvt: Can't read segment table");
	exit(1);
    }

    current_offset = ftell(binfile);

    for (i = 0; i < num_segs; i++) {
	printf("Segment %d:\n", i);
	printf("  Type:    %s\n", seg_type(psp[i].seg_type));
	printf("  Offset:  0x%x\n", psp[i].seg_offset);
	printf("  Entry:   0x%llx\n", psp[i].seg_entry);
	printf("  Ld Addr: 0x%llx\n", psp[i].seg_startaddr);
	printf("  Length:  0x%x\n", psp[i].seg_length);
	printf("  Chksum:  0x%x\n", psp[i].seg_cksum);

	check_data(binfile, current_offset, &(psp[i]));

	current_offset += psp[i].seg_length;
	if (current_offset != ftell(binfile))
		printf("WARNING: File pointer out of sync with data\n");
    }

}


/*
 * display_header(char *filename)
 *	Opens the .bin file and displays the contents of the header.
 */
static void
display_header(char *filename)
{
    FILE *binfile;
    evpromhdr_t ph;
    struct stat statbuf;
    uint mag_numsegs[2];

    printf("Reading header for %s\n", filename);

    if (stat(filename, &statbuf)) {
	perror("promcvt: Can't stat input file");
	exit(1);
    }

    if (statbuf.st_size < sizeof(ph)) {
	fprintf(stderr, "promcvt: File %s is too small\n", filename);
	exit(1);
    }

    if (!(binfile = fopen(filename, "rb"))) {
	perror("promcvt: Can't open input file");
	exit(1);
    }

    if (fread(&ph, 1, sizeof(ph), binfile) != sizeof(ph)) {
	perror("promcvt: Can't read prom header");
	exit(1);
    }

    if (ph.prom_magic == PROM_MAGIC) {
	printf("%s contains an old-style prom image.\n", filename);
	dump_promhdr(&ph);
    } else if (ph.prom_magic == PROM_NEWMAGIC){
	printf("%s contains a new-style prom image.\n", filename);
	dump_promhdr(&ph);
	if (fread(mag_numsegs, sizeof(uint), 2, binfile) != 2) {
		perror("promcvt: Error reading seg header");
		exit(1);
	}

	if (mag_numsegs[0] != SI_MAGIC) {
		printf("Bad segment table magic number (0x%x)\n", mag_numsegs[0]);
		exit(1);
	}

	dump_segments(binfile, mag_numsegs[1]);

    } else {
	printf("%s contains an unknown prom image (magic 0x%x)\n", filename,
			ph.prom_magic);
	exit(1);
    }



    exit(0);
}


/*
 * add_segment()
 *	Opens the given file & determines if it is an ELF file; if so
 *	it takes out the useful info; otherwise it says bye-bye
 */

static int
add_segment(seginfo_t *si, char *ifile_name, uint type)
{
    unsigned char MipsElf32Ident[EI_NIDENT] = {
        0x7f, 'E', 'L', 'F',
        ELFCLASS32, ELFDATA2MSB, EV_CURRENT,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    unsigned char MipsElf64Ident[EI_NIDENT] = {
        0x7f, 'E', 'L', 'F',
        ELFCLASS64, ELFDATA2MSB, EV_CURRENT,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    int fd;
    Elf *elf;
    int elf_size;
    char *elf_ident;
    int retval;

    VPRINTF("Reading in segment type 0x%x (%s), file = %s\n", type, 
	    seg_type(type), ifile_name);

    if ((fd = open(ifile_name, O_RDONLY)) == -1) {
        fprintf(stderr, "Error: could not open object file");
	exit(1);
    }

    if (elf_version(EV_CURRENT) == NULL) {
        fprintf(stderr, "Error: ELF library out of date.\n");
        close(fd);
        exit(1);
    }

    if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
        fprintf(stderr, "Error: could not open ELF file.\n");
        close(fd);
        exit(1);
    }

    /* match identification and get bit size of file */
    if ((elf_ident = elf_getident(elf, NULL)) == NULL) {
        fprintf(stderr, "Error: could not find ELF identification.\n");
        elf_end(elf);
        close(fd);
        exit(1);
    }

    elf_size = 32;
    if (memcmp(MipsElf32Ident, elf_ident, EI_NIDENT) != 0) {
        if (memcmp(MipsElf64Ident, elf_ident, EI_NIDENT) == 0)
            elf_size = 64;
        else {
            fprintf(stderr, "Error: not a valid MIPS ELF file.\n");
            elf_end(elf);
            close(fd);
            exit(1);
        }
    }

    if (elf_size == 64)
        retval = elf64_add_segment(si, fd, elf, type);
    else
        retval = elf32_add_segment(si, fd, elf, type);

    elf_end(elf);
    close(fd);

    if (retval == MY_ERR)
        exit(1);

    return(elf_size);
}


uint
to32addr(__int64_t address)
{
	uint new_address;

	if ((__uint64_t)address >= 0x100000000LL) {
		if (address & 0xff00000000000000LL == 0xa800000000000000LL)
			new_address = 0x80000000;
		else if (address & 0xff00000000000000LL == 0x9000000000000000LL)
			new_address = 0xa0000000;
		else
			new_address = 0x80000000;
		new_address |= address & 0x1fffffff;
		return new_address;
	} else {
		return (uint)address;
	}
}


static void
ip19_kludge(promseg_t *seg)
{
        short *half = (short *)seg->seg_data;
	__psint_t address = to32addr(seg->seg_entry);

	if (seg->seg_entry != seg->seg_startaddr) {
	    printf("WARNING: To work around the IP19 entry bug, the first\n");
	    printf("         32 bytes of prom image are being overwritten!\n");
			
            *half++ = 0x3c08;
            *half++ = (short)((address >> 16) & 0xffff);
            *half++ = 0x3508;
            *half++ = (short)(address & 0xffff);
            *half++ = 0x0100;
            *half++ = 0x0008;
            *half++ = 0;
            *half++ = 0;

	}

	return;
}

#ifdef DOING_COMPRESS
static int
lzw_compress(promseg_t *seg)
{
    int keep_it = 0;
    prom_lzw_hdr_t *lzw;

    in_buf = (char *)seg->seg_data;
    in_size = (int)seg->seg_length;
    out_size = in_size + 1024;
    out_buf = malloc(out_size);
    in_next = 0;
    out_next = sizeof(prom_lzw_hdr_t);

    in_checksum = 0;
   
    lzw = (prom_lzw_hdr_t *)out_buf; 

    lzw->lzw_magic = LZW_MAGIC;
    lzw->lzw_real_length = in_size;

    setup_compress();

    if (compress()) {
	printf("ERROR: LZW compressor returned an error.\n");
	keep_it = 0;
    } else {
	keep_it = (out_next < in_size);
    }

    lzw->lzw_real_cksum = in_checksum;

    if (keep_it) {
	VPRINTF("   Compressed from 0x%x to 0x%x (%d%%)\n", in_size, out_next,
			(100 - 100 * out_next / in_size));
	seg->seg_type |= SFLAG_LZW;
	seg->seg_length = out_next;
	if (out_next & 1) {
		out_buf[out_next] = '\0';
		lzw->lzw_padding = 1;
		seg->seg_length += 1;
	} else {
		lzw->lzw_padding = 0;
	}

	free((char *)seg->seg_data);
	seg->seg_data = (__int64_t)out_buf;
    } else {
	free(out_buf);
    }

    return keep_it;
}


static int
lzw_decompress(promseg_t *seg)
{
    prom_lzw_hdr_t *lzw;

    in_buf = (char *)seg->seg_data;
    in_size = (int)seg->seg_length;
    in_next = sizeof(prom_lzw_hdr_t) + 3; 	/* Skip out and compress's headers. */
    out_next = out_checksum = 0;

    lzw = (prom_lzw_hdr_t *)in_buf;
    in_size -= lzw->lzw_padding;

    out_size = lzw->lzw_real_length + 1024;
    out_buf = malloc(out_size);

    if (lzw->lzw_magic != LZW_MAGIC) {
	printf("ERROR: No LZW magic number! (Is 0x%x, should be 0x%x)\n",
			lzw->lzw_magic, LZW_MAGIC);
	return -1;
    }

    setup_compress();

    decompress();

    if (out_next != lzw->lzw_real_length) {
	printf("ERROR: Decompressed length (0x%x) != stored length (0x%x)\n",
		out_next, lzw->lzw_real_length);
	free(out_buf);
	return -1;
    }

    if (out_checksum != lzw->lzw_real_cksum) {
	printf("ERROR: Decompressed checksum (0x%x) != stored cksum (0x%x)\n",
		out_checksum, lzw->lzw_real_cksum);
	free(out_buf);
	return -1;
    }

    free(out_buf);
    return 0;
}

#endif /* DOING_COMPRESS */

static int
elf32_add_segment(seginfo_t *si, int fd, Elf *elf, uint type)
{
    Elf32_Phdr *phdr;
    Elf32_Ehdr *ehdr;

    uint i;
    promseg_t *seg;

    /* find the elf header */
    if ((ehdr = elf32_getehdr(elf)) == NULL)
        ERROR_N_EXIT("can't get ELF header")

    /* find program header */
    if ((phdr = elf32_getphdr(elf)) == NULL)
        ERROR_N_EXIT("no program header");
    for (i = 0; i < ehdr->e_phnum; i++)
        if (phdr->p_type == PT_LOAD)
	    break;
        else
            phdr++;
    if (i == ehdr->e_phnum)
        ERROR_N_EXIT("no loadable segment")

    /* get section information */
    if (++si->si_numsegs > NUMSEGS)
	ERROR_N_EXIT("add_segment error: Too many segments")

    seg = &(si->si_segs[si->si_numsegs - 1]);
    seg->seg_type = type;
 
    /* Snag the entry point */
    seg->seg_entry = (__int64_t)((__int32_t)ehdr->e_entry);
    seg->seg_startaddr = (__int64_t)((__int32_t)phdr->p_vaddr);

    seg->seg_length = phdr->p_filesz;
     
    /* Allocate one nice big chunk of memory for the object code */
    if ((seg->seg_data = (__int64_t)(__psint_t)malloc(phdr->p_filesz)) == NULL)
	ERROR_N_EXIT("could not allocate memory buffer")

    if ((lseek(fd, (off_t) phdr->p_offset, SEEK_SET) == -1) ||
        (read(fd, (void *)seg->seg_data, (uint) phdr->p_filesz) != (int) phdr->p_filesz))
	ERROR_N_EXIT("could not seek to / read in data")

    if (type == SEGTYPE_MASTER)
	ip19_kludge(seg);

#ifdef DOING_COMPRESS
    if (type != SEGTYPE_MASTER)
	if (lzw_compress(seg))
	    VPRINTF("   Compressed!\n");

     if ((seg->seg_type & SFLAG_COMPMASK) == SFLAG_LZW)
	if (lzw_decompress(seg))
	    printf("ERROR: DECOMPRESSION FAILED!\n");
#endif

    checksum_segment(seg);

    VPRINTF("   Segment length = 0x%x, entry = 0x%llx, checksum = 0x%x\n",
	    seg->seg_length, seg->seg_entry, seg->seg_cksum);
    
    return(0);
}

static int
elf64_add_segment(seginfo_t *si, int fd, Elf *elf, uint type)
{
    Elf64_Phdr *phdr;
    Elf64_Ehdr *ehdr;

    uint i;
    promseg_t *seg;

    /* find the elf header */
    if ((ehdr = elf64_getehdr(elf)) == NULL)
        ERROR_N_EXIT("Can't get ELF header")

    /* find program header */
    if ((phdr = elf64_getphdr(elf)) == NULL)
        ERROR_N_EXIT("no program header");
    for (i = 0; i < ehdr->e_phnum; i++)
        if (phdr->p_type == PT_LOAD)
	    break;
        else
            phdr++;
    if (i == ehdr->e_phnum)
        ERROR_N_EXIT("no loadable segment")

    /* get section information */
    if (++si->si_numsegs > NUMSEGS)
	ERROR_N_EXIT("add_segment error: Too many segments")

    seg = &(si->si_segs[si->si_numsegs - 1]);
    seg->seg_type = type;
 
    /* Snag the entry point */
    seg->seg_entry = (__int64_t)ehdr->e_entry;
    seg->seg_startaddr = (__int64_t)phdr->p_vaddr;

    seg->seg_length = (__uint32_t) phdr->p_filesz;
     
    /* Allocate one nice big chunk of memory for the object code */
    if ((seg->seg_data = (__int64_t)((__psint_t)malloc((__uint32_t)phdr->p_filesz))) == NULL)
	ERROR_N_EXIT("could not allocate memory buffer")

    if ((lseek(fd, (off_t) phdr->p_offset, SEEK_SET) == -1) ||
        (read(fd, (void *)((__psint_t)seg->seg_data), (uint) phdr->p_filesz) != (int) phdr->p_filesz))
	ERROR_N_EXIT("could not seek to / read in data")

    if (type == SEGTYPE_MASTER)
	ip19_kludge(seg);

#ifdef DOING_COMPRESS
    if (type != SEGTYPE_MASTER)
	if (lzw_compress(seg))
	    VPRINTF("   Compressed!\n");

     if ((seg->seg_type & SFLAG_COMPMASK) == SFLAG_LZW)
	if (lzw_decompress(seg))
	    printf("ERROR: DECOMPRESSION FAILED!\n");
#endif

    checksum_segment(seg);

    VPRINTF("   Segment length = 0x%x, entry = 0x%llx, checksum = 0x%x\n",
	    seg->seg_length, seg->seg_entry, seg->seg_cksum);
    
    return(0);
}


/*
 * checksum_data()
 *	Checksum a block of memory using the prom algorithm.
 */
static uint
checksum_data(void *address, uint length)
{
    uint cksum = 0;
    uint  num_shorts;
    ushort *curr_short;
    uint i;

    if (length & 1)
	    VPRINTF("WARNING!  Segment length 0x%x isn't an even halfword!\n",
			length);

    /* Calculate the checksum using the slightly whacky algorithm
       in the prom */
            
    curr_short = ((ushort*) address);
    num_shorts = length / 2;

    for (i = 0; i < num_shorts; i++, curr_short++) {
	 cksum += *curr_short;
    }

    return cksum;
}


/* 
 * checksum_segment()
 *	Scans the header information and calculates the checksum.
 */
static int
checksum_segment(promseg_t *seg)
{
    seg->seg_cksum = checksum_data((void *)((__psint_t)seg->seg_data),
				seg->seg_length);
    return 0;
}


/*
 * init_headers()
 *  	Scans through the segment list and figures out the
 * 	for each of the segments.  The master segment must be
 *	first (it has an offset of 4096 for backward compatibility).
 *	The other segments follow after it.
 */

static void 
init_headers(evpromhdr_t *ph, seginfo_t *si)
{
    unsigned i;			/* Index register for traversing the array */
    unsigned offset = 4096;	/* Base address */
    promseg_t tmpseg;		/* Temporary segment for swap operation */
    
    /* First find the master segment and make sure that it is the first
     * segment in the list 
     */
    for (i = 0; i < si->si_numsegs; i++) {
	if (si->si_segs[i].seg_type == SEGTYPE_MASTER && i != 0) {
		tmpseg = si->si_segs[0];
		si->si_segs[0] = si->si_segs[i];
		si->si_segs[i] = tmpseg;
	}
    }

    if (Obsolete)
	ph->prom_magic     = PROM_MAGIC;
    else
	ph->prom_magic     = PROM_NEWMAGIC;

    ph->prom_cksum     = si->si_segs[0].seg_cksum;
    ph->prom_startaddr = to32addr(si->si_segs[0].seg_startaddr);
    ph->prom_length    = si->si_segs[0].seg_length;
    ph->prom_entry     = to32addr(si->si_segs[0].seg_entry);
    ph->prom_version   = Version; 

    /* Now calculate the offsets of each of the segments */
    for (i = 0; i < si->si_numsegs; i++) {
	/* Offsets must be halfword aligned. */
	if (offset & 1)
		offset++;
	si->si_segs[i].seg_offset = offset;
	offset += si->si_segs[i].seg_length;
    } 
}

/*
 * write_s3()
 *	Produces an s3 format file as output.
 */

static int
write_s3(seginfo_t *si, evpromhdr_t *ph, char *ofile_name, uint debug)
{
    FILE *ofile;
    char *block;
    char *end_block;
    extern unsigned int record_address;
    unsigned int load_address;
    unsigned i;

    /* First open the output file */
    if ((ofile = fopen(ofile_name, "w")) == NULL) {
	fprintf(stderr, "write_s3 error: could not open file.\n");
	return -1;
    }

    /* Initialize the S3 stuff */
    s3_init(REC_SIZE);
    load_address = (debug ? (ph->prom_startaddr - 4096) : LOADADDR);
    s3_setloadaddr(load_address);

    /* Write out the header */
    s3_convert((char*) ph, sizeof(evpromhdr_t));
    s3_write(ofile);

    if (!Obsolete) {
	s3_convert((char*) si, sizeof(seginfo_t));
	s3_write(ofile);
    }

    s3_setloadaddr(load_address + 4096);

    /* Write out the actual prom */
    for (i = 0; i < si->si_numsegs; i++) {
	promseg_t *seg = &(si->si_segs[i]);

	block = (char *)seg->seg_data;
	end_block = block + seg->seg_length;

	while (block < end_block) {
	    size_t length;

	    if ((block + REC_SIZE) < end_block) 
		length = REC_SIZE;
	    else
		length = (size_t) (end_block - block);

	    s3_convert(block, length);
	    s3_write(ofile);
	    free((char *)seg->seg_data);

	    block += length;
	}
    }

    s3_terminate(ofile, ph->prom_entry);
    fclose(ofile);
    
    return 0;
}


/*
 * write_bin
 *	Writes a binary image of the segmented IO4 PROM out
 *	to disk.  This format is suitable for use in the flash
 *	command.
 */

static int
write_bin(seginfo_t *si, evpromhdr_t *ph, char *ofile_name)
{
    FILE *ofile;
    unsigned i;

    if ((ofile = fopen(ofile_name, "w")) == NULL) {
	fprintf(stderr, "write_bin error: could not open file.\n");
	return -1;
    }

    /* Write out the PROM header */
    if (fwrite(ph, sizeof(evpromhdr_t), 1, ofile) != 1) {
	fprintf(stderr, "Could not write prom header to file.\n");
	fclose(ofile);
	return -1;
    }

    if (!Obsolete) {
        if (fwrite(si, sizeof(seginfo_t), 1, ofile) != 1) {
	    fprintf(stderr, "Could not write segment info to file.\n");
	    fclose(ofile);
	    return -1;
	}
    }

    /* Write out the data section */
    for (i = 0; i < si->si_numsegs; i++) {
	promseg_t *seg = &(si->si_segs[i]);

	if (fwrite((char *)seg->seg_data, seg->seg_length, 1, ofile) != 1) {
	    fprintf(stderr, "Could not write prom data to file.\n");
	    fclose(ofile);
	    return -1;
	}
    }

    fclose(ofile);
 
    return 0;
}

/*
 * seg_type()
 *	Returns the human readable string corresponding to the
 *	segment type.
 */

static char*
seg_type(uint segtype)
{
    switch (segtype) {
        case SEGTYPE_R4000BE:	return "R4000BE";
        case SEGTYPE_TFP:	return "TFP";
	case SEGTYPE_R10000:	return "R10000";
        case SEGTYPE_GFX:	return "GFX";
        case SEGTYPE_MASTER:	return "Master";
        default:		return "Unknown";
    }
}
