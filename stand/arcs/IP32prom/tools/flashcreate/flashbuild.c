#include <sys/types.h>
#include <sys/IP32flash.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include "flashcommon.h"

#define OPTS "po:c:O:"

int cputypes = 0xf; /* bits from cpubits[] */
char *outfile = "prom.inst";
char *infile;
int flash_ver[2];
uint loadoffset = 0x4400;
int print_seg_info = 0;

extern int optind;
extern char *optarg;
extern int opterr;

static int
valid_seg_hdr(FlashSegment *seg)
{
    long *xslim = body(seg);
    long *xsp;
    long xsum;

    if (print_seg_info)
	printf("seg limit is 0x%x\n", xslim);

    if ((seg->magic != FLASH_SEGMENT_MAGIC) ||
	((int)seg & FLASH_PAGE_SIZE - 1) ||
	(seg->segLen & 0x3)) {
	if (print_seg_info)
	    printf("seg magic, alignment or length is invalid\n");
	return(0);
    }

    for (xsum = 0, xsp = (long *)seg; xsp != xslim; xsp++) 
	xsum += *xsp;

    if (print_seg_info)
	printf("calculated checksum is: %d\n", xsum);

    return(xsum == 0);
}

/*
 * next_seg returns a pointer to the next valid segment header
 * in the flash.  It returns NULL if no valid header is found 
 * before reaching the end of the flash address space.
 *
 * NB: if *flash points to a valid header next_seg will return
 * the value of flash.
 */
static FlashSegment *
next_seg(char *flash, char *lim)
{
    long *magicptr = (long *)((int)flash & ~0x3); /* ensure proper alignment */
    FlashSegment *seg;

    /* 
     * since we are looking for an int, align to last int
     * in the file
     */
    lim = (char *)((int)lim & ~0x3);

next:
    while (*magicptr != FLASH_SEGMENT_MAGICx) {
	if (magicptr >= (long *)(lim)) {
	    return(NULL);
	} else {
	    magicptr++;
	}
    }

    seg = (FlashSegment *)((int)magicptr - sizeof(long long));
    if (valid_seg_hdr(seg)) {
	if (seg->segLen != 0)
	    return(seg);
	else
	    return(NULL);

    } else {
	if (++magicptr >= (long *)lim)
	    return(NULL);
    }

    goto next;
}

void
print_segs(char *flash, char *lim)
{
    long *magicptr = (long *)((int)flash & ~0x3); /* ensure proper alignment */
    FlashSegment *seg;
    char strbuf[256];

    /* 
     * since we are looking for an int, align to last int
     * in the file
     */
    lim = (char *)((int)lim & ~0x3);

next:
    while (*magicptr != FLASH_SEGMENT_MAGICx) {
	if (magicptr >= (long *)(lim)) {
	    return;
	} else {
	    magicptr++;
	}
    }

    seg = (FlashSegment *)((int)magicptr - sizeof(long long));
    printf("segment address\t: 0x%x\n", seg);
    printf("seg->reserved\t: 0x%llx\n", seg->reserved);
    printf("seg->magic\t: 0x%x\n", seg->magic);
    printf("seg->segLen\t: 0x%x\n", seg->segLen);

    printf("seg->nameLen\t: 0x%x\n", seg->nameLen);
    printf("seg->vsnLen\t: 0x%x\n", seg->vsnLen);
    printf("seg->segType\t: 0x%x\n", seg->segType);
    printf("seg->pad    \t: 0x%x\n", seg->pad);

    strncpy(strbuf, seg->name, seg->nameLen);
    strbuf[seg->nameLen] = '\0';
    printf("seg->name\t: %s\n", strbuf);

    strncpy(strbuf, seg->version, seg->vsnLen);
    strbuf[seg->vsnLen] = '\0';
    printf("seg->version\t: %s\n", strbuf);

    printf("seg->chksum\t: %d\n", seg->chksum);

    if (valid_seg_hdr(seg)) {
	printf("This segment is VALID\n\n");
    } else {
	printf("This segment is INVALID\n\n");
    }

    magicptr++;
    goto next;
}

/*
 * find_named_flash_seg() returns a pointer to the first data byte
 * in the named flash ROM segment.
 */
static FlashSegment *
find_named_flash_seg(char *name, char *fp, int len)
{
    FlashSegment *seg = (FlashSegment *)fp;

    while (seg = next_seg((char *)seg, (char *)((int)fp + (len - 1)))) {
	if (!strncmp(seg->name, name, seg->nameLen))
	    return(seg);
	seg = (FlashSegment *)((int)seg + seg->segLen);
    }
    return(NULL);
}

#ifdef isdigit
#undef isdigit
#endif
#define isdigit(x) (((x) >= '0') && ((x) <= '9'))
#define toint(x) ((int)(x) - (int)('0'))

static void
get_flash_version(char *fp, int len)
{
    FlashSegment *seg = find_named_flash_seg("version", fp, len);
    char *verno;

    flash_ver[FLASHPROM_MAJOR] = flash_ver[FLASHPROM_MINOR] = 0;

    if (seg == NULL) {
    	seg = find_named_flash_seg("firmware", fp, len);
	if (seg == NULL) {
	    fprintf(stderr, "no valid version or firmware segment found\n");
	    exit(4);
	}
    }

    if (seg->vsnLen == 0)
	return;

    verno = seg->version;
      
    /*
     * flash version numbers are of the form x.y where x and y
     * are both decimal integers.  Check to ensure that first
     * character of the version string is a decimal digit.
     */
    if (!isdigit(*verno))
	return;

    /*
     * while reading decimal digits calculate the integer version number
     */
    while (isdigit(*verno)) {
	flash_ver[FLASHPROM_MAJOR] = (flash_ver[FLASHPROM_MAJOR] * 10) + toint(*verno);
	verno++;
    }

    /*
     * we should have stopped at a decimal point
     * if not, we don't have a valid version string, reset major to 0
     * and return
     */
    if (*verno++ != '.') {
	flash_ver[FLASHPROM_MAJOR] = 0;
	return;
    }

    /*
     * a digit should follow the decimal point, if not, we don't
     * have a valid version string.
     */
    if (!isdigit(*verno)) {
	flash_ver[FLASHPROM_MAJOR] = 0;
	return;
    }
	
    /*
     * get the minor version number.
     */
    while (isdigit(*verno)) {
	flash_ver[FLASHPROM_MINOR] = (flash_ver[FLASHPROM_MINOR] * 10) + toint(*verno);
	verno++;
    }
}

int 
flash_version(int which, char *fp, int len)
{
    static int beenthere = 0;

    if (beenthere)
	return(which ? flash_ver[FLASHPROM_MINOR] : flash_ver[FLASHPROM_MAJOR]);

    get_flash_version(fp, len);

    beenthere = 1;

    return(which ? flash_ver[FLASHPROM_MINOR] : flash_ver[FLASHPROM_MAJOR]);
}

void
create_header(promhdr_t *hp, char *fp, int len, int cputypes, uint off)
{
    hp->magic = PROM_MAGIC;
    hp->offset = off;
    hp->len = len;
    hp->version[FLASHPROM_MAJOR] = flash_version(FLASHPROM_MAJOR, fp, len);
    hp->version[FLASHPROM_MINOR] = flash_version(FLASHPROM_MINOR, fp, len);
    hp->cputypes = cputypes;
    hp->cksum = gensum(fp, len);
}

void
write_outfile(char *outfile, promhdr_t *hp, char *fp, int len)
{
    int fd;
    char hdrbuf[FLASH_PAGE_SIZE];
    
    if ((fd = open(outfile, O_RDWR|O_TRUNC|O_CREAT, 0644)) < 0) {
	perror("open");
	exit(3);
    }

    bzero(hdrbuf, FLASH_PAGE_SIZE);
    bcopy(hp, hdrbuf, sizeof(promhdr_t));

    if (write(fd, hdrbuf, FLASH_PAGE_SIZE) < 0) {
	perror("write");
	exit(3);
    }

    if (write(fd, fp, len) < 0) {
	perror("write");
	exit(3);
    }

    close(fd);
}
	
void
unknown_cpu(void)
{
    int i;

    fprintf(stderr, "Unknown cpu type.  Recognized cpu types are:\n");
    for (i = 0; i < NCPUTYPES; i++) {
	fprintf(stderr, "\t%s\n", cpustrings[i]);
    }
}

void
usage(void)
{
    fprintf(stderr,
       "flashbuild [-p] [-o file] [-c cputype[,cputype...]] [-O offset] file\n");
}

main(int argc, char **argv)
{
    int o;
    struct stat sbuf;
    promhdr_t hdr;
    char *options;
    int fd;
    char *fp;
    int len;

    opterr = 0;

    if (argc < 2) {
	usage();
	exit(1);
    }

    while ((o = getopt(argc, argv, OPTS)) != EOF) {
	switch (o) {
	case 'p':
	    print_seg_info = 1;
	    break;
	case 'o':
	    outfile = optarg;
	    break;
	case 'c':
	    options = optarg;
	    cputypes = 0;
	    while (*options) {
		int i;
		char *value;
		switch (i = getsubopt(&options, cpustrings, &value)) {
		case R4600:
		case R4600SC:
		case R5000:
		case R5000SC:
		case R5000LM:
		case R5000SCLM:
		case R10000:
		case R10000MP:
		case R10000LM:
		case R10000MPLM:
		    cputypes |= cpubits[i];
		    break;
		default:
		    unknown_cpu();
		    usage();
		    exit(1);
		}
	    }
	    break;
	case 'O':
	    loadoffset = strtoul(optarg, NULL, 0);
	    break;
	default:
	    usage();
	    exit(1);
	}
    }

    if (optind != argc - 1) {
	usage();
	exit(1);
    }

    infile = argv[optind];
		
    if ((fd = open(infile, O_RDONLY)) < 0) {
	perror("open");
	exit(2);
    }

    if (fstat(fd, &sbuf) < 0) {
	perror("fstat");
	exit(2);
    }

    /* round file length up to nearest 256 byte boundry */
    len = (sbuf.st_size + 255) & ~0xff;

    if ((int)(fp = mmap(0, len, PROT_READ, MAP_SHARED, fd, 0)) == -1) {
	perror("mmap");
	exit(2);
    }

    if (print_seg_info) {
	print_segs(fp, (char *)(fp + len - 1));
	exit(0);
    }

    /*
     * current flash images have a segment header as the first
     * structure in the file -- check to see if this is so.
     * XXX: if the layout of the flash image file changes, this
     * assumption may no longer hold true.
     */
    if (!valid_seg_hdr((FlashSegment *)fp)) {
	fprintf(stderr, "not a valid flash image file\n");
	exit(4);
    }

    create_header(&hdr, fp, len, cputypes, loadoffset);
    
    write_outfile(outfile, &hdr, fp, len);

    exit(0);
}
