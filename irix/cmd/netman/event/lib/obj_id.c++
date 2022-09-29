/*
 * obj_id.c++ -- implementation of the objID class
 *	
 *  The obj_id class defines the naming and addressing objects on the network. 
 *  Each object potentially a name,  network address,  and MAC address.
 * 
 * $Revision: 1.4 $
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

#include <string.h> 
#include <CC/osfcn.h>
#include <sys/param.h>
#include <sys/types.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "event.h"


/*
 * The constructor for the class attempts to get a network address for an object
 * for which it is only given a name. The philosophy is that the more info to 
 * work with the better.
 */
EV_objID::EV_objID(char *n, char *a, char *m )
{
    /*
     * Must check the pointer since my duping of the string uses strdup
     * which will core dump with a NULL pointer passed in.
     * XXXX Do I want to protect the user from this? What if it is NULL?
     * When will the user know about the mistake? Perhaps I should core
     * dump.
     */
     
    name = NULL;
    addr = NULL;
    MACAddr= NULL;
    
    if (n) {
	name = strdup(n);
    }
    
    if (a) {
	addr = strdup(a);
    }
    else if (n) { // name supplied but no address. See if I can get the address
	struct hostent	*h;
	h = gethostbyname(n);
	if (h) {
	    addr = strdup(inet_ntoa(*(struct in_addr *)(h->h_addr_list[0])));
	}

    }
    
    if (m) {
	MACAddr = strdup(m);
    }
}

EV_objID::~EV_objID(void) {
    delete [] addr;
    delete [] name;
    delete [] MACAddr;
}

/*
 * The set??? functions set class variables name, addr,  and MACAddr
 * XXXX I check that the user gives me a good pointer. Maybe I sholuldn't.
 * Perhaps,  I should let the user application core dump.
 */
void
EV_objID::setName(char *n) {
    if (n) {			// make sure use supplied a name
	if (name) {		// already pointing to a name
	    delete [] name;	// deallocate that space
	}		
	name = strdup (n);	// allocate and copy
	if (addr) {		// see if I can find the host address as well
	    delete [] addr;
	    addr = NULL;
	}
	struct hostent  *h;
	h = gethostbyname(n);
	if (h) {
	    addr = strdup(inet_ntoa(*(struct in_addr *)(h->h_addr_list[0])));
	}
	    
    }
}
 
void
EV_objID::setAddr(char *a) {
    if (a) {			// make sure user supplied an addr
	if (addr) {		// already pointing to an addr
	    delete [] addr;	// deallocate that space
	}		
	addr = strdup (a);	// allocate and copy
    }
}

void
EV_objID::setMACAddr(char *m) {
    if (m) {			// make sure user supplied a MACaddr
	if (MACAddr) {		// already pointing to a MACaddr
	    delete [] MACAddr;	// deallocate that space
	}		
	MACAddr = strdup (m);	// allocate and copy
    }
}
/*
 * These get??? routines return a pointer to the data. It can be modified by the user
 * but it should not be. I could change this to allocate memory and make a copy, 
 * returning a pointer to the copy. But this is expensive and forces the user to
 * remember to tidy up.
 */
 
char *
EV_objID::getName(void) {
    return (name);
}

char *
EV_objID::getAddr(void) {
    return (addr);
}

char *
EV_objID::getMACAddr(void) {
    return (MACAddr);
}

/*
 * The localHost is a subclass of the obj_ID class.
 */
 
EV_localHost::EV_localHost (void){
    char *lname = new  char[MAXHOSTNAMELEN + 1];
    if (gethostname( lname,  MAXHOSTNAMELEN) != 0) {
	perror("EV_localHost: gethostname");
    }
    else {
	setName(lname);
    }
    delete [MAXHOSTNAMELEN + 1] lname;
}

EV_localHost::~EV_localHost(void) {

}
