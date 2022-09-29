#ifndef __top_h
#define __top_h
/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Top nodes class
 *
 *	$Revision: 1.8 $
 *	$Date $
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

#include <tuXExec.h>

#include "addrlist.h"
#include "constants.h"
#include "histogram.h"

extern "C" {
#include <rpc/types.h>
#include <index.h>
#include "snoopstream.h"
}

class tuGadget;
class tuTimer;
class tuXExec;

class NetTop;
class EV_event;


struct topKey {
    struct etheraddr    tk_eaddr;
    unsigned long	tk_naddr;
    char		tk_name[AL_NAMELEN];
};

struct topPairKey {
    struct etheraddr    tpk_Seaddr;
    unsigned long	tpk_Snaddr;
    struct etheraddr    tpk_Deaddr;
    unsigned long	tpk_Dnaddr;
    char		tpk_Sname[AL_NAMELEN];
    char		tpk_Dname[AL_NAMELEN];
};

struct topValue {
    struct counts	tv_count;
};


class Top
{
public:
	Top(NetTop *);
	
	void		setExec(tuXExec *e);
	tuXExec*	getExec()		{ return exec; }
	void		removeSnpStmCB()	{ exec->removeCallBack(alSnpStm.ss_sock); }

	
	int		setNodeUpdateTime(int);
	int		getNodeUpdateTime()	{ return nodeUpdateTime; }

	int		startSnooping();
	int		stopSnooping();
	void		setOutput(tuBool out)	{ doOutput = out; }
	
	void		setWhichAxes(int);
	void		setWhichSrcs(int);
	void		setWhichDests(int);
	void		setWhichNodes(int);
	void		setBusyBytes(tuBool);
	tuBool		getBusyBytes()		{ return busyBytes; }

	void		setNumSrcs(int);
	void		setNumDests(int);
	
	void		newFilter();

	void		setUseEaddr(tuBool);
	tuBool		getUseEaddr();
	int		restartSnooping();

private:
	NetTop		*nettop;
	tuXExec		*exec;
	SnoopStream	alSnpStm;
	
	EV_event	*event;
	EV_event	*alarmEvent;
	
	struct alpacket	al;
	struct hostent* hp;
	tuTimer		*topTimer;
	
	h_info*		currentSnooperp;
	h_info*		newSnooperp;
	
	char		filter[300];
	int		nodeUpdateTime;	// in tenths of a second
	int		entries;
	int		highestBin;
	
	int		whichAxes; // srcs&dests, pairs, nodes&filters
	int		whichSrcs; // busy, specific nodes
	int		whichDests;
	int		whichNodes;
	tuBool		busyBytes;
	
	int		numSrcs;
	int		numDests;
	int		hashSize;
	tuBool		snooping;
	tuBool		subscribed;
	tuBool		doOutput;

	Index		*topPairIndex;
	Index		*topSrcIndex;
	Index		*topDestIndex;
	
	int		snoopInit();
	void		doFilter();
	void		readData(tuGadget*);
	void		updatePairEntry(struct topPairKey*, struct alentry*);
	void		updateEntry(Index*, struct topKey*, struct alentry*);
	void		calcTop(tuGadget*);
	void		findTopNPairs();
	void		findTopN(Index*, int, int);
	void		free_index(Index*);
	
};
#endif /* __top_h */
