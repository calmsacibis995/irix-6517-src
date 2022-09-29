#include <sys/param.h>
#include <sys/dkio.h>
#include <sys/dvh.h>
#include <ctype.h>
#include <libsc.h>
#include <libsk.h>
#include <uif.h>
#include "scsi.h"

static int scanpass(void);
static int rwcheck(daddr_t bn, int sectors);
static void exrun(void);
static void TestPatFill(char *, int);
static int SetTestPhase(int x);
static void SetTestPat(unsigned char *, int);
static void errlog_summary(void);

# define MAX_SECTRACK	150	/* This had to increase for IPI! */
# define MAX_TRACKBYTES	(MAX_SECTRACK*BBSIZE)
#define MAXSECTORTRIES 2

extern struct volume_header vh;
char extrack[MAX_TRACKBYTES];


struct err
{
	int hits[4];
	int drive;
	daddr_t bn;
};

# define MAX_ERRBLOCKS 100

static struct exercise
{
	char modifier;
	char aflag;
	char iflag, lflag;
	daddr_t firstbn, lastbn;
	int (*stepper)(void);
	int passes;

	int rsize;
	char *buf;
	int bufsize;
	daddr_t curbn;
	int phase;

	struct err eb[MAX_ERRBLOCKS];
	int neb;
} E;

static unsigned char defpat[3] = { 
	0xDB, 0x6D, 0xB6 };
# define MXR	001		/* read */
# define MXW	002		/* write */
# define MXS	004		/* seek */
# define MXC	010		/* compare */
# define MAX_TESTPAT	BBSIZE

static int TestPhase = 0, TestCycle = 3;
static unsigned char DefTestPat[MAX_TESTPAT] = { 
	0xDB, 0x6D, 0xB6 },
	*TestPat = DefTestPat;

/*
 * compute next block number for sequential exercise
 */
int
seqstepper(void)
{
	E.curbn += E.rsize;

	return 0;
}

/*
 * sequential exercise
 */
int
sequential_func(void)
{
	E.aflag = 1;
	E.modifier = MXR|MXC;
	E.firstbn = (daddr_t)0;
	E.lastbn = E.firstbn + exarea (E.firstbn);
	E.passes = 1;
	E.rsize = 128;	/* hardcode, similar to fx, but a bit smaller */
	E.stepper = seqstepper;
	exrun();
	return (E.neb);
}


/*
 * initialize exercise globals
 */
void
init_ex(void)
{
	bzero((char *)&E, sizeof E);
	SetTestPat(defpat, sizeof defpat);
}



/*
 * run an exercise, called from main exercise routines.
 */
static void
exrun(void)
{
	int passno;
	char s[64];
	char s2[64];

	E.buf = extrack;
	E.bufsize = sizeof extrack;

	if( E.rsize > MAX_SECTRACK )
		E.rsize = MAX_SECTRACK;

	for( passno = 0; passno++ < E.passes; )
	{
		msg_printf(VRB, "sequential pass %d: scanning [%d, %d)\n",
		    passno, E.firstbn, E.lastbn);
		SetTestPhase(passno);
		TestPatFill(E.buf, E.bufsize);
		E.phase = passno;
		E.curbn = E.firstbn;
		if (scanpass()) break;   /* user abort, or error  */
		if( E.neb > 0 )
			errlog_summary();
	}
}

/*
 * do one pass of an exercise
 * Keep track of consecutive errors; if we see a cyl worth it's likely that
 * there's a drive hardware problem. Warn the user & give a chance to abort.
 * To that end, return 0 normally, 1 if aborting.
 */

int err_last_pass = 0;
int consec_errs = 0;

/*
   This is used instead of the dot printing code in fx.  The idea is to
   flash the led periodically so the user knows we are still alive.
*/
static void
showbusy(void)
{
	static unsigned flash, lastflash;

	/* alternate between off and on; dependent on tracks/cyl, but
	 * approximately once/second for most drives */
	if(lastflash != (++flash % 20)/10) {
		lastflash = (lastflash+1)&1;
		busy(lastflash);
	}
}

static
int
scanpass(void)
{
	int steps, sectors, err;
	for( steps = (E.lastbn-E.firstbn+E.rsize-1)/E.rsize; --steps >= 0; )
	{
		if( E.curbn < E.firstbn )
			E.curbn = E.firstbn;
		if( E.curbn >= E.lastbn )
			E.curbn = E.lastbn-1;
		sectors = E.rsize;
		if( sectors > E.lastbn - E.curbn )
			sectors = E.lastbn - E.curbn;
		showbusy();
		if (err=rwcheck(E.curbn, sectors))
		{
			msg_printf(ERR, "%s at block %d (%d sectors)\n",
				err== -1 ? "read error" : "Data miscompare",
				E.curbn, sectors);
			busy(0);
			return 1;
		}
		(*E.stepper)();
	}
	busy(0);
	return 0;
}


/*
 * one chunk of an exercise, unadorned
 */
char chktrack[MAX_TRACKBYTES];
int
rwcheck(daddr_t bn, int sectors)
{
	if( E.modifier & MXR )
	{
		if( gread(bn, E.buf, sectors) < 0 )
			return -1;
	}

	if( E.modifier & MXC )
	{
		if( gread(bn, chktrack, sectors) < 0 )
			return -1;

		if( bcmp(chktrack, E.buf, stob(sectors)) != 0 )
			return -2;
	}
	return 0;
}

/* ----- error logging routines ----- */
/* error flags */
# define EH	01		/* hard error (as opp to recovered error) */
# define EW	02		/* write error (as opp to read error) */
/*
 * add to the error log
 */
void
adderr(int drive, daddr_t bn, char *s)
{
	register int i;
	register struct err *ep;
	for( ep = E.eb , i = E.neb; --i >= 0; ep++ )
		if( ep->bn == bn && ep->drive == drive )
			break;
	if( i < 0 )
	{
		if( E.neb >= MAX_ERRBLOCKS )
			return;
		ep = E.eb+E.neb++;
		ep->bn = bn;
		ep->drive = drive;
		ep->hits[0] = ep->hits[1]
		    = ep->hits[2] = ep->hits[3] = 0;
	}
	i = 0;
	if( s[0] == 'h' )
		i |= EH;
	if( s[1] == 'w' )
		i |= EW;
	ep->hits[i]++;
}

static
void
errlog_summary(void)
{
	register int i;
	register struct err *ep;
	struct err e;
	bzero((char *)&e, sizeof e);
	ep = E.eb;
	for( i = E.neb; --i >= 0; )
	{
		if( ep->hits[0] ) e.hits[0]++;
		if( ep->hits[1] ) e.hits[1]++;
		if( ep->hits[2] ) e.hits[2]++;
		if( ep->hits[3] ) e.hits[3]++;
		ep++;
	}
	msg_printf(VRB, "TOTAL ERR BLOCKS r%d(%d) w%d(%d)\n",
	    e.hits[0], e.hits[1],
	    e.hits[2], e.hits[3]);
}

static void
SetTestPat(unsigned char *s, int n)
{
	TestPat = s;
	TestCycle = n;
}

static
int
SetTestPhase(register int x)
{
	return TestPhase = x;
}

#if 0
/*ARGSUSED*/
int
BumpTestPat(int x)
{
	return ((TestPhase>=TestCycle?(TestPhase=0):0)
	    , TestPat[TestPhase++]);
}
#endif

# define BumpTestPat(x)	((TestPhase>=TestCycle?(TestPhase=0):0) \
	, TestPat[TestPhase++])
void
TestPatFill(register char *buf, register int len)
{
	while( --len >= 0 )
		*buf++ = BumpTestPat(0);
}

int
TestPatVerify(register unsigned char *buf, register int len)
{
	register int botch;
	botch = 0;
	while( --len >= 0 )
		if( *buf++ != BumpTestPat(0) )
			botch--;
	return botch;
}
