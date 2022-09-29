/* Convert a single AMD relocatable COFF file
 * to a C source file initializing an array of bytes
 * or to an "Extended Tektronix Hex Format" file.
 */

#ident "$Revision: 1.8 $"

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <values.h>
#include <sys/file.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

#include "coff.h"			/* the AMD version */


static void usage(void);
static void ckread(void *buf, int len);
static void ckskip(int off);
static void outfirm(void);
static void outhex(void);


static int ifd = 0;			/* use stdin by default */
static FILE *ofile = stdout;		/* and stdout */
static char *ofilename;

static int haderr;

static char* pgmname;
static int verbose;

/* generate a version number from the last word of binary, and then
 * remove that word from the result.
 */
static int doversion;

/* generate a Tektronic Extended Hex file
 */
static int hexfile;

/* generate a checksum and append it to the output file
 */
static int docksum;

/* the checksum is only 16-bits wide
 */
static int cksum16;

/* use old, IPG style initial execution address convention, where the
 * address is generate at address 0
 */
static int oldexec;


static char *firmname;
static char *comment = "FDDI firmware";

#define MEMLEN (128*1024)
static unsigned long mem[MEMLEN/4];
static int base;
static int maxaddr;
static int minaddr = MAXINT;
static int maxwaddr, minwaddr;

static int version, yr, mon, day, hr, min;

static struct filehdr filehdr;
#define MAXSCNS 10
static struct scnhdr scnhdrs[MAXSCNS];


main(int argc, char *argv[])
{
    int i;
    int scn;
    char *p;
    int addr;


    pgmname = argv[0];

    while ((i = getopt(argc, argv, "i:t:o:n:b:vhdcCx")) != EOF)
	switch (i) {
	case 'i':
	    ifd = open(optarg, O_RDONLY, 0);
	    if (ifd < 0) {
		(void)fprintf(stderr, "%s: %s: %s\n",
			      pgmname, optarg, strerror(errno));
		exit(1);
	    }
	    break;

	case 'o':
	    ofilename = optarg;
	    break;

	case 'n':
	    firmname = optarg;
	    break;

	case 't':
	    comment = optarg;
	    break;

	case 'b':
	    base = strtol(optarg,&p,0);
	    if (*p != '\0' || base < 0) {
		(void)fprintf(stderr, "%s: bad base \"%s\"\n",
			      pgmname, optarg);
		exit(1);
	    }
	    break;

	case 'v':
	    verbose++;
	    break;

	case 'h':
	    hexfile = 1;
	    break;

	case 'd':
	    doversion = 1;
	    break;

	case 'c':
	    docksum = 1;
	    break;

	case 'C':
	    docksum = 1;
	    cksum16 = 1;
	    break;

	case 'x':
	    oldexec = 1;
	    break;

	default:
	    usage();
    }
    if (!firmname && !hexfile)
	usage();

    ckread(&filehdr, sizeof(filehdr));
    if (verbose) {
	(void)fprintf(stderr,
		      "magic=%#o nscns=%#o nsyms=%-4ld opthdr=%u flags=%#o\n",
		      filehdr.f_magic,
		      filehdr.f_nscns,
		      filehdr.f_nsyms,
		      filehdr.f_opthdr,
		      filehdr.f_flags);
    }

    /* skip the a.out header
     */
    if (filehdr.f_opthdr != 0)
	ckskip(filehdr.f_opthdr);

    /* read the section headers */
    for (scn = 0; scn < filehdr.f_nscns; scn++) {
	if (scn >= MAXSCNS) {
	    (void)fprintf(stderr, "%s: too many sections\n", pgmname);
	    exit(1);
	}
	ckread(&scnhdrs[scn], sizeof(scnhdrs[scn]));
	if (verbose) {
	    (void)fprintf(stderr, "section %-9s flags=0x%04lx ",
			  scnhdrs[scn].s_name,
			  scnhdrs[scn].s_flags);
	    (void)fprintf(stderr,
			  "paddr=%#-8lx size=%#-8lx nreloc=%-2u\n",
			  scnhdrs[scn].s_paddr,
			  scnhdrs[scn].s_size,
			  scnhdrs[scn].s_nreloc);
	}
    }

    /* read the text
     */
    for (scn = 0; scn < filehdr.f_nscns; scn++) {
	if (scnhdrs[scn].s_size == 0)
	    continue;

	/* ignore comment and junk sections
	 */
	if (scnhdrs[scn].s_flags & (STYP_NOLOAD | STYP_INFO | STYP_BSS)) {
	    if (verbose)
		(void)fprintf(stderr,
			      "skipping %#lx bytes of section \"%s\"\n",
			      scnhdrs[scn].s_size, scnhdrs[scn].s_name);
	    ckskip(scnhdrs[scn].s_size);
	    continue;
	}

	/* ignore skip bss
	 */
	if (scnhdrs[scn].s_flags & STYP_BSS) {
	    if (verbose)
		(void)fprintf(stderr,
			      "skipping %#lx bytes of section \"%s\"\n",
			      scnhdrs[scn].s_size, scnhdrs[scn].s_name);
	    continue;
	}

	if (scnhdrs[scn].s_nreloc != 0
	    || !(scnhdrs[scn].s_flags & STYP_ABS)) {
	    (void)fprintf(stderr,
			  "%s: cannot handle relocatable \"%s\" section\n",
			  pgmname, scnhdrs[scn].s_name);
	    exit(1);
	}

	addr = scnhdrs[scn].s_paddr - base;
	if (addr < 0) {
	    (void)fprintf(stderr,
			  "%s: address %#lx below base %#x in \"%s\"\n",
			  pgmname, scnhdrs[scn].s_paddr, base,
			  scnhdrs[scn].s_name);
	    exit(1);
	}

	i = addr + scnhdrs[scn].s_size;
	if (i >= MEMLEN) {
	    (void)fprintf(stderr, "%s: section \"%s\" is too big at %#lx\n",
			  pgmname, scnhdrs[scn].s_name,
			  addr + scnhdrs[scn].s_size);
	    exit(1);
	}

	if (verbose)
	    (void)fprintf(stderr,
			      "reading %#lx bytes of section \"%s\"\n",
			      scnhdrs[scn].s_size, scnhdrs[scn].s_name);
	ckread(&mem[(addr+3)/4], scnhdrs[scn].s_size);
	if (i > maxaddr)
	    maxaddr = i;
	if (addr < minaddr)
	    minaddr = addr;
    }

    minwaddr = minaddr/4;
    maxwaddr = (maxaddr+3)/4;

    if (doversion) {
	maxwaddr--;
	maxaddr -= 4;
	version = mem[maxwaddr];
	yr = (version/(60*24*32*13)) + 92;
	mon = (version/(60*24*32)) % 13;
	day = (version/(60*24)) % 32;
	hr = (version/60) % 24;
	min = version % 60;
    }

    /*  compute the IPG-style checksum and append it to the program
     */
    if (docksum) {
	unsigned long cksum = 0;
	addr = minwaddr;
	if (oldexec)
		addr++;
	if (cksum16) {
	    while (addr < maxwaddr) {
		cksum -= (mem[addr] + (mem[addr]>>16));
		addr++;
	    }
	    cksum &= 0xffff;
	} else {
	    while (addr < maxwaddr)
		cksum -= mem[addr++];
	}
	mem[maxwaddr++] = cksum;
	maxaddr += 4;
    }

    if (ofilename != 0) {
	ofile = fopen(ofilename, "w");
	if (!ofile) {
	    (void)fprintf(stderr, "%s: %s: %s\n",
			  pgmname, ofilename, strerror(errno));
	    exit(1);
	}
    }

    if (hexfile)
	outhex();
    else
	outfirm();

    return haderr;
}



/* output the data statements
 */

#define DDATA 0
#define DDATA_LIM 127
#define DZERO (DDATA_LIM+1)
#define DZERO_LIM 232
#define DNOP (DZERO_LIM+1)
#define DNOP_LIM 255
#define DNULL (-1)

#define NOP 0x70400101			/* a 29K NOP instruction */


static int pos = 0;			/* current line position */
static int addr0;
static int pcomp;


/* add a number to the output
 */
static void
pbyte(char *pat, unsigned char x)
{
    (void)fprintf(ofile,pat, x);
    pos += 6;
    if (pos > 72) {
	(void)putc('\n',ofile);
	pos = 0;
    }
}

static void
pword(unsigned long w)
{
    pbyte(" 0x%02x,", w >> 24);
    pbyte(" 0x%02x,", w >> 16);
    pbyte(" 0x%02x,", w >> 8);
    pbyte(" 0x%02x,", w);
}

static void
flushdata()
{
    if (pcomp == DNULL)
	return;

    pbyte(" %4d,", pcomp);
    for (; pcomp >= DDATA && pcomp <= DDATA_LIM; pcomp--) {
	pword(mem[addr0++]);
    }
    fflush(ofile);
    pcomp = DNULL;
}


/* output a .firm file
 */
static void
outfirm(void)
{
    int v;
    int addr;

    pos = 0;
    pcomp = DNULL;

    if (doversion) {
	(void)fprintf(ofile,"/* %s %s */\n\n", firmname, comment);

	(void)fprintf(ofile,"static int %s_minaddr = 0x%x;\n", firmname,
		      base+minaddr);
	(void)fprintf(ofile,"static int %s_maxaddr = 0x%x;\n", firmname,
		      base+maxaddr);
	(void)fprintf(ofile,"static int %s_vers = %#x;\n", firmname, version);
    } else {
	/* break it up to keep RCS from fidding with the string */
	(void)fprintf(ofile,"/* %s %s\n *\t$Revision: ", firmname, comment);
	(void)fprintf(ofile,"xx$\n */\n\n");
    }

    (void)fprintf(ofile, "\n#define %s_DDATA %d\n", firmname, DDATA);
    (void)fprintf(ofile, "#define %s_DZERO %d\n", firmname, DZERO);
    (void)fprintf(ofile, "#define %s_DNOP %d\n", firmname, DNOP);

    (void)fprintf(ofile, "static u_char %s_txt[] = {\n", firmname);

    if (oldexec) {
	if (minaddr != 0) {
	    pword(0);
	    haderr++;
	} else {
	    pword(mem[0]);
	    minaddr = 4;
	    minwaddr = 1;
	}
    }

    addr = minwaddr;
    while (addr < maxwaddr) {
	v = mem[addr++];

	if (0 == v) {
	    if (pcomp >= DZERO_LIM || pcomp < DZERO) {
		flushdata();
		pcomp = DZERO;
	    } else {
		pcomp++;
	    }

	} else if (NOP == v) {
	    if (pcomp >= DNOP_LIM || pcomp < DNOP) {
		flushdata();
		pcomp = DNOP;
	    } else {
		pcomp++;
	    }

	} else {
	    if (pcomp >= DDATA_LIM || pcomp < DDATA) {
		flushdata();
		addr0 = addr-1;
		pcomp = DDATA;
	    } else {
		pcomp++;
	    }
	}
    }

    flushdata();
    (void)fputs("};\n", ofile);
}


#define MAXRLEN 32
static u_char hbuf[MAXRLEN];
static int hbuflen;
static int saddr;

/* compute extended Textronix checksum for a word
 */
static unsigned int
ckhex(unsigned long v)
{
    unsigned int ck;

    ck = 0;
    while (v != 0) {
	ck += (v & 0xf);
	v >>= 4;
    }

    return ck;
}

/* Extended Tektronix Hexadecimal Format
 * item    characters   description
 * -------------------------------------------------------------------
 * %		  1	signifies Extended Tek Hex format
 * block length	  2	Number of characters in record, minus the %, in hex
 * block type	  1	6 = data record,  8 = termination record
 * checksum	  2	2 hex digits, sum modulo 256, of rest of record.
 * address length 1	hex digit, this many following characters are address
 *			0 means 16 characters follow.
 * load address variable  Address to load following data (or begin program)
 * Load data	variable & optional.  Pairs of hex digits.
 *			Length is block_length - (6 + address_length)


/* output a .hex line
 */
static void
flushhex(void)
{
    int hcksum, i, alen, rlen;

    if (hbuflen != 0) {
	alen = 1;
	for (i = saddr>>4; i != 0; i >>= 4)
	    alen++;

	rlen = sizeof("lltcca")-1 + hbuflen*2 + alen;

	hcksum = ckhex(saddr) + ckhex(alen) + ckhex(rlen) + 6;
	for (i = 0; i < hbuflen; i++)
	    hcksum += ckhex(hbuf[i]);

	(void)fprintf(ofile, "%%%02X6%02X%X%X",
		      rlen, hcksum & 0xff, alen, saddr);
	for (i = 0; i < hbuflen; i++)
	    (void)fprintf(ofile, "%02X", hbuf[i]);
	(void)fputs("\r\n",ofile);

	saddr += hbuflen;
	hbuflen = 0;
    }
}

/* Output a termination record
 */
static void
terminate_hex( int start_addr )
{
	int hcksum, i, alen, rlen;

	alen = 1;
	for (i = start_addr>>4; i != 0; i >>= 4)
		alen++;

	rlen = sizeof("lltcca")-1 + alen;

	hcksum = ckhex(start_addr) + ckhex(alen) + ckhex(rlen) + 8;

	(void)fprintf(ofile, "%%%02X8%02X%X%X\r\n",
			rlen, hcksum & 0xff, alen, start_addr);

}

/* output a .hex file
 */
static void
outhex(void)
{
    int addr;
    unsigned long v;

    hbuflen = 0;
    addr = minwaddr;
    saddr = base+minaddr;
    while (addr < maxwaddr) {
	v = mem[addr++];
	hbuf[hbuflen] = v>>24;
	if (++hbuflen >= MAXRLEN)
	    flushhex();
	hbuf[hbuflen] = v>>16;
	if (++hbuflen >= MAXRLEN)
	    flushhex();
	hbuf[hbuflen] = v>>8;
	if (++hbuflen >= MAXRLEN)
	    flushhex();
	hbuf[hbuflen] = v;
	if (++hbuflen >= MAXRLEN)
	    flushhex();
    }

    flushhex();
    terminate_hex( base+minaddr );
}


static void
usage(void)
{
    (void)fprintf(stderr, "%s: usage: [-vhdcCx] -n name [-t \"type comment\"]"
		  " [-i ifile] [-o ofile] [-b base]",
		  pgmname);
    exit(1);
}


static void
ckread(void* buf, int len)
{
    int res;

    res = read(ifd, buf, len);
    if (len != res) {
	haderr++;
	if (res != 0) {
	    (void)fprintf(stderr,
			  "%s: premature EOF, read %#x instead of %#x\n",
			  pgmname, res, len);
	    if (res > 0)
		return;
	} else {
	    (void)fprintf(stderr, "%s: read: %s\n",
			  pgmname, strerror(errno));
	}
	    exit(1);
    }
}

static void
ckskip(int off)
{
    if (-1 == lseek(ifd, off, L_INCR)) {
	(void)fprintf(stderr, "%s: seek: %s\n", pgmname, strerror(errno));
	exit(1);
    }
}
