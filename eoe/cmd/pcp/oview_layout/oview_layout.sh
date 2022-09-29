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
# $Id: oview_layout.sh,v 1.38 1998/02/25 02:51:46 kenmcd Exp $
# Generate topology specification for oview(1)
# from hinv.interconnect.
#

tmp=/tmp/$$
trap "_on_exit; exit \$status" 0 1 2 3 15
prog=`basename $0`

_on_exit()
{
    _errorFlush
    rm -f $tmp.*
}
_usage()
{
    _error "Usage: $prog [options]" 
    _error "Options:" 
    _error "  -a archive    metrics source is an archive log" 
    _error "  -h host       metrics source is PMCD on host" 
    _error "  -n pmnsfile   use an alternative PMNS"
    exit 1
}

_parse_args()
{
    SOURCE=
    TYPE="host"
    PMNS=
    VERBOSE=false

    while getopts "?a:c:h:n:p:v" c $*
    do
	case $c
	in
	a)
	    if [ -z "$OPTARG" ]
	    then
		_error "$prog: Error: missing argument for -a"
		_usage
	    fi
	    if [ ! -z "$SOURCE" ]
	    then
		_error "$prog: Error: only one of -a, -c, -h can be specified"
		_usage
	    fi
	    TYPE="archive"
	    SOURCE=$OPTARG
	    ;;
	c)
	    if [ -z "$OPTARG" ]
	    then
		_error "$prog: Error: missing argument for -c"
		_usage
	    fi
	    if [ ! -z "$SOURCE" ]
	    then
		_error "$prog: Error: only one of -a, -c, -h can be specified"
		_usage
	    fi
	    TYPE="config"
	    SOURCE=$OPTARG
	    ;;
	h)
	    if [ -z "$OPTARG" ]
	    then
		_error "$prog: Error: missing argument for -h"
		_usage
	    fi
	    if [ ! -z "$SOURCE" ]
	    then
		_error "$prog: Error: only one of -a, -c, -h can be specified"
		_usage
	    fi
	    TYPE="host"
	    SOURCE=$OPTARG
	    ;;
	n)
	    if [ -z "$OPTARG" ]
	    then
		_error "$prog: Error: missing argument for -n"
		_usage
	    fi
	    PMNS="-n $OPTARG"
	    ;;
	p)
	    prog=$OPTARG
	    ;;
	v)
	    VERBOSE=true
	    ;;
	\?)
	    _usage
	    ;;
	esac
    done
}

_error()
{
    echo "$*" >> $tmp.errors
}

_errorCat()
{
    if [ $# -ne 0 ]
    then
	cat $* >> $tmp.errors
    fi
}

_errorFlush()
{
    if [ -f $tmp.errors -a -s $tmp.errors ]
    then
	case "$PCP_STDERR"
	in
	"DISPLAY")
	    /usr/bin/X11/xconfirm -file $tmp.errors -c -B OK \
	    -icon info -useslider -header "PCP Information" >/dev/null 2>&1
	    ;;
	"")
	    cat $tmp.errors >&2
	    ;;
	*)
	    cat $tmp.errors >>$PCP_STDERR
	    ;;
	esac
	cp /dev/null $tmp.errors
    fi
}



# get hinv metric
#
_get_metric_value()
{
    metric=$1

    if pmprobe $PMNS $SOURCEFLAGS -v hinv.$metric 1>$tmp.$metric 2>&1
    then
	:
    else
        msg=`sed < $tmp.$metric 's/pmprobe: //'`
	_error "$prog: Error: could not obtain hinv.$metric metric from $TYPE \"$SOURCE\""
        _error "$prog: $msg"
	exit 1
    fi

    num=`cut -f2 -d' ' $tmp.$metric`
    if [ $num -eq 1 ]
    then
	:
    elif [ $num -lt 0 ]
    then
	msg=`sed < $tmp.$metric 's/.*-[0-9][0-9]* //'`
	_error "$prog: Error: could not obtain hinv.$metric metric from $TYPE \"$SOURCE\": $msg "
	exit 1
    else
	_error "$prog: Error: could not obtain hinv.$metric metric from $TYPE \"$SOURCE\""
	exit 1
    fi
}

_get_metric_array()
{
    metric=$1

    if pminfo $PMNS $SOURCEFLAGS -f hinv.$metric 1>$tmp.$metric 2>&1
    then
	:
    else
        msg=`sed < $tmp.$metric 's/pminfo: //'`
	_error "$prog: Error: could not obtain hinv.$metric metric from $TYPE \"$SOURCE\""
	_error "$prog: $msg "
	exit 1
    fi

    if grep 'inst \[.*\] value' $tmp.$metric > /dev/null
    then 
	:
    else
	_error "$prog: Error: could not obtain hinv.$metric metric from $TYPE \"$SOURCE\""
	_errorFlush
	exit 1
    fi
}


_get_net_pminfo()
{
    outfile=$1

    if [ "$TYPE" = "host" ]
    then
	if [ ! -z "$SOURCE" ]
	then
	    SOURCEFLAGS="-h $SOURCE"
	else
	    SOURCEFLAGS=""
	    SOURCE="localhost"
	fi

    elif [ "$TYPE" = "archive" ]
    then
	SOURCEFLAGS="-a $SOURCE"
    fi

    _get_metric_value nrouter
    _get_metric_value nnode
    NROUTER=`cut -f3 -d' ' $tmp.nrouter`
    NNODE=`cut -f3 -d' ' $tmp.nnode`
    if [ $NNODE -eq 0 -a $NROUTER -eq 0 ]
    then
	_error "$prog: Error: no Origin hardware detected \c"
	[ "$TYPE" = "host" -o -z "$TYPE" ] && _error "on host \"$SOURCE\""
	[ "$TYPE" = "archive" ] && _error "in archive \"$SOURCE\""
        if [ "$prog" = "oview" -a -z "$SOURCEFLAGS" ]
	then
	    _error '
oview is a tool for monitoring the performance of a local or
remote Origin system, see oview(1) for details.

To monitor a remote Origin system,
(a) use the -h option from the command line, or
(b) from the IRIX Interactive Desktop drag and drop a host icon onto
    the oview icon in the PerfTools page of the icon book, or
(c) press ALT and double left-mouse click on the oview icon
    (this starts a launch dialog that can be used to specify
    arguments to oview).'
        fi
    	exit 1
    fi

    _get_metric_array map.cpu
    _get_metric_array map.node

    if [ $NROUTER -gt 0 ]
    then
	_get_metric_array interconnect
    else
        touch $tmp.interconnect
    fi

    if $VERBOSE
    then
	_error "--- hinv metrics ---"
        _error "NROUTER=$NROUTER NNODE=$NNODE"
	_error "-- interconnect --"
	_errorCat $tmp.interconnect
	_error "-- cpu map --"
	_errorCat $tmp.map.cpu
	_error "-- node map --"
	_errorCat $tmp.map.node
    fi

    # $tmp.net format is
    # module slot port linked-module linked-slot linked-object router|node [cpu-a] [cpu-b]
    # m1 s1 p1 m2 s2 router|node
    # $1 $2 $3 $4 $5 $6
    #   If $6 == "node", then $7,$8,$9 is cpu_a and $10,$11,$12 is cpu_b
    #
    # Note: "$tmp.cpus" and "$tmp.nodes" are only used in _layout_special()
    #
    cat $tmp.map.cpu $tmp.map.node $tmp.interconnect \
    | sed  \
	    -e '/mrouter/d' \
	    -e '/mrport/d' \
	    -e '/hinv/d' \
	    -e 's/:/ /g' \
	    -e 's/"//g' \
	    -e 's/\[//g' \
	    -e 's/\]//g' \
    | nawk -v cpu_file=$tmp.cpus -v node_file=$tmp.nodes '
	NF == 7 && $4 == "cpu" {
	    cpus[$5] = 1
	    n = split($5, a, "\.");
	    print a[1], a[2], a[3] >> cpu_file
	}
	NF == 7 && $4 == "node" {
	    n = split($5, a, "\.");
	    print a[1], a[2] >> node_file
	}
	NF == 8 && $4 == "rport" {
	    printf("%s %s %s", $5, $8, $7);
	    if ($7 == "node") {
		if (cpus[$8".a"] == 1)
		    printf " %s a", $8
		if (cpus[$8".b"] == 1)
		    printf " %s b", $8
	    }
	    printf "\n"
	}' \
    | sort -n \
    | sed -e 's/\./ /g' > $outfile

    if $VERBOSE
    then
	_error "-- the net --"
	_errorCat $outfile
	_error "-- cpus ---"
	_errorCat $tmp.cpus
	_error "-- nodes ---"
	_errorCat $tmp.nodes
	_errorFlush
    fi

}


_get_net_config()
{
    netfile=$1

    if $VERBOSE 
    then
	_error "--- get_net_config from $SOURCE ---"
    fi

    if [ $SOURCE = "-" ]
    then
	_input=""
    else
	_input="$SOURCE"
    fi

    cat $_input |\
    nawk \
	-v cpu_file=$tmp.cpus \
	-v node_file=$tmp.nodes \
	-v err=$tmp.errors \
	-v netfile=$netfile \
	-v nnode=$tmp.nnodes \
	-v nrouter=$tmp.nrouters '
	/^#/ {
	    next
	}
	$1 ~ /router/ { 
		routers[$1] = 1;
		next;
		}
	$1 == "link" {
		src = $2; dest = $3;
		n = split(src, s, "[.:]");
	        if (n != 4)
		    printf("%s: Error parsing src link in config\n", prog) >> err; 
		n = split(dest, d, "[.:]");
	        if (n != 3)
		    printf("%s: Error parsing dest link in config\n", prog) >> err; 

		printf("%d %d %d %d %d %s", s[2], s[3], s[4], d[2], d[3], d[1]) > netfile ;

		# handle the cpus
		if (d[1] == "node") {
		    printf "%d %d\n", d[2], d[3] >> node_file
		    if (NF >= 4) { # assume 1 cpu
			n = split($4, cpua, "[.:]");
			printf(" %d %d %s", cpua[2], cpua[3], cpua[4]) > netfile ;
			printf(" %d %d %s\n", cpua[2], cpua[3], cpua[4]) > cpu_file ;
		    }
		    if (NF >= 5) { # assume 2 cpus
			n = split($5, cpub, "[.:]");
			printf(" %d %d %s", cpub[2], cpub[3], cpub[4]) > netfile ;
			printf(" %d %d %s\n", cpub[2], cpub[3], cpub[4]) > cpu_file ;
		    }
		}

		printf("\n") > netfile;

		if (d[1] == "node") {
		    nodes[dest] = 1;
		}
		next;
	}	
	END {
		num = 0;
		for (r in routers) {
		    n = split(r, a, "[.:]");
		    if (a[1] >= 0 && a[2] >= 0)
			num++;
		}
		print num > nrouter;
		num = 0;
		for (n in nodes)
		    num++;
		print num > nnode;
	}
    '

    NROUTER=`cat $tmp.nrouters`
    NNODE=`cat $tmp.nnodes`

    if $VERBOSE; then _error "NROUTER=$NROUTER NNODE=$NNODE"; fi
    if [ $NNODE -eq 0 -a $NROUTER -eq 0 ]
    then
	_error "$prog: Error: no Origin hardware detected in file \"$netfile\""
	_errorFlush
	exit 1
    fi

    if $VERBOSE
    then
	_error "-- the net (from config) --"
	_errorCat $netfile
	_error "-- cpus ---"
	_errorCat $tmp.cpus
	_error "-- nodes ---"
	_errorCat $tmp.nodes
	_errorFlush
    fi
}

_get_net()
{
    netfile=$1

    if [ "$TYPE" = "config" ]
    then
	_get_net_config $netfile
    else
	_get_net_pminfo $netfile
    fi
}



# Make sure that the network satisfies our assumptions
# Expect the net on stdin.
# Each line of form:  m1 s1 p1 m2 s2 node|router
# Output valid lines on $outfile
# Invalid lines are suppressed and an error msg generated
# Check:
#
#     test#1
#     All links are two way
#     All links are to same port number
#
#     test#2
#     All modules have both routers
#     Routers in modules are connected by one link
#
_verify_net()
{
    infile=$1
    outfile=$2
    
    nawk < $infile -v err=$tmp.errors -v prog=$prog -v "verboseflag=$VERBOSE" -v outfile=$outfile '
    BEGIN {
	verbose = (verboseflag == "true");
        }

    $6 != "router" { print $0 > outfile; next }

	# store the links
	{
	   links[sprintf("%d %d %d",$1,$2,$3)] = sprintf("%d %d",$4,$5);
	   modules[$1] = 1;
        }

    END {
	   if (verbose) {
		printf("--- router links ---\n") >> err
		for (i in links) {
		    printf("%s --> %s\n", i, links[i]) >> err
		}
	   }

	   # test#1 - test each link has corresponding backlink on same port
	   # e.g. 
	   # 1.1.1 -> 2.1, A -> B
	   # 2.1.1 -> 1.1, C -> D
	   for (msp in links) { 
		split(msp, x); # 1.1.1, A
		port = x[3];
		if (port <= 0 || port > 6)
		    printf("Warning: illegal port number: %d\n", port);
		dest_ms = links[msp]; # 2.1, B
		split(dest_ms, y); # 2.1	
		# look up dest on same port
		try_msp = sprintf("%d %d %d", y[1], y[2], port); # 2.1.1, B.1
		if (try_msp in links) {
		    back = links[try_msp]; # 1.1
		    back_port = sprintf("%s %d", back, port);
		    if (back_port == msp) {
			# found the little sucker
			# output the given link
			printf("%d %d %d %d %d router\n", x[1], x[2], x[3], y[1], y[2]) > outfile;
		    }
		    else {
			printf("Warning: Ignored one-way link (%s->%s): missing link (%s->%s)\n", msp, dest_ms, try_msp, msp) >> err
		    }
		}
		else {
		    printf("Warning: Ignored one-way link (%s->%s): port (%s) not connected to anything\n", msp, dest_ms, try_msp) >> err
		}
           }#for all links

	   if (verbose) {
	       # test#2 - test module has 2 connected routers
	       # e.g.
	       # m.1 -> m.2
	       for (m in modules) {
		    found = 0;
		    # go thru each port; expect it to be port 6
		    for (p = 6; p >= 1; p--) {
			# m.1.p
			msp = sprintf("%d %d %d", m, 1, p);
			if (msp in links) {
			    dest = links[msp];
			    want = sprintf("%d %d", m, 2);
			    if (dest == want) {
				found = 1;
				break;
			    }
			}
		    }
		    if (!found) {
			printf("Info: module %d is missing a router\n", m) >> err
		    }
	       }#for all modules
	   }

    }'

    if [ ! -s "$outfile" ]
    then
	touch $outfile
    fi

    if $VERBOSE
    then
	_error "--- culled net ---"
	_errorCat $outfile
	_errorFlush
    fi
}



_get_cubes()
{
    #
    # topology layout
    #
    infile=$1
    edges_file12=$2
    edges_file3=$3
    cubes_file=$4

    touch $cubes_file

    # --- get top ports ---
    nawk < $infile -v err=$tmp.errors -v verboseflag=$VERBOSE -v ports=$tmp.ports '
    BEGIN	{ 
			verbose = (verboseflag == "true");
		}
    $6 == "router" {
		if ($2 == $5) { # s1 == s2
		    port_use[$3]++;
		}
	}
    END {
		if (verbose)
		    printf("--- port_use ---\n") >> err;
		# get top 2 values and other
		# on ties, choose smallest port#
		for (i in port_use) {
		    x = port_use[i];
		    if (verbose)
			printf("port %d: %d times\n", i, x) >> err;
		    if (x > max2 || (x == max2 && i < i2)) {
			if (x > max1 || (x ==  max1 && i < i1)) {
			    max3 = max2;
			    i3 = i2;
			    max2 = max1;
			    i2 = i1;
			    max1 = x;
			    i1 = i;
			}
			else {
			    max3 = max2;
			    i3 = i2;
			    max2 = x;
			    i2 = i;
			}
		    }
		    else { # the other one - we only have 3 ports
			max3 = x;
			i3 = i;
		    }
		}#for
		if (verbose)
		    printf("top ports: %d %d %d\n", i1, i2, i3) >> err;
		printf("port1=%d port2=%d port3=%d\n", i1, i2, i3) > ports;
	}'
    if [ -s $tmp.ports ]
    then
	eval `cat $tmp.ports`
    fi
    if $VERBOSE
    then
	_errorCat $tmp.ports
    fi

    nawk < $infile -v cubesfile=$cubes_file -v err=$tmp.errors -v verboseflag=$VERBOSE -v port3=$port3 -v edges12=${edges_file12}.tmp -v edges3=${edges_file3}.tmp '
    BEGIN	{ 
			verbose = (verboseflag == "true");
		}
    $6 == "router" {
			
		if ($3 == port3) {
		    if ($2 == $5) {# same slot
			printf("%d %d %d\n", $1, $4, $3) > edges3; # m1, m2, p
		    }
		    next;
		}

		# do not consider port3

		if ($2 == $5) { # same slot
		    printf("%d %d %d\n", $1, $4, $3) > edges12; # m1, m2, p
		    plane_modules[$1] = 1;
		    plane_modules[$4] = 1;
		}
		else if ($1 == $4) { # same module different slot
		    other_modules[$1] = 1;
		}
		else {
		    cross_modules[$1] = 1;
		    cross_modules[$4] = 1;
		}
        }
    END {
		# Output all modules which are not linked to
		# another module via links on the same slots
		for (i in other_modules) {
		    if (! (i in plane_modules) && ! (i in cross_modules)) {
			printf("%d -1 -1 -1\n", i) >> cubesfile;
		    }
		}
		for (i in cross_modules) {
		    if (! (i in plane_modules))
			printf("Warning: module %d only has cross links\n", i) >> err;
		}
	}
	'

    if [ -s ${edges_file12}.tmp ]
    then
	cat ${edges_file12}.tmp | sort -n | uniq > $edges_file12
    fi
    if [ -s ${edges_file3}.tmp ]
    then
	cat ${edges_file3}.tmp | sort -n | uniq > $edges_file3
    fi

    if $VERBOSE
    then
	_error "--- edges to edges ---"
	[ -s $edges_file12 ] && _errorCat $edges_file12
	[ -s $edges_file3 ] && _errorCat $edges_file3
	_error "--- matching algorithm ---"
    fi

    if [ -s $edges_file12 ]
    then
        nawk < $edges_file12 -v err=$tmp.errors -v planes=$cubes_file \
                -v verboseflag=$VERBOSE -v prog=$prog \
		-v port1=$port1 -v port2=$port2 '

    BEGIN		{ 
			verbose = (verboseflag == "true");
		    }
    !($1 in used) && !($2 in used) && ($3 == port1) {
			used[$1] = $2; used[$2] = $1;
			if (verbose)
			    printf("1. plane (port %d): %s -> %s\n", $3, $1, $2) >> err
			next;
		    }
    (!($1 in used) || used[$1] != $2) && !($1 in remember) && !($2 in remember) && ($3 == port2){
			remember[$1] = $2; remember[$2] = $1;
			if (verbose)
			    printf("2. plane: (port %d): %s -> %s\n", $3, $1, $2) >> err
			next;
		    }
		    { 
			other[$1] = $2; other[$2] = $1;
			if (verbose)
			    printf("3. plane (port %d): %s -> %s\n", $3, $1, $2) >> err
		    }
    END		{
			# Note: we use int(x) < int(y)
			# to ensure only output in one direction
			# as we store both directions

			# Driven by port1 planes
			for (i in used) {
			    j = used[i];
			    if (j != -1 && int(i) < int(j)) {
				if (i in remember)
				    ir = remember[i];
				else
				    ir = -1;
				if (j in remember)
				    jr = remember[j];
				else
				    jr = -1;
				# 1 plane
				if (ir == -1 && jr == -1)
				    printf("%d %d -1 -1\n", i, j) >> planes;
				# 2 planes
				else if (ir == -1) {
				    remember[j] = -1; remember[jr] = -1;
				    printf("%d %d -1 %d\n", i, j, jr) >> planes;
				}
				# 2 planes
				else if (jr == -1) {
				    remember[i] = -1; remember[ir] = -1;
				    printf("%d %d %d -1\n", i, j, ir) >> planes;
				}
				# all planes
				else if (ir in used && used[ir] != jr) {
				    remember[i] = -1; remember[ir] = -1;
				    remember[j] = -1; remember[jr] = -1;
				    printf("missing plane for cube: %d %d %d %d\n", i, j, ir, jr) >> err;
				    printf("%d %d %d %d\n", i, j, ir, jr) >> planes;
				}
				else {
				    remember[i] = -1; remember[ir] = -1;
				    remember[j] = -1; remember[jr] = -1;
				    used[ir] = -1; used[jr] = -1; # do not reuse plane
				    printf("%d %d %d %d\n", i, j, ir, jr) >> planes;
				}
			    }
			}#for

			# Go thru any left over port2 planes.
			# Expect to be all used up.
			for (i in remember) {
			    ir = remember[i];
			    if (ir != -1 && int(i) < int(ir)) {
				printf("%d -1 %d -1\n", i, ir) >> planes;
			    }
			}
		    }'
    fi

    if [ ! -s "$cubes_file" ]
    then
	_error "$prog: Error: no cubes or partial cubes generated\n"
	exit 1	
    fi

    if $VERBOSE
    then
	_error "--- cubes ---"
	_errorCat $cubes_file
	_errorFlush
    fi

}


_layout_special()
{
    culled_file=$1
    cpu_file=$2
    node_file=$3

    if [ $NROUTER -eq 0 ]
    then
        if [ $NNODE -eq 0 ]
	then
	    #
	    # non Origin Hardware
	    #
	    # should be detected earlier
	    _error "$prog: Error: no Origin routers or nodes detected"
	    exit 1
	elif [ $NNODE -le 2 ]
	then
	    # O200 with 1 or 2 nodes
	    # NOTE: special case in oview for an O200
	    # (router with module < 0 and slot < 0 means link the two nodes)
	    if $VERBOSE
	    then
		_error "_layout_special: cpu_file";
		_errorCat $cpu_file
		_error "_layout_special: node_file";
		_errorCat $node_file
		_errorFlush
	    fi

	    echo "router:-1.-1 0 0 0"
	    nawk -v cpu_file=$cpu_file '
		BEGIN {
		    nnodes = 0
		    ncpus = 0
		    while (getline l <cpu_file == 1) {
			ncpus++
			n = split(l, a, " ");
			cpus[ncpus] = a[1]" "a[2]" "a[3]
		    }
		    close(cpu_file)
		}
		NF == 2 {
		    nnodes++
		    modules[nnodes] = $1
		    slots[nnodes] = $2
		}
		END {
		    for (i=1; i <= nnodes; i++) {
			m = modules[i]
			s = slots[i]
			printf "\tlink rport:-%d.-%d.-%d node:%d.%d", i, i, i, m, s
			for (c=1; c <= ncpus; c++) {
			    if (cpus[c] == m" "s" a")
				printf " cpu:%d.%d.a", m, s
			    if (cpus[c] == m" "s" b")
				printf " cpu:%d.%d.b", m, s
			}
			printf "\n"
		    }
		} ' $node_file
	else
	    _error "$prog: Error: do not support more than 2 nodes for 0 routers\n"
	    exit 1
	fi
    elif [ $NROUTER -eq 1 ]
    then
	#
	# Minimal O2000 or "Star" configuration.
	#
	r=`nawk '$6 == "node" {printf "%d.%d", $1, $2; exit}' $culled_file`
	echo "router:$r 0 0 0"
	nawk -v cpu_file=$cpu_file '
	    $6 == "node" {
		printf "\tlink rport:%d.%d.%d node:%d.%d", $1, $2, $3, $4, $5
		while (getline l <cpu_file == 1) {
		    n = split(l, a, " ");
		    if (n == 3 && a[1] == $4 && a[2] == $5)
			printf " cpu:%d.%d.%s", a[1], a[2], a[3]
		}
		close(cpu_file)
		printf "\n"
	    }' $culled_file
    else
	_error "$prog: Internal Error in _layout_special(): number of routers is greater than 1\n"
	exit 1
    fi
}

_layout_cubes()
{
    culled_file=$1
    cubes_file=$2
    layout_file=$3

    #
    # O2000 with one or more cubes or part thereof
    #
    rows=`echo "scale=5; sqrt($numcubes)" | bc | sed 's/\..*//'`
    cols=$rows
    size=`expr $rows \* $cols`
    if $VERBOSE
    then
        _error "--- coordinates ---"
	_error "numcubes = $numcubes, rows = $rows, cols = $cols, size = $size"
    fi
    if [ $size -lt $numcubes ]
    then
	rows=`expr $rows + 1`
    fi

    # create input file of 
    # <culled_file>
    # ----
    # <cubes_file>
    sep="----"
    input_file=$tmp.combo
    cp $culled_file $input_file
    echo $sep >> $input_file
    cat $cubes_file >> $input_file

    nawk <$input_file -v sep=$sep -v num=$numcubes -v rows=$rows -v cols=$cols -v err=$tmp.errors -v "verboseflag=$VERBOSE" '

    function exists_router(module, slot)
    {
	ms = sprintf("%d %d", module, slot);
	return (ms in routers);
    }

    BEGIN	{ 
	       verbose = (verboseflag == "true");
	       net=1; 
	       xpos=0; zpos=0; row=0; col=0; 
	       if (num == 2) shift=2; else shift=3; 
	       if (verbose) {
		   printf("rows = %d, cols = %d\n", rows, cols) >> err;
	       }
	    } 

    $1 == sep { net = 0; 
		if (verbose) {
		    printf("--- routers ---\n") >> err;
		    for (i in routers)
			printf "%s\n", i >> err;
		}
		next;
	      }

    net == 1  {
		    # note the modules for later use
		    ms = sprintf("%d %d", $1, $2);
		    routers[ms] = 1;
		    next;
	      }


     {
		if (exists_router($1, 1))
		    printf("%d 1 %d %d %d\n", $1, xpos, 1, zpos+1);
		if (exists_router($1, 2))
		    printf("%d 2 %d %d %d\n", $1, xpos+1, 1, zpos+1);
		if (exists_router($2, 1))
		    printf("%d 1 %d %d %d\n", $2, xpos, 1, zpos);
		if (exists_router($2, 2))
		    printf("%d 2 %d %d %d\n", $2, xpos+1, 1, zpos);
		if (exists_router($3, 1))
		    printf("%d 1 %d %d %d\n", $3, xpos, 0, zpos+1);
		if (exists_router($3, 2))
		    printf("%d 2 %d %d %d\n", $3, xpos+1, 0, zpos+1);
		if (exists_router($4, 1))
		    printf("%d 1 %d %d %d\n", $4, xpos, 0, zpos);
		if (exists_router($4, 2))
		    printf("%d 2 %d %d %d\n", $4, xpos+1, 0, zpos);
    }

    {
	col++;
	if (col == cols) {
	    row++;
	    col = 0;
	}
	xpos=col*shift;
	zpos=row*shift;
	if (verbose) {
	    printf("coord: col = %d, row = %d, xpos = %d, zpos = %d\n", col, row, xpos, zpos) >> err;
	}
    }' >$layout_file

    if [ ! -s "$layout_file" ]
    then
	_error "$prog: Error: no layout file generated\n"
	exit 1	
    fi

    if $VERBOSE
    then
	_error "--- layout ---"
	_errorCat $layout_file
	_errorFlush
    fi

    return 0
}


_show_links()
{
    #
    # Links
    #
    layout=$1
    culled=$2
    cat $layout \
    | while read mod slot x y z
    do
	echo "router:$mod.$slot $x $y $z"

	nawk -v mod=$mod -v slot=$slot '
	    $1 == mod && $2 == slot {
		printf("\tlink rport:%d.%d.%d %s:%d.%d", $1, $2, $3, $6, $4, $5)
		if ($6 == "node") {
		    if (NF == 9)
			printf " cpu:%s.%s.%s", $7, $8, $9
		    else
		    if (NF == 12)
			printf " cpu:%s.%s.%s cpu:%s.%s.%s", $7, $8, $9, $10, $11, $12
		}
		printf("\n")
	}' $culled
    done
}

_orient_cubes()
{
    edge_file=$1 # only has port3 links
    cube_file=$2
    
    cp $cube_file $tmp.cubes.in # copy for input

    if $VERBOSE
    then
	_error "--- reorienting cube... ---"
    fi

    nawk < $edge_file > $cube_file -v port=$port -v cube_file=$tmp.cubes.in -v verboseflag=$VERBOSE -v err=$tmp.errors '
        BEGIN { 
	        verbose = (verboseflag == "true");
	}
	{ edges[$1] = $2; } # extract the non-cube links
	END {
		sts = getline line < cube_file;
		if (sts <= 0) {
		    printf("Internal Error: orient_cubes: failed to read 1st line of %s\n", cube_file) >> err;
		    exit;
		}
		n = split(line, cube1, " ");
		if (n != 4) {
		    printf("Internal Error: orient_cubes: wrong number of fields: %d\n", n) >> err;
		    exit;
		}
		sts = getline line < cube_file;
		if (sts <= 0) {
		    printf("Internal Error: orient_cubes: failed to read 2nd line of %s\n", cube_file) >> err;
		    exit;
		}
		n = split(line, cube2, " ");
		if (n != 4) {
		    printf("Internal Error: orient_cubes: wrong number of fields: %d\n", n) >> err;
		    exit;
		}

		printf("%d %d %d %d\n", cube1[1], cube1[2], cube1[3], cube1[4]); 
		for (i = 1; i <= 4; i++) {

		    src = cube1[i];
		    if (verbose) {
			printf("i = %d, src = %d\n", i, src) >> err;
		    }
		    if (src in edges) {
			dest = edges[src];
			if (i == 1) { # diff inner
			    j = 2; k = 3; l = 4;
			}
			else if (i == 2) { # diff outer
			    j = 1; k = 4; l = 3;
			}
			else if (i == 3) {
			    j = 4; k = 1; l = 2;
			}
			else { # i == 4
			    j = 3; k = 2; l = 1;
			}
			if (verbose) {
			    printf("dest = %d\n", dest) >> err;
			    printf("i = %d, j = %d, k = %d, l = %d\n", i, j, k, l) >> err;
			}
			if (dest == cube2[j]){ 
			    # all ok 
			    printf("%d %d %d %d\n", cube2[1], cube2[2], cube2[3], cube2[4]); 
			    done = 1;
			    break;
			}
			else if (dest == cube2[i]) {
			    # inner_flip
			    printf("%d %d %d %d\n", cube2[2], cube2[1], cube2[4], cube2[3]); 
			    done = 1;
			    break;
			}
			else if (dest == cube2[k]) {
			    # inner_flip + outer_flip
			    printf("%d %d %d %d\n", cube2[4], cube2[3], cube2[2], cube2[1]); 
			    done = 1;
			    break;
			}
			else if (dest == cube2[l]) {
			    # outer_flip
			    printf("%d %d %d %d\n", cube2[3], cube2[4], cube2[1], cube2[2]); 
			    done = 1;
			    break;
			}
		    }

		}#for
	    if (!done) {
		printf("%d %d %d %d\n", cube2[1], cube2[2], cube2[3], cube2[4]); 
	    }
	}
    ' 

    if [ ! -s "$cube_file" ]
    then
	_error "$prog: Error: no cubes or partial cubes generated\n"
	exit 1	
    fi

    if $VERBOSE
    then
	_error "--- reoriented cubes ---"
	_errorCat $cube_file
	_errorFlush
    fi
}

#
# main
#
#
_parse_args $*
_get_net $tmp.net
_verify_net $tmp.net $tmp.culled

if [ $NROUTER -lt 2 ]
then

    $VERBOSE && _errorFlush
    _layout_special $tmp.culled $tmp.cpus $tmp.nodes

else
    _get_cubes $tmp.net $tmp.edges12 $tmp.edges3 $tmp.cubes

    numcubes=`wc -l $tmp.cubes | awk '{print $1}'`
    if [ $numcubes -eq 2 -a -s $tmp.edges3 ] 
    then
	_orient_cubes $tmp.edges3 $tmp.cubes
    fi

    _layout_cubes $tmp.net $tmp.cubes $tmp.layout

    $VERBOSE && _errorFlush
    _show_links $tmp.layout $tmp.culled
fi

exit 0
