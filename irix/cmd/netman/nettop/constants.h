#ifndef __constant_h
#define __constant_h

/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	various constants for NetTop
 *
 *	$Revision: 1.7 $
 *	$Date: 1992/10/14 02:22:37 $
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

#define ENV_NAME	"NETTOPOPTS"
#define CONFIG_FILE	".nettoprc"
#define GENERAL_HELP_CARD "/usr/lib/HelpCards/NetTop.general.help"
#define DATA_HELP_CARD	  "/usr/lib/HelpCards/NetTop.traffic.help"
#define NODE_HELP_CARD	  "/usr/lib/HelpCards/NetTop.node.help"

#define FILENAMESIZE	256
#define LINESIZE	256
#define NAMELEN		30

#define MAX_NODES        10 /* max number of src or dest nodes */

#define MIN_DATA_UPDATE_TIME	1
#define MAX_DATA_UPDATE_TIME	600
#define MIN_NODE_UPDATE_TIME	10
#define MAX_NODE_UPDATE_TIME	600
#define MIN_SCALE_UPDATE_TIME	1
#define MAX_SCALE_UPDATE_TIME	600
#define MIN_INTERP_PERC		0
#define MAX_INTERP_PERC		100

#define ERR_NAME "error"

/*			DATA OPTIONS */

/*		what to display */
#define TYPE_PACKETS	0
#define TYPE_BYTES	1
#define TYPE_PPACKETS	2
#define TYPE_PBYTES	3
#define TYPE_PETHER	4
#define TYPE_PFDDI	5
#define TYPE_PNPACKETS	6
#define TYPE_PNBYTES	7
#define NUM_GRAPH_TYPES	8

static char* graphTypes[] = { "Packets", "Bytes", "%Packets", "%Bytes",
                "%Ether", "%FDDI", "%Npackets", "%Nbytes", 0 };

static char* heading[] = {
	"Packets Per Second", 
	"Bytes Per Second", 
	"Percent of total packets", 
	"Percent of total bytes", 
	"Percent of Ethernet capacity", 
	"Percent of FDDI capacity", 
	"packets/sec", 
	"bytes/sec", 
	0
    };
    
#define FIXED		1
#define KEEP_MAX	0
#define TIMED		2
#define NUM_RESCALE_TYPES 3
static char* rescaleTypes[] = { "Lock", "NeverReduce", "Reduce", 0 };


/*			DISPLAY OPTIONS */

/*		projection type */
#define PROJ_ORTHO	0
#define PROJ_PERSP	1

/*		how to draw */
#define DRAW_SQUARE	0
#define DRAW_ROUND	1
#define DRAW_WIRE	2
#define DRAW_SURFACE	3


/*			NODE OPTIONS */

/*		how to display name (largely copied from netlook/visualnet.h) */
#define DMFULLNAME	1   /* Display as host name and domain  */
#define DMHOSTNAME	2   /* Display as local name            */
#define DMINETADDR	3   /* Display as Internet address      */
#define DMDECNETADDR    4   /* Display as DECnet address        */
#define DMENETVENDOR    5   /* Display as vendor code           */
#define DMENETADDR	6   /* Display as ethernet address      */
#define DMFILTER	7   /* Display the filter               */
#define DMMAX		DMFILTER
#define NUM_LABEL_TYPES DMMAX
static char* labelTypes[] = { "", "full", "local", "ip", "dec", "vendor",
				"physical", "filter", 0 };

/*		what to display */
#define NODES_TOP	    0
#define NODES_SPECIFIC	    1
#define NUM_NODE_TYPES	    2
static char* srcTypes[] =  { "topSrcs",  "specificSrcs", 0};
static char* destTypes[] = { "topDests", "specificDests", 0};
static char* nodeTypes[] = { "topNodes", "specificNodes", 0};
static char* genericTypes[] = { "Busy", "Specific", 0};

#define TOP_SRCS	    0
#define TOP_DESTS	    1
#define TOP_NODES	    2

#define AXES_SRCS_DESTS	    0
#define AXES_BUSY_PAIRS     1
#define AXES_NODES_FILTERS  2
#define NUM_AXES_TYPES	    3
static char* axesTypes[] = { "SourcesAndDestinations", "BusyPairs",
			    "NodesAndFilters", 0};

#define BUSY_BYTES	    0
#define BUSY_PACKETS	    1
#define NUM_BUSY_TYPES	    2
static char* busyTypes[] = { "bytes", "packets", 0};


/* this is here only so snoop.c, netTop.c++ and top.c++
 * can include it
 * (all other .h files are c++,  so snoop.c can't use them)
 */
typedef struct {
    char    node[100];		/* xxx length ?? */
    char    interface[200];	/* xxx length ?? need long for pathname*/
} h_info;


#endif /* __constant_h_ */
