/*
 * Copyright 1989-1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Communications functions for netlook
 *
 *	$Revision: 1.39 $
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

#include "comm.h"
#include "netlook.h"
#include "conntable.h"
#include "connection.h"
#include "node.h"
#include "network.h"
#include <tuGLGadget.h>
#include "netlookgadget.h"
#include "glmapgadget.h"
#include "event.h"

#include <sys/ioctl.h>
#include <net/if.h>
#include <netdb.h>
#include <signal.h>
#include <string.h>
#include <bstring.h>
#include <osfcn.h>
#include <expr.h>
#include <X11/Xlib.h>
extern "C" {
#include <exception.h>
#include <addrlist.h>
#include <snoopstream.h>
#include <protocol.h>
int sethostresorder(const char *);
}

// XXX can this go away?
extern NetLook *netlook;
extern NetLookGadget *netlookgadget;
extern GLMapGadget *glmapgadget;

// Shut off NIS flag
extern int _yp_disabled;

extern "C" {
struct etheraddr *ether_aton(char *);
int inet_isaddr(char *, struct in_addr *);
}

static const unsigned int drawSrcNet = 0x01;
static const unsigned int drawDstNet = 0x02;
static const unsigned int drawSrcNode = 0x04;
static const unsigned int drawDstNode = 0x08;
static const unsigned int drawAll = 0x10;

Comm::Comm(unsigned int b, unsigned int i)
{
    FD_ZERO(&openfds);
    sigset(SIGPIPE, SIG_IGN);
    badNode = snoopList = 0;
    buflen = b;
    interval = i;
    snooplen = 128;
    alp = new struct alpacket;
    interrupt = 0;
    timeout.tv_sec = timeout.tv_usec = 0;

    // Zero filter
    bzero(filter, sizeof filter);

    // Initialize protocol module
    _yp_disabled = 1;
    initprotocols();
    _yp_disabled = 0;
}

Comm::~Comm(void)
{
    delete alp;
}

unsigned int
Comm::open(Node *node)
{
    if (!node->interface->ipaddr.isValid())
	return 0;

    struct sockaddr_in hostaddr;
    hostaddr.sin_addr = *node->interface->ipaddr.getAddr();
    hostaddr.sin_family = AF_INET;
    hostaddr.sin_port = 0;

    node->snoopstream = new SnoopStream;
    if (!ss_open(node->snoopstream, node->display, &hostaddr, 0)) {
	delete node->snoopstream;
	node->snoopstream = 0;
	return 0;
    }

    if (!ss_subscribe(node->snoopstream, SS_ADDRLIST, node->interface->name,
		      buflen, 0, interval)) {
	ss_close(node->snoopstream);
	delete node->snoopstream;
	node->snoopstream = 0;
	return 0;
    }

    if (!ss_setsnooplen(node->snoopstream, snooplen)) {
	ss_close(node->snoopstream);
	delete node->snoopstream;
	node->snoopstream = 0;
	return 0;
    }

    ExprError err;
    int rc;
    if (filter[0] == 0)
	rc = ss_add(node->snoopstream, 0, &err);
    else {
	ExprSource src;
	src.src_path = "CommOpen";
	src.src_line = 0;
	src.src_buf = filter;
	rc = ss_compile(node->snoopstream, &src, &err);
    }
    if (rc < 0) {
	if (err.err_token != 0)
	    exc_raise(0, "snoopd on %s: %s: \"%s\"",
			 node->display, err.err_message, err.err_token);
	(void) ss_unsubscribe(node->snoopstream);
	ss_close(node->snoopstream);
        delete node->snoopstream;
        node->snoopstream = 0;
        return 0;
    }

    if (!ss_start(node->snoopstream)) {
	(void) ss_unsubscribe(node->snoopstream);
	ss_close(node->snoopstream);
	delete node->snoopstream;
	node->snoopstream = 0;
	return 0;
    }

    FD_SET(node->snoopstream->ss_sock, &openfds);
    node->snoop = snoopList;
    snoopList = node;
    return 1;
}

unsigned int
Comm::close(Node *node)
{
    FD_CLR(node->snoopstream->ss_sock, &openfds);
    int rc = ss_stop(node->snoopstream) && ss_unsubscribe(node->snoopstream);
    ss_close(node->snoopstream);

    delete node->snoopstream;
    node->snoopstream = 0;

    // Node must be in list
    if (node == snoopList) {
	snoopList = node->snoop;
	node->snoop = 0;
	return rc;
    }
    for (Node *next, *h = snoopList; h != 0; h = next) {
	next = h->snoop;
	if (next == node) {
	    h->snoop = next->snoop;
	    next->snoop = NULL;
	    return rc;
	}
    }

    // Should not be reached
    return 0;
}

unsigned int
Comm::changeFilter(Node *n)
{
    if (!ss_stop(n->snoopstream))
	return 0;

    if (!ss_unsubscribe(n->snoopstream))
	return 0;

    if (!ss_subscribe(n->snoopstream, SS_ADDRLIST, n->interface->name,
		      buflen, 0, interval))
	return 0;

    if (!ss_setsnooplen(n->snoopstream, snooplen))
	return 0;

    ExprError err;
    int rc;
    if (filter[0] == 0)
	rc = ss_add(n->snoopstream, 0, &err);
    else {
	ExprSource src;
	src.src_path = "CommChangeFilter";
	src.src_line = 0;
	src.src_buf = filter;
	rc = ss_compile(n->snoopstream, &src, &err);
    }
    if (rc < 0) {
	if (err.err_token != 0)
	    exc_raise(0, "%s: \"%s\"", err.err_message, err.err_token);
	return 0;
    }

    if (!ss_start(n->snoopstream))
	return 0;

    return 1;
}

unsigned int
Comm::read(void)
{
    int ready;
    fd_set readfds;

    if (interrupt) {
	interrupt = decode(alp);
	if (interrupt)
	    return 1;
    }

    readfds = openfds;
    ready = select(FD_SETSIZE, &readfds, 0, 0, &timeout);

    if (ready < 0) {
	perror("netlook: error in select");
	exit(-1);
    }

    for (Node *node = snoopList; node != NULL; node = node->snoop) {
	if (XPending(netlook->getDpy()) != 0)
	    return 1;
	if (FD_ISSET(node->snoopstream->ss_sock, &readfds)) {
	    if (!ss_read(node->snoopstream, (xdrproc_t) xdr_alpacket, alp)) {
		badNode = node;
		return 0;
	    }

#ifdef DEBUG
	    printf("\n===== From %s: %d entries =====\n",
		   inet_ntoa(*(struct in_addr *) &alp->al_source),
		   alp->al_nentries);
#endif

	    interrupt = decode(alp);
	    if (interrupt)
		return 1;
	}
    }

    return 1;
}

unsigned int
Comm::decode(struct alpacket *alp)
{
    static unsigned int count = 0;
    const int check = 2;

    if (alp->al_type != AL_DATATYPE)
    	return 0;

    IPAddr snoopaddr = alp->al_source;
    InterfaceNode *snoopif = netlook->getIfDB()->match(snoopaddr);
    if (snoopif == 0)
    	return 0;

    Display *dpy = netlook->getDpy();
    for ( ; count < alp->al_nentries; count++) {
	// Check for a pending event and return if there is
	if (count % check == 0 && XPending(dpy) != 0)
	    return 1;
#ifdef DEBUG
	printf("-- %d --\n", count);
#endif
	if (packetDecode(snoopif, &alp->al_entry[count]) != 0)
	    return 0;
    }
    count = 0;
    return 0;
}

unsigned int
Comm::packetDecode(InterfaceNode *snoopif, struct alentry *ale)
{
    NetworkNode *srcnet, *dstnet, *snoopnet;
    SegmentNode *srcseg, *dstseg, *snoopseg;
    InterfaceNode *srcif, *dstif;
    PhysAddr srcpa, dstpa;
    IPAddr srcip, dstip;
    Node *s, *d;
    char *sp;
    int redraw = 0;

#ifdef DEBUG
    printf("src: %s %s %s\n", ether_ntoa((char *) &ale->ale_src.alk_paddr),
			      inet_ntoa(*(struct in_addr *)
			      &ale->ale_src.alk_naddr),
			      ale->ale_src.alk_name);
    printf("dst: %s %s %s\n", ether_ntoa((char *) &ale->ale_dst.alk_paddr),
			      inet_ntoa(*(struct in_addr *)
						&ale->ale_dst.alk_naddr),
			      ale->ale_dst.alk_name);
    printf("packets: %5.0f bytes: %5.0f\n", ale->ale_count.c_packets,
					    ale->ale_count.c_bytes);
#endif

    /* Remove trailing blanks from names */
    sp = strchr(ale->ale_src.alk_name, ' ');
    if (sp != 0)
		*sp = '\0';
    sp = strchr(ale->ale_dst.alk_name, ' ');
    if (sp != 0)
		*sp = '\0';

    /* Don't use the names if they are addresses */
    if (ether_aton(ale->ale_src.alk_name) != 0
	|| inet_isaddr(ale->ale_src.alk_name, 0) != 0)
	ale->ale_src.alk_name[0] = '\0';

    if (ether_aton(ale->ale_dst.alk_name) != 0
	|| inet_isaddr(ale->ale_dst.alk_name, 0) != 0)
	ale->ale_dst.alk_name[0] = '\0';

    // Snoop node
    snoopseg = (SegmentNode *) snoopif->parent;
    snoopnet = (NetworkNode *) snoopseg->parent;

    srcif = 0;
    srcpa = ale->ale_src.alk_paddr;
    srcip = ale->ale_src.alk_naddr;
    redraw = get(snoopnet, snoopseg, snoopif,
		 srcpa, srcip, ale->ale_src.alk_name,
		 &srcnet, &srcseg, &srcif);
    if (srcif == 0)
	return 0;
    s = (Node *) srcif->data;
    if ((s->info & HIDDEN) != 0 || (s->getNetwork()->info & HIDDEN) != 0)
	return 0;

    dstif = 0;
    dstpa = ale->ale_dst.alk_paddr;
    dstip = ale->ale_dst.alk_naddr;
    redraw |= get(snoopnet, snoopseg, snoopif,
		  dstpa, dstip, ale->ale_dst.alk_name,
		  &dstnet, &dstseg, &dstif);
    if (dstif == 0)
	return 0;
    d = (Node *) dstif->data;
    if ((d->info & HIDDEN) != 0 || (d->getNetwork()->info & HIDDEN) != 0)
	return 0;

    redraw |= connect(s, d, &ale->ale_count);

    if (netlook->getTrafficMode() == HOP) {
	redraw |= checkGateway(srcip, srcnet, srcif, &ale->ale_count);
	redraw |= checkGateway(dstip, dstnet, dstif, &ale->ale_count);
    }

    if (redraw == 0)
	return 0;

    if ((redraw & drawAll) != 0)
	netlookgadget->render();
    else {
	if ((redraw & drawSrcNet) != 0)
	    netlookgadget->render(s->getNetwork());
	else if ((redraw & drawSrcNode) != 0)
	    netlookgadget->render(s->getNetwork(), s);

	if ((redraw & drawDstNet) != 0)
	    netlookgadget->render(d->getNetwork());
	else if ((redraw & drawDstNode) != 0)
	    netlookgadget->render(d->getNetwork(), d);
    }
    glmapgadget->render();
    return 0;
}

unsigned int
Comm::get(NetworkNode *snoopnet, SegmentNode *snoopseg, InterfaceNode* snoopif,
	  PhysAddr& pa, IPAddr& ip, char *name,
	  NetworkNode **n, SegmentNode **s, InterfaceNode **i)
{
    if (netlook->getTrafficMode() == HOP && !pa.isGroup()) {
	// Try to find the interface by physical address
	*i = netlook->getIfDB()->match(pa, snoopnet);
	if (*i != 0 && (*i)->ipaddr.isValid()) {
	    *s = (SegmentNode *) (*i)->parent;
	    *n = (NetworkNode *) (*s)->parent;
	    return 0;
	}

	// Find the IP network of the interface
	if (ip.isValid()) {
	    IPNetNum nn = ip;
	    *n = netlook->getNetDB()->match(nn);
	    if (*n == 0)
		goto badnet;

	    if ((*n)->ipmask.isValid()) {
		nn.setAddr(ip, (*n)->ipmask);
		*n = netlook->getNetDB()->match(nn);
	    }
	    if (*n != snoopnet)
		goto badnet;

	    // The node is on the same network
	    // as the snooper, so the IP address
	    // is also that of the interface.
	    if (*i != 0) {
		// Got an IP address for this interface.
		(*i)->ipaddr = ip;
		if (name[0] != 0 && (*i)->getName() == 0)
		    (*i)->setName(name);
		(*i)->complete();
		netlook->getIfDB()->enter(*i);
		((Node *)(*i)->data)->newInfo();
		return drawAll;
	    } else {
		*i = netlook->getIfDB()->match(ip);
		if (*i == 0)
		    goto badnet;

		*s = (SegmentNode *) (*i)->parent;

		unsigned int update = 0;
		if (!(*i)->physaddr.isValid()) {
		    (*i)->physaddr = pa;
		    update = drawAll;
		}
		if (name[0] != '\0' && (*i)->getName() == 0) {
		    (*i)->setName(name);
		    update = drawAll;
		}
		if (update != 0) {
		    (*i)->complete();
		    netlook->getIfDB()->enter(*i);
		    ((Node *)(*i)->data)->newInfo();
		}
		return update;
	    }
	}
badnet:
	if (*i != 0) {
	    // Couldn't get an IP address for this interface
	    *s = (SegmentNode *) (*i)->parent;
	    *n = (NetworkNode *) (*s)->parent;
	    return 0;
	}
	*n = snoopnet;
	*s = snoopseg;		// XXX - Best guess?
    } else if (!ip.isValid()) {
	if (pa.isGroup()) {
	    // Look for the address in this segment
	    for (InterfaceNode *b = (InterfaceNode *) snoopseg->child;
			b != 0; b = (InterfaceNode *) b->next) {
		if (b->physaddr == pa) {
		    *i = b;
		    *s = snoopseg;
		    *n = snoopnet;
		    return 0;
		}
	    }
	    for (SegmentNode *q = (SegmentNode *) snoopnet->child;
			q != 0; q = (SegmentNode *) q->next) {
		if (q == snoopseg)
		    continue;
		for (b = (InterfaceNode *) q->child;
			b != 0; b = (InterfaceNode *) b->next) {
		    if (b->physaddr == pa) {
			*i = b;
			*s = snoopseg;
			*n = snoopnet;
			return 0;
		    }
		}
	    }
	} else {
	    // Try to find the interface by physical address
	    *i = netlook->getIfDB()->match(pa, snoopnet);
	    if (*i != 0) {
		*s = (SegmentNode *) (*i)->parent;
		*n = (NetworkNode *) (*s)->parent;
		return 0;
	    }
	}
	*n = snoopnet;
	*s = snoopseg;		// XXX - Try it by DECnet
    } else {
	// Try to find the interface by IP address
	*i = netlook->getIfDB()->match(ip);
	if (*i != 0) {
	    *s = (SegmentNode *) (*i)->parent;
	    *n = (NetworkNode *) (*s)->parent;
	    unsigned int update = 0;
	    if (*n == snoopnet && !(*i)->physaddr.isValid()) {
		(*i)->physaddr = pa;
		update = drawAll;
	    }
	    if (name[0] != '\0' && (*i)->getName() == 0) {
		(*i)->setName(name);
		update = drawAll;
	    }
	    if (update != 0) {
		(*i)->complete();
		netlook->getIfDB()->enter(*i);
		((Node *)(*i)->data)->newInfo();
	    }
	    return update;
        }

	// Match the network by IP network number
	IPNetNum nn = ip;
	*n = netlook->getNetDB()->match(nn);
	if (*n == 0) {
	    if (netlook->getNetIgnore() == IGNORE)
		return 0;

	    // Create the new network
	    *n = new NetworkNode(netlook->getDatabase());
	    (*n)->ipnum = nn;
	    (*n)->complete();
	    netlook->getNetDB()->enter(*n);
	}

	if ((*n)->ipmask.isValid()) {
	    // The network has an IP netmask associated with it.
	    // Reform the IP network number using the netmask and
	    // try to find that network.
	    nn.setAddr(ip, (*n)->ipmask);
	    *n = netlook->getNetDB()->match(nn);
	    if (*n == 0) {
		if (netlook->getNetIgnore() == IGNORE)
		    return 0;

		// Create the new network
		*n = new NetworkNode(netlook->getDatabase());
		(*n)->ipnum = nn;
		(*n)->complete();
		netlook->getNetDB()->enter(*n);
	    }
	}

	// Check the network of the physical address
	*i = netlook->getIfDB()->match(pa, *n);
	if (*i != 0 && ((NetworkNode *)((*i)->parent->parent)) == *n) {
	    // The physical address gives the same network as the
	    // IP address, so assume that the physical address is
	    // the correct interface.
	    *s = (SegmentNode *) (*i)->parent;
	    (*i)->ipaddr = ip;
	    if (name[0] != '\0' && (*i)->getName() == 0)
		(*i)->setName(name);
	    (*i)->complete();
	    netlook->getIfDB()->enter(*i);
	    ((Node *)(*i)->data)->newInfo();
	    return drawAll;
	}

	// Match the segment by IP network number
	*s = netlook->getSegDB()->match(nn);
	if (*s == 0) {
	    if (netlook->getNetIgnore() == IGNORE)
		return 0;

	    // Create the new segment
	    *s = new SegmentNode(*n);
	    (*s)->ipnum = nn;
	    (*s)->complete();
	    netlook->getSegDB()->enter(*s);
	    (*s)->data = new Network(*s);
	    netlook->position();

	    // Send new network event
	    if (netlook->getSendDetectEvents() != False) {
		EV_objID snoopObject(snoopif->getName(),
				     snoopif->ipaddr.getString(),
				     snoopif->physaddr.getString());
		EV_objID netObject((*s)->getName(),
				   (*s)->ipnum.getString());
		EV_event newNetEvent(NV_NEW_NET, OBJ_NET, &snoopObject,
				     filter, 0, &netObject);
		netlook->getEventHandler()->send(&newNetEvent);
	    }
	}
    }

    if (netlook->getNodeIgnore() == IGNORE)
	return 0;

    // XXX - See if this host already exists
    HostNode *hn = new HostNode;

    // Add the new interface
    *i = new InterfaceNode(*s);
    (*i)->setHost(hn);
    if (netlook->getTrafficMode() == HOP)
	(*i)->physaddr = pa;
    else {
	if ((*n) == snoopnet)
	    (*i)->physaddr = pa;
	(*i)->ipaddr = ip;
	if (name[0] != '\0')
	    (*i)->setName(name);
    }
    // XXX - DECnet address?
    (*i)->complete();
    netlook->getIfDB()->enter(*i);
    Node *h = new Node(*i);
    (*i)->data = h;
    h->getNetwork()->place();

    // Send new node event
    if (netlook->getSendDetectEvents() != False) {
	EV_objID snoopObject(snoopif->getName(),
			     snoopif->ipaddr.getString(),
			     snoopif->physaddr.getString());
	EV_objID nodeObject((*i)->getName(),
			    (*i)->ipaddr.getString(),
			    (*i)->physaddr.getString());
	EV_event newNodeEvent(NV_NEW_NODE, OBJ_NODE, &snoopObject,
			      filter, &nodeObject);
	netlook->getEventHandler()->send(&newNodeEvent);
    }

    return drawAll;
}

unsigned int
Comm::connect(Node *s, Node *d, struct counts *count)
{
    unsigned int redraw = 0;

    /* Find this connection */
    Connection *c = netlook->getConnectionTable()->lookup(s, d);
    if (c == NULL) {
	/* Add the new connection */
	if (netlook->getNetShow() == ACTIVE) {
	    if (s->getNetwork()->connections == 0)
		redraw |= drawSrcNet;
	    if (d->getNetwork()->connections == 0)
		redraw |= drawDstNet;
	}
	if (netlook->getNodeShow() == ACTIVE) {
	    if (s->connections == 0)
		redraw |= drawSrcNode;
	    if (d->connections == 0)
		redraw |= drawDstNode;
	}
	c = netlook->getConnectionTable()->insert(s, d);
    }

    // See if the source in the struct connection is the same as the
    // source of this packet, if not, do everything reversed.
    time_t t = time(0);  
    if (s == c->src.node) {
	c->src.packets += (unsigned short) count->c_packets;
	c->src.bytes += (unsigned int) count->c_bytes;
	s->timestamp = c->src.timestamp = t;
    } else {
	c->dst.packets += (unsigned short) count->c_packets;
	c->dst.bytes += (unsigned int) count->c_bytes;
	d->timestamp = c->dst.timestamp = t;
    }

    c->update();
    return redraw;
}

unsigned int
Comm::checkGateway(IPAddr& ip, NetworkNode *n, InterfaceNode *i,
		   struct counts *count)
{
    IPNetNum nn = ip;
    if (!nn.isValid())
	return 0;
    NetworkNode *net = netlook->getNetDB()->match(nn);
    if (net == 0 || net == n)
	return 0;
    if (n->ipmask.isValid()) {
	nn.setAddr(ip, n->ipmask);
	net = netlook->getNetDB()->match(nn);
	if (net == 0 || net == n)
	    return 0;
    }
    SegmentNode *seg = netlook->getSegDB()->match(nn);
    if (seg == 0)
	return 0;
    HostNode *hn = i->getHost();
    Node *h = (Node *) i->data;
    h->info |= GATEWAY;
    for (InterfaceNode *in = (InterfaceNode *) hn->child;
		in != 0; in = in->nextif) {
	if (seg == (SegmentNode *) in->parent) {
	    if (connect((Node *) in->data, h, count) != 0)
		return drawAll;
	}
    }
    return 0;
}

void
Comm::setFilter(const char *f)
{
    strcpy(filter, f);
}
