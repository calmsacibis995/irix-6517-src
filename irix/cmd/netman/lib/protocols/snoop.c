/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * Snoop input pseudo-protocol, nested in all link-layer protocols.
 */
#include <time.h>
#include <sys/types.h>
#include <net/raw.h>
#include "enum.h"
#include "protodefs.h"

#ifdef sun
extern long timezone;
#endif

/*
 * Struct snoopheader field ids and descriptors.
 */
enum snoopfid {
	SEQ, FLAGS, LEN, TIME,
	PROMISC, ERROR,
	    FRAME, CHECKSUM, TOOBIG, TOOSMALL, NOBUFS, OVERFLOW, MEMORY
};

#define	PFI_FLAG(name, title, id, off) \
	PFI(name, title, id, DS_ZERO_EXTEND, -1, off, EXOP_NUMBER, 0)

static ProtoField snoopfields[] = {
/* struct snoopheader fields */
	PFI_UINT("seq",    "Sequence Number", SEQ,   u_long,  0),
	PFI_UINT("flags",  "State Flags",     FLAGS, u_short, 0),
	PFI_UINT("len",    "Packet Length",   LEN,   u_short, 0),
	PFI_ADDR("time",   "Reception Time",  TIME,  sizeof(struct timeval), 0),
/* bitfield aliases for enumerated flags bits */
	PFI_FLAG("promisc",  "Promiscuous Flag",       PROMISC,   32),
	PFI_FLAG("error",    "Error Flag",             ERROR,     33),
	PFI_FLAG("frame",    "Frame Error Flag",       FRAME,     47),
	PFI_FLAG("checksum", "Bad Checksum Flag",      CHECKSUM,  46),
	PFI_FLAG("toobig",   "Oversized Packet Flag",  TOOBIG,    45),
	PFI_FLAG("toosmall", "Undersized Packet Flag", TOOSMALL,  44),
	PFI_FLAG("nobufs",   "Buffer Shortage Flag",   NOBUFS,    43),
	PFI_FLAG("overflow", "Input Overflow Flag",    OVERFLOW,  42),
	PFI_FLAG("memory",   "Memory Error Flag",      MEMORY,    41),
};

#define	SNOOPFID(pf)	((enum snoopfid) (pf)->pf_id)
#define	SNOOPFIELD(fid)	snoopfields[(int) fid]

/*
 * Enumerated flag names.
 */
Enumerator snoopflags[] = {
	EI_VAL("PROMISC",	SN_PROMISC),
	EI_VAL("ERROR",		SN_ERROR),
	EI_VAL("FRAME",		SNERR_FRAME),
	EI_VAL("CHECKSUM",	SNERR_CHECKSUM),
	EI_VAL("TOOBIG",	SNERR_TOOBIG),
	EI_VAL("TOOSMALL",	SNERR_TOOSMALL),
	EI_VAL("NOBUFS",	SNERR_NOBUFS),
	EI_VAL("OVERFLOW",	SNERR_OVERFLOW),
	EI_VAL("MEMORY",	SNERR_MEMORY),
};

int	numsnoopflags = lengthof(snoopflags);

/*
 * Snoop protocol interface and operations.
 */
#define	snoop_setopt	pr_stub_setopt
#define	snoop_embed	pr_stub_embed
#define	snoop_match	pr_stub_match

DefineProtocol(snoop, "snoop", "Snoop", PRID_SNOOP, DS_BIG_ENDIAN,
	       sizeof(struct snoopheader), 0, PR_COMPILETEST, 0, 0, 0);

static int gmtoff;

static int
snoop_init()
{
	if (!pr_register(&snoop_proto, snoopfields, lengthof(snoopfields),
			 lengthof(snoopflags))) {
		return 0;
	}
	pr_addnumbers(&snoop_proto, snoopflags, lengthof(snoopflags));

#ifdef sun
	tzset();
	gmtoff = timezone;
#else
	tzset();
	gmtoff = daylight ? altzone : timezone;
#endif
	return 1;
}

#define	EXOP_TIMESTAMP	EXOP_ADDRESS

struct timestamp {
	u_short	hour;	/* hour packet was captured */
	u_short	min;	/* minute */
	u_short	sec;	/* second */
	u_short	msec;	/* millisecond */
};

#define	ex_ts(ex)	A_CAST(&(ex)->ex_addr, struct timestamp)

/* ARGSUSED */
Expr *
snoop_resolve(char *str, int len, struct snooper *sn)
{
	unsigned hour, min, sec, msec, off;
	Expr *ex;
	struct timestamp *ts;

	min = sec = msec = 0;
	if (sscanf(str, "%u:%u:%u.%u", &hour, &min, &sec, &msec) <= 0)
		return 0;
	ex = expr(EXOP_TIMESTAMP, EX_NULLARY, str);
	ts = ex_ts(ex);
	ts->msec = msec;
	off = gmtoff;
	ts->sec = sec + off % 60, off /= 60;
	ts->min = min + off % 60, off /= 60;
	ts->hour = hour + off % 24;
	return ex;
}

/* ARGSUSED */
static ExprType
snoop_compile(ProtoField *pf, Expr *mex, Expr *tex, ProtoCompiler *pc)
{
	long mask, match;

	if (!pc_intmask(pc, mex, &mask))
		return ET_ERROR;
	if (!pc_intmask(pc, tex, &match))
		return ET_ERROR;
	switch (SNOOPFID(pf)) {
#define	matchflag(f)	(match = (match & 1) ? f : 0)
	  case FLAGS:
		break;
	  case ERROR:
		matchflag(SN_ERROR);
		break;
	  case FRAME:
		matchflag(SNERR_FRAME);
		break;
	  case CHECKSUM:
		matchflag(SNERR_CHECKSUM);
		break;
	  case TOOBIG:
		matchflag(SNERR_TOOBIG);
		break;
	  case TOOSMALL:
		matchflag(SNERR_TOOSMALL);
		break;
	  case NOBUFS:
		matchflag(SNERR_NOBUFS);
		break;
	  case OVERFLOW:
		matchflag(SNERR_OVERFLOW);
		break;
	  case MEMORY:
		matchflag(SNERR_MEMORY);
		break;
	  default:
		return ET_COMPLEX;
	}
	*pc->pc_errflags |= (match & mask);
	return ET_COMPLEX;
}

/* ARGSUSED */
static int
snoop_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct snoopheader *sh;
	struct timestamp *ts;
	long units;

	sh = ps->ps_snhdr;
	rex->ex_op = EXOP_NUMBER;
	switch (SNOOPFID(pf)) {
	  case SEQ:
		rex->ex_val = sh->snoop_seq;
		break;
	  case FLAGS:
		rex->ex_val = sh->snoop_flags;
		break;
	  case LEN:
		rex->ex_val = sh->snoop_packetlen;
		break;
	  case TIME:
		rex->ex_op = EXOP_TIMESTAMP;
		ts = ex_ts(rex);
		ts->msec = sh->snoop_timestamp.tv_usec / 1000;
		units = sh->snoop_timestamp.tv_sec % (60*60*24);
		ts->sec = units % 60, units /= 60;
		ts->min = units % 60, units /= 60;
		ts->hour = units % 24;
		break;
	  case PROMISC:
		rex->ex_val = (sh->snoop_flags & SN_PROMISC) != 0;
		break;
	  case ERROR:
		rex->ex_val = (sh->snoop_flags & SN_ERROR) != 0;
		break;
	  case FRAME:
		rex->ex_val = (sh->snoop_flags & SNERR_FRAME) != 0;
		break;
	  case CHECKSUM:
		rex->ex_val = (sh->snoop_flags & SNERR_CHECKSUM) != 0;
		break;
	  case TOOBIG:
		rex->ex_val = (sh->snoop_flags & SNERR_TOOBIG) != 0;
		break;
	  case TOOSMALL:
		rex->ex_val = (sh->snoop_flags & SNERR_TOOSMALL) != 0;
		break;
	  case NOBUFS:
		rex->ex_val = (sh->snoop_flags & SNERR_NOBUFS) != 0;
		break;
	  case OVERFLOW:
		rex->ex_val = (sh->snoop_flags & SNERR_OVERFLOW) != 0;
		break;
	  case MEMORY:
		rex->ex_val = (sh->snoop_flags & SNERR_MEMORY) != 0;
		break;
	}
	return 1;
}

/* ARGSUSED */
static void
snoop_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	struct snoopheader *sh;
	struct tm *tm;

	sh = ps->ps_snhdr;
	pv_showfield(pv, &SNOOPFIELD(SEQ), &sh->snoop_seq,
		     "%10lu", sh->snoop_seq);
	pv_showfield(pv, &SNOOPFIELD(FLAGS), &sh->snoop_flags,
		     "%-14s", en_bitset(snoopflags, lengthof(snoopflags),
					sh->snoop_flags));
	pv_showfield(pv, &SNOOPFIELD(LEN), &sh->snoop_packetlen,
		     "%5u", sh->snoop_packetlen);
	tm = localtime(&sh->snoop_timestamp.tv_sec);
	pv_showfield(pv, &SNOOPFIELD(TIME), &sh->snoop_timestamp,
		     "%02d:%02d:%02d.%03ld",
		     tm->tm_hour, tm->tm_min, tm->tm_sec,
		     sh->snoop_timestamp.tv_usec / 1000);
}
