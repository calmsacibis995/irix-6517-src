#include <sys/types.h>
#include <sys/IP32flash.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/syssgi.h>
#include <sys/sbd.h>
#include <invent.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include "flashcommon.h"

#define OPTS	"O:cvfqTs:y"

#define IMP_MAX_TYPES 6

#define IMP_PLAIN	0
#define IMP_SC		1
#define IMP_LM		2
#define IMP_SCLM	3
#define IMP_MP		4
#define IMP_MPLM	5

/*
 * coprocessor revision identifiers
 */
union rev_id {
    unsigned int	ri_uint;
    struct {
#ifdef MIPSEB
	unsigned int	Ri_fill:16,
				Ri_imp:8,		/* implementation id */
				Ri_majrev:4,		/* major revision */
				Ri_minrev:4;		/* minor revision */
#endif
#ifdef MIPSEL
		unsigned int	Ri_minrev:4,		/* minor revision */
				Ri_majrev:4,		/* major revision */
				Ri_imp:8,		/* implementation id */
				Ri_fill:16;
#endif
	} Ri;
};
#define	ri_imp		Ri.Ri_imp
#define	ri_majrev	Ri.Ri_majrev
#define	ri_minrev	Ri.Ri_minrev

struct imp_tbl {
    uint *it_types;
    unsigned it_imp;
};

uint r4600_tbl[] = {0x1, 0x2, 0x0, 0x0, 0x0, 0x0};
uint r5000_tbl[] = {0x4, 0x8, 0x10, 0x20, 0x0, 0x0};
uint r10k_tbl[]  = {0x0, 0x40, 0x0, 0x100, 0x80, 0x200};

/* The code assumes that any number greater than the number in the
 * table is that type of processor.  They must be in decending numerical
 * order.  The below table is in the kernel also, so the values must
 * be updated there also if cpu_imp_tbl is changed in hinv.c.
 */
struct imp_tbl cpu_imp_tbl[] = {
	{ NULL,		C0_MAKE_REVID(C0_IMP_UNDEFINED,0,0) },
	{ r5000_tbl,	C0_MAKE_REVID(C0_IMP_R5000,0,0) },
	{ NULL,		C0_MAKE_REVID(C0_IMP_R4650,0,0) },
	{ NULL,		C0_MAKE_REVID(C0_IMP_R4700,0,0) },
	{ r4600_tbl,	C0_MAKE_REVID(C0_IMP_R4600,0,0) },
	{ NULL,		C0_MAKE_REVID(C0_IMP_R8000,0,0) },
	{ r10k_tbl,	C0_MAKE_REVID(C0_IMP_R10000,0,0) },
	{ NULL,		C0_MAKE_REVID(C0_IMP_R6000A,0,0) },
	{ NULL,		C0_MAKE_REVID(C0_IMP_R4400,C0_MAJREVMIN_R4400,0) },
	{ NULL,		C0_MAKE_REVID(C0_IMP_R4000,0,0) },
	{ NULL,		C0_MAKE_REVID(C0_IMP_R6000,0,0) },
	{ NULL,		C0_MAKE_REVID(C0_IMP_R3000A,C0_MAJREVMIN_R3000A,0) },
	{ NULL,		C0_MAKE_REVID(C0_IMP_R3000,C0_MAJREVMIN_R3000,0) },
	{ NULL,		C0_MAKE_REVID(C0_IMP_R2000A,C0_MAJREVMIN_R2000A,0) },
	{ NULL,		C0_MAKE_REVID(C0_IMP_R2000,0,0) },
	{ NULL,		0 }
};

char *progname;

int 
correct_rev(int *rev)
{
    inventory_t *invent;
    int major_is_same = 0;
    int rv = 0;

    setinvent();
    while (invent = getinvent()) {
	if (invent->inv_class == INV_MEMORY && invent->inv_type == INV_PROM) {
	    if (invent->inv_controller > rev[FLASHPROM_MAJOR]) {
		break;
	    }
	    else if (invent->inv_controller == rev[FLASHPROM_MAJOR]) {
		major_is_same = 1;
	    }
	    if (invent->inv_unit >= rev[FLASHPROM_MINOR] && major_is_same) {
		break;
	    }
	    rv = 1;
	    break;
	}
    }
    endinvent();

    return(rv);
}

void
showversion()
{
    int major, minor;
    inventory_t *invent;

    setinvent();
    while (invent = getinvent()) {
	if (invent->inv_class == INV_MEMORY && invent->inv_type == INV_PROM) {
	    major = invent->inv_controller;
	    minor = invent->inv_unit;
	    break;
	}
    }
    endinvent();
    printf("Flashprom version is %d.%d\n", major, minor);
}

int
lm_present(void)
{
    return(0);
}

int
mp_present(void)
{
    return(0);
}

int
sc_present(void)
{
    inventory_t *invent;	
    union rev_id ri;

    setinvent();
    while (invent = getinvent()) {
	if (invent->inv_class == INV_MEMORY && 
	    ((invent->inv_type == INV_SIDCACHE) ||
	     (invent->inv_type == INV_SDCACHE) ||
	     (invent->inv_type == INV_SICACHE))) {
	    endinvent();
	    return (1);
	}
    }
    endinvent();
    return(0);
}

uint 
cpu_subtype(uint *types)
{
    int config = 0;

    if (types == NULL)
	return(0);

    config |= sc_present();
    config |= lm_present();
    config |= mp_present();

    switch (config & 7) {
    case 0:
	config = IMP_PLAIN;
	break;
    case 1: 
	config = IMP_SC;
	break;
    case 2:
	config = IMP_LM;
	break;
    case 3:
	config = IMP_SCLM;
	break;
    case 4:
    case 6:
	return(0);	/* this is impossible */
	break;
    case 5:
	config = IMP_MP;
	break;
    case 7:
	config = IMP_MPLM;
	break;
    }
    return(types[config]);
}

int 
this_cputype(union rev_id ri)
{
    struct imp_tbl *itp;
    for (itp = cpu_imp_tbl; itp->it_imp; itp++) {
	if (((ri.ri_imp << 8) | (ri.ri_majrev << 4) | (ri.ri_minrev))
	    >= itp->it_imp)
	    return(cpu_subtype(itp->it_types));
    }

    return(0);
}

int
supported_cpu(int types)
{
    inventory_t *invent;	
    union rev_id ri;

    setinvent();
    while (invent = getinvent()) {
	if (invent->inv_class == INV_PROCESSOR && 
	    invent->inv_type == INV_CPUCHIP) {
	    ri.ri_uint = (uint)(invent->inv_state);
	    endinvent();
	    return (types & this_cputype(ri));
	}
    }
    endinvent();
    return(0);
}

void
usage(void)
{
    fprintf(stderr,
       "%s [-O offset] [-c] [-f] [-q] [-T] [-s #] file\n", progname);
    fprintf(stderr,
       "%s [-v] [-y]\n", progname);
    fprintf(stderr, "\t-O offset\t-- override default offset\n");
    fprintf(stderr, "\t-f\t\t-- force installation of flash\n");
    fprintf(stderr, "\t-v\t\t-- print current flash version\n");
    fprintf(stderr, "\t-q\t\t-- quiet\n");
    fprintf(stderr, "\t-T\t\t-- Total, flash all 512K\n");
    fprintf(stderr, "\t-c\t\t-- compare flash images (ignored)\n");
    fprintf(stderr, "\t-s #\t\t-- slot number (ignored)\n");
    fprintf(stderr, "\t-y\t\t-- proceed if -T specified\n");
    
}

main(int argc, char **argv)
{
    int o, c;	
    int versoverride = 0;
    int cpuoverride = 0;
    uint offset = 0;
    int displayvers = 0;
    int fd;
    char *fp;
    char *prom;
    int len;
    int quiet = 0;
    promhdr_t *hp;
    struct stat sbuf;
    char *infile;
    int dowhole = 0;
    int doit = 0;
    int dontask = 0;

    progname = argv[0];

    while ((o = getopt(argc, argv, OPTS)) != EOF) {
	switch (o) {
	case 'O':
	    offset = strtoul(optarg, NULL, 0);
	    break;
	case 'f':
	    versoverride = 1;
	    cpuoverride = 1;
	    break;
	case 'v':
	    displayvers = 1;
	    break;
	case 'T':
	    dowhole = 1;
	    break;
	case 's':
	case 'c':
	    break;
	case 'q':
	    quiet = 1;
	    break;
	case 'y':
	    dontask = 1;
	    break;
	default:
	    usage();
	    exit(1);
	    break;
	}
    }

    if (displayvers && (offset||versoverride||cpuoverride||(argc != 2))) {
	usage();
	exit(1);
    } else if (displayvers && argc != 2) {
	usage();
	exit(1);
    } else if (displayvers) {
	showversion();
	exit(0);
    }

    if (dowhole && offset) {
	usage();
	exit(1);
    }

    infile = argv[optind];

    if (infile == NULL) {
	usage();
	exit(1);
    }
		
    if ((fd = open(infile, O_RDONLY)) < 0) {
	perror("open");
	exit(2);
    }

    if (fstat(fd, &sbuf) < 0) {
	perror("fstat");
	exit(2);
    }

    /* round file length up to nearest 256 byte boundry */
    len = (sbuf.st_size + FLASH_PAGE_SIZE) & ~(FLASH_PAGE_SIZE - 1);

    if ((int)(fp = mmap(0, len, PROT_READ, MAP_SHARED, fd, 0)) == -1) {
	perror("mmap");
	exit(2);
    }

    hp = (promhdr_t *)fp;

    if (hp->magic != PROM_MAGIC) {
	if (!quiet)
	    fprintf(stderr, "%s: bad magic number\n", progname);
	exit(2);
    }

    if (!offset && !dowhole)
	offset = hp->offset;

    if (checksum(fp + sizeof(promhdr_t), hp->len) + hp->cksum != 0) {
	if (!quiet)
	    fprintf(stderr, "%s: bad PROM checksum\n", progname);
	exit(3);
    }

    if (!correct_rev(hp->version) && !versoverride) {
	if (!quiet) {
	    fprintf(stderr, "%s: bad PROM revision\n", progname);
	    exit(4);
	}
	exit(0);
    }

    if (!supported_cpu(hp->cputypes) && !cpuoverride) {
	if (!quiet)
	    fprintf(stderr, "%s: unsupported cpu type\n", progname);
	exit(5);
    }

    if (offset & (FLASH_PAGE_SIZE - 1)) {
	if (!quiet)
	    fprintf(stderr, "%s: bad starting alignment\n", progname);
	exit(6);
    }

    if (hp->len > FLASH_SIZE) {
	if (!quiet)
	    fprintf(stderr, "%s: PROM image size too large\n", progname);
	exit(7);
    }

    if (dowhole && !dontask) {
	printf("About to write entire contents of flash ram\n");
	printf("This could result in corruption of the protected\n");
	printf("flash segment.  Are you sure (y/n)? ");
	while ((c = getchar()) != EOF) {
	    switch (c) {
	    case 'y':
	    case 'Y':
		doit = 1;
		break;
	    case 'n':
	    case 'N':
		printf("Flash installation cancelled\n");
		exit(0);
		break;
	    default:
		printf("Please type 'y' or 'n': ");
		break;
	    }
	    if (doit)
		break;
	}
    }

    if (!quiet)
	printf("Programming flash.  This will take about 30 seconds.\n");

    /* allow output to drain */
    (void)sleep(5);

    prom = (char *)((int)fp + sizeof(promhdr_t) + offset);
    if (syssgi(SGI_WRITE_IP32_FLASH, prom, hp->len - offset, offset)) {
	if (!quiet) {
	    if (errno == EIO)
		perror("WARNING: flash programming failed");
	    else
		perror("Bad parameter for SGI_WRITE_IP32_FLASH");
	}
	exit(8);
    }
    exit(0);
}
