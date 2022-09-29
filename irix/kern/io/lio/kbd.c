/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)uts-3b2:io/kbd/kbd.c	1.2"		*/

#ident "$Revision: 1.18 $"

/*
 *	Keyboard Mapping & String Translation module.  See the
 *	"In.Doc" document, as well as user-level documents for an
 *	overview of what this is supposed to be doing.  The code
 *	assumes you've read the documentation.
 */

/*
 *	There is one unreachable statement in "kbd.c" about line 2856 or so.
 *	If the statement is ever "reached", it means there's a bug in the
 *	giant infinite loop in the same function (kbdproc).  The unreachble
 *	statement, if reached, will return a reasonable value and do something
 *	"reasonable" with the message block to keep from losing it (we hope!).
 */

#include <sys/eucioctl.h>	/* EUC ioctl calls */
#include <sys/types.h>
#include <sys/stream.h>
#include <sys/strmp.h>
#include <sys/kmem.h>
#include <sys/signal.h>
#include <sys/errno.h>		/* for error numbers */
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/param.h>
#include <sys/mload.h>
#include <sys/cred.h>

#define Chunk_o_Mem()	kmem_alloc(sizeof(struct kbdusr), KM_NOSLEEP)
#define Free_a_Chunk(Q)	kmem_free(Q, sizeof(struct kbdusr))
#define MemRi_Matic(Z)	kmem_zalloc(Z, KM_NOSLEEP)

#include <sys/kbdm.h>		/* local kbd-subsystem defs */
#include <sys/kbduser.h>	/* local kbd module defs */
#include <sys/alp.h>		/* ALP defs */
#include <string.h>

#define ZIP (struct kbd_tab *) 0

/*
 * The asymmetry in module_info reflects "major usage", which is in
 * interactive sessions.
 */
static struct module_info kbdrinfo = { 0, "kbd", 0, INFPSZ, 256, 16 };
static struct module_info kbdwinfo = { 0, "kbd", 0, INFPSZ, 1024, 128 };

static int kbd_ttype(struct kbd_tab *, struct kbd_tab *);
static int kbdclose(queue_t *, int, struct cred *);
static int kbdwsrv(queue_t *);
static int kbdwput(queue_t *, mblk_t *);
static int kbdrsrv(queue_t *);
static int kbdrput(queue_t *, mblk_t *);
static int kbdopen(queue_t *, dev_t *, int, int, struct cred *);
static int kbdresult(int, unsigned char *, mblk_t **);
static int kbdproc(queue_t *, mblk_t *, struct kbdusr *, struct tablink *,
	mblk_t **, int);
static int zzcompare(unsigned char *, unsigned char *);
static int choo_choo(queue_t *, mblk_t *, struct kbdusr *);
static int kbdlist(queue_t *, mblk_t *, int);
static int kbd_unload(struct kbduu *, mblk_t *mp, struct iocblk *);
static int kbd2big(int, int, int);
static int frisk(struct kbd_tab *);
static void kbd_timeout(struct kbdusr *);
static void kbdlaux(struct kbd_tab *, struct kbduu *, struct kbd_query *);
static void kbd_setime(struct kbdusr *, struct tablink *);
static void kbdioc(queue_t *, mblk_t *, struct iocblk *, struct kbdusr *);
static void kbdload(queue_t *, mblk_t *, struct iocblk *);
static void kbd_runtime(struct kbdusr *);
static void hotsw(struct kbdusr *);
static void kbd_prvlink(struct kbd_tab *, struct kbduu *);
static void tabfill(struct kbd_tab *);
static void kbd_publink(struct kbd_tab *);
static void pay_the_piper(struct kbd_tab *, unsigned char c, mblk_t **);
static void freetable(struct kbd_tab *);
static void kbd_deref(register struct kbd_tab *, int);
static void kbdverb(queue_t *, struct kbdusr *);
static void kbdunlink(struct tablink *);
static struct kbd_tab *kbdnewcomp(struct kbd_tab *, struct kbdusr *);
static struct kbd_tab *kbdlinktab(mblk_t *, struct iocblk *, struct kbduu *);
static struct kbd_tab *kbd_newalp(unsigned char *, struct kbduu *);
static struct kbd_tab *kbdfind(unsigned char *, struct kbduu *);
static struct kbd_tab *newtable(struct kbd_load *);
static struct tablink *kbd_attached(unsigned char *, struct kbdusr *);
static struct tablink *grabtl(struct kbd_tab *);
static struct kbdusr *new_side(struct kbdusr **);
static mblk_t *buf_to_msg(unsigned char *, unsigned char *);

static struct qinit rinit = {
	kbdrput, kbdrsrv, kbdopen, kbdclose, NULL, &kbdrinfo, NULL
};
static struct qinit winit = {
	kbdwput, kbdwsrv, NULL,    NULL,     NULL, &kbdwinfo, NULL
};
struct streamtab kbdinfo = { &rinit, &winit, NULL, NULL };

int kbddevflag = 0;

char *kbdmversion = M_VERSION;

/*
 * Definitions in the master file:
 */
extern int kbd_maxu;	/* max tabs a user can attach at once */
extern int kbd_umem;	/* max bytes of private tables, per user */
extern int kbd_timer;	/* default "timed mode" timer, in ticks */

static struct tablink *tlinkl = (struct tablink *) 0;	/* free tablinks */
static struct kbd_tab *tables;	/* linked list of tables */
/*
 * We need a "have been run" flag, because init routines don't seem to
 * be able to use "kmem_alloc".
 */
static int virgin = ~0;

/* Our Hero */
#define the_shining_one(A)	(A == 0)

/* slot states: */
#define FREE	0
#define USED	0xff

/* machine state flags (l_state) */
#define IDLE	1	/* idling */
#define SRCH	2	/* currently in search mode */

/* user state flags (u_state) */
#define ALRM	1	/* alarm went off */
#define HASHOT	4	/* has a hot key defined */

/*

A Note on tables & tablinks.  Tablinks are used to keep a linked list
of the tables attached to a particular user.  The public tables are kept
linked together by the t_next pointers in the kbd_tab structure.  If a
user has some private tables loaded, those are also linked into a list
by the t_next pointers but are not linked into the public list.  The
user's attached tables are linked by the "l_next" fields of the tablink
structures. The "l_me" pointers point to the tables themselves. Also,
tablinks hold state information relating to the particular user and the
state of the table when it's "in use".

For composite tables, an ARRAY of tablinks is stored in the table COPY
that the user attaches to.  When a user attaches a composite table, a
new copy is made and put on the user's "attached" list; the ref counts
of all components are incremented.  When the user detaches, the copy is
destroyed and the ref counts of components decremented.  A side-effect
of always using copies for composites is that the reference counts of
composite tables are always 0; composites may be "unloaded" without
affecting users who are using them, because users are using the copies. 
This leads to a space-saving trick: if you want to use a composite but
save some core space, you can load the components, make the composite link,
attach the composite, then unload the composite.  The "working copy" is
left, and it doesn't show on the list of available tables; the
space saved is the size of the table header plus some pointers.  When
you detach the working copy, the composite disappears.

 */

/*
 * kbdopen	Turn on, tune in, JMP 0.
 *
 *	register queue_t *q;	pointer to read queue 
 *	dev_t	*dev;		major/minor device number
 *	int	flag;		file open flag -- zero for modules
 *	int	sflag;		stream open flag
 */

/* ARGSUSED */
static int
kbdopen(register queue_t *q, dev_t *dev, int flag, int sflag,
	struct cred *crp)
{
	register int sx;
	register struct kbduu *uu;

	if (sflag != MODOPEN) {	/* can't open as a driver or clone */
		return(EIO);
	}
	if (virgin) {
		virgin = the_shining_one(1);
		if (kbd_timer < MINTV)	/* force kbd_timer in bounds */
			kbd_timer = MINTV;
		if (kbd_timer > MAXTV)
			kbd_timer = MAXTV;
		/*
		 * Staying at STR priority grab a bunch of free tablinks.
		 * Later we'll allocate out of this pool and grab more
		 * when it's exhausted.
		 */
		(void) grabtl(ZIP);
	}
	/*
	 * Check first for multiple opens
	 */
	STR_LOCK_SPL(sx);
	if (! q->q_ptr) {	/* is a first-time open */
		q->q_ptr = (caddr_t) kmem_alloc(sizeof(struct kbduu),
				KM_NOSLEEP);
		if (! q->q_ptr) {
			STR_UNLOCK_SPL(sx);
			return(ENOMEM);
		}
	}
	else {	/* is an n-th time open, OK */
		STR_UNLOCK_SPL(sx);
		return(0);
	}
	/*
	 * One "kbduu" structure allocated per Stream.
	 */
	uu = (struct kbduu *) q->q_ptr;
	new_side(&uu->oside);
	new_side(&uu->iside);
	if (! uu->iside || ! uu->oside) {
		if (uu->oside)	/* make sure all come free if we fail */
			kmem_free(uu->oside, sizeof(struct kbdusr));
		if (uu->iside)
			kmem_free(uu->iside, sizeof(struct kbdusr));
		kmem_free(uu, sizeof(struct kbduu));
		STR_UNLOCK_SPL(sx);
		return(ENOMEM);
	}
	uu->u_use = USED;
	uu->ltmp = uu->u_link = (struct kbd_tab *) 0;
	uu->lpos = (unsigned char *) 0;
	uu->zuid = crp->cr_uid; /* our security blanket */
	uu->zumem = uu->lsize = 0;
	uu->lalsz = 0;
	WR(q)->q_ptr = (caddr_t) uu;
	uu->iside->u_q = q;
	uu->oside->u_q = WR(q);
	STR_UNLOCK_SPL(sx);
	return(0);
}

/*
 * kbdclose	Turn off, tune out, shut down.  Unlink all our lists,
 *		free local memory and messages.
 *	register queue_t *q;	 pointer to the read queue
 *	int	flag;		file open flags - zero for modules
 */
/* ARGSUSED */
static int
kbdclose(register queue_t *q, int flag, struct cred *unused)
{
	register struct kbduu *uu;
	register struct kbd_tab *t;
	register int sx;

	uu = (struct kbduu *) q->q_ptr;

/*
 * FIX ME: This detachment and the next should be re-structured.
 * All copies & originals of composites MUST be deref'd and freed
 * before freeing any simple tables to avoid possible bad "deref"
 * references to tables that have been unloaded.  (Given the structure
 * of the list, it's impossible to get things ordered to lead to a
 * bad reference; the fix is to avoid problems if linking strategy
 * is changed -- right now new links go to the HEAD of the list; if they
 * went to the tail, things would break.
 */
	/*
	 * Free attached tablinks & copies of composites.
	 */
	if (uu->iside) {
		if (uu->iside->u_str[0])
			kbdverb(q, uu->iside);
		kbdunlink(uu->iside->u_list);
		Free_a_Chunk(uu->iside);
	}
	if (uu->oside) {
		if (uu->oside->u_str[0])
			kbdverb(q, uu->oside);
		kbdunlink(uu->oside->u_list);
		Free_a_Chunk(uu->oside);
	}
	while (uu->u_link) {	/* Free private tables */
		t = uu->u_link;
		uu->u_link = t->t_next;
		t->t_next = ZIP;
		if (t->t_flag == KBD_COT)
			kbd_deref(t, R_AV);
		freetable(t);
	}
	uu->u_link = ZIP;

	STR_LOCK_SPL(sx);
	kmem_free(uu, sizeof(struct kbduu));
	q->q_ptr = 0;
	OTHERQ(q)->q_ptr = 0;
	STR_UNLOCK_SPL(sx);
	return(42);	/* satisfy lint with the ultimate answer */
}

/*
 * Unlink a user's "u_list" and decrement reference counts on all the
 * attached maps.  We chug down the line, unlinking each tablink
 * structure, and returning them to the head of the free list.  Before
 * we do each one, we decrement ref counts on the tables.  Since this
 * is the "attached" list, we free the copies of composite tables.
 * We do NOT keep track of uu->zumem, as this only called when closing.
 */

static void
kbdunlink(register struct tablink *t)
{
	register struct tablink *nxt;
	register struct kbd_tab *tmp;
	register int sx;

	nxt = t;

	STR_LOCK_SPL(sx);
	/*
	 * Drizzle drazzle druzzle drone, time for this one to come home.
	 */
	while (t) {
		--(t->l_me->t_ref);
		nxt = t->l_next;	/* save next */
		/*
		 * More delicate messing with the free list of tablinks
		 */
		t->l_next = tlinkl;	/* link to head of free list */
		tlinkl = t;
		if (tlinkl->l_me->t_flag == KBD_COT) {
			/*
			 * Always free attached composites: they're copies.
			 */
			tmp = tlinkl->l_me;
			tlinkl->l_me = ZIP;
			STR_UNLOCK_SPL(sx);
			kbd_deref(tmp, R_TC);
			freetable(tmp); /* allow interrupts, etc. */
			STR_LOCK_SPL(sx);
		}
		tlinkl->l_me = ZIP;
		t = nxt;		/* move on down the line */
	}
	STR_UNLOCK_SPL(sx);
}

/*
 * This is the function that calls "kbdproc" to dispose of data
 * messages.  It has to take into account the fact that kbdproc
 * may have switched tables in mid-stream, and so re-call kbdproc
 * with the new table if that's what's necessary.  It also takes
 * care of "putnext" on the message and all that.  The only reason
 * we really have this function is to avoid duplicating this code
 * everyplace we want to call kbdproc to process a message.
 */

static int
choo_choo(queue_t *q, register mblk_t *mp, register struct kbdusr *d)
{
	register struct tablink *cur;
	mblk_t *out;	/* need its address, folks */
	register int i; /* relative position */

	/*
	 * If kbdproc comes back with 0, it means that a table switch
	 * occurred in mid-stream.  It switched the table, and left
	 * "mp" as the un-processed part.  Anything that it has already
	 * processed is in "out".  In that case, we have to send "out"
	 * on its way, and re-process "mp" with the new table.  If
	 * kbdproc returns a 0, then it MUST NOT FREE "mp"!
	 */
chew_it_again:
	i = 0;
	/*
	 * This case for composite tables.
	 */
	if (d->u_current && (d->u_current->t_flag == KBD_COT)) {
		cur = d->u_cp->l_me->t_child;
		out = (mblk_t *) 0;
		while (cur && mp) {
			if (! (kbdproc(q, mp, d, cur, &out, i))) {
				cur = (struct tablink *) 0;
				/*
				 * If there's a processed part, send it,
				 * then do "mp".
				 */
				if (out)
					putnext(q, out);
				out = (mblk_t *) 0;
				if ((mp->b_rptr < mp->b_wptr) || mp->b_cont)
					goto chew_it_again;
				else {
					freemsg(mp);
					return(1);
				}
			}
			cur = cur->l_next;
			mp = out;
			out = (mblk_t *) 0;
			++i;
		}
		if (mp)
			putnext(q, mp);
	}
	/*
	 * This case for simple tables.
	 */
	else {
		out = (mblk_t *) 0;
		if (! (kbdproc(q, mp, d, d->u_cp, &out, 0))) {
			if (out)
				putnext(q, out);
			out = (mblk_t *) 0;
			if ((mp->b_rptr < mp->b_wptr) || mp->b_cont)
				goto chew_it_again;
			else {
				freemsg(mp);
				return(1);
			}
		}
		if (out)
			putnext(q, out);
	}
	return(42);
}

/*
 * kbd_runtime(d)
 *	This is the function called when an alarm has gone off for
 *	a timing-dependent table.  We check the current table (or run
 *	through the composite components) until we find any tables that
 *	have a non-zero l_lbolt value <= lbolt.  When we find one, we
 *	take whatever data it has pending, reset its state to IDLE, and
 *	either putnext or call the rest of the tables given that data.
 *	The processing is similar to "choo_choo", but can start up a
 *	composite "in the middle", not just at the beginning.
 *	MUST NOT be called if d->u_current is null.  If the table gets
 *	switched on us, everything will have been flushed by the
 *	table-switch routine, so we just return.
 */

static void
kbd_runtime(register struct kbdusr *d)
{
	register struct tablink *cur;
	register mblk_t *mp;
	mblk_t *out;
	register int i, bolt;

	i = 0;
	bolt = lbolt;	/* for local consistency */
	if (d->u_current->t_flag == KBD_COT) {
		cur = d->u_cp->l_me->t_child;
		out = (mblk_t *) 0;
		while (cur) {	/* find offendors in the list */
			if (cur->l_lbolt && cur->l_lbolt <= bolt)
				break;	/* found one */
			else {
				cur = cur->l_next;
				++i;	/* count of position */
			}
		}
		if (cur) {	/* found one with timed-out buffer */
			cur->l_lbolt = 0;
			if (cur->l_ptr > (unsigned char *) &(cur->l_msg[0])) {
				cur->l_node = 0;	/* reset idle state */
				cur->l_state |= IDLE;
				cur->l_state &= ~SRCH;
				mp = buf_to_msg(cur->l_msg, cur->l_ptr);
				cur->l_ptr =(unsigned char *) &(cur->l_msg[0]);
				if (! mp)
					return;
				pay_the_piper(cur->l_me, *mp->b_rptr++, &out);
/*
 * FIX ME: should do it this way:
 * Send the output of pay_the_piper to the next tables, then send the
 * rest of the stuff back through the "bad guy" and the rest.  At the moment,
 * we're just dumping the whole buffer on the next guy.  With short
 * timeouts, it may not be worth it to do it the "hard" but correct way...
 */
				if (out) {
					if (mp->b_wptr > mp->b_rptr)
						linkb(out, mp);
					mp = out;
					out = (mblk_t *) 0;
				}
				cur = cur->l_next;	/* send to next guy */
				++i;
				while (cur && mp) {
					if (! (kbdproc(d->u_q, mp, d, cur, &out, i))) {
						if (out)
							putnext(d->u_q, out);
						freemsg(mp);
						return;
					}
					cur = cur->l_next;
					mp = out;
					out = (mblk_t *) 0;
					++i;
				}
				if (mp)
					putnext(d->u_q, mp);
			}
		}
		return;
	}
	else {
		/*
		 * It's a simple table: don't run the FIRST BYTE
		 * back through kbdproc!  We have to figure out if
		 * we have an error, and if so send it instead of
		 * the first byte.  If there's anything left, then
		 * send it along.
		 */
		cur = d->u_cp;
		if (bolt < cur->l_lbolt) { /* eh? didn't it go off? */
cmn_err(CE_NOTE, "UX:KBD: Error in alarm timing\n");
			if (cur->l_tid) {
				untimeout(cur->l_tid);
				cur->l_tid = 0;
				cur->l_lbolt = 0;
			}
			return;
		}
		/*
		 * If anything in the message...
		 */
		if (cur->l_ptr > (unsigned char *) &(cur->l_msg[0])) {
			/*
			 * FIX ME: we could avoid an un-necessary allocation
			 * when there's only one byte by re-writing this.
			 */
			out = (mblk_t *) 0;
			cur->l_node = 0;	/* reset idle state */
			cur->l_state |= IDLE;
			cur->l_state &= ~SRCH;
			mp = buf_to_msg(cur->l_msg, cur->l_ptr);
			cur->l_ptr =(unsigned char *) &(cur->l_msg[0]);
			if (! mp)
				return;
			pay_the_piper(cur->l_me, *mp->b_rptr++, &out);
			/*
			 * now, "out" is the error string or first byte and
			 * "mp" is the rest.
			 */
			if (out) {
				putnext(d->u_q, out);
				out = (mblk_t *) 0;
			}
			if (mp->b_wptr > mp->b_rptr) {
				if (! (kbdproc(d->u_q, mp, d, cur, &out, i))) {
					if (out)
						putnext(d->u_q, out);
					freemsg(mp);
				}
			}
			else
				freemsg(mp);
		}
	}
}

/*
 * kbdwput	Put procedure for WRITE side of module.  Sends IOCTL
 *		calls to "kbdioc".  Data goes to kbdproc, same as the
 *		read side.
 *	register queue_t *q;	pointer to the write queue
 *	register mblk_t *mp;	message pointer
 */

static int
kbdwput(register queue_t *q, register mblk_t *mp)
{
	register struct iocblk *iocp;
	register struct kbdusr *d;
	register struct kbduu *uu;

	uu = (struct kbduu *)q->q_ptr;
	d = (struct kbdusr *) uu->oside;

	switch (mp->b_datap->db_type) {
	case M_IOCTL:
		iocp = (struct iocblk *) mp->b_rptr;
		if (((iocp->ioc_cmd & 0xFFFFFF00) == KBD) ||
		    ((iocp->ioc_cmd & EUC_IOC) == EUC_IOC)) {
			kbdioc(q, mp, iocp, d);
		}
		else
			putnext(q, mp);
		break;
	case M_DATA:
		if (! d->u_doing) {
			putnext(q, mp);
			return(42);
		}
		/*
		 * Check for data ordering...
		 */
		else if (q->q_first || d->u_running) {
			putq(q, mp);
			qenable(q);
		}
		else
			choo_choo(q, mp, d);
		break;
	case M_FLUSH:
		flushq(q, FLUSHDATA);
		putnext(q, mp);
		break;
	default:
		/*
		 * Shuffle Control messages off to Buffalo.  Otherwise "if
		 * it looks and smells delicious, it's probably poison".
		 */
		if (mp->b_datap->db_type & QPCTL)
			putnext(q, mp);	/* high priority poison */
		else if (q->q_first || d->u_running) {
			putq(q, mp);	/* attempt to keep it in order */
			qenable(q);
		}
		else
			putnext(q, mp); /* Hope it takes care of itself */
	}
	return(42);
}

/*
 * Write server.  Only entered for queued messages when "u_doing" or if
 * the alarm went off.  Set "u_running" on entry & unset on exit.
 */

static int
kbdwsrv(register queue_t *q)
{
	register struct kbdusr *d;
	register mblk_t *mp;

	d = (struct kbdusr *) ((struct kbduu *)q->q_ptr)->oside;
	d->u_running = 1;
	if (d->u_state & ALRM) {	/* alarm went off */
		d->u_state &= ~ALRM;	/* turn off flag */
		if (d->u_current)
			kbd_runtime(d);
	}
	while (mp = getq(q)) {
		if (mp->b_datap->db_type == M_DATA) {
			if (! canput(q->q_next)) {
				putbq(q, mp);
				d->u_running = 0;
				return(42);
			}
			else
				choo_choo(q, mp, d);
		}
		else {
			if (canput(q->q_next))
				putnext(q, mp);
			else {
				putbq(q, mp);
				d->u_running = 0;
				return(42);
			}
		}
	}
	d->u_running = 0;
	return(42);
}

/*
 * kbdrput	Read side put procedure.  Snag all M_DATA messages and
 *		sniff them for goodies.
 *	register queue_t *q;	pointer to the read queue 
 *	register mblk_t *mp;	message pointer
 */

static int
kbdrput(register queue_t *q, register mblk_t *mp)
{
	register struct kbdusr *d;

	d = (struct kbdusr *) ((struct kbduu *)q->q_ptr)->iside;
	switch (mp->b_datap->db_type) {
	case M_FLUSH:
		flushq(q, FLUSHDATA);
		putnext(q, mp);
		break;
	case M_DATA:
		if (d->u_doing) {	/* if we're "on" */
			if (q->q_first || d->u_running) {
				putq(q, mp);
				return(42);
			}
			else {
				if (! canput(q->q_next))
					putq(q, mp);
				else
					choo_choo(q, mp, d);
				return(42);
			}
		}
		/* fall thru */
	case M_IOCACK:
	case M_IOCNAK:
		putnext(q, mp);
		break;
	default:
		/*
		 * Not food
		 */
		if (mp->b_datap->db_type & QPCTL)
			putnext(q, mp);
		else if (q->q_first || d->u_running)
			putq(q, mp);
		else
			putnext(q, mp);
	}
	return(42);
}

/*
 * Read side service.  Only called if necessary.  See comments
 * above on data ordering.
 */
static int
kbdrsrv(register queue_t *q)
{
	register struct kbdusr *d;
	register mblk_t *mp;

	d = (struct kbdusr *) ((struct kbduu *)q->q_ptr)->iside;
	d->u_running = 1;
	if (d->u_state & ALRM) {	/* alarm went off */
		d->u_state &= ~ALRM;	/* turn off flag */
		if (d->u_current)
			kbd_runtime(d);
	}
	while (mp = getq(q)) {
		if (mp->b_datap->db_type == M_DATA) {
			if (! canput(q->q_next)) {
				putbq(q, mp);
				d->u_running = 0;
				return(42);
			}
			else
				choo_choo(q, mp, d);
		}
		else
			putnext(q, mp);
	}
	d->u_running = 0;
	return(42);
}

/*
 * kbdioc	Completely handle one of our ioctl calls, including the
 *		qreply of the message.  Quickly dispose of swindlers,
 *		cutthroats, liars & knaves.
 *
 * A security note:  We limit most ioctl access to the uid that pushed
 * the module.  There's a covert channel in the query function: if you
 * query and get reference counts, the counts on public tables (as well
 * as their names) can be used to send information to other users.  It
 * can be easily taken care of by paring down the information that is
 * transmitted back from this call, or restricting it to private tables.
 * The names of public tables could constitute a pretty high-bandwidth
 * channel.
 */
static void
kbdioc(queue_t *q, mblk_t *mp, register struct iocblk *iocp,
       register struct kbdusr *d)	/* q is write queue pointer */
{
	register int type, sx;
	register struct kbduu *uu;
	register struct kbd_tab *t;

	uu = (struct kbduu *) q->q_ptr;
	/*
	 * This is a preliminary switch to catch all of the
	 * permission stuff first.
	 */
	switch (iocp->ioc_cmd) {
	case EUC_IXLOFF:
	case EUC_IXLON:
	case EUC_MSAVE:
	case EUC_MREST:
	case EUC_OXLOFF:
	case EUC_OXLON:
	case EUC_WSET:
	case EUC_WGET:
			break;	/* all the Good Guys */
	case KBD_LOAD:
	case KBD_CONT:
	case KBD_END:
	case KBD_UNLOAD:
	case KBD_ATTACH:
	case KBD_DETACH:
	case KBD_LINK:
	case KBD_HOTKEY:
	case KBD_ON:
	case KBD_OFF:
	case KBD_QUERY:
	case KBD_VERB:
	case KBD_LIST:
	case KBD_TSET:
	case KBD_EXT:
/*
 * FIX ME:
 * ON SVR4.0: there's an anomaly in "iocp->ioc_uid"; it's always 0!
 * Look at this again before release.  I've never been able to get
 * an ioctl message that had a NON-ZERO ioc_uid!!
 */
/* printf("KBD ioc: zu=%d,io=%d\n", uu->zuid, iocp->ioc_uid); */
			if (the_shining_one(iocp->ioc_uid) ||
			   (uu->zuid == iocp->ioc_uid)) {
				break;
			}
			else {	/* clam up & clamp down */
/* printf("KBD: Perm. denied on ioctl %d-%d\n",uu->zuid,iocp->ioc_uid); */
				iocp->ioc_error = EPERM;
				mp->b_datap->db_type = M_IOCNAK;
				iocp->ioc_rval = -1;
				qreply(q, mp);
				return;
			}
	default:
			break;
	}
	/*
	 * The real thing
	 */
	switch (iocp->ioc_cmd) {
	case EUC_IXLOFF:	/* Question: should we respond to EUC */
	case EUC_IXLON:		/* translation ioctls, or leave these */
	case EUC_OXLOFF:	/* first four calls alone?            */
	case EUC_OXLON:		/* Are we a Translator or Converter?  */
	case EUC_MSAVE:		/* Is this a philosophical point?     */
	case EUC_MREST:		/* Perhaps it is.  I'm nervous but... */
	case EUC_WSET:		/* If we see SET/GET switches here,   */
	case EUC_WGET:		/* we're REALLY in deep trouble, but  */
				/* we ignore it and it will go away.  */
				/* Bon Voyage! We're now a Converter. */
			putnext(q, mp);
			return;
	case KBD_LOAD:
	case KBD_CONT:
	case KBD_END:
	case KBD_UNLOAD:
			kbdload(q, mp, iocp);
			return;
#if 0	/* This is really a debugging call, not for production use. */
	case KBD_QUERY:
			/*
			 * At the moment, just give statistics
			 */
			if (the_shining_one(iocp->ioc_uid)) {
				type = KBD_QUERY;
				goto makelist;
			}
			else {
				iocp->ioc_error = EPERM;
				goto nakit;
			}
#endif
	/*
	 * Attach/detach errors:
	 *	Bad data format - EPROTO
	 *	Table not found (or not attached) - ENOENT
	 *	ATT: Table already attached - EMLINK
	 *	DET: Table is current - EMLINK (no longer an error)
	 *	No resources (tablinks) to attach table - ENOSR
	 *	Too many tables attached - EBUSY
	 */
	case KBD_ATTACH:
			if ((mp->b_cont->b_wptr - mp->b_cont->b_rptr) !=
			    sizeof(struct kbd_tach)) {
				iocp->ioc_error = EPROTO;
				goto nakit;
			}
			{
				register struct kbd_tach *z;
				register struct tablink *tl;
				register int tcnt; /* count attached tabs */

				z = (struct kbd_tach *)mp->b_cont->b_rptr;
				t = kbdfind(z->t_table, uu);
				if (! t) {
					iocp->ioc_error = ENOENT;
					goto nakit;
				}
				if (z->t_type & Z_UP)
					d = uu->iside;	/* Hand waving */
				if (! d) {
					iocp->ioc_error = ENOSR;
					goto nakit;
				}
				tl = d->u_list;
				tcnt = 0;
				while (tl) { /* is table already attached? */
					++tcnt;
					if ((tl->l_me == t) ||
					    zzcompare(tl->l_me->t_name, t->t_name)) {
						/* Tilt! */
						iocp->ioc_error = EMLINK;
						goto nakit;
					}
					tl = tl->l_next;
				}
				/*
				 * If everything's OK, then see if we
				 * have enough elbow room...
				 */
				if (tcnt >= kbd_maxu) {
					iocp->ioc_error = EBUSY;
					goto nakit;
				}
/*
 * If a composite table, make an expanded "copy" for the attached list.
 * Composite copies also get tacked onto a user's quota (zumem).
 */

				if (t->t_flag == KBD_COT)
					t = kbdnewcomp(t, d);
				if (! (tl = grabtl(t))) {
				/* free copies of COT tables on failure! */
					if (t->t_flag == KBD_COT) {
						kbd_deref(t, R_TC);
						freetable(t);
					}
					iocp->ioc_error = ENOSR;
					goto nakit;
				}
				if (t->t_flag == KBD_COT) {
					uu->zumem += t->t_asize;
				}
				else if (t->t_flag & KBD_ALP) {
					t->t_alpfunc = (unsigned char *)
						alp_con(t->t_name,
						    (caddr_t *)(&(tl->l_alpid)));
				}
				tl->l_next = d->u_list;
				d->u_list = tl;
				++(t->t_ref);
				/*
				 * If no current and there's only ONE
				 * table attached, make it current.
				 */
				if (! d->u_current) {
					if (d->u_list && ! d->u_list->l_next)
						hotsw(d);
				}
				if ((t->t_flag & KBD_TIME) && (t->t_flag != KBD_COT))
					tl->l_time = d->u_time;
			}
			goto ackit;
	case KBD_DETACH:
			/*
			 * Find an attached thingy and return the
			 * tablink structure to the free list.  We
			 * don't, of course, unlink the table itself,
			 * just the user attachment.
			 */
			if ((mp->b_cont->b_wptr - mp->b_cont->b_rptr) !=
			    sizeof(struct kbd_tach)) {
				iocp->ioc_error = EPROTO;
				goto nakit;
			}
			{
				register struct kbd_tach *z;
				register struct tablink *tl, *ttmp;
				register mblk_t *svp;

				z = (struct kbd_tach *)mp->b_cont->b_rptr;
				if (z->t_type & Z_UP)
					d = uu->iside;	/* Hand waving */
				tl = kbd_attached(z->t_table, d);
				if (! tl) {
					iocp->ioc_error = ENOENT;
					goto nakit;
				}
				/*
				 * disconnect & flush if it's an
				 * external function
				 */
				if ((t->t_flag != KBD_COT) &&
				    (t->t_flag & KBD_ALP)) {
					if (svp = alp_discon(t->t_name,
							(caddr_t)tl->l_alpid))
						putnext(RD(q), svp);
					tl->l_alpid = (struct tablink *) 0;
				}
				if (tl->l_me == d->u_current) { /* Uh-oh */
/*
 * Allow this one to be detached.  We just act as if we got the hot
 * key, if one's set and it's a toggle-off mode; otherwise, force the
 * thing to NOT be the current table.  Annoyed users requested this
 * change, which makes it much easier to undo mistakes.
 */
					if (d->u_hot && d->u_hkm)
						hotsw(d);
					else {	/* do it the hard way */
						d->u_current = (struct kbd_tab *)0;
						d->u_cp = (struct tablink *) 0;
						d->u_ocp = d->u_cp;
					}
					if (d == uu->iside && d->u_str[0])
						kbdverb(q, d);
				}
				ttmp = d->u_list;
				if (ttmp == tl) { /* immediate satisfaction */
					STR_LOCK_SPL(sx);
					--(tl->l_me->t_ref);
					d->u_list = tl->l_next;
					if (tl->l_me->t_flag == KBD_COT) {
						uu->zumem -= tl->l_me->t_asize;
						kbd_deref(tl->l_me, R_TC);
						freetable(tl->l_me);
					}
					tl->l_me = ZIP;
				/*
				 * Return a tablink to free list
				 */
					tl->l_next = tlinkl;
					tlinkl = tl;
					STR_UNLOCK_SPL(sx);
				}
				/*
				 * ...dig up the predecessor,
				 * link it to the next one & send this one
				 * back to the dungeon.
				 */
				else {
					while (ttmp) {
					    if (ttmp->l_next == tl) {
						STR_LOCK_SPL(sx);
						--(tl->l_me->t_ref);
						ttmp->l_next = tl->l_next;
						if (tl->l_me->t_flag == KBD_COT) {
							uu->zumem -= tl->l_me->t_asize;
							kbd_deref(tl->l_me, R_TC);
							freetable(tl->l_me);
						}
						tl->l_me = ZIP;
						tl->l_next = tlinkl;
						tlinkl = tl;
						STR_UNLOCK_SPL(sx);
						break;
					    }
					    ttmp = ttmp->l_next;
					}
				}
			}
			goto ackit;
	case KBD_TGET:
			{
			register struct kbd_ctl *cx;

			if ((! mp->b_cont) || (iocp->ioc_count != sizeof(struct kbd_ctl))) {
				iocp->ioc_error = EPROTO;
				goto nakit;
			}
			cx = (struct kbd_ctl *) mp->b_cont->b_rptr;
			cx->c_type = uu->iside->u_time;
			cx->c_arg = uu->oside->u_time;
			}
			goto ackit;
	case KBD_TSET:	/*
			 * Set new timer value.
			 */
			{
				register struct kbd_ctl *cx;
#if 0	/* only if the #if 0 below is used */
				register struct tablink *tl, *tc;
#endif

				if ((! mp->b_cont) || (iocp->ioc_count != sizeof(struct kbd_ctl))) {
					iocp->ioc_error = EDOM;
					goto nakit;
				}
				cx = (struct kbd_ctl *)mp->b_cont->b_rptr;
				if (cx->c_type & Z_UP)
					d = uu->iside;
				d->u_time = (unsigned short) cx->c_arg;
				if (d->u_time < MINTV)
					d->u_time = MINTV;
				if (d->u_time > MAXTV)
					d->u_time = MAXTV;
#if 0	/* This code can be used to reset ALL attached tables. */
	/* It is not currently used, as we opted for doing only */
	/* subsequently attached tables. */
				tl = d->u_list;
				while (tl) {
					if (tl->l_me->t_flag == KBD_COT) {
						tc = tl->l_me->t_child;
						while (tc) {
							if (tc->l_time)
								tc->l_time = d->u_time;
							tc = tc->l_next;
						}
					}
					else {
						if (tl->l_time)
							d->u_time;
					}
					tl = tl->l_next;
				}
#endif /* 0 */
			}
			goto ackit;
	case KBD_HOTKEY:
			/*
			 * Adjust the hotkeys.
			 */
			{
			struct kbd_ctl *cx;
			mblk_t *Paul, *Moses; /* Mixed mode */

			if (iocp->ioc_count != sizeof(struct kbd_ctl)) {
				iocp->ioc_error = EPROTO;
nakit:
				mp->b_datap->db_type = M_IOCNAK;
				iocp->ioc_rval = -1;
				qreply(q, mp);
				return;
			}
			cx = (struct kbd_ctl *)mp->b_cont->b_rptr;
			if (cx->c_type & Z_UP)
				d = uu->iside;	/* more hand waving */
			if (cx->c_type & Z_SET) {
				d->u_hot = (unsigned char)(cx->c_arg & 0x00FF);
				d->u_hkm = (unsigned char) ((cx->c_arg & 0xFF00) >> 8);
				if (d->u_hot)
					d->u_state |= HASHOT;
				else
					d->u_state &= ~HASHOT;
				/*
				 * Spread the good news to outlying
				 * organizations: when the hot-key
				 * changes, we send M_CTL both up and
				 * downstream to inform neighbors.
				 * currently, this is unused.
				 */
				if (Paul = copymsg(mp)) {
					Paul->b_datap->db_type = M_CTL;
					/* Prophetable cloning operation */
					if (Moses = copymsg(Paul))
						putnext(OTHERQ(q), Moses);
					putnext(q, Paul);
				}
			}
			else {
				cx->c_arg = d->u_hot;
				cx->c_arg |= (d->u_hkm << 8);
				iocp->ioc_count = sizeof(struct kbd_ctl);
			}
ackit:
			mp->b_datap->db_type = M_IOCACK;
			iocp->ioc_rval = iocp->ioc_error = 0;
			qreply(q, mp);
			}
			break;

	case KBD_ON:
			if (iocp->ioc_count != sizeof(struct kbd_ctl)) {
				iocp->ioc_error = EPROTO;
				goto nakit;
			}
			{
				struct kbd_ctl *cx;

				cx = (struct kbd_ctl *) mp->b_cont->b_rptr;
				if (cx->c_type & Z_UP)
					d = uu->iside;
				d->u_doing = 1;
			}
			goto ackit;
	case KBD_OFF:
			if (iocp->ioc_count != sizeof(struct kbd_ctl)) {
				iocp->ioc_error = EPROTO;
				goto nakit;
			}
			{
				struct kbd_ctl *cx;

				cx = (struct kbd_ctl *) mp->b_cont->b_rptr;
				if (cx->c_type & Z_UP)
					d = uu->iside;
				d->u_doing = 0;
			}
			goto ackit;
	case KBD_VERB:
			if ((! mp->b_cont) || (iocp->ioc_count > KBDVL)) {
				iocp->ioc_error = EPROTO;
				goto nakit;
			}
			d = uu->iside;
			{
				register unsigned char *s, *t;

				/* copy in the verbose message */
				s = mp->b_cont->b_rptr;
				*(s + (iocp->ioc_count - 1)) = '\0';
				t = d->u_str;
				while (*s)
					*t++ = *s++;
				*t = '\0';
				/* then do it immediately */
				if (d->u_str[0])
					kbdverb(q, d);
			}
			goto ackit;
	case KBD_LIST:
			type = KBD_LIST;
#if 0
makelist:
#endif
			if (! mp->b_cont) {
				iocp->ioc_error = EPROTO;
				goto nakit;
			}
			if (iocp->ioc_error = kbdlist(q, mp->b_cont, type))
				goto nakit;

			if (type == KBD_QUERY) /* from KBD_QUERY above */
				iocp->ioc_count = 0;
			else
				iocp->ioc_count = sizeof(struct kbd_query);
			goto ackit;
	case KBD_LINK:
			if (! kbdlinktab(mp, iocp, uu))
				goto nakit;	/* kbdlinktab sets ioc_error */
			goto ackit;
	case KBD_EXT:
			/* This makes a reference to an ALP function */
			if ((! mp->b_cont) || (iocp->ioc_count < 1)) {
				iocp->ioc_error = EPROTO;
				goto nakit;
			}
			if (( ~(*mp->b_cont->b_rptr++) & 0x0F) != Z_PUBLIC) {
				iocp->ioc_error = EPROTO;
				goto nakit;
			}
			if (*mp->b_cont->b_rptr++ != 0x96) {
				iocp->ioc_error = EPROTO;
				goto nakit;
			}
			/*
			 * Create if none, else just get it.  If can't
			 * do either, then nak it.
			 */
			iocp->ioc_count -= 2;
			if (! kbd_newalp(mp->b_cont->b_rptr, uu)) {
				iocp->ioc_error = ENOENT;
				goto nakit;
			}
			goto ackit;
	default:
			/*
			 * Red alert!  Someone is taking pot-shots at
			 * the tty subsystem.  Chicken out and let
			 * somebody else blast the perpetrator.
			 */
			putnext(q, mp);
	}
	return;
}

/*
 * grabtl(t)	Grab a "new" tablink structure and attach it to table "t".
 *		Belts & suspenders puh-leez while mucking with the
 *		free list.  (This is grabbing a tablink structure from
 *		the free list.)  If called without a "t" pointer, it can
 *		be used to prime the freelist.
 */

static struct tablink *
grabtl(register struct kbd_tab *t)
{
	register struct tablink *tl;
	register int sx, i;
	register struct tablink *newl;

	STR_LOCK_SPL(sx);
	tl = tlinkl;
	if (! tl) {	/* prime the freelist */
		newl = (struct tablink *) MemRi_Matic(NTLINKS *
						sizeof(struct tablink));
		if (! newl) {
			STR_UNLOCK_SPL(sx);
			return((struct tablink *) 0);
		}
		for (i = 0; i < NTLINKS; i++) {
			newl[i].l_next =
				(struct tablink *) &(newl[i+1]);
		}
		newl[NTLINKS - 1].l_next = (struct tablink *) 0;
		tlinkl = newl;
		tl = tlinkl;
	}
	if (! t || ! tl) {
		STR_UNLOCK_SPL(sx);
		return((struct tablink *) 0);
	}
	tlinkl = tlinkl->l_next;
	STR_UNLOCK_SPL(sx);
	tl->l_me = t;
	tl->l_state = IDLE;
	tl->l_node = 0;
	tl->l_time = 0;
	tl->l_lbolt = 0;
	tl->l_tid = 0;
	tl->l_next = (struct tablink *) 0;
	tl->l_ptr = &(tl->l_msg[0]);
	return(tl);
}

/*
 * Send part of an existing message to "where".  Used to flush
 * out the beginning parts of messages when we've discovered something
 * we're interested in, and there were things before it.  We copy
 * whatever is before, then reset the rptr on the message itself.
 * In pre-pipelining days, it used to "send" it on the Stream.  Nowdays,
 * the thing could probably be re-written to work in concert with
 * "kbdresult" to try to keep messages big & unfragmented.  For most
 * applications, the user is slower than the CPU so it's not a big deal.
 */

static void
kbdsend(register mblk_t *mp, register unsigned char *rp,
	register mblk_t **where)
{
	register unsigned char *c;
	register mblk_t *up;

	if (rp >= mp->b_wptr)	/* nothing to send */
		return;
	if (! (up = allocb(rp - mp->b_rptr, BPRI_HI)))
		return;		/* nothin' to do to save the day... */
	c = mp->b_rptr;
	while (c < rp)
		*up->b_wptr++ = *c++;
	if (*where)		/* link it at the "output place" */
		linkb(*where, up);
	else
		*where = up;
	mp->b_rptr = rp;
}

/*
 * Initialize a kbdusr structure with default values.
 */
static int
kbdfill(register struct kbdusr *d)
{
	register int i;

	d->u_current = ZIP;
	d->u_list = d->u_cp = d->u_ocp = (struct tablink *) 0;
	d->u_time = (unsigned short) kbd_timer;
	for (i = 0; i < KBDVL; i++)
		d->u_str[i] = 0;
	d->u_cp = (struct tablink *) 0;
	d->u_doing = '\0';	/* need ioctl to turn on */
	d->u_hot = '\0';
	d->u_hkm = 1;	/* default to list + non-mapped */
	d->u_running = 0;
	return(1);	/* no sweat */
}

/*
 * Free a table structure.  Figure out how big it is.  We don't
 * adjust the pools, the caller has to do that.
 */

static void
freetable(struct kbd_tab *t)
{
	kmem_free(t, t->t_asize);
			/* this is why we have a "size" field...! */
}

/*
 * kbd_deref	Decrement reference counts on all elements of a composite.
 *		This is done prior to freeing the table.
 */

static void
kbd_deref(register struct kbd_tab *t, int type)
{
	register struct kbd_tab **tb;
	register struct tablink *tl;
	register int i;

	if (t->t_flag != KBD_COT)
		return;	/* safety valve */
	if (type == R_TC) {
		tl = t->t_child;
		while (tl) {
			--(tl->l_me->t_ref);
			tl = tl->l_next;
		}
	}
	else {
		tb = (struct kbd_tab **) t->t_child;
		for (i = 0; i < (int) t->t_nodes; i++) {
			--((*tb)->t_ref);
			++tb;
		}
	}
}

/*
 * Allocate a new table structure to hold a table of the given
 * size.  Fill some fields (esp. the name, ref count).
 */

static struct kbd_tab *
newtable(struct kbd_load *ld)
{
	register int siz, i;
	register struct kbd_tab *t;

	siz = ld->z_tabsize + ld->z_onesize + ld->z_nodesize + ld->z_textsize;
	if ((siz < sizeof(struct kbd_tab)) || (siz > KBDTMAX))
		return(ZIP);
	/*
	 * The capability for "downloadable tables of arbitrary size"
	 * requires dynamic allocation.  This used to be done from a
	 * small static pool, not it's done via kmem_alloc.
	 */
	if (t = (struct kbd_tab *) kmem_alloc(siz, KM_NOSLEEP)) {
		t->t_nodes = t->t_text = t->t_flag = 0;
		t->t_ref = 0;
		t->t_next = ZIP;
		t->t_nodep = (struct cornode *)
				((unchar *) t + sizeof(struct kbd_tab));
		t->t_oneone = (unchar *) 0;
		for (i = 0; i < KBDNL; i++)
			t->t_name[i] = ld->z_name[i];
		t->t_name[KBDNL-1] = '\0'; /* ward off evil spirits */
		t->t_asize = siz;
		/* printf("Newtable %s, size:%d (t->t_asize=%d)\n",
			ld->z_name, siz, t->t_asize); */
	}
	return((struct kbd_tab *) t);
}

/*
 * kbdload	Handle the LOAD, CONT, END ioctls.  LOAD and CONT have
 *		attached data blocks up to 256 bytes.  END has an
 *		attached structure that tells us whether to make the
 *		table private or public.  Z_PUBLIC is only usable by
 *		root to make the table public.
 *	q == write queue
 */

static void
kbdload(queue_t *q, register mblk_t *mp, register struct iocblk *iocp)
{
	register struct kbd_tab *t;
	register struct kbduu *uu;
	struct kbd_ctl *c;
	struct kbd_load *ld;

	iocp->ioc_rval = 0;
	uu = (struct kbduu *) q->q_ptr;
	/*
	 * Common loader errors here.  General permission errors are
	 * caught by the caller.
	 *
	 * Lack of data, bad format, bad LOAD/CONT order (EPROTO)
	 * Out of public or private space (ENOSPC)
	 * Alloc failed (ENOMEM)
	 * END or CONT without prior LOAD (EFAULT)
	 * CONT: loading something bigger than originally declared (E2BIG)
	 * END: smaller than declared originally (EINVAL)
	 * Table already exists by that name (EEXIST)
	 * Smaller than a breadbox or larger than a whale (EFBIG)
	 *
	 * Secret Incantation Failure (EXDEV) - We're sorry, the
	 *	number you have reached is not a working number...
	 */
	switch (iocp->ioc_cmd) {
	case KBD_UNLOAD:
		if ((! mp->b_cont)||(*(ALIGNER *)mp->b_cont->b_rptr != 0x6361)) {
			iocp->ioc_error = EXDEV;
			goto badioc;
		}
		else {
			mp->b_cont->b_rptr += ALIGNMENT;
			iocp->ioc_count -= ALIGNMENT;
		}
		if (iocp->ioc_count != sizeof(struct kbd_tach)) {
			iocp->ioc_error = EPROTO;
			goto badioc;
		}
		/*
		 * Try to unload some merchandise.  If no good,
		 * nak it.  kbd_unload did the error setting.
		 */
		if (! kbd_unload(uu, mp, iocp))
			goto badioc;
		break;
	case KBD_LOAD:
		if (! mp->b_cont || *(ALIGNER *)mp->b_cont->b_rptr != 0x3305) {
			iocp->ioc_error = EXDEV;
			goto badioc;
		}
		else {
			mp->b_cont->b_rptr += ALIGNMENT;
			iocp->ioc_count -= ALIGNMENT;
		}
		if (uu->lpos) {	/* Already occupied...a drastic error, folks */
			iocp->ioc_error = EPROTO;
			goto badioc;
		}
		if (iocp->ioc_count == sizeof(struct kbd_load)) {
			register int siz;

			ld = (struct kbd_load *) mp->b_cont->b_rptr;
			siz = ld->z_tabsize + ld->z_onesize +
				ld->z_nodesize + ld->z_textsize;
			if (kbd2big(uu->zumem, siz, (int) iocp->ioc_uid)) {
				iocp->ioc_error = EFBIG;
				goto badioc;
			}
			if (kbdfind(ld->z_name, uu)) {
				iocp->ioc_error = EEXIST;
				goto badioc;
			}
			if (! (t = newtable(ld))) {
				iocp->ioc_error = ENOMEM;
				goto badioc;
			}
			uu->ltmp = t;
			uu->lpos = (unchar *) t;
			uu->lsize = siz;
			uu->lalsz = t->t_asize;	/* save for later */
		}
		else {
			iocp->ioc_error = EPROTO; /* Apples & Oranges */
			goto badioc;
		}
		break;
	case KBD_CONT:
		if (! mp->b_cont || (*(ALIGNER *)mp->b_cont->b_rptr != 0x3680)) {
			iocp->ioc_error = EXDEV;	/* 86 it. */
			goto badioc;
		}
		else {
			mp->b_cont->b_rptr += ALIGNMENT;
			iocp->ioc_count -= ALIGNMENT;
		}
		t = uu->ltmp;
		if (! t) {
			iocp->ioc_error = EFAULT; /* Surely you're joking. */
			goto badioc;
		}
		/*
		 * Copy data from mp->b_cont->b_rptr to q->q_ptr->lpos,
		 * and increment lpos past the added data.  We check for
		 * legitimate size here as well, to insure that our cup
		 * not runneth over.
		 */
		{
			register unchar *s, *r, *lim;
			register int siz, tmp;

			siz = uu->lsize;
			r = uu->lpos;
			s = mp->b_cont->b_rptr;
			lim = mp->b_cont->b_wptr;
			/*
			 * Check for overflow of the table.  Prevents a
			 * user from declaring a breadbox, and actually
			 * trying load a whale.
			 */
			tmp = lim - s;
			if (tmp > siz) {
				iocp->ioc_error = E2BIG;
				goto badioc;
			}
			uu->lpos += tmp;
			uu->lsize -= tmp;
			while (s < lim)
				*r++ = *s++;
			freemsg(mp->b_cont);
			mp->b_cont = NULL;
		}
		break;
	case KBD_END:
		if (iocp->ioc_count == sizeof(struct kbd_ctl)) {
			c = (struct kbd_ctl *) mp->b_cont->b_rptr;
			if (c->c_arg != 0x2212) {
				iocp->ioc_error = EXDEV;
				goto badioc;
			}
			t = ((struct kbduu *)q->q_ptr)->ltmp;
			if (! t) { /* I've already heard that one */
				iocp->ioc_error = EFAULT;
				goto badioc;
			}
			/*
			 * Sanity check: did the user declare a whale
			 * and then load a breadbox?
			 */
			if (uu->lsize) {
				iocp->ioc_error = EINVAL;
				goto badioc;
			}
			c = (struct kbd_ctl *)mp->b_cont->b_rptr;
			tabfill(t);
/*
 * SECURITY:
 * Scrutinize the minutest details of this thing to make sure it's not
 * an attempt to subvert the system.  If it IS a covert attempt, blast
 * it.  In practice, this sequence of "putctl1" messages on frisk failure
 * results in both the calling program and the user's shell dying (i.e.,
 * the user gets logged out).  We are intentionally nasty here, because
 * (1) the table is probably a trap for someone, (2) the compiler won't
 * produce a table that fails the frisk, which means it's been "doctored".
 * If we actually USED a "bad" table it could crash the system.
 */

			if (frisk(t)) {
				putctl1(RD(q)->q_next, M_PCSIG, SIGQUIT);
				putctl1(RD(q)->q_next, M_PCSIG, SIGKILL);
				iocp->ioc_error = EPERM;
				goto badioc;
			}
			/*
			 * We have to restore the size of the allocation,
			 * because the table was completely over-written
			 * by the download.  There's some "extra" work
			 * being done by "newtable()" that can be FIX MEd
			 * the next time around.
			 */
			t->t_asize = uu->lalsz;
			if ((c->c_type == Z_PUBLIC) && the_shining_one(iocp->ioc_uid))
				kbd_publink(t);
			else	/* force privatization, like it or not */
				kbd_prvlink(t, uu);
			uu->ltmp = ZIP;
			uu->lpos = 0;
		}
		else {
			iocp->ioc_error = EPROTO;
			goto badioc;
		}
		break;
	default:	/*
			 * Severe Reality Failure.  This REALLY
			 * can't happen in a consistent universe.
			 */
		panic("KBD: Severe Reality Failure.\n");
		iocp->ioc_error = EINVAL;
		/* "fall thru after panic..." */
badioc:
		if (uu->ltmp) {
			kmem_free((caddr_t) uu->ltmp, (size_t) (uu->ltmp->t_asize));
			uu->ltmp = ZIP;
			uu->lpos = 0;
		}
		iocp->ioc_rval = -1;
		mp->b_datap->db_type = M_IOCNAK;
		qreply(q, mp);
		return;	/* don't fall through into the slimy pit! */
	}
	mp->b_datap->db_type = M_IOCACK;
	iocp->ioc_rval = 0;
	iocp->ioc_count = 0;
	qreply(q, mp);
}

/*
 * `Liberate' the goods in a `delicate daylight operation'.
 * Errors are:
 *		EFENCE - no fence; 86 'em.
 *		EMLINK - table is being used (returned both for
 *			simple & compound tables).
 *		ENOENT - no goods is no good.
 *		ESRCH - Say what?  We found it once, but then can't
 *			find it again...the plot thickens...
 *		EPERM - Non-root attempt to unload public table.
 */

static int
kbd_unload(register struct kbduu *uu, register mblk_t *mp,
	  register struct iocblk *iocp)
{
	register struct kbd_tach *tc;
	register struct kbd_tab *t, *tmp;
	
	tc = (struct kbd_tach *)mp->b_cont->b_rptr;
	tc->t_table[KBDNL-1] = '\0'; /* Stomp On Saboteurs */
	t = kbdfind(&(tc->t_table[0]), uu);
	if (! t) {
		iocp->ioc_error = ENOENT;
		return(0);
	}
	if (t->t_ref != 0) {
		iocp->ioc_error = EMLINK;
		return(0);
	}
	/*
	 * Check private list, then public list.  A note:  When a user
	 * has a composite table, it will be found in the "private"
	 * list, and then just unloaded.  Any "public" copy will not
	 * be unloaded.
	 */
	if (uu->u_link) {	/* any private list? */
		tmp = uu->u_link;
		if (tmp == t) {	/* first one */
			uu->u_link = t->t_next;
			if (tmp->t_flag == KBD_COT)
				kbd_deref(tmp, R_AV);
			uu->zumem -= tmp->t_asize;
			freetable(tmp);
			return(1);
		}
		else {
			while (tmp) {
				if (tmp->t_next == t) {
					tmp->t_next = t->t_next;
					if (tmp->t_flag == KBD_COT)
						kbd_deref(tmp, R_AV);
					uu->zumem -= tmp->t_asize;
					freetable(t);
					return(1);
				}
				tmp = tmp->t_next;
			}
		}
	}
	/*
	 * check public list
	 */
	tmp = tables;
	if (tmp == t) {	/* first one */
		if (! the_shining_one(iocp->ioc_uid)) {
			iocp->ioc_error = EPERM;
			return(0);
		}
		tables = t->t_next;
		if (tmp->t_flag == KBD_COT)
			kbd_deref(tmp, R_AV);
		freetable(tmp);
		return(1);
	}
	else {
		while (tmp) {
			if (! the_shining_one(iocp->ioc_uid)) {
				iocp->ioc_error = EPERM;
				return(0);
			}
			if (tmp->t_next == t) {
				tmp->t_next = t->t_next;
				if (tmp->t_flag == KBD_COT)
					kbd_deref(tmp, R_AV);
				freetable(t);
				return(1);
			}
			tmp = tmp->t_next;
		}
	}
	iocp->ioc_error = ESRCH;
	return(0);	/* eh? */
}

/*
 * kbdlinktab makes "composite" tables.  It takes in the data portion
 * a 2-byte indicator for public/private link table.  (If public, then
 * user must be root.)  Following this is a string that is the x:y,z
 * string of names to manufacture.  The first component (before the
 * colon) is the terminal name, the rest (comma-separated) are the
 * components.
 *
 * Errors:
 *	EPROTO - no data or bad format (no indicator in first byte, or
 *		 fewer than 2 tables being linked).
 *	EPERM  - not root & request Z_PUBLIC.
 *	ENOMEM - can't alloc table or get other memory.
 *	EEXIST - table of final name already exists.
 *	EINVAL - one or more arguments don't exist.
 *	E2BIG  - too many names linked (limit is arbitrarily KBDLNX).
 *	EFAULT - attempt to link a composite table. (Someday this may
 *		 be allowed...it would involve expanding the composite
 *		 "in line" with the rest of the elements)
 *	EACCES - request was public and a member was found to be private.
 *		 (a public composite cannot contain private members.)
 */

static struct kbd_tab *
kbdlinktab(mblk_t *mp, struct iocblk *iocp, struct kbduu *uu)
{
	unsigned char *list[KBDLNX+2];
	struct kbd_tab *tabs[KBDLNX+2];
	register int i, j;
	register unsigned char *s;
	register struct kbd_tab **tb;
	register int type;	/* public or private */
	struct kbd_load ld;	/* for faking newtable() */

	if ((! mp->b_cont) || (iocp->ioc_count < 1)) {
		iocp->ioc_error = EPROTO;
		return(ZIP);
	}
	s = mp->b_cont->b_rptr;
	*(s + iocp->ioc_count) = '\0';	/* Safety! */
	/*
	 * Permission checks.  If not specified it's a protocol error;
	 * if non-root tries PUBLIC it's a permission error, but is
	 * forced to private status rather than being rejected.
	 */
	type = ~(*s++) & 0x0F;	/* keep bottom bits only */
	if ((type != (unsigned char) Z_PUBLIC) &&
	    (type != (unsigned char) Z_PRIVATE)) {
		iocp->ioc_error = EPROTO;
		return(ZIP);
	}
	if ((type == Z_PUBLIC) && (! the_shining_one(iocp->ioc_uid))) {
		/* only root can make a public tree... */
		type = Z_PRIVATE;	/* force off */
	}
	if (*s++ != 0x69) {
		iocp->ioc_error = EXDEV;
		return(ZIP);
	}
	list[0] = s;	/* first component */
	while (*s && *s != ':')
		++s;
	if (*s != ':') {
		iocp->ioc_error = EPROTO;	/* no ":" */
		return(ZIP);
	}
	*s++ = '\0';	/* skip over colon */
	i = 1;
	while (*s) {
		list[i] = s;
		while (*s && (*s != ','))
			++s;
		if (*s == ',')
			*s++ = '\0';
		++i;
		if (i >= KBDLNX) {
			iocp->ioc_error = E2BIG;
			return(ZIP);
		}
	}
	/*
	 * Now, "i" is the NUMBER of strings including terminal name.
	 * (i.e., the number of children + 1).
	 */
	if (i < 3) {	/* enough names (need final + at least 2 args) */
		iocp->ioc_error = EINVAL;
		return(ZIP);
	}
	for (j = 0; j < i; j++) {	/* any null names? */
		if (! *list[j]) {
			iocp->ioc_error = EINVAL;
			return(ZIP);
		}
	}
	if (tabs[0] = kbdfind(list[0], uu)) {	/* target exists? */
		iocp->ioc_error = EEXIST;
		return(ZIP);
	} 
	/*
	 * Check for (1) table existence, (2) simple table, (3) table type.
	 * Must be (1) found, (2) a simple table, (3) if request is for
	 * public, all members must be public; if request if for private,
	 * then members can be private or public.
	 */
	for (j = 1; j < i; j++) {
		tabs[j] = kbdfind(list[j], uu);
		if (! tabs[j]) {
#if 0
/*
 * Future consideration:
 * Check here with "kbd_newalp" to see if it's external.  This hook is
 * for "auto" loading of externals into composites.  I haven't decided
 * whether I want this hook or not...maybe I'll reserve it for "root"?
 * If the hook were here, then one could link an external routine into
 * a composite just by NAMING it from user level!  Would this pollute
 * the name space?
 */
			if (the_shining_one(iocp->ioc_uid)) {
				if (! (tabs[j] = kbd_newalp(list[j], uu))) {
					iocp->ioc_error = EINVAL;
					return(ZIP);
				}
			}
#else
/* no auto-loading of externals into composites */
			iocp->ioc_error = EINVAL; /* not found! */
			return(ZIP);
#endif
		}
		if (tabs[j]->t_flag == KBD_COT) { /* a composite */
			iocp->ioc_error = EFAULT;
			return(ZIP);
		}
		if ((kbd_ttype(tabs[j], uu->u_link) != Z_PUBLIC) &&
					(type == Z_PUBLIC)) {
			iocp->ioc_error = EACCES;
			return(ZIP);
		}
	}
	tabs[i] = ZIP;	/* null end of list */
	s = list[0];	/* make ld struct for newtable */
	j = 0;
	while ((ld.z_name[j++] = *s++) && (j < KBDNL))
		;
	ld.z_name[KBDNL-1] = '\0';
	ld.z_tabsize = sizeof(struct kbd_tab);
	ld.z_onesize = ld.z_nodesize = 0;
	/* alloc nchildren + 1 pointers: */
	ld.z_textsize = i * sizeof(struct kbd_tab *);
	/* note...newtable really sets alloced size in t_asize field. */
	if (kbd2big(uu->zumem, ld.z_tabsize + ld.z_textsize, iocp->ioc_uid)) {
		iocp->ioc_error = EFBIG;
		return(ZIP);
	}
	if (! (tabs[0] = newtable((struct kbd_load *) &ld))) {
		iocp->ioc_error = ENOMEM;
		return(ZIP);
	}
	/*
	 * Now that we got a table, fill in the blanks.  In a composite
	 * when it's on the "available" list "t_textp", "t_nodep" and
	 * "t_child" all point at an ARRAY of kbd_tab pointers (stored just
	 * after the kbd_tab).  When the user attaches one of these,
	 * then a bunch of tablink structures will be alloced, and the
	 * pointers copied into the tablinks and state set up properly.
	 */
	tabs[0]->t_textp = (unsigned char *) tabs[0]->t_nodep;
	tabs[0]->t_child = (struct tablink *) tabs[0]->t_textp;
	tabs[0]->t_flag = KBD_COT;
	tb = (struct kbd_tab **) tabs[0]->t_child;
	for (j = 1; j < i; j++) {
		*tb = tabs[j];
		++(tabs[j]->t_ref); /* increment ref count of each element */
		++tb;
	}
	*tb = ZIP;	/* null terminate list */
	tabs[0]->t_nodes = i - 1; /* t_nodes is number of tablinks later. */
	/*
	 * Now, link this thing somewhere!
	 */
	if (type == Z_PUBLIC)	/* permissions checked above already */
		kbd_publink(tabs[0]);
	else
		kbd_prvlink(tabs[0], uu);
	return(tabs[0]); /* return number of tables involved as members */
}

/*
 * kbd_publink	Link a table to the FRONT of the public table list.
 */
static void
kbd_publink(struct kbd_tab *t)
{
	register int sx;

	STR_LOCK_SPL(sx);
	if (tables)
		t->t_next = tables;
	tables = t;
	STR_UNLOCK_SPL(sx);
}

/*
 * kbd_prvlink	Link a table to the front of some user's private list.
 */

static void
kbd_prvlink(struct kbd_tab *t, struct kbduu *uu)
{
	register int sx, tmp;

	STR_LOCK_SPL(sx);
	t->t_next = uu->u_link;
	uu->u_link = t;
	tmp = t->t_asize;
	uu->zumem += tmp;
	STR_UNLOCK_SPL(sx);
}

/*
 * kbdnewcomp	Make a new composite table.  Take a pointer to a
 *		composite table on the "available" list and make
 *		a new table that's a copy of the kbd_tab struct,
 *		does NOT contain the "Text" part, and contains a
 *		bunch of tablinks for it.  Set state to IDLE.
 *		(Note: Attaching a composite will never cause the
 *		quota to be exceeded.)
 */

static struct kbd_tab *
kbdnewcomp(struct kbd_tab *t, struct kbdusr *d)
{
	struct kbd_load ld;
	register unsigned char *s;
	register int i;
	register struct kbd_tab *new, **tb;
	register struct tablink *tl;

	if (t->t_flag != KBD_COT)
		return(t);
	s = &(t->t_name[0]);
	i = 0;
	/*
	 * copy name & make load structure
	 */
	while ((ld.z_name[i++] = *s++) && (i < KBDNL))
		;
	ld.z_name[KBDNL-1] = '\0';
	ld.z_tabsize = sizeof(struct kbd_tab);
	ld.z_onesize = ld.z_nodesize = 0;
	ld.z_textsize = t->t_nodes * sizeof(struct tablink);
	if (! (new = newtable((struct kbd_load *) &ld))) {
		return(ZIP);
	}
	new->t_flag = KBD_COT;
	new->t_textp = (unsigned char *) new->t_nodep;
	tl = (struct tablink *) new->t_nodep;
	new->t_ref = 0;
	new->t_child = tl;
	tb = (struct kbd_tab **) t->t_child;	/* &(t->t_child[0]) */
	new->t_nodes = t->t_nodes;
	for (i = 0; i < (int) t->t_nodes; i++) {
		tl->l_me = *tb;
		++((*tb)->t_ref); /* increment ref count of each element */
		if ((*tb)->t_flag & KBD_TIME)
			tl->l_time = d->u_time; /* set timer if needed */
		tl->l_next = tl+1;
		tl->l_ptr = (unsigned char *) &(tl->l_msg[0]);
		tl->l_node = 0;
		tl->l_state = IDLE;
		++tl; ++tb;
	}
	--tl; /* because the loop ++ed it past last element */
	tl->l_next = (struct tablink *) 0;
	return(new);
}

/*
 * Allocate a new kbd_tab for the given external function (in "s").
 * Make it public.  Can be used also to "auto-create" tables for
 * all loaded external functions.  (See the note above about
 * possibly auto-linking externals into composites.)  If the thing
 * already exists, it is just returned.
 */

static struct kbd_tab *
kbd_newalp(register unsigned char *s, struct kbduu *uu)
{
	register struct algo *a;
	register struct kbd_tab *t;

	if (t = kbdfind(s, uu))	/* already exists */
		return(t);
	if (a = alp_query(s)) {	/* if there is one by this name... */
		if (! (t = MemRi_Matic(sizeof(struct kbd_tab))))
			return(ZIP);
		t->t_asize = sizeof(struct kbd_tab);
		t->t_flag = KBD_ALP;
		t->t_alp = (struct cornode *) a;
		strcpy((char *)t->t_name, (char *)s);	/* SVR4.0 only! */
		t->t_alpfunc = (unsigned char *) a->al_func;
		t->t_nodes = t->t_text = t->t_error = 0;
		t->t_ref = 0;
		kbd_publink(t);
		return(t);
	}
	return(ZIP);
}

/*
 * tabfill	Fills in the "in-core" fields of a kbd_tab structure.
 *		Only for "simple" tables.
 */

static void
tabfill(register struct kbd_tab *t)
{
	register unchar *cp;

	cp = (unchar *) t;
	cp += sizeof(struct kbd_tab);
	if (t->t_flag & KBD_ONE) {
		t->t_oneone = cp;
		cp += 256;	/* size of oneone table */
	}
	else
		t->t_oneone = (unchar *) 0;
	t->t_nodep = (struct cornode *) cp;
	cp += (t->t_nodes * sizeof(struct cornode));
	t->t_textp = cp;
	t->t_ref = 0;
}

/*
 * kbdlist returns a "kbd_query" structure, based on seq and type.
 * If it can't go "seq" down in the "type"th list, it returns EAGAIN.
 *	queue_t *q;     write-side queue
 *	mblk_t *mp;	the cont block
 *	int type;	KBD_QUERY or KBD_LIST
 */

static int
kbdlist(queue_t *q, mblk_t *mp, int type)
{
	register struct kbd_tab *t;
	register struct kbduu *uu;
	register struct kbd_query *uqs;	/* user's query struct */

	uu = (struct kbduu *)q->q_ptr;
	switch (type) {
	case KBD_QUERY:	/* Debugging ioctls: DON'T leave them in. */
#if 0	/* for debugging */
		querytabs(uu);
#endif
		return(0);
	case KBD_LIST:
		uqs = (struct kbd_query *) mp->b_rptr;
		if (uqs->q_flag & KBD_QF_PUB)
			t = tables;
		else
			t = uu->u_link;
		while (uqs->q_seq && t) {	/* go "seq" times */
			t = t->t_next;		/* or until no next */
			--uqs->q_seq;
		}
		if (! t)			/* can't go? end of list */
			return(EAGAIN);
		kbdlaux(t, uu, uqs);
		return(0);
	default:
		panic("KBD: unreachable case.\n");
		return(EAGAIN);	/* Anzen Dai-ichi !!! */
	}
}

/*
 * Auxiliary list routine.  copy info into kbd_query for one table.
 */

static void
kbdlaux(register struct kbd_tab *t, register struct kbduu *uu,
	register struct kbd_query *uqs)
{
	register int i;
	register struct kbd_tab **tb;

	for (i = 0; i < KBDNL; i++)
		uqs->q_name[i] = t->t_name[i];
	uqs->q_id = (long) t;
	uqs->q_ref = t->t_ref;
	uqs->q_asize = t->t_asize;
	uqs->q_flag = 0;
	if (t->t_flag == KBD_COT) {
		uqs->q_flag |= KBD_QF_COT;
		uqs->q_nchild = t->t_nodes;
		/*
		 * We are only doing tables on the "available" list,
		 * not "attached" tables; therefore, we "know" that
		 * t_child is an array of pointers to the tables...
		 */
		tb = (struct kbd_tab **) t->t_child;
		for (i = 0; i < uqs->q_nchild; i++) {
			uqs->q_child[i] = (long) *tb;
			if ((*tb)->t_flag & KBD_TIME)
				uqs->q_chtim[i] = 1;
			else
				uqs->q_chtim[i] = 0;
			++tb;
		}
	}
	else {
		uqs->q_nchild = 0;
		if (t->t_flag & KBD_TIME)
			uqs->q_flag |= KBD_QF_TIM;
		if (t->t_flag & KBD_ALP)
			uqs->q_flag |= KBD_QF_EXT;
	}
	if (kbd_ttype(t, uu->u_link) == Z_PUBLIC)
		uqs->q_flag |= KBD_QF_PUB;
	uqs->q_tach = 0;
	if (uu->iside && kbd_attached(t->t_name, uu->iside))
		uqs->q_tach |= Z_UP;
	if (uu->oside && kbd_attached(t->t_name, uu->oside))
		uqs->q_tach |= Z_DOWN;
	if (uu->iside)
		uqs->q_hkin = uu->iside->u_hot;
	else
		uqs->q_hkin = '\0';
	if (uu->oside)
		uqs->q_hkout = uu->oside->u_hot;
	else
		uqs->q_hkout = '\0';
}

/*
 * Do one-one keyboard mapping on the message; change the message
 * in-place, don't copy it.
 */

static void
kbdone(mblk_t *mp, unsigned char *s)
{
	unsigned char *r, *w;

	do {
		r = mp->b_rptr;
		w = mp->b_wptr;
		while (r < w) {
			*r = s[*r];
			++r;
		}
		mp = mp->b_cont;
	} while (mp);
}

/*
 * Is it reasonable to even try to load this thing?  If it's bigger than
 * the max table or smaller than the header, it's bad.  If a normal user
 * would exceed quota, it's too big.
 */

static int
kbd2big(register int umem, register int siz, register int id)
{
	/*
	 * If smaller than a breadbox or larger than a whale,
	 * don't even try.
	 */
	if ((siz > KBDTMAX) || (siz <= sizeof(struct kbd_tab))) {
		return 1;	/* bad size */
	}
	if (! the_shining_one(id)) {
		if ((umem + siz) > kbd_umem)
			return 1;	/* no space left to load it */
	}
	return 0;	/* Baby Bear's porridge is just right... */
}

/*
 * kbdfind	Find out if there is a table by the given name that
 *		is accessible to this user.  We first check all the
 *		private tables the user has in core, then check the
 *		public list of tables.  If we find one, return its
 *		address.
 */
static struct kbd_tab *
kbdfind(register unsigned char *name, register struct kbduu *uu)
{
	register struct kbd_tab *t;

	/*
	 * Check private before public.  Shovel shovel dig dig...
	 */
	t = uu->u_link;
	while (t) {
		if (zzcompare(name, t->t_name)) {
			return(t);
		}
		t = t->t_next;
	}
	t = tables;
	while (t) {
		if (zzcompare(name, t->t_name)) {
			return(t);
		}
		t = t->t_next;
	}
	return(ZIP);	/* No banana for Washoe */
}

/*
 * Look in a user's list of currently attached tables and see if
 * the named table is attached.  If so, return the address of the
 * tablink structure that points to it.  We compare by NAME for
 * a couple of reasons: (1) checking for attachments during LOAD
 * ioctl we only get the name, (2) when checking attachments for
 * the LIST ioctl, we need to check by name, because it may be
 * a composite table, and the "attached" table will NOT have the
 * same address as the "available" composite (see kbdlinktab() and
 * kbdnewcomp()).
 */

static struct tablink *
kbd_attached(register unsigned char *name, struct kbdusr *d)
{
	register struct tablink *tl;

	tl = d->u_list;
	while (tl) {
		if (zzcompare(name, tl->l_me->t_name))
			return(tl);	/* Bread substitute */
		tl = tl->l_next;
	}
	return((struct tablink *) 0);	/* Icky plastic coating */
}

/*
 * Compare two strings.  Return 1 if match, else 0 if no match.
 */
static int
zzcompare(register unsigned char *x, register unsigned char *y)
{
	while (*x == *y) {
		if (*x == '\0') return 1; /* Peas in a pod */
		++x; ++y;
	}
	return 0;
}

/*
 * Routines to allocate the kbdusr structures for different sides of
 * the module.  Take a pointer to one of either "iside" or "oside" to
 * put a pointer to the new structure in.  The double indirection
 * saves having two separate functions, one for each side.
 */
static struct kbdusr *
new_side(register struct kbdusr **addr)
{
	*addr = (struct kbdusr *) Chunk_o_Mem();
	if (*addr)
		kbdfill(*addr);
	return(*addr);
}

/*
 * Return Z_PRIVATE or Z_PUBLIC depending on where the table
 * (ut) is found.  Check private list first.  We just check to
 * see if the table (ut) is the same as one on a list.  This is
 * called on tables retrieved via "kbdfind()". "ut" is the table,
 * "re" is the list.
 */

static int
kbd_ttype(register struct kbd_tab *ut, register struct kbd_tab *re)
{
	while (re) {	/* check private first */
		if (ut == re)	/* medieval humor */
			return(Z_PRIVATE);
		re = re->t_next;
	}
	re = tables;
	while (re) {	/* then check public */
		if (ut == re)
			return(Z_PUBLIC);
		re = re->t_next;
	}
	return(mi_contra_fa);	/* pointedly erroneous */
}

/*
 * RESULTNODE is true for nodes that are a result.  Their "child" pointers
 * point to text space offset, rather than to the next level in the tree.
 */
#define RESULTNODE(Q)	(n[Q].c_flag & ND_RESULT)
/*
 * LASTNODE is true for nodes that are the last node at their level.
 * They signal the end of processing for a certain tree level.  (They
 * may, of course, also be result nodes.)
 */
#define LASTNODE(Q)	(n[Q].c_flag & ND_LAST)
/*
 * INLINENODE is true when the "result" (a one-byte string) has been
 * hoisted in-line; that is, "child" IS the result.  We can save some
 * space, etc. with this.
 */
#define INLINENODE(Q)	(n[Q].c_flag & ND_INLINE)
/*
 * A match is found when the current pointer is equal to the value
 * of the node we're looking at.
 */
#define MATCHNODE(Laurel, Hardy)	(Laurel == n[Hardy].c_val)
/*
 * This is what kbdproc does to "put" what it produces:
 * Usually, it's linkb(*out, mp) or *out = mp.  It's also used
 * by the subsidiary routine kbdresult (below).
 */
#define PROCPUT(x, y) if (x) linkb(x, y); else x = y;

/*
 * kbdproc
 *
 *	This is where we do mysterious things.  First, we check for one-one
 *	mapping and get that out of the way.  Then we check for HOT keys in
 *	the "not-translating" state (i.e., no current table), which will allow
 *	us to switch into a "translating" state.  In the translating state,
 *	we check the whole message for hot keys (if idle), or if we're in
 *	the "search" state, we check each byte against the current level
 *	in the table.  When we find a match of the current byte to something
 *	at the current level then we either (1) jump to the next level, if
 *	it's NOT a result node, or (2) send up the result.  The code is
 *	somewhat contorted trying to cover all the bases.  Inside the
 *	outermost "while" loop, there are basically two things.  One
 *	is if we have NO table, the other is if we HAVE a table.  Inside
 *	the "no table" block, we just loop again looking for something to
 *	turn us on (this process is called "thrill seeking").  If we find
 *	anything, we break back out to the outer loop, otherwise, we can
 *	dispose of the message very quickly.
 *
 *	This function should really be broken up into some sub-functions
 *	it's way too long & complicated.  ("But it's 'fast'...")
 *
 *	Return value is 0 if we switched tables, else non-zero.  If we
 *	switch tables, we have to run the rest of the message on the
 *	new table (if any).  If switch to "no table", then return the
 *	message in "out" (caller will put it).
 *	queue_t *q;
 *	register mblk_t *mp;		 mp, but don't need queue 
 *	register struct kbdusr *d;	 iside or oside pointer 
 *	register struct tablink *sta;	 tablink of current table 
 *	register mblk_t **out;		 where to put results 
 *	int pos;  pos tells position in the composite "stack"; 0= bottom
 */
static int
kbdproc(queue_t *q, register mblk_t *mp, register struct kbdusr *d,
	register struct tablink *sta, register mblk_t **out, int pos)
{
	register unsigned char *rp;
	register int j;
	register struct cornode *n;
	register struct kbd_tab *t;
	register mblk_t *svp;	/* if there are any registers left... */
#if 0
	/*
	 * Some debugging stuff for the big wheel.
	 */
	register int ntimes; ntimes = 0;
#endif

	t = (sta ? sta->l_me : ZIP);
	/*
	 * If it's an external algorithm, run it.  The stuff around
	 * "t->t_alpfunc" is casting a cornode pointer (t_oneone, alias
	 * "t_alpfunc") into a pointer to a function returning a pointer
	 * to an mblk_t...and then calling it with two arguments.
	 */
	if (t && (t->t_flag & KBD_ALP)) {
		/*
		 * If we're checking for hotkeys then:
		 */
		if ((pos == 0) && (d->u_hot)) {
			if (mp->b_cont)
				pullupmsg(mp, -1); /* cat together */
			rp = mp->b_rptr;
			while (rp < mp->b_wptr) {
				if (*rp == d->u_hot) {
					/*
					 * Split the message in 2 pieces:
					 * process "svp", leave "mp".
					 */
					svp = buf_to_msg(mp->b_rptr, rp);
					mp->b_rptr = ++rp; /* after hotkey! */
					if (svp) {
/*
 * The first call sends stuff BEFORE the hot key to the algorithm;
 * the second makes sure it's flushed completely, because we're switching
 * off of it.  If the state is not currently synchronized, there will
 * be some "blips" leftover...
 */
						if (svp = (*(AF_CAST t->t_alpfunc))(svp, (caddr_t)sta->l_alpid))
							PROCPUT(*out, svp);

						if (svp = (*(AF_CAST t->t_alpfunc))((mblk_t *) 0, (caddr_t)sta->l_alpid))
							PROCPUT(*out, svp);
					}
					hotsw(d);
					if (d->u_str[0])
						kbdverb(q, d);
					return(0);	/* switched */
				}
				++rp;
			}
		}
		if (mp = (*AF_CAST t->t_alpfunc)(mp, (caddr_t)sta->l_alpid))
			PROCPUT(*out, mp);
		return(1);
	}

	if (t && (t->t_flag & KBD_ONE)) { /* do any one-one swapping FIRST */
		kbdone(mp, t->t_oneone);
	}
again_sam:
	rp = mp->b_rptr;
	for (;;) {		/* Snidely Whiplash, heh heh heh */
#if 0
  ++ntimes;
  if (ntimes > 4097) {
    printf("mp = %x, rp = %x, mp->b_rptr = %x\n", mp, rp, mp->b_rptr);
    panic("KBD: infinite loop panic.\n");
  }
#endif
		/*
		 * See if we have looked at a block without doing
		 * anything to it.  If we have, unlink & ship it.
		 */
		if (rp >= mp->b_wptr) {
			svp = mp; /* unlink & toss little fish back */
			mp = unlinkb(mp);
			if (rp > svp->b_rptr) {	/* Eat the evidence */
				PROCPUT(*out, svp);
			}
			else
				freeb(svp);
			if (! mp)
				return(1);
			rp = mp->b_rptr;
		}
		/*
		 * If no table, look for hot keys (if user defined a
		 * hot key).
		 */
		if (! t) {	/* not xlating: start now */
			if (d->u_hot) {
				while (rp < mp->b_wptr) {
					if (*rp++ == d->u_hot) {
						hotsw(d);
						/*
						 * Ship up all before this.
						 */
						if (rp > mp->b_rptr &&
						    rp < mp->b_wptr) {
							kbdsend(mp, rp - 1, out);
						}
						mp->b_rptr = rp;
						/*
						 * In verbose mode, inform the
						 * user of the new table.
						 */
						if (d->u_str[0])
							kbdverb(q, d);
						return(0);
					}
				}
				/*
				 * We just ran through whole block and saw
				 * nothing interesting.  Unlink if linked,
				 * else just send it on its merry way.
				 */
				if (! mp->b_cont) {
					PROCPUT(*out, mp);
					return(1);
				}
				else {
					svp = mp;
					mp = unlinkb(mp);
					PROCPUT(*out, svp);
					if (! mp)
						return(1);
					goto again_sam;
				}
				
			}
			else {	/*
				 * You're probably wondering why I'm here!
				 * No hot key, no table, no dice.  This is
				 * really a dead end street.
				 */
				PROCPUT(*out, mp);
				return(1);
			}
		}
if (! sta) {
cmn_err(CE_WARN, "UX:KBD: 'kbdproc' without state\n");
return(1);
}
		j = sta->l_node;
		if (sta->l_state & IDLE) {
			/*
			 * Check for hot key before anything else.  Toggle
			 * to next or "off" depending on mode.  Only the
			 * lowermost table (! pos) checks for hot keys
			 * here.  If we don't have a table, then hot keys
			 * are checked above.
			 */
			if (! pos) {
				if (d->u_hot && (*rp == d->u_hot)) {
					++rp;	/* save the day */
					++mp->b_rptr;
					hotsw(d);
					if (d->u_str[0])
						kbdverb(q, d);
					return(0);
				}
			}
			/*
			 * Trap sneaky operators...some tables may not
			 * have any nodes...!  This keeps us from dredging
			 * up icky glop from random addresses...
			 */
			if (! t->t_nodes) {
				++rp;
				continue;
			}
			/*
			 * This checks the current table.  We start at the
			 * top node (0) of the table and look straight
			 * through nodes until we find a correspondence,
			 * or until we run out of nodes (the ND_LAST bit
			 * of c_flag is set).
			 */
			n = t->t_nodep;
/*
 * If a "full table", do by index operation, otherwise, use the do loop.
 * In a full table, the first (tmax-tmin) nodes will be filled with "root"
 * stuff.  We'll index, and if we come up with an "ND_INLINE" node, then
 * we'll replace the contents we have with what's in "child".  If we have
 * something that's outside of tmin/tmax boundaries, it will never fit, so
 * just continue immediately.  If we have a NON-inline node, and it's in
 * bounds, then it MUST be the "top" of a subtree, so when we "goto
 * doit_doit", we'll have a match immediately.  Could be somewhat more
 * optimal in where it goes from here (i.e., we have a match, so goto
 * INSIDE the next do loop).
 */
			if (t->t_flag & KBD_FULL) {
				if ((*rp > t->t_max) || (*rp < t->t_min)) {
					/* no chance to map */
					++rp;
					continue; /* still idle */
				}
				j = (int) (*rp - t->t_min); /* re-use j */
/* DEBUGGING STUFF: this is never supposed to happen */
#if 0
if (! MATCHNODE(*rp, j)) {
	printf("UX:KBD: fatal node mismatch (%c, %c)\n",
		*rp, n[j].c_val);
}
#endif
/* END DEBUG */
				if (! INLINENODE(j)) {
					--j; /* because of "++j" below */
					goto doit_doit;
				}
				else {
					/* do it here */
					*rp++ = (unsigned char) n[j].c_child;
					continue;
				}
			}
			j = (-1);	/* see the other do loop */
		doit_doit:
			do {
				++j;
				if (MATCHNODE(*rp, j)) {
/*
 * Workin' on the chain gang.  This handles "new style" one-one mappings,
 * where the child IS the result. We just replace the contents, then act as
 * if we haven't seen anything.  The result is that the char is replaced
 * by the result, and we just keep going: stay IDLE! NOTE: we could re-work
 * some of the code above so that we just do an index above then jump into
 * this loop at this point, because we "know" that we have a match.
 */
					if (INLINENODE(j)) {
						*rp = (unsigned char) n[j].c_child;
						break;	/* out of do...while */
					}
					if (rp > mp->b_rptr)
						kbdsend(mp, rp, out);
					++rp;
					++mp->b_rptr;
					if (RESULTNODE(j)) {
/*
 * Found a result that's a one-many mapping.
 * Send it, then play it again...
 */
						kbdresult((int) n[j].c_child, t->t_textp, out);
						sta->l_node = 0;
						sta->l_state |= IDLE;
						sta->l_state &= ~SRCH;
						goto idlebreak;
					}
					else {
/*
 * This is the ugly part.  We have to save up all the stuff that goes
 * into making up one of these input strings so we can spit it back
 * out if things gang agley.
 */
						sta->l_state &= ~IDLE;
						sta->l_state |= SRCH;
						sta->l_node = n[j].c_child;
						sta->l_ptr = &(sta->l_msg[0]);
						*sta->l_ptr++ = *(rp - 1);
						if (sta->l_time)
							kbd_setime(d, sta);
					}
					goto planB;
				}
			} while (! LASTNODE(j)); /* see next do loop */
			/*
			 * If we're still idle, check the rest...At this
			 * point, we have something at the beginning of
			 * the message and it doesn't match.  We have
			 * to conserve it, not toss it...which is why
			 * we use "rp".
			 */
			if (sta->l_state & IDLE) {
				++rp;
				continue;
			}
		}

planB:		/*
		 * Here's the crux move (5.10a):
		 * l_state & SRCH is set when we have seen the first of a
		 * sequence.  We look for subsequent bytes of the sequence.
		 * If we find a match, we put it in l_node.  If we have an
		 * actual result, we struck gold.  If we get to the end of
		 * the list at the current level, and we STILL haven't seen
		 * a match, then we bit the big one.  Luckily, we've saved
		 * the input sequence in l_msg, so we can reconstruct what
		 * happened!
		 */
		if ((sta->l_state & SRCH) && (rp < mp->b_wptr)) {
			register int oldglory; /* if we must have a flag */
			register mblk_t *tp;
			j = sta->l_node - 1;	/* you'll see why */
			n = t->t_nodep;
			oldglory = 0;
			do {
				++j;	/* sleight of hand */
				if (MATCHNODE(*rp, j)) {
					++oldglory;
					++rp;
					++mp->b_rptr;
					if (RESULTNODE(j)) {
					/*
					 * GOLD in them thar hills...
					 */
/*
 * Here, we have to catch ND_INLINE nodes also.  They are RESULT
 * nodes, but we don't need kbdresult().  Just decrement b_rptr,
 * stuff in the c_child contents, and go idle.  If we ever get
 * any other stuff, the IDLE code will notice that rp > b_rptr,
 * and send it upstream...we hope!
 */
						if (INLINENODE(j))
							*(--mp->b_rptr) = (unsigned char) n[j].c_child;
						else
							kbdresult((int) n[j].c_child, t->t_textp, out);
						sta->l_node = 0;
						sta->l_state |= IDLE;
						sta->l_state &= ~SRCH;
						if (sta->l_tid) {
							untimeout(sta->l_tid);
							sta->l_lbolt = 0;
							sta->l_tid = 0;
						}
						goto idlebreak;
					}
					else {
						sta->l_node = n[j].c_child;
						*sta->l_ptr++ = *(rp - 1);
						if (rp >= mp->b_wptr)
							goto idlebreak;
					}
				}
			} while (! LASTNODE(j)); /* that's why */
			/*
			 * Check the score. Oldglory tells us whether we're
			 * still in business or whether it's fool's gold.
			 */
			if (! oldglory) {
				/*
				 * Diabolus One, Clementine Nothin'.
				 * Reset the l_node counter and spit up the
				 * first byte of the original.  Leave the
				 * rest in the buffer by copying the
				 * sta->l_msg buffer and try it next
				 * as a top level entry (note: we never
				 * incremented rp after seeing that it
				 * didn't match, so it's still at "rp".
				 */
				sta->l_node = 0;
				sta->l_state |= IDLE;
				sta->l_state &= ~SRCH;
				if (sta->l_tid) {
					untimeout(sta->l_tid);
					sta->l_lbolt = 0;
					sta->l_tid = 0;
				}
				tp = buf_to_msg(sta->l_msg, sta->l_ptr);
				sta->l_ptr = &(sta->l_msg[0]);
/*
 * At this point "rp" is the byte that failed; tp holds all
 * the rest of the stuff.  We send the first byte of tp, then if there's
 * anything left, link "mp" onto the end of "tp", reset "mp" to point at
 * "tp" and loop.  This lets us check out everything EXCEPT the first
 * byte of the stuff that didn't match.  In the case where it's a 2-byte
 * sequence, the 2nd of which didn't match, "tp" will end up being empty,
 * so we chuck it, which leaves the current "rp" as-is, with the state
 * reset so we'll check it against the top-most level.  (Whew...)
 */
				if (tp) { /* TANSTAAFL! */
					pay_the_piper(t, *tp->b_rptr++, out);
/*
 * Save the byte that failed.  The rest of the stuff in "mp" had better
 * be duplicated in "tp"...
 */
					mp->b_rptr = rp;
					if (tp->b_rptr < tp->b_wptr) {
						tp->b_cont = mp;
						mp = tp;
					}
					else
						freemsg(tp);
				}
				goto again_sam;
			}
		}
	idlebreak:
		if (rp >= mp->b_wptr) {
			svp = mp; /* unlink & toss little fish back */
			mp = unlinkb(mp);
			if (rp > svp->b_rptr) {
				PROCPUT(*out, svp); /* Eat the evidence */
			}
			else
				freeb(svp);
			if (! mp)
				return(1);
			rp = mp->b_rptr;
		}
		/* round & round the big wheel goes... */
	}
	/* NOTREACHED */
	/*
	 * This code MUST be unreachable: if this statement
	 * is ever REACHED by the compiler, you have a bug in the
	 * Snidely Whiplash loop.
	 */
#if 0
	PROCPUT(*out, mp);
	return(42);	/* in a pinch */
#endif
}

/*
 * This is what it's all about.  Package up the prize in a nice
 * little box & ship it Special Delivery Postpaid.
 *	int off;	offset in table text
 */

static int
kbdresult(int off, unsigned char *textp, mblk_t **where)
{
	register unsigned char *p, *s;
	register mblk_t *mp;
	register int len;

	p = s = textp + off;
	len = 0;
	while (*s++)
		++len;
	if (! (mp = allocb(len, BPRI_HI))) {
		return 0;		/* Shock & Despair */
	}
	while (*p)
		*mp->b_wptr++ = *p++;
	PROCPUT(*where, mp);	/* Anti-Climax. */
	return 1;
}

/*
 * We need to know what kind of queue this is.  We send the
 * verbose message out the write queue side only.  Reset the
 * queue if it's the wrong side.  Realistically, people won't
 * be putting verbose messages on the wrong side.
 */

#define is_a_reader(x)	(x->q_flag & QREADR)	/* living dangerously */

static void
kbdverb(register queue_t *q, register struct kbdusr *d)
{
	register unchar *s, *t;
	register int len;
	register mblk_t *mp;
	register int chk;

	s = t = d->u_str;
	len = 0;
	while (*t++)	/* figure out max length of string to send */
		len++;
	if (d->u_current) {	/* don't index if none... */
		t = d->u_current->t_name;
		while (*t++)
			len++;
	}
	/*
	 * Try to get 128 bytes; if that fails, try for "len".
	 * Print up to the available buffer size only.
	 */
	if (! (mp = allocb(128, BPRI_MED))) {
		if (! (mp = allocb(len, BPRI_MED)))
			return;		/* fail mysteriously */
	}
	else
		len = 128;
	chk = 0;
	while (*s && chk < len) {
		if ((*s == '%') && (*(s+1) == 'n')) {
			/*
			 * If no table, don't print %n, leave it blank.
			 */
			if (d->u_current) {
				/* insert the table name for "%n" */
				t = d->u_current->t_name;
				while (*t) {
					*mp->b_wptr++ = *t++;
					++chk;
				}
			}
			s += 2;
		}
		else {
			*mp->b_wptr++ = *s++;
			++chk;
		}
	}
	if (is_a_reader(q))	/* Never send a verbose message on */
		q = WR(q);	/* the wrong side, use the write side */
	putnext(q, mp);
}

/*
 * Security check: frisk a table for illegal weapons.  We already know
 * the size had better match properly, so check ALL the nodes for proper
 * range, check the last node to make sure it has a last-node flag set,
 * check the last byte of text to make sure it's null.  If anything
 * fails, return POSITIVE.  If it's clean, return 0.
 *
 * The printf calls that are commented out are for debugging the table
 * compiler.  If uncommented, they'll show where the table failed
 * security check so hopefully the compiler problem can be traced.
 */

static int
frisk(register struct kbd_tab *t)
{
	register unsigned short i, bound, tx;
	register struct cornode *n;

	n = t->t_nodep;
	bound = (unsigned short) t->t_nodes;
	tx = (unsigned short) t->t_text;
	for (i = 0; i < bound; i++) {
		/*
		 * for "text pointer" type nodes:
		 */
		if (! (n[i].c_flag & ND_INLINE)) {
			/*
			 * If not a result and child is not in node bounds
			 * then fail.
			 */
			if ((!(n[i].c_flag & ND_RESULT)) && n[i].c_child >= bound) {
/* printf("KBD: Frisk 1\n"); */
				return 1;  /* pointer node out of bounds */
			}
			/*
			 * If it is a result node and child out of TEXT
			 * bounds then fail it.
			 */
			if ((n[i].c_flag & ND_RESULT) && (n[i].c_child >= tx)){
/* printf("KBD: Frisk 2\n"); */
				return 1;  /* text pointer out of bounds */
			}
		}
		/*
		 * and for "inline result" type nodes:
		 */
		else {
			if (! (n[i].c_flag & ND_RESULT)) {
/* printf("KBD: Frisk 3\n"); */
				return 1;
			}
		}
		if (n[i].c_flag & ND_RESERVED) {
/* printf("KBD: Frisk 4\n"); */
			return 1;  /* illegal bits (later compiler ver.) */
		}
	}
	if (t->t_nodes &&
	    ! (n[t->t_nodes - 1].c_flag & ND_LAST)) {
/* printf("KBD: Frisk 5\n"); */
		return 1;	/* node list not terminated */
	}
	if (t->t_text &&
	    (*((char *) t->t_textp + t->t_text - 1))) {
/* printf("KBD: Frisk 6\n"); */
		return 1;	/* text not terminated */
	}
	return 0;
}

#if 0
/*
 * A traditional debugging ritual.
 */

static int
spitup(mp)

	mblk_t *mp;
{
	register unsigned char *p;

contin:
	p = mp->b_rptr;
	while (p < mp->b_wptr)
		printf(" %d", *p++);
	printf("\n");
	if (mp->b_cont) {
		printf("\tCONT:");
		mp = mp->b_cont;
		goto contin;
	}
}
#endif

/*
 * Switch on hot key, set current table.
 *
 * hot key modes:
 *
 * 0	Toggle through the list endlessly.  Never toggle to the
 *	off state.  E.g. tab1 tab2 tab1 tab2...
 *
 * 1	(Default) Toggle to the off state at the end of the table
 *	list.  After the off state, starts at top of list again.
 *	E.g.: tab1 tab2 off tab1...
 *
 * 2	If more than one table, toggles to the off state between
 *	every table, otherwise, behaves as mode 0.
 *	E.g. tab1 off tab2 off tab1...
 *
 * When we switch, we have to set the NEW table to "IDLE" so we don't
 * start up in the middle somewhere.
 */

static void
hotsw(struct kbdusr *d)
{
	struct tablink *enfant;

	if (! d->u_list)	/* no train, no tracks, no way... */
		return;
	switch (d->u_hkm) {
	default:
	case Z_HK0:
		if (! d->u_cp) {	/* if no current, set head */
			d->u_cp = d->u_list;
			d->u_current = d->u_list->l_me;
			d->u_ocp = (struct tablink *) 0;
			break;
		}
		if (d->u_cp->l_next) {	/* have one, next exists */
			d->u_ocp = d->u_cp;
			d->u_cp = d->u_cp->l_next;
			d->u_current = d->u_cp->l_me;
			break;
		}
		d->u_ocp = d->u_cp;	/* wrap */
		d->u_cp = d->u_list;
		d->u_current = d->u_cp->l_me;
		break;
	case Z_HK1:
		if (! d->u_cp) {	/* nothing now, set to list */
			d->u_cp = d->u_list;
			d->u_current = d->u_list->l_me;
			d->u_ocp = (struct tablink *) 0;
			break;
		}
		if (d->u_cp->l_next) {	/* working through list */
			d->u_current = d->u_cp->l_next->l_me;
			d->u_ocp = d->u_cp;
			d->u_cp = d->u_cp->l_next;
			break;
		}
		d->u_ocp = d->u_cp;	/* at end of list */
		d->u_current = ZIP;
		d->u_cp = (struct tablink *) 0;
		break;
	case Z_HK2:
		if (! d->u_current) {	/* turn on next */
			if (d->u_ocp && d->u_ocp->l_next)
				d->u_current = d->u_ocp->l_next->l_me;
			else
				d->u_current = ZIP;
			if (! d->u_current) {
				d->u_current = d->u_list->l_me;
				d->u_cp = d->u_list;
			}
			if (! d->u_ocp)
				d->u_ocp = d->u_cp;
		}
		else {	/* turn off current */
			d->u_ocp = d->u_cp;	/* save current */
			d->u_current = ZIP;
			d->u_cp = (struct tablink *) 0;
		}
		break;
	}
	/*
	 * If we switched onto a table, set the state to IDLE.
	 * For composites, set all the children to IDLE.
	 */
	if (d->u_cp) {
		d->u_cp->l_state = IDLE;
		if (d->u_current->t_flag == KBD_COT) {
			enfant = d->u_current->t_child;
			while (enfant) {
				enfant->l_state = IDLE;
				enfant = enfant->l_next;
			}
		}
	}
}

/*
 * The price we pay for having an "error" string.  If there's an
 * error string, send it, otherwise send the byte "c", because there's
 * nothing else to do except silently gobble it.  (It's better
 * to cheerfully spit it back now rather than risk ulcers later.)
 */
static void
pay_the_piper(register struct kbd_tab *t, unsigned char c, mblk_t **where)
{
	register mblk_t *mp;
	register int i;
	register unsigned char *s, *err;

	if (t->t_flag & KBD_ERR) {
		s = err = (t->t_textp + t->t_error);
		i = 0;
		while (*s++)	/* find length of error string */
			++i;
		if (mp = allocb(i, BPRI_MED)) {
			s = err;	/* again */
			while (*s)
				*mp->b_wptr++ = *s++;
		}
		else	/* Headline: Modest Piper Refuses Payment! */
			return;
	}
	else {
		if (mp = allocb(1, BPRI_MED))
			*mp->b_wptr++ = c;
		else
			return;	/* crushed beneath the wheel */
	}
	PROCPUT(*where, mp);
}

/*
 * Sigh.  Yet another... Take pointers into a buffer, alloc a message
 * and copy the buffer contents into the message.  There should be
 * a standard Streams routine for doing this...
 * (top-1 points to last byte)
 */
static mblk_t *
buf_to_msg(unsigned char *base, unsigned char *top)
{
	register mblk_t *mp;

	if (base >= top)
		return((mblk_t *) 0);
	if (! (mp = allocb((top - base), BPRI_HI)))
		return((mblk_t *) 0);
	while (base < top)
		*mp->b_wptr++ = *base++;
	return(mp);
}

/*
 * kbd_setime(d, tl)	Set a timer.  We keep track of what id got
 *			associated with what d and tl.
 */

static void
kbd_setime(register struct kbdusr *d, register struct tablink *tl)
{
	tl->l_lbolt = lbolt + tl->l_time;
	tl->l_tid = timeout(kbd_timeout, d, tl->l_time);
}
		
/*
 * kbd_timeout(i)	Called at splhi() by timeout.  Set a flag and
 *			schedule the queue to be run.
 */

static void
kbd_timeout(register struct kbdusr *d)
{
	d->u_state |= ALRM;
	qenable(d->u_q);
}

/*
 * Another debugging ritual.
 */
#if 0
static
querytabs(uu)
	struct kbduu *uu;
{
	register struct kbd_tab *t;

	if (t = tables) {
		printf("PUBLIC:\n");
		prinlist(t, (int) 'V');
	}
	if (t = uu->u_link) {
		printf("PRIVATE:\n");
		prinlist(t, (int) 'V');
	}
}

static
prinlist(t, type)
	register struct kbd_tab *t;
	int type;	/* type is aVailable or Attached */
{
	register struct tablink *tl;
	register struct kbd_tab **tb;
	register int i;

	while (t) {
		printf("\t(%x) %s: nod:%d txt:%d flg:%x siz:%d nxt:%x\n",
			t, t->t_name, (int) t->t_nodes, (int) t->t_text,
			(int) t->t_flag, (int) t->t_asize, t->t_next);
		if (t->t_flag == KBD_COT && type == 'A') {
			printf("\t\t\tA/chld(%d):\t", (int) t->t_nodes);
			tl = t->t_child;
			while (tl) {
				printf("(%x)", tl->l_me);
				printf("%s ", tl->l_me->t_name);
				tl = tl->l_next;
			}
			printf("\n");
		}
		else if (t->t_flag == KBD_COT && type == 'V') {
			printf("\t\t\tV/chld(%d):\t", (int) t->t_nodes);
			tb = (struct kbd_tab **) t->t_child;
			for (i = 0 ; i < (int) t->t_nodes; i++) {
				printf("[%x] ", *tb);
				++tb;
			}
			printf("\n");
		}
		t = t->t_next;
	}
}
#endif	/* end of debugging ritual */
