/*
 * mkdisc.h
 *
 * header file for CD-R programs.
 */

#include <sys/types.h>

#define CACHE_SIZE	2048		/* Cache size/user data size */
#define G0_BSIZE	  32		/* number of CACHE_SIZE blocks, small */
#define G1_BSIZE	(512*1024) 	/* number of CACHE_SIZE blocks, large */
#define SPINUP		0x01
#define SPINDOWN	0x00
#define SESSION		0x00
#define NEWTRACK	0x00
#define MODE1		0x01
#define MODE2		0x10
#define MIX		0x01
#define NOMIX		0x00
#define TOCCDDA		0x00
#define TOCCDROM	0x01
#define ONPSET		0x01
#define ONPCLEAR	0x00
#define IMMSE	 	0x01
#define IMMCLEAR	0x00
#define NPASET		0x01
#define NPACLEAR	0x00
#define BVY_BLANK	0x01
#define BVY_MEDIUM	0x00

#define REZERO		0x01		/* Philips CDD521/10 commands */
#define READ0		0x08
#define WRITE0		0x0a
#define SEEK0		0x0b
#define INQUIRY		0x12
#define STOPSTART	0x1b
#define MEDIUMLOCK	0x1e
#define READ1		0x28
#define WRITE1		0x2a
#define SEEK1		0x2b
#define VERIFY1		0x2f
#define FLUSHCACHE	0x35
#define FIRSTWRITE	0xe2
#define RESERVETRACK	0xe4
#define READTRACK	0xe5
#define READDISC	0x43
#define WRITETRACK	0xe6
#define MEDIUMLOAD	0xe7
#define FIXATION	0xe9
#define RECOVER		0xec

#define FIXPROTO        (uchar_t *)
#define CDEW1_SENSELEN	  22
#define DTOB(i)		(i/CACHE_SIZE)
#define INTTOTWODIGITBCD(i)      ((((i)/10)<<4)|((i)%10))

#define SENSESTATUS(dsp) { \
    if (STATUS(dsp) != STA_GOOD) { \
      fprintf(stderr, "Error in last command...\n"); \
      if (debug) PRINTSENSE(dsp); \
      return -1; \
    } }

#define PRINTSENSE(dsp) { \
    int i; \
    if (STATUS(dsp) == STA_CHECK) { \
	for (i = 0; i < CDEW1_SENSELEN; i++) \
	    fprintf(stderr, "%02x ", SENSEBUF(dsp)[i]); \
	fprintf(stderr, "\n"); \
    } }


typedef struct trkinfo_s {
    u_char blen, ntrks, saddr[4], trklen[4], trkstmode, dmode, fblks[4], res[3];
} trkinfo_t;

typedef struct discinfo_s {
    u_char toclen[2], firsttrk, lasttrk;
} discinfo_t;

typedef struct firstwrite_s {
    u_char dbl, la0, la1, la2, la3, res;
} firstwrite_t;

typedef struct modesense_s {
    u_char sdl, mtype, hac, bdl;
} modesense_t;

typedef struct inquiry_s {
    u_char pdt, dtq, ver, rdf, al, res[3], vid[8], pid[16], level[4], date[8];
} inquiry_t;

typedef struct inquirylong_s {
    u_char pdt, dtq, ver, rdf, al, dcpv, dt, dtr, hrn, smrn, dsn[3];
    u_char ltv, stv, mnd, los, scid[36], cu, cs[2];
} inquirylong_t;
