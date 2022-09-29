/***********************************************************************\
*	File:		promgen.c					*
*									*
*	Author:		Curt McDowell					*
*			Rewritten from John Kraft's promcvt		*
*			and Steve Whitney's nextract			*
*									*
*	Date:		October 30, 1995				*
*									*
*	Assembles the image of a PROM consisting of a header		*
*	and one or more loadable segments.				*
*									*
*	Output is sent to stdout in either straight binary or s3 hex	*
*	record format.							*
*									*
\***********************************************************************/

#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ar.h>
#include <scnhdr.h>
#include <filehdr.h>
#include <getopt.h>
#include <libelf.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/SN/promhdr.h>

#include "dozip.h"
#include "unzip.h"
#include "s3.h"

#define DEF_VERSION 1
#define DEF_REVISION 0
#define DEF_OFILE	"prom.out"
#define DEF_OFILE_S3	"prom.s3"

#define VPRINTF		if (Verbose) printf

unsigned int		Version		= DEF_VERSION;
unsigned int		Revision	= DEF_REVISION;
unsigned int		Verbose		= 0;
unsigned int		Compress	= 1;

/* Forward function declarations */

static int		add_segment(char *, char *);
static __uint64_t	sum_data(char *, __uint64_t);
static void		calc_offsets(promhdr_t *);
static void		write_bin(promhdr_t *, char *);
static void		write_s3(promhdr_t *, char *, __uint64_t);
static void		display_header(char *);
static void		gzip_compress(promhdr_t *, int);
static void		gzip_decompress(promhdr_t *, int);

static void		elf32_add_segment(char *, int, Elf *, int);
static void		elf64_add_segment(char *, int, Elf *, int);

promhdr_t		promhdr;

char		       *pdata[PROM_MAXSEGS];
char		       *pdata_c[PROM_MAXSEGS];

extern	int		errno;
extern	char	       *sys_errlist[];

#define ERRSTR		sys_errlist[errno]

void warn(char *fmt, ...)
{
	va_list ap;
	va_start(ap,fmt);
	fputs("*** promgen WARNING: ", stderr);
	vfprintf(stderr, fmt, ap);
	fflush(stderr);
}

void fatal(char *fmt, ...)
{
	va_list ap;
	va_start(ap,fmt);
	fputs("*** promgen FATAL: ", stderr);
	vfprintf(stderr, fmt, ap);
	exit(1);
}

#define USG(x)	fprintf(stderr, x)

void usage(void)
{
    USG("Usage: promgen [-VQsbCU] [-v version.revision] [-o ofile] [-h prom.out]\n");
    USG("               [-a s3_addr] [-N segname -E elffile]\n");
    USG("    -V                    Turn on Verbose mode\n");
    USG("    -Q                    Turn off Verbose mode (initial state)\n");
    USG("    -s                    Produce an s3 record format file\n");
    USG("    -b                    Produce a binary file (default)\n");
    USG("    -C                    Turn on compression (initial state)\n");
    USG("    -U                    Turn off compression\n");
    USG("    -h image              Display header of a binary file\n");
    USG("    -v vers.rev           Set version number\n");
    USG("    -o ofile              Name of output file (default prom.out)\n");
    USG("    -a s3_addr            Specify alternate S3 load address\n");
    USG("    -N segname            Specify name of next segment\n");
    USG("    -E segname            Insert ELF loadable file segment\n");
    USG("Notes:\n");
    USG("1.  Capital letter options may occur more than once.\n");
    USG("2.  Options are processed in order.\n");
    USG("Examples:\n");
    USG("1.  promgen -V -N SN0IO6 -E io6prom -U -N Graphics -E gfxprom\n");
    USG("2.  promgen -h prom.out\n");
    exit(1);
}

void main(int argc, char **argv)
{
    int c;
    uint use_binary = 0;
    uint use_s3	    = 0;
    uint errflg	    = 0;
    uint oflg	    = 0;
    uint disp_hdr   = 0;
    char *ofile_name = DEF_OFILE;
    char *oseg_name = 0;
    __uint64_t s3_addr   = 0;
    int i,j;
    char temp[16];
    char vers[16];

    /* Clear basic data structure so segments can be added */

    memset(&promhdr, 0, sizeof(promhdr));

    /* Parse the arguments on the command line */
    while ((c = getopt(argc, argv, "bh:o:a:sv:N:E:VQUC")) != -1) {
	switch (c) {
	  case '?':		/* Display help */
	    errflg = 1;
	    break;

	  case 'b':		/* Produce a binary file */
	    use_binary = 1;
	    if (use_s3)
		errflg = 1;
	    if (! oflg)
		ofile_name = DEF_OFILE;
	    break;

	  case 'h':
	    disp_hdr = 1;
	    ofile_name = optarg;
	    break;

	  case 'o':		/* Specify the name of the output file */
	    oflg = 1;
	    ofile_name = optarg;
	    break;

	  case 'a':
	    s3_addr = strtoull(optarg, 0, 0);
	    break;

	  case 's':		/* Produce an s3 file */
	    use_s3 = 1;
	    if (use_binary)
		errflg = 1;
	    if (! oflg)
		ofile_name = DEF_OFILE_S3;
	    break;

	  case 'v':		/* Set the version and revision number */
	    i=0;
	    strcpy(temp,optarg);
	    while ((temp[i] != '.') && (i < strlen(temp))) {
		    vers[i] = temp[i];
		    i++;
	    }
	    vers[i] = '\0';
	    i++;
	    Version = (uint) atoi(vers);
	    j=0;
	    while (i < strlen(temp)) {
		    vers[j] = temp[i];
		    i++;j++;
	    }
	    vers[j] = '\0';
	    Revision = (uint) atoi(vers);
	    if (Version == 0)
		fatal("Illegal version specified: '%s'\n", optarg);
	    break;

	  case 'N':
	    if (strlen(optarg) + 1 > 16)
		fatal("Name %s too long, limit 16 characters\n", optarg);

	    oseg_name = optarg;
	    break;

	  case 'E':		/* ELF loadable segment */
	    if (oseg_name == 0)
		fatal("Must use -N before each -E\n");

	    add_segment(optarg, oseg_name);
	    break;

	  case 'V':		/* Set verbosity on */
	    Verbose = 1;
	    break;

	  case 'Q':		/* Set verbosity off */
	    Verbose = 0;
	    break;

	  case 'U':		/* Do not use compression */
	    Compress = 0;
	    break;

	  case 'C':		/* Use compression */
	    Compress = 1;
	    break;

	  default:
	    fatal("Internal error in flag parsing.\n");
	}
    }

    if (optind < argc)
	warn("Extra argument(s) ignored!\n");

    if (disp_hdr) {
	if (argc > 3)
	    usage();
	else
	    display_header(ofile_name);
    }

    if (errflg || promhdr.numsegs == 0)
	usage();

    calc_offsets(&promhdr);

    if (use_s3)
	write_s3(&promhdr, ofile_name, s3_addr);
    else
	write_bin(&promhdr, ofile_name);

    exit(0);
}

/*
 * static void dump_promhdr
 *	Dump the information in a promhdr_t structure.
 */

static void
dump_promhdr(promhdr_t *ph)
{
    printf("PROM Header contains:\n");
    printf("  Magic:       0x%llx\n", ph->magic);
    printf("  Version:     %lld.%lld\n", ph->version, ph->revision);
    printf("  Length:      0x%llx\n", ph->length);
    printf("  Segments:    %lld\n", ph->numsegs);
}

static void
check_data(FILE *binfile, promhdr_t *ph, int segnum)
{
    __uint64_t sum;
    promseg_t *seg = &ph->segs[segnum];

    printf("Checking segment data...\n");

    if (fseek(binfile, (long) seg->offset, SEEK_SET) < 0)
	fatal("Can't seek to data offset 0x%llx: %s\n", seg->offset, ERRSTR);

    if (ftell(binfile) != seg->offset)
	fatal("Seek to data offset 0x%llx failed.\n", seg->offset);

    if ((seg->flags & SFLAG_COMPMASK) != 0) {
	if (pdata_c[segnum])
	    free(pdata_c[segnum]);

	if ((pdata_c[segnum] = malloc(seg->length_c)) == 0)
	    fatal("Out of memory\n");

	if (fread(pdata_c[segnum], 1, seg->length_c, binfile) != seg->length_c)
	    fatal("Can't read prom compressed data: %s\n", ERRSTR);

	sum = sum_data(pdata_c[segnum], seg->length_c);

	if (sum != seg->sum_c)
	    warn("Compressed data sum error (stored: 0x%llx, actual: 0x%llx)\n",
		 seg->sum_c, sum);
	else
	    printf("Compressed data sum matches\n");

	printf("Decompressing...\n");

	switch (seg->flags & SFLAG_COMPMASK) {
	case SFLAG_GZIP:
	    gzip_decompress(&promhdr, segnum);
	    break;
	default:
	    exit(78);
	}

	printf("Successful decompression and data sum\n");
    } else {
	if (pdata[segnum])
	    free(pdata[segnum]);

	if ((pdata[segnum] = malloc(seg->length)) == 0)
	    fatal("Out of memory\n");

	if (fread(pdata[segnum], 1, seg->length, binfile) != seg->length)
	    fatal("Can't read prom image data: %s\n", ERRSTR);

	sum = sum_data(pdata[segnum], seg->length);

	if (sum != seg->sum)
	    warn("Data sum error (stored: 0x%llx, actual: 0x%llx)\n",
		 seg->sum, sum);
	else
	    printf("Data sum matches\n");
    }
}

static void
dump_segments(FILE *binfile, promhdr_t *ph, int start_seg, int num_segs)
{
    int i;

    for (i = start_seg; i < start_seg + num_segs; i++) {
	promseg_t *seg = &ph->segs[i];

	printf("Segment %d:\n", i);
	printf("  Name:        %s\n", seg->name);
	printf("  Flags:       0x%llx\n", seg->flags);
	printf("  Offset:      0x%llx\n", seg->offset);
	printf("  Entry:       0x%llx\n", seg->entry);
	printf("  Load addr:   0x%llx\n", seg->loadaddr);
	printf("  True length: 0x%llx\n", seg->length);
	printf("  True sum:    0x%llx\n", seg->sum);
	printf("  Mem length:  0x%llx\n", seg->memlength);

	if (seg->flags & SFLAG_COMPMASK) {
	    printf("  Cmprsd len:  0x%llx\n", seg->length_c);
	    printf("  Cmprsd sum:  0x%llx\n", seg->sum_c);
	}

	check_data(binfile, ph, i);
    }
}

/*
 * display_header(char *filename)
 *	Opens a prom.out file and displays the contents of the header.
 */

static void
display_header(char *filename)
{
    FILE *binfile;
    struct stat statbuf;

    printf("Reading header for %s\n", filename);

    if (stat(filename, &statbuf))
	fatal("Can't stat input file %s: %s\n", filename, ERRSTR);

    if (statbuf.st_size < sizeof(promhdr))
	fatal("File %s is smaller than header!\n", filename);

    if ((binfile = fopen(filename, "rb")) == 0)
	fatal("Can't open input file %s: %s\n", filename, ERRSTR);

    if (fread(&promhdr, 1, sizeof(promhdr), binfile) != sizeof(promhdr))
	fatal("Can't read prom header from %s\n", filename);

    if (promhdr.magic != PROM_MAGIC)
	fatal("%s contains an unknown prom image (magic 0x%llx)\n",
	      filename, promhdr.magic);

    dump_promhdr(&promhdr);
    dump_segments(binfile, &promhdr, 0, (int)promhdr.numsegs);

    exit(0);
}

/*
 * scan routines are for dozip and unzip
 */

static char *scan_in;
static char *scan_in_end;
static char *scan_out;

static int scan_getc(void)
{
    return (scan_in < scan_in_end) ? *(unsigned char *)scan_in++ : EOF;
}

static void scan_putc(int c)
{
    *scan_out++ = c;
}

static void
gzip_compress(promhdr_t *ph, int segnum)
{
    int clen;
    promseg_t *seg = &ph->segs[segnum];

    if (pdata_c[segnum])
	free(pdata_c[segnum]);

    if ((pdata_c[segnum] = malloc((int)seg->length + 1024)) == 0)
	fatal("Out of memory for compress output.\n");

    scan_in = pdata[segnum];
    scan_in_end = pdata[segnum] + (int)seg->length;
    scan_out = pdata_c[segnum];

    /*
     * Bad news if it actually expands, but this will never happen (gulp)
     */

    if ((clen = dozip(scan_getc, scan_putc)) < 0)
	fatal("Compression failed.\n");

    seg->length_c = (__uint64_t) clen;

    VPRINTF("   Compressed from 0x%llx to 0x%llx (%lld%%)\n",
	    seg->length, seg->length_c,
	    (100 - 100 * seg->length_c / seg->length));

    seg->flags |= SFLAG_GZIP;
    seg->sum_c = sum_data(pdata_c[segnum], seg->length_c);
}

static void
gzip_decompress(promhdr_t *ph, int segnum)
{
    int tmpsize;
    __uint64_t sum;
    int dlen;
    promseg_t *seg = &ph->segs[segnum];
    char *tmpbuf;

    tmpsize = (int)seg->length + 102400;	/* Just guessing */

    if ((tmpbuf = malloc(tmpsize)) == 0)
	fatal("Out of memory for compress temp space.\n");

    if (pdata[segnum])
	free(pdata[segnum]);

    if ((pdata[segnum] = malloc((int)seg->length + 1024)) == 0)
	fatal("Out of memory for compress output.\n");

    scan_in = pdata_c[segnum];
    scan_in_end = pdata_c[segnum] + (int)seg->length_c;
    scan_out = pdata[segnum];

    if ((dlen = unzip(scan_getc, scan_putc, tmpbuf, tmpsize)) < 0)
	fatal("Decompression failed: %s\n", unzip_errmsg(dlen));

    if ((__uint64_t) dlen != seg->length)
	fatal("Decompressed to the wrong size\n");

    sum = sum_data(pdata[segnum], seg->length);

    if (sum != seg->sum)
	fatal("Decompressed sum (0x%llx) != stored sum (0x%llx)\n",
	      sum, seg->sum);

    free(tmpbuf);
}

#if 0
uint
to32addr(__uint64_t address)
{
	uint new_address;

	if (address >= 0x100000000L) {
		if (address & 0xff00000000000000L == 0xa800000000000000L)
			new_address = 0x80000000;
		else if (address & 0xff00000000000000L == 0x9000000000000000L)
			new_address = 0xa0000000;
		else
			new_address = 0x80000000;
		new_address |= (int) address & 0x1fffffff;
		return new_address;
	} else {
		return (uint)address;
	}
}
#endif

/*
 * add_segment
 *	Opens the given file & determines if it is an ELF file; if so
 *	it takes out the useful info; otherwise it says bye-bye
 */

static int
add_segment(char *ifile_name, char *seg_name)
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
    int segnum;
    promseg_t *seg;

    VPRINTF("Reading in segment name (%s), file = %s\n",
	    seg_name, ifile_name);

    if ((fd = open(ifile_name, O_RDONLY)) == -1)
	fatal("Could not open object file %s: %s\n", ifile_name, ERRSTR);

    if (elf_version(EV_CURRENT) == NULL)
	fatal("ELF library out of date for %s.\n", ifile_name);

    if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL)
	fatal("Could not open ELF file %s: %s\n", ifile_name, ERRSTR);

    /* match identification and get bit size of file */

    if ((elf_ident = elf_getident(elf, NULL)) == NULL)
	fatal("Could not find ELF identification in %s: %s\n",
	      ifile_name, ERRSTR);

    elf_size = 32;
    if (memcmp(MipsElf32Ident, elf_ident, EI_NIDENT) != 0) {
	if (memcmp(MipsElf64Ident, elf_ident, EI_NIDENT) == 0)
	    elf_size = 64;
	else
	    fatal("%s: Not a valid MIPS ELF file.\n", ifile_name);
    }

    /* allocate new segment */
    if ((segnum = (int) promhdr.numsegs++) >= PROM_MAXSEGS)
	fatal("Too many segments");

    seg = &promhdr.segs[segnum];

    memset(seg, 0, sizeof (*seg));

    strcpy(seg->name, seg_name);

    if (elf_size == 64)
	elf64_add_segment(ifile_name, fd, elf, segnum);
    else
	elf32_add_segment(ifile_name, fd, elf, segnum);

    seg->sum = sum_data(pdata[segnum], seg->length);

    VPRINTF("   Segment length=%lld, sum=0x%llx, entry=0x%llx\n",
	    seg->length, seg->sum, seg->entry);

    if (Compress) {
	gzip_compress(&promhdr, segnum);

	seg->flags |= SFLAG_GZIP;

	VPRINTF("   Compressed length=%lld, sum=0x%llx\n",
		seg->length_c, seg->sum_c);

	gzip_decompress(&promhdr, segnum);
	VPRINTF("   Compression verified\n");
    }

    seg->flags |= SFLAG_LOADABLE;

    elf_end(elf);
    close(fd);

    return(elf_size);
}

static void
elf32_add_segment(char *fname, int fd, Elf *elf, int segnum)
{
    Elf32_Phdr *phdr;
    Elf32_Ehdr *ehdr;

    int i;
    long len;
    promseg_t *seg;

    seg = &promhdr.segs[segnum];

    /* find the elf header */
    if ((ehdr = elf32_getehdr(elf)) == NULL)
	fatal("Can't get ELF header from %s: %s\n", fname, ERRSTR);

    /* find program header */
    if ((phdr = elf32_getphdr(elf)) == NULL)
	fatal("Can't get ELF program header from %s: %s\n", fname, ERRSTR);

    for (i = 0; i < ehdr->e_phnum; i++)
	if (phdr->p_type == PT_LOAD)
	    break;
	else
	    phdr++;

    if (i == ehdr->e_phnum)
	fatal("No loadable segment in %s.", fname);

    seg->entry = ehdr->e_entry;
    seg->loadaddr = phdr->p_vaddr;
    seg->length = phdr->p_filesz;
    seg->memlength = phdr->p_memsz;

    /* Allocate one nice big chunk of memory for the object code */
    if ((pdata[segnum] = malloc(phdr->p_filesz)) == NULL)
	fatal("Out of memory.\n");

    if (lseek(fd, (off_t) phdr->p_offset, SEEK_SET) == -1)
	fatal("Could not seek in %s: %s\n", fname, ERRSTR);

    if ((len = read(fd, pdata[segnum], phdr->p_filesz)) < 0)
	fatal("Could not read from %s: %s\n", fname, ERRSTR);

    if (len != phdr->p_filesz)
	fatal("Truncated input file: %s\n", fname);
}

static void
elf64_add_segment(char *fname, int fd, Elf *elf, int segnum)
{
    Elf64_Phdr *phdr;
    Elf64_Ehdr *ehdr;

    int i;
    long len;
    promseg_t *seg;

    seg = &promhdr.segs[segnum];

    /* find the elf header */
    if ((ehdr = elf64_getehdr(elf)) == NULL)
	fatal("Can't get ELF header from %s: %s\n", fname, ERRSTR);

    /* find program header */
    if ((phdr = elf64_getphdr(elf)) == NULL)
	fatal("Can't get ELF program header from %s: %s\n", fname, ERRSTR);

    for (i = 0; i < ehdr->e_phnum; i++)
	if (phdr->p_type == PT_LOAD)
	    break;
	else
	    phdr++;

    if (i == ehdr->e_phnum)
	fatal("No loadable segment in %s.", fname);

    seg->entry = ehdr->e_entry;
    seg->loadaddr = phdr->p_vaddr;
    seg->length = phdr->p_filesz;
    seg->memlength = phdr->p_memsz;

    /* Allocate one nice big chunk of memory for the object code */
    if ((pdata[segnum] = malloc(phdr->p_filesz)) == NULL)
	fatal("Out of memory.\n");

    if (lseek(fd, (off_t) phdr->p_offset, SEEK_SET) == -1)
	fatal("Could not seek in %s: %s\n", fname, ERRSTR);

    if ((len = read(fd, pdata[segnum], phdr->p_filesz)) < 0)
	fatal("Could not read from %s: %s\n", fname, ERRSTR);

    if (len != phdr->p_filesz)
	fatal("Truncated input file: %s\n", fname);
}

/*
 * sum_data
 *
 *   Efficiently computes the sum of bytes in a memory range.
 *   (Same code as lo_memsum in IP27 PROM)
 */

static __uint64_t
sum_data(char *base, __uint64_t len)
{
    __uint64_t	sum, mask, v, part;
    int		i;

    mask	= 0x00ff00ff00ff00ffLL;
    sum		= 0;

    while (len > 0 && (__psunsigned_t) base & 7) {
	sum += *(unsigned char *) base++;
	len--;
    }

    while (len >= 128 * 8) {
	part = 0;

	for (i = 0; i < 128; i++) {
	    v = *(__uint64_t *) base;
	    base += 8;
	    part += v	   & mask;
	    part += v >> 8 & mask;
	}

	sum += part	  & 0xffff;
	sum += part >> 16 & 0xffff;
	sum += part >> 32 & 0xffff;
	sum += part >> 48 & 0xffff;

	len -= 128 * 8;
    }

    while (len > 0) {
	sum += *base++;
	len--;
    }

    return sum;
}

/*
 * calc_offsets
 *	Initializes the PROM header fields.
 *  	Scans through the segment list and figures out the offsets
 * 	for each of the segments.  The first segment is PROM_DATA_OFFSET.
 */

static void
calc_offsets(promhdr_t *ph)
{
    int i;
    __uint64_t offset = PROM_DATA_OFFSET;	/* Base address (aligned 8) */
    __uint64_t len;

    ph->magic = PROM_MAGIC;
    ph->version = Version;
    ph->revision = Revision;

    /* Now calculate the offsets of each of the segments */
    for (i = 0; i < ph->numsegs; i++) {
	/* Offsets are double-word aligned. */
	if (ph->segs[i].flags & SFLAG_COMPMASK)
	    len = ph->segs[i].length_c;
	else
	    len = ph->segs[i].length;
	ph->segs[i].offset = offset;
	offset = (offset + len + 7) & ~7;
    }

    ph->length = offset;	/* Also aligned */
}

/*
 * write_s3
 *	Produces an s3 format file as output.
 */

static void
write_s3(promhdr_t *ph, char *ofile_name, __uint64_t s3_addr)
{
    FILE *ofile;
    int i;

    /* First open the output file */
    if ((ofile = fopen(ofile_name, "w")) == NULL)
	fatal("Could not open s3 file %s: %s\n", ofile_name, ERRSTR);

    /* Initialize the S3 stuff */
    s3_init(REC_SIZE);

    s3_setloadaddr(s3_addr);

    s3_process((char*) ph, sizeof(promhdr_t), ofile);

    /* Write out the actual prom */
    for (i = 0; i < ph->numsegs; i++) {
	s3_setloadaddr(s3_addr + ph->segs[i].offset);
	s3_process(pdata_c[i], ph->segs[i].length_c, ofile);
    }

    s3_terminate(ofile, 0 /* ph->entry */);
    fclose(ofile);
}

/*
 * write_bin
 *	Writes a binary image of the segmented IO6 PROM out
 *	to disk.  This format is suitable for use in the flash
 *	command.
 */

static void
write_bin(promhdr_t *ph, char *ofile_name)
{
    FILE *ofile;
    unsigned i;
    __uint64_t offset;

    offset = 0;

    if ((ofile = fopen(ofile_name, "w")) == NULL)
	fatal("Could not open output file %s: %s.\n", ofile_name, ERRSTR);

    /* Write out the PROM header */
    if (fwrite(ph, sizeof(promhdr_t), 1, ofile) != 1)
	fatal("Could not write PROM header to %s: %s\n", ofile_name, ERRSTR);

    offset += sizeof (promhdr_t);

    /* Write out the data section */
    for (i = 0; i < ph->numsegs; i++) {
	promseg_t *seg = &ph->segs[i];

	while (offset < seg->offset) {
	    fputc(0, ofile);
	    offset++;
	}

	if (seg->flags & SFLAG_COMPMASK) {
	    if (fwrite(pdata_c[i], 1, seg->length_c, ofile) != seg->length_c)
		fatal("Could not write compressed data to %s: %s\n",
		      ofile_name, ERRSTR);

	    offset += seg->length_c;
	} else {
	    if (fwrite(pdata[i], 1, seg->length, ofile) != seg->length)
		fatal("Could not write PROM data to %s: %s\n",
		      ofile_name, ERRSTR);

	    offset += seg->length;
	}
    }

    while (offset < ph->length) {
	fputc(0, ofile);
	offset++;
    }

    fclose(ofile);
}
