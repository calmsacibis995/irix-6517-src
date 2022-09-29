/* proctbl.c++  - keep track of event server client pids
 * 
 *	NetVisualyzer Event Server
 *
 *	$Revision: 1.2 $
 *
 */
 
/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
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

#include "proctbl.h"

#define D_PRINTF(stuff) \
    if (EV_debugging) fprintf stuff;

extern int EV_debugging;

/*
 * procTable - constructor. create the table and initialize it
 */
procTable::procTable(int sz) {
    size = sz;			//total size of table
    pcount = 0;			// count of client pids
    pa = new pid_t[size];	// create the table itself
    for (int i = 0; i < size; i++) {
	pa[i] = (PID_MAX+1);	//initialize entries to invalid pid
    }
}

/*
 * grow -- double the size of the table if it become full.
 */
int
procTable::grow(void) {
    int newsize = 2*size;
    int i;
    D_PRINTF ((stderr, "procTable::grow expanding table from %d to %d\n", 
		size, newsize));
    if (EV_debugging) dump();
    tpa = new pid_t[newsize];	//allocate a new larger table
    if (! tpa) {
	return 0;
    }
    for (i = 0; i < size; i++) {    //copy current table
	tpa[i] = pa[i];
    }
    for (i = size; i < newsize; i++) {	//initialize the new blank entries
	tpa[i] = (PID_MAX+1);
    }
    delete [size] pa;	//delete the old space
    pa = tpa;		//point to the new space
    size = newsize;	//record new size
    return (size);
}

/*
 * add - add a client to the table
 */
Bool
procTable::add(pid_t pid) {
    if (pcount == size) {   //	I'm full
	if (! grow()) {
	    syslog(LOG_ERR,
		"procTable::add: cannot grow table to add client %d, pid %d", 
		 (pcount +1), pid);
	    return FALSE;
	}
    }
    
    /*
     * Find first empty slot and stuff the pid in it.
     */
    for (int i = 0; i < size; i++) {
	if (pa[i] == (PID_MAX+1)) {
	    pa[i] = pid;
	    pcount++;
	    break;
	}
    }
    if (EV_debugging) dump();
    return TRUE;
}

/*
 * remove - remove an entry from the table
 */
int
procTable::remove (pid_t pid) {
    int oldpcount = pcount;
    for (int i = 0; i < size; i++) {
	if (pa[i] == pid) {
	    pa[i] = (PID_MAX+1);
	    pcount--;
	    break;
	}
    }
    // if the pid wasn't found return 0 else the new count
    return ((pcount == oldpcount) ? 0 : pcount);
}

/*
 * scan the pid table looking to see if any of the processes have disappeared.
 * If they have, remove the entry from the table.
 * Return FALSE (0) if there are no processes left at all otherwise TRUE(1)
 * 
 * Note I always scan the entire table.
 */
int
procTable::scan() {
    int found = FALSE;
    for (int i = 0; i < size; i++) {	// scan entire table
	if (pa[i] != (PID_MAX+1)) {	// look at only the possible existing
	  if (extant(pa[i])) {		// client still there?
	      found = TRUE;		// yup. Found at least one
	  }
	  else {
	      pa[i] = (PID_MAX+1);	//client gone, mark it so		
	  }
	 
	}
    }
    return (found);
}

Bool 
procTable::extant (pid_t pid) {
    int rslt;
    if( (rslt = kill(pid, 0) == -1) && errno == ESRCH){
	D_PRINTF ( (stderr, "procTable::extant pid %d does not exist\n", pid));
	return FALSE;
    }
    else if (rslt == 0) {
	D_PRINTF ((stderr, "procTable::extant pid %d exists\n", pid));
	return TRUE;
    }
    else {
	syslog (LOG_ERR, "procTable::extant: pid %d: kill errno %d",
		pid,  errno);
	return TRUE;
    }
}

void
procTable::dump(void) {
    D_PRINTF ((stderr, "procTabel::dump: # clients: %d ", pcount));
    for (int i = 0; i < size; i++) {
	D_PRINTF((stderr, "%d: %d, ", i, pa[i]));
    }
    D_PRINTF ((stderr, "\n"));
    
}
