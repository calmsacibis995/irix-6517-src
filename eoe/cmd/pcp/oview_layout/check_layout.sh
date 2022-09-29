#! /bin/sh
#
# Copyright 1997, Silicon Graphics, Inc.
# ALL RIGHTS RESERVED
# 
# UNPUBLISHED -- Rights reserved under the copyright laws of the United
# States.   Use of a copyright notice is precautionary only and does not
# imply publication or disclosure.
# 
# U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
# Use, duplication or disclosure by the Government is subject to restrictions
# as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
# in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
# in similar or successor clauses in the FAR, or the DOD or NASA FAR
# Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
# 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
# 
# THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
# INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
# DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
# PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
# GRAPHICS, INC.
#Tag 0x00010D13
#
# $Id: check_layout.sh,v 1.5 1997/11/28 03:17:50 markgw Exp $
#
# check output from oview_layout
# Output errors. If no errors, then output the input.
#
tmp=/tmp/$$
trap "rm -f $tmp $tmp.*; exit" 0 1 2 3 15
prog=`basename $0`

tee $tmp.input \
| sed -e 's/:/ /g' |\
nawk '
function vec(s, d)
{
    if (s == d)
	return 0;
    return 1
}

BEGIN {
    nlinks=0
    nrouters=0
}

/^#/ {
    next
}

$1 == "router" { 
    routers[$2] = $2;
    r_x[$2] = $3
    r_y[$2] = $4
    r_z[$2] = $5
    nrouters++
    next;
}

$1 == "link" && $4 != "node" {
    nlinks++
    n = split($3, s, "[.:]");
    link_src[nlinks] = s[1]"."s[2]
    link_port[nlinks] = s[3]
    link_dest[nlinks] = $5
    next;
}	

END {
    printf "Checking %d routers with %d router links.\n", nrouters, nlinks

    printf "\n1. link lengths ...\n"
    for (i=1; i <= nlinks; i++) {
	if (routers[link_src[i]] != 0 && routers[link_dest[i]] != 0) {
	    sx = r_x[link_src[i]]; sy = r_y[link_src[i]]; sz = r_z[link_src[i]]
	    dx = r_x[link_dest[i]]; dy = r_y[link_dest[i]]; dz = r_z[link_dest[i]]

	    len = sqrt((sx-dx)*(sx-dx) + (sy-dy)*(sy-dy) + (sz-dz)*(sz-dz))

	    if (len != 1.0 && len != 3) {
		if (len == sqrt(2) && nrouters <= 4)
		    continue

		if (len == sqrt(3) && nrouters > 4 && nrouters <= 8)
		    continue;
			
		printf "\tError: bad length for link on port %d from router:%-4s to router:%-4s\n", \
		    link_port[i], routers[link_src[i]], routers[link_dest[i]]
		printf "\t\tlength=%.3f with vector (%d,%d,%d) -> (%d,%d,%d)\n", len, sx,sy,sz, dx,dy,dz
	    }
	}
    }

    printf "\n2. consistent axis orientation for all links on same port ...\n"
    for (p=1; p <= 6; p++) {
	found=0
	for (i=1; i <= nlinks; i++) {
	    if (link_port[i] == p) {
		sx = r_x[link_src[i]]; sy = r_y[link_src[i]]; sz = r_z[link_src[i]]
		dx = r_x[link_dest[i]]; dy = r_y[link_dest[i]]; dz = r_z[link_dest[i]]
		vx = vec(sx, dx);
		vy = vec(sy, dy);
		vz = vec(sz, dz);
		found=1
		break;
	    }
	}
	if (found == 0) {
	    # this port not used anywhere
	    continue;
	}
	printf "   Port %d, reference vector is (%d,%d,%d) ...\n", p, vx, vy, vz

	for (i=1; i <= nlinks; i++) {
	    if (link_port[i] == p) {
		sx = r_x[link_src[i]]; sy = r_y[link_src[i]]; sz = r_z[link_src[i]]
		dx = r_x[link_dest[i]]; dy = r_y[link_dest[i]]; dz = r_z[link_dest[i]]

		if (vx != vec(sx, dx) || vy != vec(sy, dy) || vz != vec(sz, dz)) {
		    printf "\tError: inconsistent alignment port %d from router:%-4s to router:%-4s\n", \
			p, routers[link_src[i]], routers[link_dest[i]]
		    printf "\t\tvector (%d,%d,%d) for link between (%d,%d,%d) -> (%d,%d,%d)\n", \
			vec(sx, dx), vec(sy, dy), vec(sz, dz), sx,sy,sz, dx,dy,dz
		}
	    }
	}
    }
}' >$tmp.out

if grep '^	' $tmp.out > /dev/null 2>&1
then
    cat $tmp.out
else
    cat $tmp.input
fi
exit 0
