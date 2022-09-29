#ifndef __constant_h
#define __constant_h

/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	various constants for NetGraph
 *
 *	$Revision: 1.7 $
 *	$Date: 1992/10/14 02:53:20 $
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

#define CONFIG_FILE	".netgraphrc"
#define GENERAL_HELP_CARD "/usr/lib/HelpCards/NetGraph.general.help"
#define EDIT_HELP_CARD	  "/usr/lib/HelpCards/NetGraph.edit.help"
#define PARAM_HELP_CARD	  "/usr/lib/HelpCards/NetGraph.param.help"

#define FILENAMESIZE	256
#define LINESIZE	256
#define NAMELEN		30


#define MIN_DATA_UPDATE_TIME	1
#define MAX_DATA_UPDATE_TIME	600
#define MIN_SCALE_UPDATE_TIME	1
#define MAX_SCALE_UPDATE_TIME	600
#define DEF_UPD_TIME		-1
#define DEF_INTERVAL		10


/*		what to display   */
#define TYPE_PACKETS	0
#define TYPE_BYTES	1
#define TYPE_PPACKETS	2
#define TYPE_PBYTES	3
#define TYPE_PETHER	4
#define TYPE_PFDDI	5
#define TYPE_PTOKENRING	6
#define TYPE_PNPACKETS	7
#define TYPE_PNBYTES	8
#define NUM_GRAPH_TYPES	9

static char* graphTypes[] = { "packets", "bytes", "%packets", "%bytes",
		"%ether", "%fddi", "%tokenring", "%npackets", "%nbytes", 0 };

static char* heading[] = {
	"packets per second", 
	"bytes per second", 
	"percent of total packets", 
	"percent of total bytes", 
	"percent of Ethernet capacity", 
	"percent of FDDI capacity", 
	"percent of Token Ring capacity", 
	"packets/sec", 
	"bytes/sec", 
	0
    };
 

#define STYLE_BAR	0
#define STYLE_LINE	1
#define NUM_GRAPH_STYLES	2

static char* graphStyles[] = { "bar", "line", 0 };
static char* graphStyles2[] = { "bar graph", "line graph", 0 };

/* the following are ways we can rescale */
#define SCALE_VARIABLE      0x0
#define SCALE_CONSTANT      0x1
#define SCALE_MAXVALUE      0x2


/* the following are the various ways of displaying the time labels */
#define TIME_SCROLLING 0
#define TIME_ABSOLUTE  1
#define TIME_RELATIVE  2
#define TIME_NONE      3
#define NUM_TIME_TYPES 4

static char* timeTypes[] = { "scrolling", "absolute", "relative", "none", 0};

#define BAD_TKN		-1
#define TYPE_TKN	0
#define STYLE_TKN	1
#define INT_TKN		2
#define ALARMSET_TKN	3
#define ALARMBELL_TKN	4
#define FLOAT_TKN	5


static char* alarmSets[] = { "noalarm", "alarm", 0};
static char* alarmBells[] = { "silent", "bell", 0};

#define ALM_NONE    0

/*** I now use "NV_RATE_THRESH_HI_MET", etc, instead of these 
  #define ALM_WAS_LO  1
  #define ALM_NOW_LO  2
  #define ALM_WAS_HI  3
  #define ALM_NOW_HI  4
***/

static char* snooperTypes[] = { "null", "trace", "local", "remote" };


/* max number of error strings to save up */
#define MAXERRS 10
/* how many lines there can be in the rc file */
#define MAX_RC_LINES 50

#define NOTIFY_DESTROY	700
#define EDIT		800
#define ADD		801
#define DELETE		802
#define SAVE		803
#define SAVEANDQUIT	804
#define QUIT		805
#define CANCEL		806
#define CONTINUE	807
#define NUM		808
#define HELP_OPEN	809
/* HELP_CLOSE is defined in events.h */
#define CATCHUP		810
#define STYLE		811
#define PARAMS		812
#define FREEZE		813


/* this is here only so both snoop.c and netGraph.c++ can include it */
typedef struct {
    char    node[100];		/* xxx length ?? */
    char    interface[200];	/* xxx length ?? must be long for pathname*/
} h_info;



#endif /* __constant_h_ */
