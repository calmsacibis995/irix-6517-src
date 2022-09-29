/*
 * Copyright 1996, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Id: Config.c++,v 1.5 1997/11/28 03:15:29 markgw Exp $"

#include <stdlib.h>
#include <stdio.h>
#include <iostream.h>
#include <string.h>
#include <math.h>

#include "pmapi.h"
#include "Bool.h"
#include "Topology.h"

RealPos * RPos = NULL;
Link * RLink = NULL;
Link * NLink = NULL;
int * RMap = NULL;
OMC_Bool NoRouter = OMC_false;

/*
 * router indicies (r_idx etc) index RPos
 * router ordinals (r_ord etc) map 1-1 to the 1.2 namespace for routers
 * node ordinals (n_ord etc) map 1-1 to the 1.2 namespace for nodes
 * node indicies (n_idx etc) index NLink
 *   (there's a 1-1 mapping from nodes to node links, so we can do this)
 *
 * RLink contains:
 * FROM_ (a router index)
 * TO_ (a router index)
 * PORT_ (a router port number) (no mapping: this is the number we're given)
 * DUP_ (duplicate link flag)
 *
 * NLink contains:
 * FROM_ (a router index)
 * TO_ (a node *ordinal*)
 * PORT_ (a router port number)
 * CPU_ (bit 1 indicates presence of cpu 'a', bit 2 for 'b')
 *
 * RPos contains a float triple placing the router
 *
 * RMap maps a router index to a router ordinal
 *
 * NoRouter indicates a no-router configuration using a bogus router
 */

void add_cpu(int n_idx, char c);
void det_duplinks(int link);
void det_duppos(int router);
void det_unilinks();

/* stands in for link2topo: reads RPos, RLink, and NLink from a file
 * returns the number of routers */

int
genRPos(FILE *fp)
{
    int		i, count;
    int		r0, r1, rport, to0, to1, dum;
    char	c0, c1;
    char	buf[BUFSIZ];
    char	linkto[BUFSIZ];
    OMC_Bool	full = OMC_false;
    int		nrouters = 0;
    int		nrlinks = 0;
    int		nnlinks = 0;
    int		routerLen = strlen("router");
    int		nodeLen = strlen("node");
    IntPos	pos;

    while (full || fgets(buf, BUFSIZ, fp)) { // routers
	if (buf[0] == '#' || strlen(buf) == 0)
	    continue;
	full = OMC_false;

	count = sscanf(buf, "router:%d.%d %d %d %d", 
		       &r0, &r1, pos, pos+1, pos+2);
	if (r0 < 0) NoRouter = OMC_true;

	if (count != 5) {
	    if (strlen(buf) && buf[strlen(buf)-1] == '\n')
		buf[strlen(buf)-1] = '\0';
	    pmprintf("%s: Expected router specification, not \"%s\"\n",
		     pmProgname, buf);
	    goto fail;
	}

	nrouters++;
	RPos = (RealPos *)srealloc(RPos, sizeof(RealPos)*nrouters);
	for(i=0; i<3; i++)
	    RPos[nrouters-1][i] = pos[i];
	det_duppos(nrouters-1);
	RMap = (int *)srealloc(RMap, sizeof(int)*nrouters);
	RMap[nrouters-1] = router_ord(r0, r1);

	while (fgets(buf, BUFSIZ, fp)) { // links
	    if (buf[0] == '#' || strlen(buf) == 0)
		continue;
	    if (*buf == 'r') { full = OMC_true; break; } // back to top while

	    count = sscanf(buf,
			   " link rport:%d.%d.%d %s cpu:%d.%d.%c cpu:%d.%d.%c",
			   &r0, &r1, &rport, linkto, // COUNT1
			   &dum, &dum, &c0,	       // COUNT2
			   &dum, &dum, &c1);	       // COUNT3

#define COUNT1 4
#define COUNT2 7
#define COUNT3 10

	    if (count != COUNT1 && count != COUNT2 && count != COUNT3) {
		if (strlen(buf) && buf[strlen(buf)-1] == '\n')
		    buf[strlen(buf)-1] = '\0';
		pmprintf("%s: Expected link specification, not \"%s\"\n",
			 pmProgname, buf);
		goto fail;
	    }

	    if (strncmp(linkto, "router", routerLen) == 0) {

		// router
		if (sscanf(linkto, "router:%d.%d", &to0, &to1) != 2) {
		    pmprintf("%s: Expected router:M.S, not \"%s\"\n",
			     pmProgname, linkto);
		    goto fail;
		}
		nrlinks++;
		RLink = (Link *)srealloc(RLink, sizeof(Link)*nrlinks);
		RLink[nrlinks-1][FROM_] = nrouters-1; // router_ord(r0, r1);
		RLink[nrlinks-1][TO_] = router_ord(to0, to1); // remap later
		RLink[nrlinks-1][PORT_] = rport;
		RLink[nrlinks-1][DUP_] = 0;
		det_duplinks(nrlinks-1);

	    } else if (strncmp(linkto, "node", nodeLen) == 0) {

		// node
		if (sscanf(linkto, "node:%d.%d", &to0, &to1) != 2) {
		    pmprintf("%s: Expected node:M.S, not \"%s\"\n",
			     pmProgname, linkto);
		    goto fail;
		}
		nnlinks++;
		NLink = (Link *)srealloc(NLink, sizeof(Link)*nnlinks);
		NLink[nnlinks-1][FROM_] = nrouters-1; // router_ord(r0, r1);
		NLink[nnlinks-1][TO_] = node_ord(to0, to1);
		NLink[nnlinks-1][PORT_] = rport;

		// cpus
		NLink[nnlinks-1][CPU_] = 0;
		if (count >= COUNT2) add_cpu(nnlinks-1, c0);
		if (count == COUNT3) add_cpu(nnlinks-1, c1);
	    }
	    else {
		pmprintf("%s: Expected router or node, not \"%s\"\n",
			 pmProgname, linkto);
		goto fail;
	    }
	}				// rports
    }				// routers

    // remap RLink TO_ from ordinal to index values
    for (i=0; i<nrlinks; i++)
	RLink[i][TO_] = rord2idx(RLink[i][TO_]);

    // lazy end-markers for links arrays
    RLink = (Link *)srealloc(RLink, sizeof(Link)* (nrlinks+1));
    NLink = (Link *)srealloc(NLink, sizeof(Link)* (nnlinks+1));
    RLink[nrlinks][FROM_] = RLink[nrlinks][TO_] = -1;
    NLink[nnlinks][FROM_] = NLink[nnlinks][TO_] = -1;

    det_unilinks();

    if (NoRouter && nrouters > 1)	// shouldn't be more than one
	pmprintf("%s: %d routers in no-router configuration\n",
		 pmProgname, nrouters);

#ifdef UNDEF
    int a, b, c, d;

    for (i=0; i<nrouters; i++)
	debug("router[%d] %.1f %.1f %.1f", 
	      i, RPos[i][0], RPos[i][1], RPos[i][2]);

    for (i=0; i<nrlinks; i++) {
	router_name(ridx2ord(RLink[i][FROM_]), a, b);
	router_name(ridx2ord(RLink[i][TO_]), c, d);
	debug("rlink[%d] %d (%d.%d.%d) -> %d (%d.%d) [%d]",
	      i, RLink[i][FROM_], a, b, RLink[i][PORT_], RLink[i][TO_], c, d,
	      RLink[i][DUP_]);
    }
    debug("rlink terminator (%d) : %d %d", i, RLink[i][FROM_], RLink[i][TO_]);

    for (i=0; i<nnlinks; i++) {
	router_name(ridx2ord(NLink[i][FROM_]), a, b);
	node_name(NLink[i][TO_], c, d);
	debug("nlink[%d] %d (%d.%d.%d) <- %d (%d.%d):%d",
	      i, NLink[i][FROM_], a, b, NLink[i][PORT_], NLink[i][TO_], c, d,
	      NLink[i][CPU_]);
    }
    debug("nlink terminator (%d) : %d %d", i, NLink[i][FROM_], NLink[i][TO_]);

    for (i=0; i<nrouters; i++)
	debug("RMap[%d] %d", i, RMap[i]);
    for (i=0; i<nrouters; i++) {
	router_name(ridx2ord(i), a, b);
	debug("idx:%d ridx2ord:%d name(%d.%d) 2idx:%d",
	      i, ridx2ord(i), a, b, rord2idx(ridx2ord(i)));
    }
#endif /* UNDEF */

    return nrouters;

 fail:

    pmprintf("%s: Fatal errors in configuration, exiting.\n",
	     pmProgname);
    pmflush();
    exit(1);
    /*NOTREACHED*/
}

void det_unilinks()
{
    int i,j;
    OMC_Bool found = OMC_false;
    for (i=0; RLink[i][FROM_] >= 0; i++) {
	for (j=0; RLink[j][FROM_] >= 0; j++) {
	    if (i == j) continue;
	    if (RLink[i][FROM_] == RLink[j][TO_] &&
		RLink[i][TO_] == RLink[j][FROM_])
		break;
	}
	if (RLink[j][FROM_] < 0) {	// no match from loop above
	    found = OMC_true;
	    pmprintf("%s: One-way link from router %d (port %d) to router %d\n",
		pmProgname, RLink[i][FROM_], RLink[i][PORT_], RLink[i][TO_]);
	}
    }
    if (found) {
	pmprintf("%s: One-way CrayLinks detected!\n", pmProgname);
	pmprintf("%s: Fatal errors in configuration, exiting.\n", pmProgname);
	pmflush();
	exit(1);
	/* NOTREACHED */
    }
}

void det_duppos(int router)
{
    int	i, j;

    if (!router)
	return;

    for (i=router-1; i >= 0; i--) {
	for (j=0; j<3; j++) {
	    if (!eqErr(RPos[router][j], RPos[i][j]))
		break;
	}
	if (j == 3)		// all equal
	    pmprintf("%s: Routers %d and %d share same position!"
		       " [%.1f %.1f %.1f]\n", pmProgname, router, i,
		       RPos[i][xAxis], RPos[i][yAxis], RPos[i][zAxis]);
    }
}

void det_duplinks(int link)
{
    if (!link) return;
    int dup_count = 0;
    for (int i=link-1; i >= 0; i--) {
	if (RLink[link][FROM_] == RLink[i][FROM_] &&
	    RLink[link][TO_] == RLink[i][TO_]) {
	    /* could set DUP_ to link here, and it'd never be 0,
	     * but we've found no use for it so far,
	     * and we *do* have a use for dup_count */
	    RLink[i][DUP_] = ++dup_count;
	}
    }
}

void add_cpu(int n_idx, char c)
{
    if (c == 'a') {
	NLink[n_idx][CPU_] |= 1;
    } else if (c == 'b') {
	NLink[n_idx][CPU_] |= 2;
    } else {
	pmprintf("%s: CPU id \"%c\" unrecognised\n", pmProgname, c);
    }
}

// namespace stuff

// returns n_idx for the n'th node associated with r_idx
int node_index(int r_idx, int n)
{
    for(int i=0; NLink[i][FROM_] >= 0 ;i++) {
	if (NLink[i][FROM_] == r_idx) {
	    if (!n--)
		return i; // n_ord : NLink[i][TO_];
	}
    }
    // impossible!
    pmprintf("%s: %d too few nodes found for router %d\n",
	     pmProgname, n+1, r_idx);
    pmflush();
    exit(1);
    /* NOTREACHED */
}

// returns the port number for the link from from to to
int port_number(int from_idx, int to_idx)
{
    for (int i=0; RLink[i][FROM_] >= 0; i++) {
	if (RLink[i][FROM_] == from_idx
	    && RLink[i][TO_] == to_idx)
	    return RLink[i][PORT_];
    }
    // impossible!
    pmprintf("%s: Can't find link from %d to %d\n",
	     pmProgname, from_idx, to_idx);
    pmflush();
    exit(1);
    /* NOTREACHED */
}

#ifdef PCP_DEBUG
#include <stdarg.h>
void debug (char * msg ...)
{
    va_list arg;
    va_start(arg, msg); 
    fprintf(stderr, ">> ");
    vfprintf(stderr, msg, arg);
    fprintf(stderr, "\n");
    va_end(arg);
}
#endif
