#ident "scsicontrol.c: $Revision $"

/* Compile as:
	cc -O scsicontrol.c -lds -o scsicontrol
   This program should compile and work on any IRIX 4.0.1 or later
   release, although there might be some defines missing from releases
   earlier than IRIX 5.2.
*/

#include <sys/types.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <dslib.h>


typedef	struct
{
	unchar	pqt:3;	/* peripheral qual type */
	unchar	pdt:5;	/* peripheral device type */
	unchar	rmb:1,	/* removable media bit */
		dtq:7;	/* device type qualifier */
	unchar	iso:2,	/* ISO version */
		ecma:3,	/* ECMA version */
		ansi:3;	/* ANSI version */
	unchar	aenc:1,	/* async event notification supported */
		trmiop:1,	/* device supports 'terminate io process msg */
		res0:2,	/* reserved */
		respfmt:4;	/* SCSI 1, CCS, SCSI 2 inq data format */
	unchar	ailen;	/* additional inquiry length */	
	unchar	res1;	/* reserved */
	unchar	res2;	/* reserved */
	unchar	reladr:1,	/* supports relative addressing (linked cmds) */
		wide32:1,	/* supports 32 bit wide SCSI bus */
		wide16:1,	/* supports 16 bit wide SCSI bus */
		synch:1,	/* supports synch mode */
		link:1,	/* supports linked commands */
		res3:1,	/* reserved */
		cmdq:1,	/* supports cmd queuing */
		softre:1;	/* supports soft reset */
	unchar	vid[8];	/* vendor ID */
	unchar	pid[16];	/* product ID */
	unchar	prl[4];	/* product revision level*/
	unchar	vendsp[20];	/* vendor specific; typically firmware info */
	unchar	res4[40];	/* reserved for scsi 3, etc. */
	unchar	vendsp2[159];	/* more vendor specific (fill to 255 bytes) */
	/* more vendor specific information may follow */
} inqdata;


struct msel {
	unsigned char rsv, mtype, vendspec, blkdesclen;	/* header */
	unsigned char dens, nblks[3], rsv1, bsize[3];	/* block desc */
	unsigned char pgnum, pglen; /* modesel page num and length */
	unsigned char data[240]; /* some drives get upset if no data requested
		on sense*/
};

#define hex(x) "0123456789ABCDEF" [ (x) & 0xF ]

/* only looks OK if nperline a multiple of 4, but that's OK.
 * value of space must be 0 <= space <= 3;
 */
void
hprint(unsigned char *s, int n, int nperline, int space, int ascii)
{
	int   i, x, startl;

	for(startl=i=0;i<n;i++)  {
		x = s[i];
		printf("%c%c", hex(x>>4), hex(x));
		if(space)
			printf("%.*s", ((i%4)==3)+space, "    ");
		if ( i%nperline == (nperline - 1) ) {
			if(ascii == 1) {
				putchar('\t');
				while(startl < i) {
					if(isprint(s[startl]))
						putchar(s[startl]);
					else
						putchar('.');
					startl++;
				}
			}
			putchar('\n');
			if(ascii>1 && i<(n-1))	/* hack hack */
				printf("%.*s", ascii-1, "        ");
		}
	}
	if(space && (i%nperline))
		putchar('\n');
}

/* aenc, trmiop, reladr, wbus*, synch, linkq, softre are only valid if
 * if respfmt has the value 2 (or possibly larger values for future
 * versions of the SCSI standard). */

static char pdt_types[][16] = {
	"Disk", "Tape", "Printer", "Processor", "WORM", "CD-ROM",
	"Scanner", "Optical", "Jukebox", "Comm", "Unknown"
};
#define NPDT (sizeof pdt_types / sizeof pdt_types[0])

void
printinq(struct dsreq *dsp, inqdata *inq, int allinq)
{
	int neednl = 1;

	if(DATASENT(dsp) < 1) {
		printf("No inquiry data returned\n");
		return;
	}
	printf("%-10s", pdt_types[(inq->pdt<NPDT) ? inq->pdt : NPDT-1]);
	if (DATASENT(dsp) > 8)
		printf("%12.8s", inq->vid);
	if (DATASENT(dsp) > 16)
		printf("%.16s", inq->pid);
	if (DATASENT(dsp) > 32)
		printf("%.4s", inq->prl);
	printf("\n");
	if(DATASENT(dsp) > 1)
		printf("ANSI vers %d, ISO ver: %d, ECMA ver: %d; ",
			inq->ansi, inq->iso, inq->ecma);
	if(DATASENT(dsp) > 2) {
		unchar special = *(inq->vid-1);
		if(inq->respfmt >= 2 || special) {
			if(inq->respfmt < 2)
				printf("\nResponse format type %d, but has "
				  "SCSI-2 capability bits set\n", inq->respfmt);
			
			printf("supports: ");
			if(inq->aenc)
				printf(" AENC");
			if(inq->trmiop)
				printf(" termiop");
			if(inq->reladr)
				printf(" reladdr");
			if(inq->wide32)
				printf(" 32bit");
			if(inq->wide16)
				printf(" 16bit");
			if(inq->synch)
				printf(" synch");
			if(inq->link)
				printf(" linkedcmds");
			if(inq->cmdq)
				printf(" cmdqueing");
			if(inq->softre)
				printf(" softreset");
		}
		if(inq->respfmt < 2) {
			if(special)
				printf(".  ");
			printf("inquiry format is %s",
				inq->respfmt ? "SCSI 1" : "CCS");
		}
		if(allinq) {
			if (DATASENT(dsp) > offsetof(inqdata, vendsp)) {
				printf("\nvendor specific data:\n");
				hprint(inq->vendsp, DATASENT(dsp)
					- (int)offsetof(inqdata, vendsp), 16, 1, 1);
				neednl = 0;
			}
			if (DATASENT(dsp) > offsetof(inqdata, res4)) {
				printf("reserved (for SCSI 3) data:\n");
				hprint(inq->res4, DATASENT(dsp)
					- (int)offsetof(inqdata, res4), 16, 1, 1);
			}
			if (DATASENT(dsp) > offsetof(inqdata, vendsp2)) {
				printf("more vendor data\n");
				hprint(inq->vendsp2, DATASENT(dsp)
					- (int)offsetof(inqdata, vendsp2), 16, 1, 1);
			}
		}
	}
	if(neednl)
		putchar('\n');
	printf("Device is  ");
	/*	do test unit ready only if inquiry successful, since many
		devices, such as tapes, return inquiry info, even if
		not ready (i.e., no tape in a tape drive). */
	if(testunitready00(dsp) != 0)
		printf("%s\n",
			(RET(dsp)==DSRT_NOSEL) ? "not responding" : "not ready");
	else
		printf("ready");
	printf("\n");
}

/* inquiry cmd that does vital product data as spec'ed in SCSI2 */
int
vpinquiry12( struct dsreq *dsp, caddr_t data, long datalen, char vu,
  int page)
{
  fillg0cmd(dsp, (uchar_t *)CMDBUF(dsp), G0_INQU, 1, page, 0, B1(datalen),
	B1(vu<<6));
  filldsreq(dsp, (uchar_t *)data, datalen, DSRQ_READ|DSRQ_SENSE);
  return(doscsireq(getfd(dsp), dsp));
}

int
startunit1b(struct dsreq *dsp, int startstop, int vu)
{
  fillg0cmd(dsp, (uchar_t *)CMDBUF(dsp), 0x1b, 0, 0, 0, (uchar_t)startstop, B1(vu<<6));
  filldsreq(dsp, NULL, 0, DSRQ_READ|DSRQ_SENSE);
  dsp->ds_time = 1000 * 90; /* 90 seconds */
  return(doscsireq(getfd(dsp), dsp));
}


int
myinquiry12(struct dsreq *dsp, uchar_t *data, long datalen, int vu, int neg)
{
  fillg0cmd(dsp, (uchar_t *)CMDBUF(dsp), G0_INQU, 0, 0, 0, B1(datalen), B1(vu<<6));
  filldsreq(dsp, data, datalen, DSRQ_READ|DSRQ_SENSE|neg);
  dsp->ds_time = 1000 * 30; /* 90 seconds */
  return(doscsireq(getfd(dsp), dsp));
}

/* for SGI specific CD-ROM drives, stop returning inquiry info as
 * though they were a hard disk, and return info for a CD-ROM again.
 * Needed at power on and after bus resets.
*/
int
docmdc9(struct dsreq *dsp, int do80)
{
  fillg1cmd(dsp, (uchar_t *)CMDBUF(dsp), 0xc9, 0, 0, 0, 0, 0, 0, 0, 0,
	do80==1?0:0x80);
  filldsreq(dsp, 0, 0, DSRQ_READ|DSRQ_SENSE);
  return(doscsireq(getfd(dsp), dsp));
}


int
logsense(struct dsreq *dsp, uchar_t *data, uint datalen,
	    int pagectrl, int pagecode, uint lun)
{
  fillg1cmd(dsp, (uchar_t*)CMDBUF(dsp), 0x4D, (lun<<5)|(0<<1)|0,
	((pagectrl&3)<<6)|(pagecode&0x3F), 0, 0, 0, 0, B2(datalen), 0);
  filldsreq(dsp, (uchar_t *)data, datalen, DSRQ_READ|DSRQ_SENSE);
  dsp->ds_time = 30*1000;
  return(doscsireq(getfd(dsp), dsp));
}

unsigned
getcap(struct dsreq *dsp)
{
	unsigned char cdata[8];
	uchar_t msbuf[256];
	uint dlen;

	memset(cdata, sizeof(cdata), 0);
	if(readcapacity25(dsp, (caddr_t)cdata, sizeof(cdata), 0, 0, 0))
		return 1;

	if(modesense1a(dsp, (caddr_t)msbuf, 255, 0, 0x3f, 0))
		dlen = 0;
	else
		dlen = DATASENT(dsp);
	printf("capacity=%u", V4(cdata)+1);
	if(dlen >= 12)
		printf(" %u byte", V2(&msbuf[10]));
	printf(" blocks\n");
	return 0;
}

int
setbsize(struct dsreq *dsp, unsigned bsz)
{
	struct msel msel;
	unsigned obsz;

	memset(&msel, 0, sizeof(msel));
	msel.pgnum = 0x3f;	/* all pages */
	msel.pglen = sizeof(msel.data)+2;
	modesense1a(dsp, (caddr_t)&msel, sizeof(msel), 0, msel.pgnum, 0);
	if(STATUS(dsp) != STA_GOOD)
		return 1;
	msel.blkdesclen = 8;
	obsz = msel.bsize[2];
	obsz += ((unsigned)msel.bsize[1]) << 8;
	obsz += ((unsigned)msel.bsize[0]) << 16;

	msel.rsv = 0;
	msel.bsize[0] = (unsigned char)(bsz>>16);
	msel.bsize[1] = (unsigned char)(bsz>>8);
	msel.bsize[2] = (unsigned char)bsz;

	/* select back with just the header and block descriptor */
	modeselect15(dsp, (caddr_t)&msel,
		offsetof(struct msel, pgnum), 0, 0);

	if(STATUS(dsp) != STA_GOOD) {
		printf("prev blksize = %u\n", obsz);
		return 1;
	}

	printf("prev blksize = %u, new blksize = %u", obsz, bsz);

	memset(&msel, 0, sizeof(msel));
	msel.pgnum = 0x3f;	/* all pages */
	msel.pglen = sizeof(msel.data)+2;
	modesense1a(dsp, (caddr_t)&msel, sizeof(msel), 0, msel.pgnum, 0);
	if(STATUS(dsp) != STA_GOOD) {
		printf(", but not able to confirm\n");
		return 1;
	}
	obsz = msel.bsize[2];
	obsz += ((unsigned)msel.bsize[1]) << 8;
	obsz += ((unsigned)msel.bsize[0]) << 16;
	if(obsz != bsz)
		printf("; oops, new blksiz accepted, but %d\n", obsz);
	printf("\n");

	return 0;
}

int
gethaflags(struct dsreq *dsp)
{
	int flags;
	if(ioctl(getfd(dsp), DS_GET, &flags)) {
		perror("unable to get hostadapter status flags");
		return 1;
	}
	if(!flags)
		printf("no status bits set; adapter may not support reporting\n");
	else {
		printf("host adapter status: ");
		if(flags & DSG_CANTSYNC)
			printf(" cantsync");
		if(flags & DSG_SYNCXFR)
			printf(" sync");
		if(flags & DSG_TRIEDSYNC)
			printf(" sync negotiated");
		if(flags & DSG_BADSYNC)
			printf(" badsync");
		if(flags & DSG_NOADAPSYNC)
			printf(" sync disabled");
		if(flags & DSG_WIDE)
			printf(" wide scsi");
		if(flags & DSG_DISC)
			printf(" disconnect enabled");
		if(flags & DSG_TAGQ)
			printf(" tagged queueing");
		printf("\n");
	}
	return 0;
}


modesenseall(struct dsreq *dsp)
{
	uchar_t msbuf[256], *mptr;
	uint dlen;

	/* all modesense pages, current values */
	if(modesense1a(dsp, (caddr_t)msbuf, 255, 0, 0x3f, 0))
		return 1;
	dlen = DATASENT(dsp);
	if(dlen < 4)
		return 1;
	mptr = msbuf;

	printf("Header:\n length %#x, medium type %#x, device specific %#x, block desc length %#x\n", msbuf[0], msbuf[1], msbuf[2], msbuf[3]);
	mptr += 4;
	dlen -= 4;
	if(msbuf[3])
		printf("Block Descriptors:\n ");
	while(msbuf[3] && dlen >= 8) {
		hprint(mptr, 8, 8, 1, 2);
		msbuf[3] -= 8;
		dlen -= 8;
		mptr += 8;
	}
	while(dlen >= 3) {
		uint len;
		printf("Page %#x, length %#x:\n ", 0x3f & *mptr, mptr[1]);
		len = dlen > (mptr[1]+2) ? mptr[1] : dlen - 2;
		hprint(&mptr[2], mptr[1], 20, 1, 2);
		mptr += len + 2;
		dlen -= len + 2;
	}

	return 0;
}

/* print logsense flags in ascii form; print in left to right order,
 * (high to low) as that's the way most manuals show them.
*/
void
lsflags(uchar_t f)
{
	static char *ls_str[] = {
		"DU",
		"DS",
		"TSD",
		"ETC",
		"",
		"",
		"RSVD",
		"LS"
	};
	static char *ls_tmc[] = {
		"all",
		"equal",
		"not_equal",
		"greater",
	};
	int shft;
	uchar_t lastf = 0;

	for(shft=0; shft<8; shft++) {
		if(shft == 4) {	/* 2 bits... */
			f<<=1;
			shft++;
			if(lastf&0x80)	/* TMC valid only if ETC valid */
				printf("TMC-%s ", ls_tmc[(f>>6)&3]);
		}
		else if(f & 0x80)
			printf("%s ", ls_str[shft]);
		lastf = f;
		f<<=1;
	}
}
	

/* note: currently set to only return current cumulative values, returns all
 * values, requests no saving, and limits to 8KB per page
 * also currently only implements lun 0, regardless of what lun device
 * we opened...  (not great, but simple...)
*/
logsenseall(struct dsreq *dsp)
{
	uchar_t msbuf[8192], *mptr, pglist[0x144], *pg;
	uint dlen, npgs;

	/* first get the list of supported pages (up to 64, plus header) */
	if(logsense(dsp, pglist, sizeof(pglist), 1, 0, 0) || DATASENT(dsp)<4) {
		printf("logsense cmd to get list of logsense pages fails\n");
		return 1;
	}
	npgs = V2(&pglist[2]);
	if(DATASENT(dsp) < (npgs+4))
		npgs = DATASENT(dsp)-4;
	
	for(pg=&pglist[4]; npgs-- > 0; pg++) {
		static char *lspage[] = {
			"",
			"buffer over/underrun",
			"write err count",
			"read err count",
			"read reverse err count",
			"verify err count",
			"non-media err count",
			"recent error events"
		};

		if(*pg == 0)
			continue;
		memset(msbuf, 0, 8);	/* make sure stuff we print before checking
			data returned not garbage */
		if(logsense(dsp, msbuf, sizeof(msbuf), 1, *pg, 0) || DATASENT(dsp)<4)
			continue;

		dlen = V2(&msbuf[2]);
		printf("Log page %#x", msbuf[0]);
		if(msbuf[0] < 8)	/* reset are reserved/vendor specific */
			printf(" (%s)", lspage[msbuf[0]]);
		printf(", len=%#x", dlen);
		if(dlen <= 4)	/* yes, I've seen this; just overal flags, etc. */
			printf(" (no parameter data)");
		printf("\n");
		if(dlen > (DATASENT(dsp)-4))
			dlen = DATASENT(dsp)-4;
		mptr = msbuf+4;
		while(dlen >= 4) {
			uint plen;
			plen = mptr[3];
			printf(" Parameter %#x, len %#x, flags %x",
				V2(mptr), plen, mptr[2]);
			if(mptr[2]) {
				printf(" ( ");
				lsflags(mptr[2]);
				printf(")\n");
			}
			else
				printf("\n");
			if((plen+4)>dlen)	/* paranoia */
				plen = dlen - 4;
			if(plen) printf("  ");
			hprint(&mptr[4], plen, 20, 1, 3);
			mptr += mptr[3] + 4;
			dlen -= plen + 4;
		}
	}

	return 0;
}
	


void
usage(char *prog)
{
    fprintf(stderr,
	"Usage: %s [-i (inquiry)] [-e (exclusive)] [-s (sync) | -a (async)]\n"
	"\t[-I (all inq data)] [-v (vital proddata)] [-r (reset)] [-D (diagselftest)]\n"
	"\t[-S (spinup/start/load)] [-H (halt/stop)] [-C (cdrom mode)] [-b blksize]\n"
	"\t[-g (get host flags)] [-d (debug)] [-q (quiet)] [ -m (modesense all)\n"
	"\t[-l (logsense)] scsidevice [...]\n",
	    prog);
    exit(1);
}

void
byteprint(caddr_t sarg, int n, int nperline, int space)
{
	int   i, x;
	char  *sp = (space) ? " ": "";
	unsigned char *s = (unsigned char *)sarg;

	for(i=0;i<n;i++)  {
		x = s[i];
		fprintf(stderr,((i%4==3)?"%c%c%s%s":"%c%c%s"),
			hex(x>>4), hex(x), sp, sp);
		if ( i%nperline == (nperline - 1) )
			fprintf(stderr,"\n");
	}
	if ( space )
		fprintf(stderr,"\n");
}

void displayerr (dsreq_t *dsp)
{
    caddr_t sp = SENSEBUF(dsp);
       
    if (SENSESENT(dsp)) {	/* decode sense data returned */
	fprintf(stderr, "sense key %x - %s\n", SENSEKEY(sp),
	ds_ctostr( (unsigned long)SENSEKEY(sp), sensekeytab));
	byteprint(SENSEBUF(dsp), SENSESENT(dsp), 16, 1);
    }
    fprintf(stderr, "sbyte %x\n", STATUS(dsp));
	fprintf(stderr,"Failing command was: %s: ",
		ds_vtostr((uint)(CMDBUF(dsp))[0], cmdnametab));
	byteprint(CMDBUF(dsp),CMDLEN(dsp),16,1);
}

main(int argc, char **argv)
{
	struct dsreq *dsp;
	char *fn;
	/* int because they must be word aligned. */
	int errs = 0, c, printname = 1;
	int vital=0, doreset=0, exclusive=0, dosync=0, allinq=0, getflags=0;
	int docdrom = 0, dostart = 0, dostop = 0, dosenddiag = 0;
	int doinq = 0, dologsense = 0, domsense = 0, docap = 0;
	unsigned bsize = 0;
	extern char *optarg;
	extern int optind, opterr;

	setlinebuf(stdout);	/* because -d output goes to stderr, and
		* we want things reasonably interleaved */

	opterr = 0;	/* handle errors ourselves. */
	while ((c = getopt(argc, argv, "cb:HIDSaserdvlgCiqm")) != -1)
	switch(c) {
	case 'c':
		docap = 1;	/* do readcapacity */
		break;
	case 'i':
		doinq = 1;	/* do inquiry */
		break;
	case 'g':
		getflags = 1;	/* get host adapter driver status bits */
		break;
	case 'D':
		dosenddiag = 1;
		break;
	case 'C':
		docdrom++;
		break;
	case 'r':
		doreset = 1;	/* do a scsi bus reset */
		break;
	case 'e':
		exclusive = O_EXCL;
		break;
	case 'd':
		dsdebug++;	/* enable debug info */
		break;
	case 'I':
		allinq = 1;	/* show the vendor specific and scsi3 data also */
		break;
	case 'l':
		dologsense = 1;	/* show the logsense data (just cumulative for now) */
		break;
	case 'q':
		printname = 0;	/* print devicename only if error */
		break;
	case 'v':
		vital = 1;	/* set evpd bit for scsi 2 vital product data */
		break;
	case 'H':
		dostop = 1;	/* send a stop (Halt) command */
		break;
	case 'S':
		dostart = 1;	/* send a startunit/spinup command */
		break;
	case 's':
		dosync = DSRQ_SYNXFR;	/* attempt to negotiate sync scsi */
		break;
	case 'a':
		dosync = DSRQ_ASYNXFR;	/* attempt to negotiate async scsi */
		break;
	case 'b':
		bsize = strtoul((const char *)optarg, NULL, 0);
		if(bsize == 0 || bsize >= (16*1024*1024)) {
			fprintf(stderr, "Bogus blocksize %s (%u)\n", optarg, bsize);
			bsize = 0;
		}
		break;
	case 'm':	/* dump all the (current, for now) modesense info */
		domsense = 1;
		break;
	default:
		usage(argv[0]);
	}

	if(optind >= argc || optind == 1)	/* need at 1 arg and one option */
		usage(argv[0]);
	if(allinq && !(doinq || vital))
		doinq = 1;

	while (optind < argc) {
		fn = argv[optind++];
		if(printname) printf("%s:  ", fn);
		if((dsp = dsopen(fn, O_RDONLY|exclusive)) == NULL && *fn != '/') {
			/* if open fails, try pre-pending /dev/scsi */
			char buf[256];
			strcpy(buf, "/dev/scsi/");
			if((strlen(buf) + strlen(fn)) < sizeof(buf)) {
				strcat(buf, fn);
				dsp = dsopen(buf, O_RDONLY|exclusive);
			}
		}
		if(!dsp) {
			/* yes, the original name, not the possibly modified one */
			if(!printname) printf("%s:  ", fn);
			fflush(stdout);
			perror("cannot open");
			errs++;
			continue;
		}

		/* try to order for reasonableness; reset first in case
		 * hung, then inquiry, etc. */

		if(doreset) {
			printf("Bus reset function no longer supported in scsicontrol"
			       "\n\tUse 'scsiha' command to reset a SCSI bus\n");
			errs++;
		}

		if(doinq) {
			int inqbuf[sizeof(inqdata)/sizeof(int)];
			if(myinquiry12(dsp, (uchar_t *)inqbuf, sizeof inqbuf, 0, dosync)) {
				if(!printname) printf("%s:  ", fn);
				printf("inquiry failure\n");
				errs++;
			}
			else
				printinq(dsp, (inqdata *)inqbuf, allinq);
		}

		if(vital) {
			unsigned char *vpinq;
			int vpinqbuf[sizeof(inqdata)/sizeof(int)];
			int vpinqbuf0[sizeof(inqdata)/sizeof(int)];
			int i, serial = 0, asciidef = 0;
			if(vpinquiry12(dsp, (char *)vpinqbuf0,
				sizeof(vpinqbuf)-1, 0, 0)) {
				if(!printname) printf("%s:  ", fn);
				printf("inquiry (vital data) failure\n");
				errs++;
				continue;
			}
			if(DATASENT(dsp) <4) {
				printf("vital data inquiry OK, but says no"
					"pages supported (page 0)\n");
				continue;
			}
			vpinq = (unsigned char *)vpinqbuf0;
			printf("Supported vital product pages: ");
			for(i = vpinq[3]+3; i>3; i--) {
				if(vpinq[i] == 0x80)
					serial = 1;
				if(vpinq[i] == 0x82)
					asciidef = 1;
				printf("%2x  ", vpinq[i]);
			}
			printf("\n");
			vpinq = (unsigned char *)vpinqbuf;
			if(serial) {
			    if(vpinquiry12(dsp, (char *)vpinqbuf,
				    sizeof(vpinqbuf)-1, 0, 0x80) != 0) {
					if(!printname) printf("%s:  ", fn);
				    printf("inquiry (serial #) failure\n");
				    errs++;
			    }
			    else if(DATASENT(dsp)>3) {
				    printf("Serial #: ");
				    fflush(stdout);
				    /* use write, because there may well be
				     *nulls; don't bother to strip them out */
				    write(1, vpinq+4, vpinq[3]);
				    printf("\n");
			    }
			}
			if(asciidef) {
			    if(vpinquiry12(dsp, (char *)vpinqbuf,
				sizeof(vpinqbuf)-1, 0, 0x82) != 0) {
				if(!printname) printf("%s:  ", fn);
				printf("inquiry (ascii definition) failure\n");
				errs++;
			    }
			    else if(DATASENT(dsp)>3) {
				printf("Ascii definition: ");
				fflush(stdout);
				/* use write, because there may well be
				 *nulls; don't bother to strip them out */
				write(1, vpinq+4, vpinq[3]);
				printf("\n");
			    }
			}
			if(allinq) {
				vpinq = (unsigned char *)vpinqbuf0;
				for(i=3; i<=(vpinq[3]+3); i++) {
					if(vpinquiry12(dsp, (char *)vpinqbuf,
						sizeof(vpinqbuf)-1, 0, vpinq[i]) == 0 &&
						DATASENT(dsp) > 4) {
						printf("Vital product page %#2x:\n", vpinq[i]);
						hprint(4+(uchar_t *)vpinqbuf, DATASENT(dsp)-4,
							16, 1, 1);
					}
				}
			}
		}

		if(docdrom && docmdc9(dsp, docdrom)) {
			if(!printname) printf("%s:  ", fn);
			printf("revert to cdrom fails\n");
			errs++;
		}

		if(getflags && gethaflags(dsp))
			errs++;

		if(dostop && startunit1b(dsp, 0, 0)) {
			if(!printname) printf("%s:  ", fn);
			printf("stopunit fails\n");
			errs++;
		}

		if(dostart && startunit1b(dsp, 1, 0)) {
			if(!printname) printf("%s:  ", fn);
			printf("startunit fails\n");
			errs++;
		}

		if(bsize && setbsize(dsp, bsize)) {
			if(!printname) printf("%s:  ", fn);
			printf("set blocksize to %u fails\n", bsize);
			errs++;
		}

		if(docap && getcap(dsp)) {
			if(!printname) printf("%s:  ", fn);
			printf("read capacity fails\n");
			errs++;
		}

		if(dosenddiag && senddiagnostic1d(dsp, NULL, 0, 1, 0, 0, 0)) {
			if(!printname) printf("%s:  ", fn);
			printf("self test fails\n");
			errs++;
		}

		if(domsense && modesenseall(dsp)) {
			if(!printname) printf("%s:  ", fn);
			printf("modesense all pages current values fails\n");
			errs++;
		}

		if(dologsense && logsenseall(dsp)) {
			if(!printname) printf("%s:  ", fn);
			printf("logsense all pages cumulative values fails\n");
			errs++;
		}

		dsclose(dsp);
	}
	return(errs);
}
