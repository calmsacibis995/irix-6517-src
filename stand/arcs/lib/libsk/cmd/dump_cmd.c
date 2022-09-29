/*
 * dump command
 */

#ident "$Revision: 1.9 $"

#include <parser.h>
#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <ctype.h>
#include <libsc.h>
#include <libsk.h>

extern void	showchar(int);
static int sign_extend(int, int);
void mem_error(char *);
static int bc(int, int);
static void dprint(unsigned long long,int, int, int);
static int fms(int);

/*
 * get_memory -- read memory location as indicated by flags
 * ??? SHOULD THIS FLUSH CACHE BEFORE DOING READ ???
 */
unsigned long long
get_memory(__psunsigned_t caddr, int width)
{
	switch (width) {

	case SW_BYTE:
		return (*(unsigned char *)caddr);

	case SW_HALFWORD:
		return (*(unsigned short *)caddr);

	case SW_WORD:
		return(*(unsigned *)caddr);

	case SW_DOUBLEWORD:
		return(load_double((long long *)caddr));
	
	default:
		mem_error("get_memory: Illegal width");
	}

	return 0;
}

#define	HEX	16
#define	DEC	-10		/* signed decimal */
#define	UNS	10		/* unsigned decimal */
#define	OCT	8
#define	BIN	2
#define	CHR	1

#define	NO_FILL	0
#define	FILL	1

static char *pad = "   ";	/* can't use pad[] due to xstr */


/*
 * dump -- dump contents of block of memory
 */
int
dump(int argc, char **argv)
{
	register int i, same_printed, different;
	register unsigned linelen, fwidth, fields;
	long long values[16], val;
	static int base;
	static int width;
	static __psunsigned_t addr, baddr;
	static long savecnt;
	long cnt;

	if (!base) {		/* set defaults on first entry */
	    base = HEX;
#if _MIPS_SIM == _ABI64
	    width = SW_DOUBLEWORD;
#else
	    width = SW_WORD;
#endif
	    addr = K0_RAMBASE;
	    savecnt = 20;
	}
	cnt = savecnt;

	while (--argc > 0) {
		if (**++argv == '-') {
			switch ((*argv)[1]) {
			
			case 'b':
				width = SW_BYTE;
				break;

			case 'h':
				width = SW_HALFWORD;
				break;

			case 'w':
				width = SW_WORD;
				break;

			case 'd':
				width = SW_DOUBLEWORD;
				break;

			case 't': /* base ten */
				base = DEC;
				break;

			case 'u':
				base = UNS;
				break;

			case 'x':
				base = HEX;
				break;

			case 'o':
				base = OCT;
				break;

			case 'B':
				base = BIN;
				break;

			case 'c':
				base = CHR;
				break;

			default:
				return(1);
			}
		} else {
			struct range range;

			if (! parse_range(*argv, width, &range))
				return(1);
			addr = range.ra_base;
			cnt = range.ra_count;
			if (cnt == 1 && !index(*argv,'#'))
				cnt = savecnt;
		}
	}

	savecnt = cnt;

	linelen = 68;
	fwidth = bc(base, width);
	fields = (int)(linelen / (fwidth + sizeof(pad)));
	fields = 1 << fms(fields);	/* force to a power of two */

	same_printed = 0;
	for (different = 1; cnt > 0; different = 0, cnt -= fields) {
		baddr = addr;
		for (i = 0; i < fields && i < cnt; i++) {
			val = get_memory(addr, width);
			if (base < 0)
				val = sign_extend(val, width);
			if (val != values[i]) {
				different = 1;
				values[i] = val;
			}
			addr += width;
		}
		if (!different && cnt > fields) {
			if (!same_printed) {
				printf("SAME\n");
				same_printed = 1;
			}
			continue;
		}

		same_printed = 0;
		puts("0x");
		dprint(baddr, HEX, 8, FILL);
		putchar(':');
		for (i = 0; i < fields && i < cnt; i++) {
			puts(pad);
			dprint(values[i], base, fwidth, NO_FILL);
		}
		putchar('\n');
	}
	return(0);
}

/*
 * sign_extend -- sign extend to 32 bits
 */
static int
sign_extend(int val, int width)
{
	switch (width) {
	case sizeof(char):
		val = (char)val;
		break;
	case sizeof(short):
		val = (short)val;
		break;
	}
	return(val);
}

/*
 * bc -- return byte count for formatted print widths
 */
static int
bc(int base, int width)
{
	switch (base) {

	case HEX:
		return((width*8+3)/4);

	case DEC:
		switch (width) {
		case SW_BYTE:
			return(4);

		case SW_HALFWORD:
			return(6);

		case SW_WORD:
			return(11);

		case SW_DOUBLEWORD:
			return(21);

		default:
			mem_error("bc");
		}

	case UNS:
		switch (width) {
		case SW_BYTE:
			return(3);

		case SW_HALFWORD:
			return(5);

		case SW_WORD:
			return(10);

		case SW_DOUBLEWORD:
			return(20);

		default:
			mem_error("bc");
		}

	case OCT:
		return((width*8+2)/3);

	case CHR:
		return(width * 4);

	case BIN:
		return(width*8);

	default:
		mem_error("bc");
	}

	return 0;
}

/*
 * dprint -- formatted output
 */
static void
dprint(register unsigned long long n, register int b, register int fwidth, int fill_wanted)
{
	register char *cp;
	register char c;
	register int i;
	char prbuf[64];
	int minus = 0;

	if (b < 0) {
		b = -b;
		if ((int)n < 0) {
			minus = 1;
			n = (unsigned long long )(-(long long)n);
		}
	}

	cp = prbuf;
	if (b == CHR) {
		/* this is pretty much a kludge, but it gets the job done */
		for (i = 0; i < (fwidth/4); i++) {
			c = n >> (i * 8);
			c &= 0xff;
			if (isprint(c))
				*cp++ = c;
			else switch (c) {
			case '\b':
				*cp++ = 'b';
				*cp++ = '\\';
				break;
			case '\f':
				*cp++ = 'f';
				*cp++ = '\\';
				break;
			case '\n':
				*cp++ = 'n';
				*cp++ = '\\';
				break;
			case '\r':
				*cp++ = 'r';
				*cp++ = '\\';
				break;
			case '\t':
				*cp++ = 't';
				*cp++ = '\\';
				break;
			case ' ':
				*cp++ = ' ';
				*cp++ = '\\';
				break;
			case '\\':
				*cp++ = '\\';
				*cp++ = '\\';
				break;
			default:
				*cp++ = (c&07) + '0';
				*cp++ = ((c>>3) & 07) + '0';
				*cp++ = ((c>>6) & 03) + '0';
				*cp++ = '\\';
				break;
			}
		}
	} else {
		do {
			*cp++ = "0123456789abcdef"[n%b];
			n /= b;
		} while (n);
	}

	fwidth -= cp - prbuf;
	if (minus)
		fwidth--;

	while (--fwidth >= 0)
		putchar(fill_wanted ? '0' : ' ');

	if (minus)
		putchar('-');

	do
		putchar(*--cp);
	while (cp > prbuf);
}

/*
 * fms -- find most significant bit
 * bits numbered from 31 (msb) to 0 (lsb)
 * returns 0 if bits == 0
 */
static int
fms(int bits)
{
	register int i;

	if (bits == 0)
		return (0);

	for (i = 31; bits > 0; i--)
		bits <<= 1;

	return (i);
}

void
mem_error(char *string)
{
    printf ("Error in memory command: %s\n", string);
}
