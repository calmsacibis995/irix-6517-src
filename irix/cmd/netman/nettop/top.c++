/*
 * Copyright 1991, 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Top nodes class
 *
 *	$Revision: 1.20 $
 *	$Date: 1992/10/23 06:21:25 $
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

#include "constants.h"


extern "C" {
#include <sys/types.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netdb.h>
#include <signal.h>
#include <string.h>
#include <bstring.h>
#include <exception.h>

#include "snoopstream.h"

char *ether_ntoa(struct etheraddr *);
struct etheraddr *ether_aton(char *);

#include "addrlist.h"
#include "expr.h"
#include "protocol.h"
#include "snooper.h"

int al_init(SnoopStream*,  h_info*, int, int);
int al_read(SnoopStream*, struct alpacket*);

} // extern C

#include <sys/time.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include <tuCallBack.h>
#include <tuTimer.h>

#include "messages.h"
#include "netTop.h"
#include "top.h"
#include "event.h"


// value to initialize topNodes array
#define INIT_VALUE -1.0
// interval for addrlist snooping (in tenths of a second)
#define ALIST_TIME 50
static tuBool useEaddr; // use ethernet addresses (rather than ip)
// xxx is this a problem that I only deal with ether and ip?????

extern EV_handler *eh;

static unsigned int
topPairHash(struct topPairKey *tpkp, int) {
    unsigned int hash;
    if (useEaddr || tpkp->tpk_Snaddr == 0)
	hash = (tpkp->tpk_Seaddr.ea_vec[3] + tpkp->tpk_Seaddr.ea_vec[4] +
		 tpkp->tpk_Seaddr.ea_vec[5]);
    else
	hash = (unsigned int) tpkp->tpk_Snaddr;
	
    if (useEaddr || tpkp->tpk_Dnaddr == 0)
	hash += (tpkp->tpk_Deaddr.ea_vec[3] + tpkp->tpk_Deaddr.ea_vec[4] +
		 tpkp->tpk_Deaddr.ea_vec[5]);
    else
	hash += (unsigned int) tpkp->tpk_Dnaddr;
    
    return hash;
}

static unsigned int
topHash(struct topKey *tkp, int) {
    if (useEaddr || tkp->tk_naddr == 0) {
	return(tkp->tk_eaddr.ea_vec[3] + tkp->tk_eaddr.ea_vec[4] +
	       tkp->tk_eaddr.ea_vec[5]);
    } else {
	return tkp->tk_naddr;
    }
}

static int
topPairCmp(struct topPairKey *tpkp1, struct topPairKey *tpkp2, int size) {
    if (useEaddr ||
	(tpkp1->tpk_Snaddr == 0 && tpkp2->tpk_Snaddr == 0) ||
	(tpkp1->tpk_Dnaddr == 0 && tpkp2->tpk_Dnaddr == 0)) {
	return bcmp(tpkp1, tpkp2, size);
	
    } else {	
	return ((tpkp1->tpk_Snaddr != tpkp2->tpk_Snaddr) ||
		(tpkp1->tpk_Dnaddr != tpkp2->tpk_Dnaddr));
    }
}

static int
topCmp(struct topKey *tkp1, struct topKey *tkp2, int size) {
    if (useEaddr || (tkp1->tk_naddr == 0 && tkp2->tk_naddr == 0)) {
	return bcmp(tkp1, tkp2, size);
    } else {
	return (tkp1->tk_naddr != tkp2->tk_naddr);
    }
}

static indexops topPairOps = { (unsigned int (*)()) topPairHash, (int (*)()) topPairCmp, 0 };
static indexops topOps = { (unsigned int (*)()) topHash, (int (*)()) topCmp, 0 };


tuDeclareCallBackClass(TopCallBack, Top);
tuImplementCallBackClass(TopCallBack, Top);

Top::Top(NetTop *nt) {
    // Save nettop pointer
    nettop = nt;
    // init some vars
    alSnpStm.ss_sock = -1;
    highestBin = -1;
    doOutput = False;
    snooping = False;
    subscribed = False;
    useEaddr = False; // xxx
    hashSize = 1024;
    
    event = new EV_event();
    alarmEvent = new EV_event();

    busyBytes  = nettop->getBusyBytes();
    whichAxes  = nettop->getWhichAxes();
    whichSrcs  = nettop->getWhichSrcs();
    whichDests = nettop->getWhichDests();
    whichNodes = nettop->getWhichNodes();
    switch (whichAxes) {
	case AXES_SRCS_DESTS:
	    numSrcs    = nettop->getNumSrcs();
	    numDests   = nettop->getNumDests();
	    break;
	case AXES_BUSY_PAIRS:
	    numSrcs    = nettop->getNumPairs();
	    break;
	case AXES_NODES_FILTERS:
	    numSrcs    = nettop->getNumNodes();
	    numDests   = nettop->getNumFilters();
	    break;
    } // switch
    
    topTimer = new tuTimer(True);
    topTimer->setCallBack(new TopCallBack(this, &Top::calcTop));
    setNodeUpdateTime(nettop->getNodeUpdateTime());
    
    
    topPairIndex  = index(hashSize, sizeof(struct topPairKey), &topPairOps);
    topSrcIndex   = index(hashSize, sizeof(struct topKey), &topOps);
    topDestIndex  = index(hashSize, sizeof(struct topKey), &topOps);

    currentSnooperp = nettop->getCurrentSnooper();
    newSnooperp = nettop->getNewSnooper();
    snoopInit();

}

// this must be called after NetTop::snoopInit(), since it doesn't
// duplicate a couple things from there.
int
Top::snoopInit() {
    extern int _yp_disabled;
    int old_yp_disabled = _yp_disabled;
    _yp_disabled = 1;
    initprotocols();
    _yp_disabled = old_yp_disabled;



    bzero(&al, sizeof al);
    int entries = 90; // xxx
    // get host, set up snoop stream
    // printf("about to init addrlist snooping;  %d entries, intvl=%d\n",
    //	entries, ALIST_TIME); // xxx
    int retVal = al_init(&alSnpStm, newSnooperp, entries, ALIST_TIME);
    if (retVal < 0) {
	alSnpStm.ss_sock = -1;
	nettop->setFatal();
	nettop->setQuitNow();
	char ebuf[64];
	sprintf (ebuf, "%s:%s", newSnooperp->node, newSnooperp->interface);
	nettop->post(-1 * retVal, ebuf);
	return -1;
    }
    subscribed = True;
           
    return startSnooping();
} // snoopInit


// when anything changes in nettop that would affect the top filter,
// this is called (number nodes, which nodes, etc.)
// if user wants specific sources and top nodes, use sources as filter
// (and vice-versa)
void
Top::doFilter() {
    // first, make sure we've got a good snoop stream going.
    if (alSnpStm.ss_sock == -1)
	return;

    int totalSrc = nettop->getTotalSrc();
    int otherSrc = nettop->getOtherSrc();
    int totalDest = nettop->getTotalDest();
    int otherDest = nettop->getOtherDest();
    char* ntFilter = nettop->getFilter();
    tuBool first = True;
    
    // if top src & specific dest, use dests as filters
    // if specific src & top dest, use srcs as filters
    // if top pairs, no filter
    // if top src & top dest, no filter
    // if top nodes & filters, use filters as filter
    // (all this is in addition to general filter, if any)
    // Filter looks like:
    //	 ((f1) || (f2) || (f3) || (f4) || (f5)) && general_filter
    // xxx what about 'locked' sources or dests?
    
    if (whichAxes == AXES_SRCS_DESTS &&
	    whichSrcs  == NODES_SPECIFIC &&
	    whichDests == NODES_TOP) {
	       
	for (int i = 0; i < numSrcs; i++) {
	    if (i == totalSrc || i == otherSrc ||
		nettop->srcInfo[i].err ||
		nettop->srcInfo[i].filter == NULL ||
		nettop->srcInfo[i].filter[0] == NULL)
		continue;
		
	    if (first) {
		first = False;
		sprintf(filter, "((%s)", nettop->srcInfo[i].filter);
	    } else {
		strcat(filter, " || (");
		strcat(filter, nettop->srcInfo[i].filter);
		strcat(filter, ")");
	    }
	} // for i
	if (first) {
	    // weren't any src filters
	    if (ntFilter[0])
		sprintf(filter, "%s", ntFilter);
	    else
		sprintf(filter, "snoop");
	} else {
	    if (ntFilter[0]) {
		strcat(filter, ") && ");
		strcat(filter, ntFilter);
	    } else
		strcat(filter, ")");
	}
	
	
    } else if ((whichAxes == AXES_SRCS_DESTS &&
		    whichSrcs == NODES_TOP &&
		    whichDests == NODES_SPECIFIC) ||
	        (whichAxes == AXES_NODES_FILTERS &&
		    whichNodes == NODES_TOP)) {
	       
	for (int i = 0; i < numDests; i++) {
	    if (i == totalDest || i == otherDest ||
		nettop->destInfo[i].err ||
		nettop->destInfo[i].filter == NULL ||
		nettop->destInfo[i].filter[0] == NULL)
		continue;
		
	    if (first) {
		first = False;
		sprintf(filter, "((%s)", nettop->destInfo[i].filter);
	    } else {
		strcat(filter, " || (");
		strcat(filter, nettop->destInfo[i].filter);
		strcat(filter, ")");
	    }
	    
	} // for i
	if (first) {
	    // weren't any dest filters
	    if (ntFilter[0])
		sprintf(filter, "%s", ntFilter);
	    else
		sprintf(filter, "snoop");
	} else {
	    if (ntFilter[0]) {
		strcat(filter, ") && ");
		strcat(filter, ntFilter);
	    } else
		strcat(filter, ")");
	}

	
    } else if (ntFilter[0])
	sprintf(filter, "%s", ntFilter);
    else
	sprintf(filter, "snoop");
	
    // printf("doFilter built: %s\n", filter); // ppp
    
    ExprSource src;
    ExprError err;

    src.src_path = "NetTop";
    src.src_line = 0;
    src.src_buf = filter;
    
    int bin = ss_compile(&alSnpStm, &src, &err);
    highestBin = bin;
    
} // doFilter



void
Top::setExec(tuXExec *e) {
    exec = e;

    if (alSnpStm.ss_sock != -1)
	exec->addCallBack(alSnpStm.ss_sock, False,
	    new TopCallBack(this, &Top::readData));

}


void
Top::newFilter() {
    // printf("Top::newFilter\n");
    if (snooping)
	restartSnooping();
    else
	startSnooping();
} // setFilter


int
Top::startSnooping() {
    if (snooping) {
	// printf("Top::startSnooping -- already snooping,  so won't start\n");
	return 0;
    }
    if ((whichAxes == AXES_SRCS_DESTS &&
	    whichSrcs  == NODES_SPECIFIC &&
	    whichDests == NODES_SPECIFIC) ||
	(whichAxes == AXES_NODES_FILTERS &&
	    whichNodes == NODES_SPECIFIC))
	return 0;
	
    // printf("Top::startSnooping\n");
    
    doFilter();

    if (!ss_start(&alSnpStm)) {
	nettop->post(NOSTARTSNOOPMSG);
	return -1;
    }
    snooping = True;

    // event->setType(NV_START_SNOOP);
    // event->setInterfaceName(newSnooperp->node);
    // eh->send(event);

    topTimer->start(nodeUpdateTime/10.0); // xxx this used to give time, but I don't think it has to.
    return 0;   
} // startSnooping

int
Top::stopSnooping() {
    // printf("Top::stopSnooping\n");
    topTimer->stop();

    if (!snooping) {
	// printf("Top::stopSnooping -- not snooping,  so won't stop\n");
	return 0;
    }
    
    // printf("Top::stopSnooping\n");
    if (alSnpStm.ss_sock != -1)
	if (!ss_stop(&alSnpStm)) {
	    nettop->post(NOSTOPSNOOPMSG);
	    return -1;
	}
    snooping = False;

    // event->setType(NV_STOP_SNOOP);
    // event->setInterfaceName(currentSnooperp->node);
    // eh->send(event);

    return 0;
} // stopSnooping


int
Top::restartSnooping() {
    // printf("Top::restartSnooping()\n");

    stopSnooping();
    
    for (int bin = highestBin; bin >= 0; bin--) {
	if (!ss_delete(&alSnpStm, bin)) {
	    nettop->post(NODELFILTERMSG);
	    return -1;
	}
    }
    highestBin = -1;
    // xxx zero out data area

    if (subscribed) {
	if (!ss_unsubscribe(&alSnpStm)) {
	    nettop->post(SSUNSUBSCRIBEERRMSG);
	    return -1;
	}
	subscribed = False;
    }

    if (alSnpStm.ss_sock != -1) {
	ss_close(&alSnpStm);
	
	exec->removeCallBack(alSnpStm.ss_sock);
	alSnpStm.ss_sock = -1;
    }
    
    int retVal = al_init(&alSnpStm, newSnooperp, entries, ALIST_TIME);
    if (retVal < 0) {
	    alSnpStm.ss_sock = -1;
	    char ebuf[64];
	    sprintf (ebuf, "%s:%s", newSnooperp->node, newSnooperp->interface);
	    nettop->post(-1 * retVal, ebuf);
	    return -1;
    }
    subscribed = True;
    
    exec->addCallBack(alSnpStm.ss_sock, False,
	new TopCallBack(this, &Top::readData));

    startSnooping();
        
    return 0;
} // restartSnooping



void
Top::readData(tuGadget*) {
    // printf("Top::readData   ");
    if (! al_read(&alSnpStm, &al)) {
	nettop->setFatal();
	nettop->post(SSREADERRMSG);
	return;
    }
    
    
    /***
    struct tm *tm;
    tm = localtime(&al.al_timestamp.tv_sec);
    printf("time:   %02d:%02d:%02d.%03ld, ",
	tm->tm_hour, tm->tm_min, tm->tm_sec,
	al.al_timestamp.tv_usec / 1000);
    printf("%d entries\n", al.al_nentries);
    ***/
    
    // hash the src & dest (one or both),
    // find it in the table (or add it),
    // and increment the stored byte & packet counts.
    
    int j;
    struct alentry *e;
	struct topKey tk;
	struct topPairKey tpk;
	
    switch (whichAxes) {
      case AXES_SRCS_DESTS:
	if (whichSrcs == NODES_SPECIFIC && whichDests == NODES_SPECIFIC)
	    return;
		
	// srcs if need to, dests if need to
	for (j = 0, e = al.al_entry; j < al.al_nentries; j++, e++) {
	    // printf("src eaddr: %s\n", ether_ntoa(&e->ale_src.alk_paddr));
	    // printf("dst eaddr: %s\n", ether_ntoa(&e->ale_dst.alk_paddr));
	    if (whichSrcs == NODES_TOP) {
		if (useEaddr || e->ale_src.alk_naddr == 0) {
		    tk.tk_eaddr = e->ale_src.alk_paddr;
		    tk.tk_naddr = 0;
		} else {
		    tk.tk_naddr = e->ale_src.alk_naddr;
		}

		// xxx careful -- phys addr won't always be the same host
		// as the name (if it's a local hop of an internet ip packet)
		strcpy(tk.tk_name, e->ale_src.alk_name);
		updateEntry(topSrcIndex, &tk, e);
	    } // srcs
	
	    // do the same for the destination alone
	    if (whichDests == NODES_TOP) {
		if (useEaddr || e->ale_dst.alk_naddr == 0) {
		    tk.tk_eaddr = e->ale_dst.alk_paddr;
		    tk.tk_naddr = 0;
		} else {
		    tk.tk_naddr = e->ale_dst.alk_naddr;
		} 
		strcpy(tk.tk_name, e->ale_dst.alk_name);
		updateEntry(topDestIndex, &tk, e);
	    } // if dests
	      
	} // for j
	break;


      case AXES_BUSY_PAIRS:
	// do pairs
	for (j = 0, e = al.al_entry; j < al.al_nentries; j++, e++) {
	    if (useEaddr || e->ale_src.alk_naddr == 0) {
		tpk.tpk_Seaddr = e->ale_src.alk_paddr;
		tpk.tpk_Snaddr = 0;
	    } else {
		tpk.tpk_Snaddr = e->ale_src.alk_naddr;
	    }
	    if (useEaddr || e->ale_dst.alk_naddr == 0) {
		tpk.tpk_Deaddr = e->ale_dst.alk_paddr;
		tpk.tpk_Dnaddr = 0;
	    } else {
		tpk.tpk_Dnaddr = e->ale_dst.alk_naddr;
	    }
	    strcpy(tpk.tpk_Sname, e->ale_src.alk_name);
	    strcpy(tpk.tpk_Dname, e->ale_dst.alk_name);
	    updatePairEntry(&tpk, e);
	} // for j
	break;


      case AXES_NODES_FILTERS:
	if (whichNodes == NODES_SPECIFIC)
	    return;

	// use topSrcIndex, but sum src & dest bytes for each node
	// (do src, then do dest, both into same index)
	    
	for (j = 0, e = al.al_entry; j < al.al_nentries; j++, e++) {
	    if (useEaddr || e->ale_src.alk_naddr == 0) {
		tk.tk_eaddr = e->ale_src.alk_paddr;
		tk.tk_naddr = 0;
	    } else {
		tk.tk_naddr = e->ale_src.alk_naddr;
	    } 
	    strcpy(tk.tk_name, e->ale_src.alk_name);
	    updateEntry(topSrcIndex, &tk, e);

	    if (useEaddr || e->ale_dst.alk_naddr == 0) {
		tk.tk_eaddr = e->ale_dst.alk_paddr;
		tk.tk_naddr = 0;
	    } else {
		tk.tk_naddr = e->ale_dst.alk_naddr;
	    } 
	    strcpy(tk.tk_name, e->ale_dst.alk_name);
	    updateEntry(topSrcIndex, &tk, e);

	} // for j
	break;

    } // switch

} // readData


void
Top::updatePairEntry(struct topPairKey *tpkp, struct alentry* e) {
    Entry **ep;
    ep = in_lookup(topPairIndex, tpkp);
    if (*ep == 0) {
	(void) in_add(topPairIndex, tpkp, 0, ep);
    }
    struct topValue *tvp;
    tvp = (struct topValue*) (*ep)->ent_value;
    if (tvp == 0) {
	tvp = new (struct topValue);
	tvp->tv_count.c_packets = e->ale_count.c_packets;
	tvp->tv_count.c_bytes   = e->ale_count.c_bytes;
	(*ep)->ent_value = tvp;
    } else {
	tvp->tv_count.c_packets += e->ale_count.c_packets;
	tvp->tv_count.c_bytes   += e->ale_count.c_bytes;
    }
}
// updatePairEntry

void
Top::updateEntry(Index *idx, struct topKey *tkp, struct alentry *e) {
    Entry **ep;
    ep = in_lookup(idx, tkp);
    if (*ep == 0) {
	(void) in_add(idx, tkp, 0, ep);
    }
    struct topValue *tvp;
    tvp = (struct topValue*) (*ep)->ent_value;
    if (tvp == 0) {
	tvp = new (struct topValue);
	tvp->tv_count.c_packets = e->ale_count.c_packets;
	tvp->tv_count.c_bytes   = e->ale_count.c_bytes;
	(*ep)->ent_value = tvp;
    } else {
	tvp->tv_count.c_packets += e->ale_count.c_packets;
	tvp->tv_count.c_bytes   += e->ale_count.c_bytes;
    }
    
} // updateEntry


// go through the lists of srcs, dests, and pick out the top n of each
// (n comes from nodeGizmo, and could be zero for either).
void
Top::calcTop(tuGadget*) {
    // printf("Top::calcTop\n");
    if ((whichAxes == AXES_SRCS_DESTS &&
	    whichSrcs  == NODES_SPECIFIC &&
	    whichDests == NODES_SPECIFIC) ||
	(whichAxes == AXES_NODES_FILTERS &&
	    whichNodes == NODES_SPECIFIC))
	return;
	
    if (whichAxes == AXES_BUSY_PAIRS)
	findTopNPairs();
    else if (whichAxes == AXES_NODES_FILTERS) {
	findTopN(topSrcIndex, TOP_NODES, 
		numSrcs - nettop->getNumSrcsLocked());
    } else {
	if (whichSrcs == NODES_TOP)
	    findTopN(topSrcIndex, TOP_SRCS,
		numSrcs - nettop->getNumSrcsLocked());
	if (whichDests == NODES_TOP)
	    findTopN(topDestIndex, TOP_DESTS,
		numDests - nettop->getNumDestsLocked());
    }

    nettop->restartSnooping();

}

void
Top::findTopNPairs() {
    Entry **ep, *ent;
    struct topValue *tvp;
    struct topPairKey *tpkp;
    int count = topPairIndex->in_buckets;
    int howMany = numSrcs - 2;
    // printf("Top::findTopNPairs(%d)\n", howMany);

    struct {
	Entry	*entry;
	float	value;
    } topPairs[MAX_NODES-2]; // 0 is the highest, 1 is next highest, etc.
    
    int i, j;
    float value;
    // init the topPairs values with -1 rather than 0,
    // so that we'll be sure to fill the array, even if there aren't howMany
    // nodes with traffic (like at a demo site).
    for (i = 0; i < MAX_NODES-2; i++) {
	topPairs[i].value = INIT_VALUE;
    }

    for (ep = topPairIndex->in_hash; --count >= 0; ep++) {
	for (ent = *ep; ent != 0; ent = ent->ent_next) {
	    if (ent->ent_value != 0) {
		tvp = (struct topValue*) ent->ent_value;


		if (busyBytes)
		    value = tvp->tv_count.c_bytes;
		else
		    value = tvp->tv_count.c_packets;

		for (i = 0; i < howMany; i++) {
		    if (value > topPairs[i].value) {
			// shift the rest
			for (j = howMany - 1; j > i; j--) 
			    topPairs[j] = topPairs[j-1];
			// put this one in
			topPairs[i].entry = ent;
			topPairs[i].value = value;
			break;
		    }
		} // for i
	    }
	} // for ent
    } // for ep

    // clear out data area, so starts fresh with next readData()
    // xxxxx topPairs[i].entry is a POINTER to the entry, which
    // GETS DELETED by free_index!!???  And yet the PRINT_TOP stuff works!
    free_index(topPairIndex);
    
    if (topPairs[0].value == INIT_VALUE) {
//	printf("hmmm.  no data yet - highest value is %f\n",  topPairs[0].value);
	return;
    }


    if (doOutput) {

	struct timeval tval;
	gettimeofday(&tval, 0);
	long clockSec = tval.tv_sec;
	struct tm* timeStruct;
	timeStruct = localtime(&clockSec);
	char timeStr[30];
	ascftime(timeStr, "%c", timeStruct);
	printf("For the %.1f seconds ending %s,\n", 
	    nodeUpdateTime / 10.0, timeStr);

	printf("with filter: %s\n", filter);
	printf("the node pairs transmitting ");
	
	if (busyBytes)
	    printf("the most bytes were:\n");
	else
	    printf("the most packets were:\n");
	
	for (i = 0; i < howMany; i++) {
	    if (topPairs[i].value == INIT_VALUE) {
		break;
	    }
	    tvp = (struct topValue*)topPairs[i].entry->ent_value;
	    tpkp = (struct topPairKey*)topPairs[i].entry->ent_key;

	    printf("   Source:");
	    if (useEaddr || tpkp->tpk_Snaddr == 0)
		printf("   %-15.15s", ether_ntoa(&tpkp->tpk_Seaddr));
	    else if (tpkp->tpk_Snaddr != 0)
		printf("   %-15.15s", inet_ntoa(*(struct in_addr*)&tpkp->tpk_Snaddr));
	    printf("   %s\n", tpkp->tpk_Sname);

	    printf("   Dest:  ");
	    if (useEaddr || tpkp->tpk_Dnaddr == 0)
		printf("   %-15.15s", ether_ntoa(&tpkp->tpk_Deaddr));
	    else if (tpkp->tpk_Dnaddr != 0)
		printf("   %-15.15s", inet_ntoa(*(struct in_addr*)&tpkp->tpk_Dnaddr));
	    printf("   %s\n", tpkp->tpk_Dname);
	
	    printf("      %5.0f packets   %7.0f bytes\n", tvp->tv_count.c_packets, tvp->tv_count.c_bytes);
	} // for i
    
    } // if doOutput


    char *blank;
    nodeInfo* info;
    int size = howMany * sizeof(char*);
	
    for (i = 0; i < howMany; i++) {
	// remember that in nettop, 0th node = "total", 1st = "other"
	// xxx This would be different if allow these to be flexible!
	int nodeNum = i+2;

	info = &nettop->srcInfo[nodeNum];
	if (info->fullname) {
	    delete [] info->fullname;
	    info->fullname = NULL;
	}
	if (info->physaddr) {
	    delete [] info->physaddr;
	    info->physaddr = NULL;
	}
	if (info->ipaddr) {
	    delete [] info->ipaddr;
	    info->ipaddr = NULL;
	}

	info = &nettop->destInfo[nodeNum];
	if (info->fullname) {
	    delete [] info->fullname;
	    info->fullname = NULL;
	}
	if (info->physaddr) {
	    delete [] info->physaddr;
	    info->physaddr = NULL;
	}
	if (info->ipaddr) {
	    delete [] info->ipaddr;
	    info->ipaddr = NULL;
	}
	    
	// If the value is nonzero, then it's good.
	// Otherwise, no more top nodes, so set name to blank, etc.
	if (topPairs[i].value == INIT_VALUE) {
	    nettop->srcInfo[nodeNum].fullname  = strdup("");
	    nettop->destInfo[nodeNum].fullname = strdup("");
	} else {
	    tpkp = (struct topPairKey*)topPairs[i].entry->ent_key;

	    if (tpkp->tpk_Sname) {
		blank = strchr(tpkp->tpk_Sname, ' ');
		if (blank)
		    *blank = '\0';
	    }
	    if (tpkp->tpk_Dname) {
		blank = strchr(tpkp->tpk_Dname, ' ');
		if (blank)
		    *blank = '\0';
	    }

	    nettop->srcInfo[nodeNum].fullname = strdup(tpkp->tpk_Sname);
	    nettop->destInfo[nodeNum].fullname = strdup(tpkp->tpk_Dname);
	    
	    if (ether_aton(tpkp->tpk_Sname) != NULL) {
		nettop->srcInfo[nodeNum].physaddr = strdup(tpkp->tpk_Sname);
		char* slash = strchr(nettop->srcInfo[nodeNum].physaddr, '/');
		if (slash) {
		    *slash = NULL;
		}

	    } else if (useEaddr || tpkp->tpk_Snaddr == 0) {
		nettop->srcInfo[nodeNum].physaddr =
		    strdup(ether_ntoa(&tpkp->tpk_Seaddr));
	    } else {
		nettop->srcInfo[nodeNum].ipaddr =
		    strdup(inet_ntoa(*(struct in_addr*)&tpkp->tpk_Snaddr));
	    }

	    if (ether_aton(tpkp->tpk_Dname) != NULL) {
		nettop->destInfo[nodeNum].physaddr = strdup(tpkp->tpk_Dname);
		char* slash = strchr(nettop->destInfo[nodeNum].physaddr, '/');
		if (slash) {
		    *slash = NULL;
		}
	    } else if (useEaddr || tpkp->tpk_Dnaddr == 0) {
		nettop->destInfo[nodeNum].physaddr =
		    strdup(ether_ntoa(&tpkp->tpk_Deaddr));
	    } else {
		nettop->destInfo[nodeNum].ipaddr =
		    strdup(inet_ntoa(*(struct in_addr*)&tpkp->tpk_Dnaddr));
	    }

	} // if value
    } // for i
    
    nettop->setLabelType(nettop->getLabelType());
} // findTopNPairs


void
Top::findTopN(Index *idx, int whichTop, int howMany) {
    // printf("Top::findTopN(%d)\n", howMany);
    Entry **ep, *ent;
    struct topValue *tvp;
    struct topKey *tkp;
    int count;
    count = idx->in_buckets;

    struct {
	Entry	*entry;
	float	value;
    } topNodes[MAX_NODES-2]; // 0 is the highest, 1 is next highest, etc.
    
    int i, j;
    float value;
    // init the topNodes values with -1 rather than 0,
    // so that we'll be sure to fill the array, even if there aren't howMany
    // nodes with traffic (like at a demo site).
    for (i = 0; i < MAX_NODES-2; i++) {
	topNodes[i].value = INIT_VALUE;
    }

    for (ep = idx->in_hash; --count >= 0; ep++) {
	for (ent = *ep; ent != 0; ent = ent->ent_next) {
	    if (ent->ent_value != 0) {
		tvp = (struct topValue*) ent->ent_value;

#ifdef PRINT_TOP
		tkp = (struct topKey*)ent->ent_key;
		printf("%s\t", ether_ntoa(&tkp->tk_eaddr));
		printf("%s\t", inet_ntoa(*(struct in_addr*)&tkp->tk_naddr));
		if (tkp->tk_name[0] != '\0')
		    printf("%s\t", tkp->tk_name);
		printf("%.0f\t%.0f\n", tvp->tv_count.c_bytes, tvp->tv_count.c_packets);
#endif // PRINT_TOP
		if (busyBytes)
		    value = tvp->tv_count.c_bytes;
		else
		    value = tvp->tv_count.c_packets;

		for (i = 0; i < howMany; i++) {
		    if (value > topNodes[i].value) {
			// shift the rest
			for (j = howMany - 1; j > i; j--) 
			    topNodes[j] = topNodes[j-1];
			// put this one in
			topNodes[i].entry = ent;
			topNodes[i].value = value;
			break;
		    }
		} // for i
	    }
	} // for ent
    } // for ep

    // clear out data area, so starts fresh with next readData()
    // xxxxx topNodes[i].entry is a POINTER to the entry, which
    // GETS DELETED by free_index!!???  And yet the PRINT_TOP stuff works!
    free_index(idx);
    
    if (topNodes[0].value == INIT_VALUE) {
	// printf("hmmm. no data yet - highest value is %f\n",  topNodes[0].value);
	return;
    }



    if (doOutput) {
    
	struct timeval tval;
	gettimeofday(&tval, 0);
	long clockSec = tval.tv_sec;
	struct tm* timeStruct;
	timeStruct = localtime(&clockSec);
	char timeStr[30];
	ascftime(timeStr, "%c", timeStruct);
	printf("For the %.1f seconds ending %s,\n",
	    nodeUpdateTime / 10.0, timeStr);

	printf("with filter: %s\n", filter);
	if (whichTop == TOP_SRCS)
	    printf("the nodes sending ");
	else if (whichTop == TOP_DESTS)
	    printf("the nodes receiving ");
	else
	    printf("the nodes sending and receiving ");
	
	if (busyBytes)
	    printf("the most bytes were:\n");
	else
	    printf("the most packets were:\n");

	for (i = 0; i < howMany; i++) {
	    if (topNodes[i].value == INIT_VALUE) {
		break;
	    }
	    tvp = (struct topValue*)topNodes[i].entry->ent_value;
	    tkp = (struct topKey*)topNodes[i].entry->ent_key;


	    if (useEaddr || tkp->tk_naddr == 0)
		printf("   %-15.15s", ether_ntoa(&tkp->tk_eaddr));
	    else if (tkp->tk_naddr != 0)
		printf("   %-15.15s", inet_ntoa(*(struct in_addr*)&tkp->tk_naddr));
	    printf("   %s", tkp->tk_name);
	    
	    printf("   %5.0f packets   %7.0f bytes\n", tvp->tv_count.c_packets, tvp->tv_count.c_bytes);
	} // for i
    } // if doOutput



    // now copy the names, addresses to nettop's info arrays
    char *blank;
    nodeInfo* info;
    int size = howMany * sizeof(char*);
    if (whichTop == TOP_DESTS)
	info = nettop->destInfo;
    else
	info = nettop->srcInfo; // for srcs and for nodes

    int nodeNum = 2; // we know it can't be total (0) or other (1)
    // xxx this would change (nodeNum = 0) if allow flexibility 
    // with total,other
    for (i = 0; i < howMany; i++, nodeNum++) {
	// find the next node that isn't "locked" (by the nodeGizmo)
	if (whichTop == TOP_DESTS)
	    while (nettop->getDestLocked(nodeNum))
		nodeNum++;
	else
	    while (nettop->getSrcLocked(nodeNum))
		nodeNum++;
	
	if (info[nodeNum].fullname) {
	    delete [] info[nodeNum].fullname;
	    info[nodeNum].fullname = NULL;
	}
	if (info[nodeNum].physaddr) {
	    delete [] info[nodeNum].physaddr;
	    info[nodeNum].physaddr = NULL;
	}
	if (info[nodeNum].ipaddr) {
	    delete [] info[nodeNum].ipaddr;
	    info[nodeNum].ipaddr = NULL;
	}
	    
	// If the value is nonzero, then it's good.
	// Otherwise, no more top nodes, so set name to blank, etc.
	if (topNodes[i].value == INIT_VALUE) {
	    info[nodeNum].fullname = strdup("");
	} else {
	    tkp = (struct topKey*)topNodes[i].entry->ent_key;
/*** // ppp
if (!tkp->tk_name) printf("tk_name null; ");
else printf("tk_name: %s; ", tkp->tk_name);
if (tkp->tk_naddr == 0) printf("tk_naddr is zero; ");
else printf("tk_naddr: %s; ", inet_ntoa(*(struct in_addr*)&tkp->tk_naddr));
if (!ether_ntoa(&tkp->tk_eaddr)) printf("tk_eaddr doesn't convert (ether_ntoa bad)\n");
else printf("tk_eaddr: %s\n", ether_ntoa(&tkp->tk_eaddr));
***/
	    if (tkp->tk_name) {
		blank = strchr(tkp->tk_name, ' ');
		if (blank)
		    *blank = '\0';
	    }

	    info[nodeNum].fullname = strdup(tkp->tk_name);
	    if (ether_aton(tkp->tk_name) != NULL) {
		info[nodeNum].physaddr = strdup(tkp->tk_name);
		char* slash = strchr(info[nodeNum].physaddr, '/');
		if (slash) {
		    *slash = NULL;
		}
		// printf("%s is an ether address:(%s)\n", tkp->tk_name, info[nodeNum].physaddr);
	    } else if (useEaddr || tkp->tk_naddr == 0) {
		info[nodeNum].physaddr =  strdup(ether_ntoa(&tkp->tk_eaddr));
		// printf("no naddr for %s; %s is the phys address\n", tkp->tk_name, info[nodeNum].physaddr);
	    } else {
		info[nodeNum].ipaddr =
		    strdup(inet_ntoa(*(struct in_addr*)&tkp->tk_naddr));
	    }

	} // if value
    } // for i
    
    nettop->setLabelType(nettop->getLabelType());
} // findTopN


void
Top::free_index(Index *idx)
{
    int count;
    Entry **ep, *ent;

    count = idx->in_buckets;
    for (ep = idx->in_hash; --count >= 0; ep++) {
	while ((ent = *ep) != 0) {
	    if (ent->ent_value != 0)
	        delete (struct topValue*) (ent->ent_value);

	    *ep = ent->ent_next;
	    delete(ent);
	}
    }
}


int
Top::setNodeUpdateTime(int t) {
    // printf("Top::setNodeUpdateTime(%d)\n", t);
    
    nodeUpdateTime = t;
    topTimer->setDuration(nodeUpdateTime/10.0);
    if (snooping)
	topTimer->start();

    // xxx what do I do with this? can't change time on al service,
    // just change my own timer for doing the calculations of who's top.
    
    return 0; // xxx -1 if failed
}



void
Top::setWhichAxes(int w) {
    whichAxes = w;
    tuBool mustSnoop;
    
    switch (w) {
    	case AXES_SRCS_DESTS:
	    numSrcs  = nettop->getNumSrcs();
	    numDests = nettop->getNumDests();
	    mustSnoop =
		(whichSrcs == NODES_TOP || whichDests == NODES_TOP);
	    break;
	case AXES_BUSY_PAIRS:
	    numSrcs = nettop->getNumPairs();
	    mustSnoop = True;
	    break;
	case AXES_NODES_FILTERS:
	    numSrcs  = nettop->getNumNodes();
	    numDests = nettop->getNumFilters();
	    mustSnoop = (whichNodes == NODES_TOP);
	    break;
    } // switch
    
    if (mustSnoop) {
	if (snooping) restartSnooping();
	else startSnooping();
    } else {
	stopSnooping();
    }
} // setWhichAxes


// this is how we find out what sources user told nodeGizmo they want to see -
// specific ones, top bytes, or top dests.
// If specific, don't keep track of sources with addrlist snooping.
void
Top::setWhichSrcs(int w) {
    whichSrcs = w;
    if (whichAxes != AXES_SRCS_DESTS)
	return;
	
    if (whichSrcs == NODES_SPECIFIC && whichDests == NODES_SPECIFIC)
	stopSnooping();
    else if (snooping) {
	restartSnooping();
    } else {
	// should seed index with current nodes, so guarantee we'll see something?
	startSnooping();
    }
} // setWhichSrcs

void
Top::setWhichDests(int w) {
    whichDests = w;
    if (whichAxes != AXES_SRCS_DESTS)
	return;
	
    if (whichSrcs == NODES_SPECIFIC && whichDests == NODES_SPECIFIC)
	stopSnooping();
    else if (snooping) {
	restartSnooping();
    } else {
	// should seed index with current nodes, so guarantee we'll see something?
	startSnooping();
    }
} // setWhichDests

void
Top::setWhichNodes(int w) {
    whichNodes = w;
    if (whichAxes != AXES_NODES_FILTERS)
	return;
	
    if (whichNodes == NODES_SPECIFIC)
	stopSnooping();
    else if (snooping) {
	restartSnooping();
    } else {
	startSnooping();
    }
} // setWhichNodes

void
Top::setNumSrcs(int s) {
    numSrcs = s;
    
    // we might have to change snoop filter
    if (snooping && whichAxes == AXES_SRCS_DESTS &&
	whichSrcs  == NODES_SPECIFIC && whichDests  == NODES_TOP) {
	    // doFilter(); restartSnooping handles this (act'y startSnooping)
	    restartSnooping();
	}
} // setNumSrcs

void
Top::setNumDests(int d) {
    numDests = d;

    if (snooping &&
	(whichAxes == AXES_SRCS_DESTS &&
	  whichSrcs  == NODES_TOP && whichDests  == NODES_SPECIFIC) ||
	(whichAxes == AXES_NODES_FILTERS && whichSrcs  == NODES_TOP)) {
	    // doFilter();
	    restartSnooping();
	}    
} // setNumDests


void
Top::setBusyBytes(tuBool b) {
    // printf("Top::setBusyBytes(%s)\n", b ? "True " : "False");
    busyBytes = b;
}

void
Top::setUseEaddr(tuBool e) {
    useEaddr = e;
}

tuBool
Top::getUseEaddr() {
    return useEaddr;
}
