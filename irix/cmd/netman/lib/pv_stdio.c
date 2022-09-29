/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * A packet viewer based on stdio.
 */
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <termio.h>
#include <time.h>
#include <sys/types.h>
#include <net/raw.h>
#include "heap.h"
#include "macros.h"
#include "packetview.h"
#include "protocol.h"

DefinePacketViewOperations(stdio_pvops,spv)

#define	MAXDEPTH	16		/* stack at most 16 frames */

typedef struct {
	int		prid;		/* frame's protocol id */
	char		*name;		/* name and name length */
	int		namlen;
} StdioViewFrame;

typedef struct {
	FILE		*file;		/* stdio output file */
	int		indentflag;	/* whether to add indentation */
	unsigned int	indent;		/* number of spaces to indent */
	unsigned int	fieldskip;	/* space before next field */
	unsigned int	columns;	/* number of output columns */
	unsigned int	cursor;		/* current column offset */
	StdioViewFrame	*top;		/* frame stack and pointer */
	StdioViewFrame	stack[MAXDEPTH];
	PacketView	view;		/* base class state */
} StdioPacketView;
#define	SPV(pv)	((StdioPacketView *) (pv)->pv_private)

#define	PROTONAMEINDENT		4	/* hanging protocol name indent */
#define	PROTOFRAMEINDENT	12	/* protocol frame data indent */
#define	FIELDWIDTHGRAIN		10	/* field width granularity */
#define	DEFAULTCOLUMNS		80	/* used if output not a tty */

static int fieldwidthbasis[] = {
	2 * FIELDWIDTHGRAIN,		/* PV_TERSE */
	2 * FIELDWIDTHGRAIN,		/* PV_DEFAULT */
	FIELDWIDTHGRAIN,		/* PV_VERBOSE */
};

PacketView *
stdio_packetview(FILE *file, char *name, int level)
{
	StdioPacketView *spv;
	struct winsize ws;

	spv = new(StdioPacketView);
	spv->file = file;
	spv->indentflag = 1;
	spv->indent = 0;
	spv->fieldskip = 0;
	if (ioctl(fileno(file), TIOCGWINSZ, &ws) < 0
	    || ws.ws_col < FIELDWIDTHGRAIN) {
		spv->columns = DEFAULTCOLUMNS;
	} else
		spv->columns = ws.ws_col;
	spv->cursor = 0;
	spv->top = &spv->stack[-1];
	pv_init(&spv->view, spv, &stdio_pvops, name, level);
	return &spv->view;
}

static void
newline(StdioPacketView *spv)
{
	putc('\n', spv->file);
	spv->cursor = 0;
	spv->indentflag = 1;
	spv->fieldskip = 0;
}

static void
breakline(StdioPacketView *spv)
{
	if (spv->cursor && spv->cursor != spv->indent)
		newline(spv);
}

#define	SPACE(spv, count) { \
	int n = (count); \
	(spv)->cursor += n; \
	while (--n >= 0) \
		putc(' ', (spv)->file); \
}

static void
indent(StdioPacketView *spv)
{
	SPACE(spv, spv->indent);
	spv->indentflag = 0;
}

#define	CHECKERROR(spv) \
	(!ferror((spv)->file) ? 1 \
	 : (clearerr((spv)->file), (spv)->view.pv_error = errno, 0))

static int
spv_head(PacketView *pv, struct snoopheader *sh, struct tm *tm)
{
	StdioPacketView *spv;
	FILE *file;

	spv = SPV(pv);
	file = spv->file;
	fprintf(file, "%04lu:%c  len %4d   time %02d:%02d:%02d.%06ld",
		sh->snoop_seq, (sh->snoop_flags & SN_ERROR) ? '!' : ' ',
		sh->snoop_packetlen, tm->tm_hour, tm->tm_min, tm->tm_sec,
		sh->snoop_timestamp.tv_usec);
	if (sh->snoop_flags & SN_ERROR) {
		if (sh->snoop_flags & SNERR_FRAME)
			fputs(" FRAME", file);
		if (sh->snoop_flags & SNERR_CHECKSUM)
			fputs(" CHECKSUM", file);
		if (sh->snoop_flags & SNERR_TOOBIG)
			fputs(" TOOBIG", file);
		if (sh->snoop_flags & SNERR_TOOSMALL)
			fputs(" TOOSMALL", file);
		if (sh->snoop_flags & SNERR_NOBUFS)
			fputs(" NOBUFS", file);
		if (sh->snoop_flags & SNERR_OVERFLOW)
			fputs(" OVERFLOW", file);
		if (sh->snoop_flags & SNERR_MEMORY)
			fputs(" MEMORY", file);
	}
	newline(spv);
	return CHECKERROR(spv);
}

static int
spv_tail(PacketView *pv)
{
	StdioPacketView *spv;

	spv = SPV(pv);
	breakline(spv);
	newline(spv);
	fflush(spv->file);
	spv->indent = 0;
	return CHECKERROR(spv);
}

/* ARGSUSED */
static int
spv_push(PacketView *pv, Protocol *pr, char *name, int namlen, char *title)
{
	StdioPacketView *spv;
	StdioViewFrame *svf;
	int offset;

	spv = SPV(pv);
	svf = ++spv->top;
	if (svf >= &spv->stack[MAXDEPTH])
		svf = 0;
	else {
		svf->prid = pr->pr_id;
		svf->name = name;
		svf->namlen = namlen;
	}

	spv->indent = PROTONAMEINDENT;
	breakline(spv);
	indent(spv);
	if (svf && svf > &spv->stack[0]) {
		--svf;
		if (pr->pr_id == svf->prid) {
			fprintf(spv->file, "%s.", svf->name);
			spv->cursor += svf->namlen + 1;
		}
	}

	fprintf(spv->file, "%s%c",
		name, (pr->pr_flags & PR_DECODESTALE) ? '?' : ':');
	namlen++;
	offset = spv->cursor + namlen;
	spv->indent = PROTOFRAMEINDENT;
	if (offset < spv->indent) {
		spv->cursor = offset;
		SPACE(spv, spv->indent - offset);
	} else {
		newline(spv);
		indent(spv);
	}
	return CHECKERROR(spv);
}

static int
spv_pop(PacketView *pv)
{
	--SPV(pv)->top;
	return 1;
}

/* ARGSUSED */
static int
spv_field(PacketView *pv, void *base, int size, ProtoField *pf,
	  char *contents, int contlen)
{
	StdioPacketView *spv;
	int minwidth, width, level;

	spv = SPV(pv);
	if (spv->indentflag)
		indent(spv);
	minwidth = pf->pf_namlen + 1 + contlen;
	width = ROUNDUP(minwidth + 1, FIELDWIDTHGRAIN);
	level = MIN(pv->pv_level, PV_VERBOSE);
	if (width < fieldwidthbasis[level])
		width = fieldwidthbasis[level];
	if (spv->cursor + spv->fieldskip + minwidth < spv->columns) {
		/* above should be <=, but wsh isn't a vt100 */
		SPACE(spv, spv->fieldskip);
	} else {
		breakline(spv);
		if (spv->indentflag)
			indent(spv);
	}
	fputs(pf->pf_name, spv->file);	/* pf->pf_namlen */
	putc(' ', spv->file);		/* 1 */
	fputs(contents, spv->file);	/* contlen */
	spv->cursor += minwidth;
	spv->fieldskip = width - minwidth;
	return CHECKERROR(spv);
}

static int
spv_break(PacketView *pv)
{
	StdioPacketView *spv;

	spv = SPV(pv);
	breakline(spv);
	return CHECKERROR(spv);
}

static int
spv_text(PacketView *pv, char *buf, int len)
{
	StdioPacketView *spv;

	spv = SPV(pv);
	if (spv->indentflag)
		indent(spv);
	if (fputs(buf, spv->file) == EOF) {
		pv->pv_error = errno;
		return 0;
	}
	spv->cursor += len;
	return 1;
}

static int
spv_newline(PacketView *pv)
{
	StdioPacketView *spv;

	spv = SPV(pv);
	newline(spv);
	return CHECKERROR(spv);
}

static int
spv_hexdump(PacketView *pv, unsigned char *bp, int off, int len)
{
	StdioPacketView *spv;
	int indent, step, skip, grab, n;
	unsigned char *save;

	spv = SPV(pv);
	if (!spv->indentflag)
		newline(spv);
	indent = spv->indent;
	if (indent < PROTOFRAMEINDENT)
		indent = PROTOFRAMEINDENT;
	step = (spv->columns - indent - 1) / (3 + 1);		/* -1+3+1 */
	if (step == 0)
		step = 1;
	else if (step > 2)
		step &= ~01;
	indent -= constrlen("ddddd:  ");

	for (;;) {
		fprintf(spv->file, "%*s%05d:  ", indent, "", off);
		skip = off % step;
		for (n = skip; n > 0; --n)
			fputs("   ", spv->file);
		grab = step - skip;
		if (grab > len)
			grab = len;

		save = bp;
		for (n = grab; --n >= 0; bp++)
			fprintf(spv->file, "%02x ", *bp);	/* 3 */
		for (n = step - skip - len; n > 0; --n)
			fputs("   ", spv->file);

		putc(' ', spv->file);				/* -1 */
		for (n = skip; n > 0; --n)
			putc(' ', spv->file);

		bp = save;
		for (n = grab; --n >= 0; bp++) {
			unsigned char c;

			c = isprint(*bp) ? *bp : '.';		/* 1 */
			putc(c, spv->file);
		}
		newline(spv);

		len -= grab;
		if (len == 0)
			break;
		off += grab;
	}
	return CHECKERROR(spv);
}

static void
spv_destroy(PacketView *pv)
{
	StdioPacketView *spv;

	spv = SPV(pv);
	if (spv->cursor)
		newline(spv);
	fflush(spv->file);
	pv_finish(pv);
	delete(spv);
}
