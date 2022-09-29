#ident "$Revision: 1.11 $"

/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 * static char sccsid[] = "@(#)dumptape.c	5.8 (Berkeley) 2/23/87";
 */

#include "dump.h"
#include "dumprmt.h"
#include <sys/mtio.h>

static long asize;		/* number of 0.1" units written on current tape,
	or number of 1K blocks (not necessarily TP_BSIZE) if C arg used */
static char (*tblock)[TP_BSIZE];/* pointer to malloc()ed buffer for tape */
static int writesize;		/* size of malloc()ed buffer for tape */
static long lastspclrec = -1;	/* tape block number of last written header */
static int trecno = 0;		/* next record to write in current block */

/* The old way was bogus if more than 2GB was being dumped though,
 * causing overflow.  So now we make btapea, a long long
 * not used so much that this is much of a perf problem.
*/
static long long btapea = 0;	/* shadows spcl.c_tapea in BB units */
static long long btrecno = 0;	/* BB count into current block */

/*
 * Concurrent dump mods (Caltech) - disk block reading and tape writing
 * are exported to several slave processes.  While one slave writes the
 * tape, the others read disk blocks; they pass control of the tape in
 * a ring via flock().	The parent process traverses the filesystem and
 * sends spclrec()'s and lists of daddr's to the slaves via pipes.
 */
struct req {			/* instruction packets sent to slaves */
	baddr_t dblk;
	int count;
};
static struct req *req;
static int reqsiz;

static int tenths;		/* length of tape used per block written */

static void chkandfixaudio(int);
static void flusht(void);
static void tperror(void);

int
alloctape(void)
{
	int pgoff = getpagesize() - 1;

	writesize = ntrec * TP_BSIZE;
	reqsiz = ntrec * sizeof(struct req);
	/*
	 * CDC 92181's and 92185's make 0.8" gaps in 1600-bpi start/stop mode
	 * (see DEC TU80 User's Guide).  The shorter gaps of 6250-bpi require
	 * repositioning after stopping, i.e, streaming mode, where the gap is
	 * variable, 0.30" to 0.45".  The gap is maximal when the tape stops.
	 * Note that the calculation is somewhat bogus for non qic/9 track tapes,
	 * so if capacity was given on command line, set it to 0.
	 */
	if(!Cflag)
		tenths = writesize/density + (cartridge ? 16 : density == 625 ? 5 : 8);
	else
		tenths = writesize / 0x400; /* actually, kbytes, not tenths */
#if TP_BSIZE % 0x400
	fprintf(stderr, "Internal error, TP_BSIZE must be multiple of 1024\n");
#endif
	/*
	 * Allocate tape buffer contiguous with the array of instruction
	 * packets, so flusht() can write them together with one write().
	 * Align tape buffer on page boundary to speed up tape write().
	 */
	req = (struct req *)malloc(reqsiz + writesize + pgoff);
	if (req == NULL)
		return(0);
	tblock = (char (*)[TP_BSIZE]) (((long)&req[ntrec] + pgoff) &~ pgoff);
	req = (struct req *)tblock - ntrec;
	return(1);
}

void
taprec(char *dp)
{
	*(union u_spcl *)(tblock[trecno]) = *(union u_spcl *)dp;
	lastspclrec = spcl.c_tapea;
	trecno++;
	spcl.c_tapea++;
	if(trecno >= ntrec)
		flusht();
}

/*
 * btrecno and btapea shadow trecno and spcl.c_tapea
 * in BB units.  They are used while a file is being dumped,
 * since the non-contiguous file blocks may be shorter than
 * one tape block.  When the file is dumped (and possibly padded),
 * we restore trecno and spcl.c_tapea.
 */

void
dmpblk_start(void)
{
	btrecno = TPTOBB(trecno);
	btapea = TPTOBB(spcl.c_tapea);
}

void
dmpblk_sync(void)	/* restore trecno and spcl.c_tapea */
{
	assert(btrecno % TPTOBB(1) == 0);
	trecno = BBTOTP(btrecno);
	assert(btapea % TPTOBB(1) == 0);
	spcl.c_tapea = BBTOTP(btapea);
}

/*
 * Dump size contiguous blocks, starting at bn.
 */
void
dmpblk(
	unsigned bn,	/* disk or src address (BB) */
	int size,	/* BB units */
	char *src)	/* data src, or NULL, meaning bread() it */
{
	int avail;
	char *dst;

	assert(size > 0);
	while ((avail = MIN(size, TPTOBB(ntrec)-btrecno)) > 0) {
		dst = (char *)(tblock[BBTOTP(btrecno)]) +
			BBTOB(btrecno)%TP_BSIZE;
		if (src != NULL)
			bcopy(src + BBTOB(bn), dst, BBTOB(avail));
		else
			bread(bn, dst, BBTOB(avail));
		btrecno += avail;
		btapea += avail;
		if (btrecno >= TPTOBB(ntrec)) {
			dmpblk_sync();
			flusht();
			dmpblk_start();
		}
		bn += avail;
		size -= avail;
	}
	assert(size == 0);
}

void
dmpblk_pad(int n)		/* emit n blocks */
{
	static char zero[BBSIZE];

	while (--n >= 0)
		dmpblk(0, 1, zero);
}

/*
 * File has been dumped -- pad if necessary, then restore counters.
 */
void
dmpblk_end(void)
{
	while (btrecno % TPTOBB(1))
		dmpblk_pad(1);
	dmpblk_sync();
}

static int	nogripe = 0;

static void
tperror(void)
{
	if (pipeout) {
		msg("Tape write error on %s\n", tape);
		msg("Cannot recover\n");
		dumpabort();
		/* NOTREACHED */
	}
	if(Cflag)	/* asize is 1K blocks when C given */
		msg("Tape write error %d Kbytes into tape %d\n", asize, tapeno);
	else
		msg("Tape write error %d feet  into tape %d\n", asize/120L, tapeno);
	broadcast("TAPE ERROR!\n");
	if (!query("Do you want to restart?", 0))
		dumpabort();
	msg("This tape will rewind.  After it is rewound,\n");
	msg("replace the faulty tape with a new one;\n");
	msg("this dump volume will be rewritten.\n");
	nogripe = 1;
	close_rewind();
	Exit(X_REWRITE);
}

void
tflush(void)
{
	int i;

	for (i = 0; i < ntrec; i++)
		spclrec();
}

static void
flusht(void)
{
	if ((host ? rmtwrite(tblock[0], writesize)
		: write(to, tblock[0], writesize)) != writesize) {
		if (errno != 0)
			perror("write");
		else
			msg("write failed\n");
		tperror();
	}
	trecno = 0;
	asize += tenths;
	blockswritten += ntrec;
	if (!pipeout && asize > tsize) {
		close_rewind();
		otape();
	}
	timeest();
}

void
dump_rewind(void)
{
	int f;
	int (*openf)(const char *, int, ...);
	int (*closef)(int);

	if (pipeout)
		return;
	while (wait(NULL) >= 0)    ;	/* wait for any signals from slaves */
	msg("Closing tape device\n");
	if (host) {
		openf = rmtopen;
		closef = rmtclose;
	} else {
		openf = open;
		closef = close;
	}
	if (to >= 0 && (*closef)(to) < 0) {
		perror("  DUMP: error closing tape");
		dumpabort();
	}
	to = -1;
	while ((f = (*openf)(tape, 0)) < 0)
		sleep(10);
	(void) (*closef)(f);
}

void
close_rewind(void)
{
	dump_rewind();
	if (!nogripe) {
		msg("Change Tapes: Mount tape #%d\n", tapeno+1);
		broadcast("CHANGE TAPES!\7\7\n");
	}
	while (!query("Is the new tape mounted and ready to go?", 0))
		if (query("Do you want to abort?", 1)) {
			dumpabort();
			/*NOTREACHED*/
		}
}

/*
 *	We implement taking and restoring checkpoints on the tape level.
 *	When each tape is opened, a new process is created by forking; this
 *	saves all of the necessary context in the parent.  The child
 *	continues the dump; the parent waits around, saving the context.
 *	If the child returns X_REWRITE, then it had problems writing that tape;
 *	this causes the parent to fork again, duplicating the context, and
 *	everything continues as if nothing had happened.
 */

void
otape(void)
{
	int	parentpid;
	int	childpid;
	int	status;
	int	wrval;
	void	(*sigint)() = signal(SIGINT, SIG_IGN);
	int	blks, i;

	parentpid = getpid();

    restore_check_point:
	signal(SIGINT, sigint);
	/*
	 *	All signals are inherited...
	 */
	childpid = dofork ? fork() : 0;
	if (childpid < 0) {
		msg("Context save fork fails in parent %d\n", parentpid);
		Exit(X_ABORT);
	}
	if (childpid != 0) {
		/*
		 *	PARENT:
		 *	save the context by waiting
		 *	until the child doing all of the work returns.
		 *	don't catch the interrupt
		 */
		signal(SIGINT, SIG_IGN);
#ifdef TDEBUG
		msg("Tape: %d; parent process: %d child process %d\n",
			tapeno+1, parentpid, childpid);
#endif /* TDEBUG */
		while ((wrval = wait(&status)) != childpid)
			msg("Parent %d waiting for child %d has another child %d return\n",
				parentpid, childpid, wrval);
		if (status & 0xFF) {
			msg("Child died with signal %d, status %d\n",
				status & 0xff, (status>>8)&0xff);
			status = X_ABORT;
		}
		status = (status >> 8) & 0xFF;
#ifdef TDEBUG
		switch(status) {
			case X_FINOK:
				msg("Child %d finishes X_FINOK\n", childpid);
				break;
			case X_ABORT:
				msg("Child %d finishes X_ABORT\n", childpid);
				break;
			case X_REWRITE:
				msg("Child %d finishes X_REWRITE\n", childpid);
				break;
			default:
				msg("Child %d finishes unknown %d\n",
					childpid, status);
				break;
		}
#endif /* TDEBUG */
		switch(status) {
			case X_FINOK:
				Exit(X_FINOK);
			case X_ABORT:
				Exit(X_ABORT);
			case X_REWRITE:
				if (host)  {
					/* May have lost connection */
					rmtreset();
				}
				goto restore_check_point;
			default:
				msg("Bad return code from dump: %d\n", status);
				Exit(X_ABORT);
		}
		/*NOTREACHED*/
	} else {	/* we are the child; just continue */
#ifdef TDEBUG
		sleep(4);	/* allow time for parent's message to get out */
		msg("Child on Tape %d has parent %d, my pid = %d\n",
			tapeno+1, parentpid, getpid());
#endif /* TDEBUG */
		/*
		 * Perhaps someday this may be fixed so that after opening the
		 * tape device, one does an ioctl(to, MTIOCGET(??), ..) to
		 * get more information about the type of tape device
		 * we are dealing with and get better estimates of the
		 * parameters density, blocking factor and length(!?) unless
		 * they are overridden with user supplied parameters.
		 * This would however have to be done before estimating the
		 * number of tapes etc in main(). -XXX
		 */
		errno = 0;
		while ((to = (host ? rmtopen(tape, 2) :
			pipeout ? 1 : creat(tape, 0666))) < 0) {
			perror(tape);
			if (!query("Cannot open tape.  Do you want to retry the open?", 0))
				dumpabort();
		}
		chkandfixaudio(to);

		asize = 0;
		tapeno++;		/* current tape sequence */
		newtape++;		/* new tape signal */
		blks = 0;
		if (spcl.c_type != TS_END)
			for (i = 0; i < spcl.c_count; i++)
				if (spcl.c_addr[i] != 0)
					blks++;
		spcl.c_count = blks + 1 - spcl.c_tapea + lastspclrec;
		spcl.c_volume++;
		spcl.c_type = TS_TAPE;
		spcl.c_flags |= DR_NEWHEADER;
		if (!bsdformat)
			spcl.c_flags |= DR_EFSDIRS;
		spclrec();
		spcl.c_flags &=~ DR_NEWHEADER;
		if (tapeno > 1)
			msg("Tape %d begins with blocks from ino %d\n",
				tapeno, ino);
	}
}

void
dumpabort(void)
{
	msg("The ENTIRE dump is aborted.\n");
	Exit(X_ABORT);
}

void
Exit(int status)
{
#ifdef TDEBUG
	msg("pid = %d exits with status %d\n", getpid(), status);
#endif /* TDEBUG */
	exit(status);
}

/* check to see if it's a drive, and is in audio mode; if it is, then
 * try to fix it, and also notify them.  Otherwise we can write in
 * audio mode, but they won't be able to get the data back
*/
static void
chkandfixaudio(int mt)
{
	static struct mtop mtop = {MTAUD, 0};
	struct mtget mtget;
	int err;
	unsigned status;

	if(host) err = rmtioctl(MTIOCGET, &mtget);
	else err = ioctl(mt, MTIOCGET, &mtget);
	if(err)
		return;
	status = mtget.mt_dsreg | ((unsigned)mtget.mt_erreg<<16);
	if(status & CT_AUDIO) {
		fprintf(stderr, "dump: Warning: drive was in audio mode, turning audio mode off\n");
		if(host) err = rmtioctl(MTIOCTOP, &mtop);
		else err = ioctl(mt, MTIOCTOP, &mtop);
		if(err)
			fprintf(stderr, "dump: Warning: unable to disable audio mode.  Tape may not be readable\n");
	}
}
