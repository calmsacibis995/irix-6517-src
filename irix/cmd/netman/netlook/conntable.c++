/*
 * Copyright 1989-1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Netlook Connection Table
 *
 *	$Revision: 1.6 $
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#include <time.h>
#include <gl/gl.h>
#include <macros.h>
#include "conntable.h"
#include "connection.h"
#include "netlook.h"
#include "node.h"
#include "network.h"

// XXX can this go away?
extern NetLook *netlook;

// Key for index
class ConnKey {
public:
	Node	*src;
	Node	*dst;
};

struct indexops ConnTable::indexOps = {
	(unsigned int (*)()) ConnTable::hash,
	(int (*)()) ConnTable::cmp,
	0 };


ConnTable::ConnTable(unsigned int count)
{
    in = index(count, sizeof(ConnKey), &indexOps);
}

Connection *
ConnTable::lookup(Node *s, Node *d)
{
    Connection *c;
    ConnKey k;
    k.src = s;
    k.dst = d;
    c = (Connection *) in_match(in, &k);
    if (c != 0)
	return c;
    k.src = d;
    k.dst = s;
    return (Connection *) in_match(in, &k);
}

Connection *
ConnTable::insert(Node *s, Node *d)
{
    ConnKey k;
    k.src = s;
    k.dst = d;

    Connection *c = new Connection;
    c->src.node = s;
    c->dst.node = d;
    s->connections++;
    s->getNetwork()->connections++;
    d->connections++;
    d->getNetwork()->connections++;

    return (Connection *) in_enter(in, &k, c)->ent_value;
}

Connection *
ConnTable::checkPick(void)
{
    static const unsigned int pickBufSize = 16;
    static const unsigned int pickExtent = 2;
    Entry **ep, *ent;
    int count = in->in_buckets;
    short pickBuf[pickBufSize];
    unsigned short n = 0;
    Rectf v = netlook->getViewPort();

    initnames();
    picksize(pickExtent, pickExtent);
    pick(pickBuf, pickBufSize);

    ortho(v.getX() + 0.5, v.getX() + v.getWidth() + 0.5,
	  v.getY() + 0.5, v.getY() + v.getHeight() + 0.5,
	  0.0, 65535.0);

    // This is only good for up to 2 ^ sizeof(short) connections.
    for (ep = in->in_hash; --count >= 0; ep++) {
	for (ent = *ep; ent != 0; ent = ent->ent_next) {
	    loadname(n++);
	    ((Connection *) ent->ent_value)->render();
	}
    }

    long writes = endpick(pickBuf);
    ortho(v.getX() + 0.5, v.getX() + v.getWidth() + 0.5,
	  v.getY() + 0.5, v.getY() + v.getHeight() + 0.5,
	  0.0, 65535.0);
    if (writes == 0 || pickBuf[0] == 0)
	return 0;
    n = pickBuf[1];
    count = in->in_buckets;
    for (ep = in->in_hash; --count >= 0; ep++) {
	for (ent = *ep; ent != 0; ent = ent->ent_next) {
	    if (n-- == 0)
		return (Connection *) ent->ent_value;
	}
    }
    return 0;
}

void
ConnTable::render(void)
{
    Entry **ep, *ent;
    int count = in->in_buckets;

    for (ep = in->in_hash; --count >= 0; ep++) {
	for (ent = *ep; ent != 0; ent = ent->ent_next)
	    ((Connection *) ent->ent_value)->render();
    }
}

void
ConnTable::rescale(void)
{
    Entry **ep, **lep, *ent;
    time_t t = time(0);
    int count = in->in_buckets;

    for (ep = in->in_hash; --count >= 0; ep++) {
	for (lep = ep, ent = *lep; ent != 0; ent = *lep) {
	    Connection *c = (Connection *) ent->ent_value;

	    if (netlook->getTrafficTimeout()
		&& (c->info & CONNPICKED) == 0) {
		time_t tt = MAX(c->src.timestamp, c->dst.timestamp)
			    + netlook->getTrafficTimeoutValue();
		if (tt < t) {
		    c->src.node->connections--;
		    c->src.node->getNetwork()->connections--;
		    c->dst.node->connections--;
		    c->dst.node->getNetwork()->connections--;
		    delete c;
		    in_delete(in, lep);
		    continue;
		}
	    }

	    /* Only drop intensity by one each rescale */
	    unsigned int oldsrc = c->src.intensity;
	    unsigned int olddst = c->dst.intensity;
	    c->calculate();
	    if (c->src.intensity < oldsrc)
		c->src.intensity = oldsrc - 1;
	    if (c->dst.intensity < olddst)
		c->dst.intensity = olddst - 1;

	    /* Reset values */
	    c->src.packets = c->src.bytes =
	    c->dst.packets = c->dst.bytes = 0;

	    lep = &ent->ent_next;
	}
    }
}

void
ConnTable::clear(void)
{
    Entry **ep, *ent;
    int count;

    count = in->in_buckets;
    for (ep = in->in_hash; --count >= 0; ep++) {
	while ((ent = *ep) != 0) {
	    Connection *c = (Connection *) ent->ent_value;
	    c->src.node->connections--;
	    c->src.node->getNetwork()->connections--;
	    c->dst.node->connections--;
	    c->dst.node->getNetwork()->connections--;
	    delete c;
	    in_delete(in, ep);
	}
    }
}

unsigned int
ConnTable::hash(ConnKey *c, int)
{
    unsigned int h = 0;

    char *ps = c->src->interface->physaddr.getAddr();
    struct in_addr *is = c->src->interface->ipaddr.getAddr();
    char *pd = c->dst->interface->physaddr.getAddr();
    struct in_addr *id = c->dst->interface->ipaddr.getAddr();
    h += (ps == 0) ? 0 : ps[5];
    h += (is == 0) ? 0 : (unsigned int) is->s_addr;
    h += (pd == 0) ? 0 : pd[5];
    h += (id == 0) ? 0 : (unsigned int) id->s_addr;
    return h;
}

int
ConnTable::cmp(ConnKey *k1, ConnKey *k2, int)
{
    return k1->src != k2->src || k1->dst != k2->dst;
}
