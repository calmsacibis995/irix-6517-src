/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)kbdload:loader.c	1.2" 			*/

#ident  "$Revision: 1.1 $"

/*
 * This is the magic module that understands all the
 * Secret Incantations.
 */

#include <stdio.h>
#include <sys/types.h>
#include <stropts.h>
#include <errno.h>
#include <pfmt.h>
#include <string.h>
#include "symtab.h"
#include <sys/kbdm.h>
#include "tach.h"

extern char *prog;

struct kbd_header hdr;
struct kbd_tab tb;
struct cornode *n;
char *text;
char *malloc();
unsigned char oneone[256];
struct strioctl sb;

loader(ifd, ofd, pub)
	register int ifd;	/* we read this */
	register int ofd;	/* and write this */
	register int pub;	/* 1 if attempting public load */
{
	register int i;

	if (! read(ifd, &hdr, sizeof(struct kbd_header)))
		trunko();
	checkmagic(&hdr);
	/*
	 * For each table, do the obvious.
	 */
	for (i = 0; i < hdr.h_ntabs; i++) {
		if (! read(ifd, &tb, sizeof(struct kbd_tab)))
			trunko();

		if (tb.t_flag == KBD_COT) { /* this does "link" stuff */
			do_link(IS_LINK, ifd, &tb, pub, (char *) 0);
			continue;
		}
		else if (tb.t_flag & KBD_ALP) {	/* do an ALP funtion */
			do_link(IS_EXT, ifd, &tb, 1, (char *) 0);
			continue;
		}

		loadhead(&tb, ofd);	/* start the ball rolling */

		if (tb.t_flag & KBD_ONE) {
			if (! read(ifd, oneone, 256))
				trunko();
			loadone(oneone, ofd);
		}
		n = (struct cornode *)
			malloc(tb.t_nodes * sizeof(struct cornode));
		if (tb.t_nodes && (! read(ifd, n, tb.t_nodes * sizeof(struct cornode))))
			trunko();
		load_N_T(ofd, n, tb.t_nodes * sizeof(struct cornode));
		free(n);
		text = malloc(tb.t_text);
		if (tb.t_text && (! read(ifd, text, tb.t_text)))
			trunko();
		load_N_T(ofd, text, tb.t_text);
		free(text);
		loadend(ofd, pub);	/* cleanup after */
	}
}

checkmagic(h)

	struct kbd_header *h;
{
	if (strncmp((char *)h->h_magic, KBD_MAGIC, strlen(KBD_MAGIC)) != 0) {
		pfmt(stderr, MM_ERROR, ":37:Bad magic (not a kbd object)\n");
		exit(1);
	}
	if (h->h_magic[KBD_HOFF] > KBD_VER) {
		pfmt(stderr, MM_ERROR, ":38:Version mismatch; this is %d, file is %d\n",
			KBD_VER, (int) (h->h_magic[KBD_HOFF]));
		exit(1);
	}
}

char tbuf[256+ALIGNMENT]; /* temporary buffer for piecemeal download */

/*
 * Bombs away.  Drop the header, watch the feedback & figure
 * out whether to continue the operation or retreat in defeat.
 */

loadhead(t, ofd)
	struct kbd_tab *t;
	int ofd;
{
	struct kbd_load z;

	z.z_tabsize = sizeof(struct kbd_tab);
	z.z_onesize = (t->t_flag & KBD_ONE) ? 256 : 0;
	z.z_nodesize = t->t_nodes * sizeof(struct cornode);
	z.z_textsize = t->t_text;
/*	fprintf(stderr, "%s: %d\n", t->t_name,
		z.z_tabsize+z.z_onesize+z.z_nodesize+z.z_textsize); */
	strcpy((char *)z.z_name, (char *)t->t_name);
	sb.ic_cmd = KBD_LOAD;
	sb.ic_timout = 30;
	sb.ic_len = sizeof(struct kbd_load) + ALIGNMENT;
	sb.ic_dp = tbuf;

	cpalign(tbuf, 0x3305);
	cpchar(&tbuf[ALIGNMENT], &z, sizeof(struct kbd_load));
	analyze(ioctl(ofd, I_STR, &sb), sb.ic_cmd);

	sb.ic_cmd = KBD_CONT;
	sb.ic_timout = 15;
	sb.ic_len = sizeof(struct kbd_tab) + ALIGNMENT;
	sb.ic_dp = tbuf;
	cpalign(tbuf, 0x3680);
	cpchar(&tbuf[ALIGNMENT], t, sizeof(struct kbd_tab));
	analyze(ioctl(ofd, I_STR, &sb), sb.ic_cmd);
}

/*
 * Load the "oneone" table, if any.
 */

loadone(one, ofd)
	char *one;
	int ofd;
{
	/* magic 3680 */
	sb.ic_cmd = KBD_CONT;
	sb.ic_timout = 15;
	sb.ic_len = 256 + ALIGNMENT;
	sb.ic_dp = tbuf;
	cpalign(tbuf, 0x3680);
	cpchar(&tbuf[ALIGNMENT], one, 256);
	analyze(ioctl(ofd, I_STR, &sb), sb.ic_cmd);
}

/*
 * Load the nodes (piecemeal), and the text (piecemeal).
 */

load_N_T(ofd, addr, size)
	int ofd;
	char *addr;
	int size;
{
	register char *beg, *end;
	/* magic 3680 */

	beg = addr;
	end = beg + size;
	while (beg < end) {
		sb.ic_cmd = KBD_CONT;
		sb.ic_timout = 15;
		sb.ic_dp = tbuf;
		cpalign(tbuf, 0x3680);
		if (size > 250) {
			sb.ic_len = 250 + ALIGNMENT;
			cpchar(&tbuf[ALIGNMENT], beg, 250);
			beg += 250;
			size -= 250;
		}
		else {
			sb.ic_len = size + ALIGNMENT;
			cpchar(&tbuf[ALIGNMENT], beg, size);
			beg += size;
			size = 0;
		}
		analyze(ioctl(ofd, I_STR, &sb), sb.ic_cmd);
	}
}

/*
 * Dump out the END ioctl with appropriate incantations.
 */

loadend(ofd, pub)
	register int ofd, pub;
{
	static struct kbd_ctl cx;

	/* magic 2212 */
	sb.ic_cmd = KBD_END;
	sb.ic_timout = 15;
	sb.ic_len = sizeof(struct kbd_ctl);
	sb.ic_dp = (char *) &cx;
	cx.c_arg = 0x2212;
	cx.c_type = pub ? Z_PUBLIC : Z_PRIVATE;
	analyze(ioctl(ofd, I_STR, &sb), sb.ic_cmd);
}

/*
 * This is the "unloader"; it unloads tables.
 */

unloader(name)
	char *name;
{
	struct strioctl sb;
	struct kbd_tach t;

	sb.ic_cmd = KBD_UNLOAD;
	sb.ic_timout = 15;
	sb.ic_len = sizeof(struct kbd_tach) + ALIGNMENT;
	sb.ic_dp = tbuf;

	strcpy((char *)t.t_table, name);
	cpalign(tbuf, 0x6361);
	cpchar(&tbuf[ALIGNMENT], &t, sizeof(struct kbd_tach));
	analyze(ioctl(0, I_STR, &sb), sb.ic_cmd);
}

/*
 * do_link	Make a link table.  On disk, a link table is a "kbd_tab"
 *		followed immediatedly by a STRING that is "t.t_max"
 *		bytes long.  We just take the string and ship it to
 *		the module, and it worries about parsing.
 */

do_link(type, ifd, t, pub, loc)
	int type;	/* IS_LINK, IS_EXT, 0 */
	int ifd;	/* input fdesc to read, if not "local" */
	register struct kbd_tab *t;	/* table, if not local */
	register int pub;	/* 1 if public */
	register char *loc;	/* for cmd-line table loading ("local") */
{
	struct strioctl sb;
	register int i, rval;

	if (type == IS_LINK) {
		oneone[0] = pub ? ~Z_PUBLIC : ~Z_PRIVATE;
		oneone[1] = 0x69;
		sb.ic_cmd = KBD_LINK;
	}
	else if (type == IS_EXT) {
		oneone[0] = ~Z_PUBLIC;	/* EXT always public */
		oneone[1] = 0x96;
		sb.ic_cmd = KBD_EXT;
	}
	if (t) {	/* if "t", it's real */
		if (read(ifd, &oneone[2], t->t_max) != t->t_max)
			trunko();
		sb.ic_len = t->t_max + 2;	/* tmax includes null terminator in length */
	}
	else {		/* is a link or extern */
		strcpy((char *)&oneone[2], loc);
		sb.ic_len = strlen(loc) + 3;	/* include null terminator */
	}
	sb.ic_timout = 15;
	sb.ic_dp = (char *) &oneone[0];
	analyze(rval = ioctl(0, I_STR, &sb), sb.ic_cmd);
		
}

/*
 * For the "-L" and "-l" options, fakes up a link table on the fly
 * and loads it.  For the "-e" option, similarly does an "external".
 * definition for an ALP function.
 * (Could be a #define macro).
 */

fakelink(s, opt, type)
	char *s;
	int opt;
	int type;
{
	do_link(type, 0, (struct kbd_tab *) 0, opt, s);
}

/*
 * Figure out what to do with the return value from "ioctl".
 * On 'zero', we just return, because it succeeded.
 */
analyze(rval, cmd)

	int cmd;
{
	if (rval == 0)
		return;
	switch (errno) {
	case EACCES:	/* invalid access (pub/priv) for link */
			pfmt(stderr, MM_ERROR,
				":103:Invalid access (pub/priv)\n");
			break;
	case EPERM:	/* permission denied to mere mortals */
			if (cmd == KBD_END)
			  pfmt(stderr, MM_ERROR,
			  	":104:Table sanity check failed.\n");
			else
			  pfmt(stderr, MM_ERROR|MM_NOGET, "%s\n",
			  	strerror(EACCES));
			break;
	case EPROTO:	/* misc protocol errors */
			pfmt(stderr, MM_ERROR, ":105:Protocol failure.\n");
			break;
	case ENOSPC:	/* out of public/private space */
			pfmt(stderr, MM_ERROR,
				":106:Out of space for table load.\n");
			break;
	case ENOMEM:	/* malloc failed */
			pfmt(stderr, MM_ERROR,
				":107:System out of memory.\n");
			break;
	case EFAULT:	/* END/CONT protocol violation */
			pfmt(stderr, MM_ERROR,
				":108:Protocol fault during download.\n");
			break;
	case E2BIG:	/* load bigger than originally declared */
			pfmt(stderr, MM_ERROR,
				":109:Declared/loaded size mismatch.\n");
			break;
	case EEXIST:	/* table exists already */
			pfmt(stderr, MM_ERROR,
				":110:A table of that name is already loaded.\n");
			break;
	case EFBIG:	/* smaller than a breadbox or larger than a whale */
			pfmt(stderr, MM_ERROR,
				":111:Table size out of range.\n");
			break;
	case ENOENT:	/* named table cannot be found */
			if (cmd == KBD_EXT)
				pfmt(stderr, MM_ERROR, ":112:External %s not found.\n",
					&(oneone[2]));
			else
				pfmt(stderr, MM_ERROR,
					":113:Table not found (or not attached).\n");
			break;
	case ESRCH:	/* table not found on second pass of unload */
			pfmt(stderr, MM_ERROR,
				":114:Unload failed (table not found).\n");
			break;
	case EMLINK:	/* already attached */
			pfmt(stderr, MM_ERROR, ":115:Table attached.\n");
			break;
	case ENOSR:	/* not enough tablinks */
			pfmt(stderr, MM_ERROR, 
				":116:Resource depletion (link structures).\n");
			break;
	case EBUSY:	/* too many attachments */
			pfmt(stderr, MM_ERROR,
				":117:Too many tables attached.\n");
			break;
	case EXDEV:	/* Secret Incantation Failure (but we can't SAY it) */
	default:	/* mysterious error */
			pfmt(stderr, MM_ERROR,
				":118:I/O Error (%d) in download.\n", errno);
	}
	exit(1);
}

trunko()

{
	pfmt(stderr, MM_ERROR, ":119:Unexpected EOF in loaded file.\n");
	exit(1);
}
