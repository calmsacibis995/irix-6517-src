/* -*- C++ -*- */

#ifndef _TOPO_H_
#define _TOPO_H_

/*
 * Copyright 1997, Silicon Graphics, Inc.
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

#ident "$Id: Topology.h,v 1.2 1997/08/21 04:03:23 markgw Exp $"

#include "oview.h"

// types

enum Link_ { FROM_, TO_, PORT_, CPU_, DUP_ = CPU_, LINK_SIZE_ };
typedef int Link[LINK_SIZE_];

// consts

const int UNASSIGNED = -1;
#define OVIEW_LAYOUT "/usr/pcp/bin/oview_layout"

// globals

extern RealPos *RPos;
extern Link *RLink;
extern Link *NLink;
extern int *RMap;
extern OMC_Bool NoRouter;

// protos

// genRPos: fill in the above globals
// readtopo version fills all the above in from a file
// link2topo version is handed RLink, and fills in RPos
// Link treatment may be different across the two...
int genRPos(Link *rl); // link2topo
int genRPos(FILE *fp); // readtopo

// misc
int node_index(int r_idx, int n); // the nth node associated with r_idx
int port_number(int from_idx, int to_idx); // port number for this router assoc

inline int router_ord(int r0, int r1)
{
    return 2*(r0) + (r1-1);	// (r0-1) for 1-numbering
}

inline int node_ord(int n0, int n1)
{
    return 4*(n0) + (n1-1);	// (n0-1) for 1-numbering
}

inline void router_name(int r_id, int &r0, int &r1)
{
    r0 = (r_id)/2;		// (r_id+2) for 1-numbering
    r1 = (r_id%2)+1;
}

inline void node_name(int n_ord, int &n0, int &n1)
{
    n0 = (n_ord)/4;		// (n_id+4) for 1-numbering
    n1 = (n_ord%4)+1;
}

inline int ridx2ord(int r_idx)
{
    return RMap[r_idx];
}

// not very safe...
inline int rord2idx(int r_ord)
{
    for (int i=0; RMap[i] != r_ord; i++)
	;
    return i;
}

#endif /* _TOPO_H_ */
