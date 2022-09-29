
#include "mbox.h"
#include "ulocks.h"
#include "stdlib.h"
#include "memory.h"
#include "sys/prctl.h"

/*
 * Mailbox
 */
#define NMSGS 50
struct mbox {
	usema_t *full;
	int fwaiting;
	int isfull;
	usema_t *empty;
	int ewaiting;
	usema_t *reply;
	mesg_t *msgs[NMSGS];
	int rmsg;
	int wmsg;
	int nodeup;
	ulock_t lock;
};
static usptr_t *mus;

void
initmbox(char *f)
{
	if ((mus = usinit(f)) == NULL) 
		abort();
}

struct mbox *
allocmbox(void)
{
	struct mbox *mbox;

	mbox = usmalloc(sizeof(*mbox), mus);
	mbox->full = usnewsema(mus, 0);
	mbox->isfull = 0;
	mbox->empty = usnewsema(mus, 0);
	mbox->lock = usnewlock(mus);
	mbox->reply = usnewsema(mus, 0);
	mbox->rmsg = mbox->wmsg = 0;
	mbox->ewaiting = mbox->fwaiting = 0;
	return mbox;
}

void
freembox(struct mbox *mb)
{
	usfreesema(mb->empty, mus);
	usfreesema(mb->full, mus);
	usfreesema(mb->reply, mus);
	usfreelock(mb->lock, mus);
	usfree(mb, mus);
}

/*
 * allocate a mesg
 */
mesg_t *
getmesg(void)
{
	mesg_t *a;
	a = usmalloc(sizeof(mesg_t), mus);
	a->flags = 0;
	return a;
}

void
freemesg(mesg_t *a)
{
	usfree(a, mus);
}

/*
 * write into a mailbox
 */
int
writembox(struct mbox *mb, mesg_t *msg)
{
	if (mb->nodeup == 0)
		return ENOSEND;

	ussetlock(mb->lock);
	while (mb->isfull) {
		/* full */
		mb->fwaiting++;
		usunsetlock(mb->lock);
		uspsema(mb->full);
		ussetlock(mb->lock);
	}
	mb->msgs[mb->wmsg] = msg;
	if (++mb->wmsg >= NMSGS)
		mb->wmsg = 0;
	if (mb->wmsg == mb->rmsg)
		mb->isfull = 1;
	if (mb->ewaiting) {
		usvsema(mb->empty);
		mb->ewaiting--;
	}
	usunsetlock(mb->lock);
	return 0;
}

int
readmbox(struct mbox *mb, mesg_t **msg)
{
	ussetlock(mb->lock);
	while (mb->rmsg == mb->wmsg && !mb->isfull) {
		/* empty */
		mb->ewaiting++;
		usunsetlock(mb->lock);
		uspsema(mb->empty);
		ussetlock(mb->lock);
	}
	*msg = mb->msgs[mb->rmsg];
	if (++mb->rmsg >= NMSGS)
		mb->rmsg = 0;
	if (mb->fwaiting) {
		usvsema(mb->full);
		mb->fwaiting--;
	}
	usunsetlock(mb->lock);
	return 0;
}

/*
 * RPC to a mailbox
 */
int
callmbox(struct mbox *mb, mesg_t *mesg)
{
	mesg->flags = MESG_RPC;
	if (writembox(mb, mesg))
		return ENOSEND;
	for (;;) {
		uspsema(mb->reply);
		if (mesg->flags & MESG_DONE)
			break;
		usvsema(mb->reply);
	}
	return (mesg->flags & MESG_ERROR) ? ENOREPLY : 0;
}

/*
 * reply to an RPC
 */
void
replymbox(struct mbox *mb, mesg_t *mesg)
{
	assert(mesg->flags & MESG_RPC);
	mesg->flags |= MESG_DONE;
	usvsema(mb->reply);
}

void
freemboxmsg(void *m)
{
	usfree(m, mus);
}

static struct mbox *nodes[64];

struct mbox *
ntombox(int nodenum)
{
	return nodes[nodenum];
}

void
setmbox(int nodenum, struct mbox *mb)
{
	nodes[nodenum] = mb;
	mb->nodeup = 1;
}

/*
 * XXX how do we reclaim mbox memory??
 */
void
nodedownmbox(int nodenum)
{
	struct mbox *mb;

	if (mb = nodes[nodenum]) {
		mb->nodeup = 0;
		ussetlock(mb->lock);
		while (mb->ewaiting) {
			usvsema(mb->empty);
			mb->ewaiting--;
		}
		usunsetlock(mb->lock);
	}
}
