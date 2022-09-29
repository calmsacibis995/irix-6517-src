/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)uts-comm:io/alp/alp.c	1.2" 		*/

#ident "$Revision: 1.8 $"

/*
 * Algorithm Pool "Registrar" module.  Leave some hooks here for
 * implementing "user-level" algorithms.  Unfortunately, not this
 * time around.
 */

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/errno.h>
#include <sys/param.h>
/* #include <io/ldterm/eucioctl.h> */	/* EUC ioctl calls */
#include <sys/alp.h>
#include <sys/systm.h>
#include <sys/kmem.h>

#define MemRi_Matic()	kmem_zalloc(sizeof(struct algo), KM_NOSLEEP)
#define Free_a_Chunk(Q)	kmem_free(Q, sizeof(struct algo));

static struct module_info alpminfo = { 0, "alp", 0, INFPSZ, 1024, 1024 };

static int alpopen(queue_t *, dev_t *, int, int, struct cred *);
static int alp_compare(unsigned char *, unsigned char *);
static int alpclose(queue_t *, int, struct cred *);
static int alprput(queue_t *, mblk_t *);
static int alpwput(queue_t *, mblk_t *);
static void alpioc(queue_t *, mblk_t *);

#define EQUAL(x, y)	alp_compare(x, y)

static struct qinit rinit = {
	alprput, NULL, alpopen, alpclose, NULL, &alpminfo, NULL
};
static struct qinit winit = {
	alpwput, NULL, NULL,    NULL,     NULL, &alpminfo, NULL
};
struct streamtab alpinfo = { &rinit, &winit, NULL, NULL };

static struct algo *algolist = (struct algo *) 0;

int alpdevflag = 0;

/*
 * The code for alp as a "module" is pretty straightforward.
 * It basically passes everything, and doesn't do anything
 * but the "query" ioctl for now.  Basic usage as a module
 * is just to push it, then query it, and pop it when the
 * list is exhausted.
 *	register queue_t *q;	pointer to read queue
 *	dev_t	dev;		major/minor device number
 *	int	flag;		file open flag -- zero for modules
 *	int	sflag;		stream open flag
 */
/* ARGSUSED */
static int
alpopen(register queue_t *q, dev_t *dev, int flag, int sflag, struct cred *notused)
{
	register struct algo *a;

	if (sflag != MODOPEN)
		return(EIO);
	if (q->q_ptr)
		return(0);	/* ok to re-open */

	if (a = (struct algo *)MemRi_Matic()) {
		q->q_ptr = (caddr_t) a;
		WR(q)->q_ptr = (caddr_t) a;
		return(0);
	}
	return(ENXIO);
}

/* ARGSUSED */
static int
alpclose(queue_t *q, int notused1, struct cred *notused2)
{
	if (q->q_ptr)
		Free_a_Chunk(q->q_ptr);
	q->q_ptr = NULL;
	WR(q)->q_ptr = NULL;

	return (0);
}

static int
alprput(queue_t *q, mblk_t *mp)
{
	putnext(q, mp);

	return (0);
}

static int
alpwput(queue_t *q, mblk_t *mp)
{
	if (mp->b_datap->db_type == M_IOCTL) {
		alpioc(q, mp);
		return (0);
	}
	putnext(q, mp);

	return (0);
}

/*
 * Handles the query ioctl call.
 */

static void
alpioc(queue_t *q, mblk_t *mp)
{
	register unsigned char *s, *t;
	register int i;
	register struct alp_q *uqs;
	register struct algo *a;
	register struct iocblk *iocp;

	/* process ioctl messages */
	iocp = (struct iocblk *)mp->b_rptr;
	switch (iocp->ioc_cmd) {
	case ALP_QUERY:
		if ((! mp->b_cont) || (iocp->ioc_count != sizeof(struct alp_q))) {
			iocp->ioc_error = EPROTO;
nakit:
			mp->b_datap->db_type = M_IOCNAK;
			iocp->ioc_rval = (-1);
			qreply(q, mp);
			return;
		}
		if (! (a = algolist)) {
			iocp->ioc_error = EAGAIN;
			goto nakit;
		}
		uqs = (struct alp_q *)mp->b_cont->b_rptr;
		while (uqs->a_seq-- && a)
			a = a->al_next;
		if (! a) {
			iocp->ioc_error = EAGAIN;
			goto nakit;
		}
		t = uqs->a_name;
		s = a->al_name;
		i = 0;
		/*
		 * These magic numbers are the same as the
		 * sizes in the "alp_q" structure.
		 */
		while ((*t++ = *s++) && (i++ < 16))
			;
		t = uqs->a_expl;
		s = a->al_expl;
		i = 0;
		while ((*t++ = *s++) && (i++ < 64))
			;
		uqs->a_flag = a->al_flag;
		iocp->ioc_rval = 0;
		mp->b_datap->db_type = M_IOCACK;
		qreply(q, mp);
		break;
	default:
		putnext(q, mp);
	}
}

/*
 * At system boot time, as each of the modules that want to
 * put themselves into the pool come in, this routine registers
 * them and then prints their names on the console.  The "init"
 * routines for each of the modules must call this in order to
 * be registered.  Of course, the "printf" could be removed.
 */

int
alp_register(struct algo *a)
{
	if (! a)
		return 1;
	a->al_flag = 0;		/* in-core */
	a->al_next = algolist;	/* link to front of list */
	algolist = a;
	printf("ALP register: %s\n", a->al_name);
	return 0;
}

/*
 *	Connection routine.  Returns a pointer to a function that
 *	returns a pointer to an mblk_t; the thing that is returned
 *	is a pointer to a function that takes two arguments (x, y)
 *	that are an mblk_t* and a caddr_t respectively.  Whew...
 *
 * bask in the heavenly light of strict typechecking...
 */
mblk_t *(*alp_con(unsigned char *name, caddr_t *id))(mblk_t *x, caddr_t y)
{
	struct algo *a;

	a = algolist;
	while (a) {
		if (EQUAL(a->al_name, name)) {
			*id = (*a->al_open)(1, 0);
			return(a->al_func);
		}
		a = a->al_next;
	}
	return(NULL);
}

/*
 * This routine can be called by other modules to see if
 * an algorithm by such-and-such a name is registered.
 * If the name is found, a POINTER to the information
 * structure (the entry in the algolist) is returned.
 * Warning: callers should not MODIFY the contents!
 */

struct algo *
alp_query(unsigned char *name)
{
	struct algo *a;

	a = algolist;
	while (a) {
		if (EQUAL(a->al_name, name))
			return(a);
		a = a->al_next;
	}
	return((struct algo *) 0);
}

/*
 * This is called by modules wishing to disconnect
 * from an instantiation.  It must be called by
 * modules that use ALP and have open connections,
 * whenever they are popped or otherwise closed.
 * To make sure things are flushed, it calls the
 * user-function with a ZERO message block pointer
 * and returns the function's return value to the
 * disconnector.
 */

mblk_t *
alp_discon(unsigned char *name, caddr_t id)
{
	struct algo *a;

	a = algolist;
	while (a) {
		if (EQUAL(a->al_name, name)) {
			return((*a->al_func)(0, id));
		}
		a = a->al_next;
	}
	return(NULL);	/* eh? */
}

/*
 * Return true if two strings are equal... basically
 * the same thing as "strcmp", but the return value
 * is ONE if they're equal, else ZERO.
 */

static int
alp_compare(unsigned char *x, unsigned char *y)
{
	if (x == y) return(1);
	for (; *x == *y; x++, y++) {
		if (!(*x))
			return(1);	/* equal */
	}
	return(0);	/* unequal */
}
