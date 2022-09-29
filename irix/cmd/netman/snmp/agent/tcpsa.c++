/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	TCP sub-agent
 *
 *	$Revision: 1.4 $
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip_var.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_seq.h>
#define  TCPSTATES
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_timer.h>
#include <sys/tcpipstats.h>
#include <sys/sysmp.h>
#include <errno.h>
extern "C" {
#include "exception.h"
};
#include <netinet/tcp_var.h>
#include <netinet/tcp_debug.h>
#include <time.h>
#include <stdio.h>
#include <syslog.h>

#include "oid.h"
#include "asn1.h"
#include "snmp.h"
#include "packet.h"
#include "subagent.h"
#include "sat.h"
#include "agent.h"
#include "table.h"
#include "tcpsa.h"
#include "knlist.h"

const subID tcpRtoAlgorithm = 1;
const subID tcpRtoMin = 2;
const subID tcpRtoMax = 3;
const subID tcpMaxConn = 4;
const subID tcpActiveOpens = 5;
const subID tcpPassivOpens = 6;
const subID tcpAttemptFails = 7;
const subID tcpEstabResets = 8;
const subID tcpCurrEstab = 9;
const subID tcpInSegs = 10;
const subID tcpOutSegs = 11;
const subID tcpRetransSegs = 12;
const subID tcpConnTable = 13;
    const subID tcpConnState = 1;
    const subID tcpConnLocalAddress = 2;
    const subID tcpConnLocalPort = 3;
    const subID tcpConnRemAddress = 4;
    const subID tcpConnRemPort = 5;
    const subID tcpConnValid = 6;	// Internal
const subID tcpInErrs = 14;
const subID tcpOutRsts = 15;

const int routeAlgVanJ = 4;

const int connStateClosed = 1;
const int connStateListen = 2;
const int connStateSynSent = 3;
const int connStateSynReceived = 4;
const int connStateEstablished = 5;
const int connStateFinWait1 = 6;
const int connStateFinWait2 = 7;
const int connStateCloseWait = 8;
const int connStateLastAck = 9;
const int connStateClosing = 10;
const int connStateTimeWait = 11;

static tcpSubAgent *tcpsa;

tcpSubAgent::tcpSubAgent(void)
{
    // For table updates
    tcpsa = this;

    // Export subtree
    subtree = "1.3.6.1.2.1.6";
    int rc = export("tcp", &subtree);
    if (rc != SNMP_ERR_genErr) {
	log(LOG_ERR, 0, "error exporting TCP subagent");
	return;
    }

    // Initialize the variables
    conntable = table(tcpConnTable, 10, updateTcpConnTable);
}

tcpSubAgent::~tcpSubAgent(void)
{
    unexport(&subtree);
    delete conntable;
}

int
tcpSubAgent::get(asnObjectIdentifier *o, asnObject **a, int *)
{
    // Must be static to work correctly. Otherwise, sysmp gives
    // errno = 14 (Bad address).
    static struct kna kna_s;
    struct kna *kna_p = &kna_s;
    struct tcpstat *tcpstat_p;

    subID *subid;
    unsigned int len;

    // Pull out the subID array and length
    oid *oi = o->getValue();
    oi->getValue(&subid, &len);

    // Check that the subID array is of valid size
    if (len == sublen)
	return SNMP_ERR_noSuchName;

    // Handle the TCP connection table separately
    if (subid[sublen] == tcpConnTable) {
	if (len != sublen + 13
	    || subid[sublen + 1] != 1
	    || subid[sublen + 2] == 0
	    || subid[sublen + 2] > tcpConnRemPort)
	    return SNMP_ERR_noSuchName;

	updateTcpConnTable();
	return conntable->get(oi, a);
    }

    // Check that the subID array is of valid size
    if (len != sublen + 2 || subid[sublen + 1] != 0)
	return SNMP_ERR_noSuchName;

    // Get the pointer to the ipstat structure
    int err = sysmp(MP_SAGET, MPSA_TCPIPSTATS, kna_p, sizeof(struct kna));
    if (err < 0) {
        exc_errlog(LOG_ERR, 0, "tcp get: sysmp error: errno=%d", errno);
        // No TCP objects will be returned
        return SNMP_ERR_noSuchName;
    }
    tcpstat_p = &(kna_p->tcpstat);

    // Switch on the subID and assign the value to *a
    switch (subid[sublen]) {
	case tcpRtoAlgorithm:
	    *a = new asnInteger(routeAlgVanJ);
	    break; 

	case tcpRtoMin:
	    *a = new asnInteger(TCPTV_MIN * (1000 / PR_SLOWHZ));
	    break; 

	case tcpRtoMax:
	    *a = new asnInteger(TCPTV_REXMTMAX * (1000 / PR_SLOWHZ));
	    break; 

	case tcpMaxConn:
	    *a = new asnInteger(-1);	// No maximum, it's dynamic
	    break; 

	case tcpActiveOpens:
	    *a = new snmpCounter((unsigned int) tcpstat_p->tcps_connattempt);
	    break; 

	case tcpPassivOpens:
	    *a = new snmpCounter((unsigned int) tcpstat_p->tcps_accepts);
	    break; 

	case tcpAttemptFails:
	    *a = new snmpCounter((unsigned int) tcpstat_p->tcps_conndrops);
	    break; 

	case tcpEstabResets:
	    *a = new snmpCounter((unsigned int) tcpstat_p->tcps_drops);
	    break; 

	case tcpCurrEstab:
	    {
		asnObject *ao;
		unsigned int conn, state;
		int rc;
		oid oi = "1.3.6.1.2.1.6.13.1.1.0.0.0.0.0.0.0.0.0.0";

		updateTcpConnTable();
		for (conn = 0; ; ) {
		    rc = conntable->formNextOID(&oi);
		    if (rc != SNMP_ERR_noError)
			break;
		    rc = conntable->get(&oi, &ao);
		    if (rc != SNMP_ERR_noError)
			continue;
		    state = ((asnInteger *) ao)->getValue();
		    delete (asnInteger *) ao;
		    if (state == connStateEstablished
			|| state == connStateCloseWait)
			conn++;
		}

		*a = new snmpGauge(conn);
	    }
	    break;

	case tcpInSegs:
	    *a = new snmpCounter((unsigned int) tcpstat_p->tcps_rcvtotal);
	    break; 

	case tcpOutSegs:
	    *a = new snmpCounter((unsigned int) (tcpstat_p->tcps_sndpack
						 - tcpstat_p->tcps_sndrexmitpack));
	    break;

	case tcpRetransSegs:
	    *a = new snmpCounter((unsigned int) tcpstat_p->tcps_sndrexmitpack);
	    break; 

	case tcpInErrs:
	    *a = new snmpCounter((unsigned int) (tcpstat_p->tcps_rcvbadsum
						 + tcpstat_p->tcps_rcvbadoff
						 + tcpstat_p->tcps_rcvshort));
	    break;

	case tcpOutRsts:
	    *a = new snmpCounter((unsigned int) tcpstat_p->tcps_sndrst);
	    break;

	default:
            return SNMP_ERR_noSuchName;
      }   

      return SNMP_ERR_noError;
}

int
tcpSubAgent::getNext(asnObjectIdentifier *o, asnObject **a, int *t)
{
    return simpleGetNext(o, a, t, tcpOutRsts);
}

int
tcpSubAgent::set(asnObjectIdentifier *, asnObject **, int *)
{
    // XXX - No sets yet
    return SNMP_ERR_noSuchName;
}

void
updateTcpConnTable(void)
{
    // Map of kernel TCP states to MIB TCP states
    static int stateTable[TCP_NSTATES] = {
					    connStateClosed,
					    connStateListen,
					    connStateSynSent,
					    connStateSynReceived,
					    connStateEstablished,
					    connStateCloseWait,
					    connStateFinWait1,
					    connStateClosing,
					    connStateLastAck,
					    connStateFinWait2,
					    connStateTimeWait,
    };

    // If packets are coming every fastRequestTime, hold off updating
    // until mustUpdateTime.

    const int mustUpdateTime = 5;
    const int fastRequestTime = 2;

    extern unsigned int packetCounter;

    static unsigned int validCounter = 0;
    static time_t lastRequestTime = 0;
    static time_t lastUpdateTime = 0;

    if (validCounter == packetCounter)
	return;
    validCounter = packetCounter;
    time_t t = time(0);
    if (t - lastUpdateTime < mustUpdateTime
	&& t - lastRequestTime <= fastRequestTime) {
	lastRequestTime = t;
	return;
    }
    lastUpdateTime = lastRequestTime = t;

    struct inpcb pcb, *inp, *prev, *next;
    struct tcpcb tcpcb;
    int tsize;
    off_t tcb, table, hoff;
    struct in_pcbhead hcb;
    snmpIPaddress ip;
    asnInteger i;
    asnObject *a;
    subID *subid;
    unsigned int len, n;
    unsigned long saddr, raddr;
    unsigned short sport, rport;
    int rc, h;
    oid oi = "1.3.6.1.2.1.6.13.1.1.0.0.0.0.0.0.0.0.0.0";
    oi.getValue(&subid, &len);

#ifdef sgi
    if (is_elf64)
       tcb = nl64[N_TCB].n_value; 
    else
#endif /* sgi */
    tcb = nl[N_TCB].n_value; 
    klseek(tcb);
    if (kread(&pcb, sizeof pcb) < 0)
	return;
    tsize = pcb.inp_tablesz;
    table = (off_t)pcb.inp_table;

    for (h = 0; h < tsize; h++) {
	hoff = table + (h * sizeof(hcb));
	prev = (struct inpcb *)hoff;
	klseek(hoff);
	if (kread(&hcb, sizeof hcb) < 0)
	    continue;
	inp = (struct inpcb *)&hcb;
	if (inp->inp_next == (struct inpcb *)hoff)
	    continue;
	while (inp->inp_next != (struct inpcb *)hoff) {
	    next = inp->inp_next;
	    klseek((off_t)next);
	    if (kread(&pcb, sizeof(pcb)) < 0) {
		break;
	    }
	    inp = &pcb;
	    if (pcb.inp_prev != prev) {
		break;
	    }

	    // Form oid
	    saddr = pcb.inp_laddr.s_addr;
	    raddr = pcb.inp_faddr.s_addr;
	    sport = pcb.inp_lport;
	    rport = pcb.inp_fport;

	    subid[len - 10] = (subID) ((saddr & 0xFF000000) >> 24);
	    subid[len - 9] = (subID) ((saddr & 0x00FF0000) >> 16);
	    subid[len - 8] = (subID) ((saddr & 0x0000FF00) >> 8);
	    subid[len - 7] = (subID) (saddr & 0x000000FF);
	    subid[len - 6] = sport;
	    subid[len - 5] = (subID) ((raddr & 0xFF000000) >> 24);
	    subid[len - 4] = (subID) ((raddr & 0x00FF0000) >> 16);
	    subid[len - 3] = (subID) ((raddr & 0x0000FF00) >> 8);
	    subid[len - 2] = (subID) (raddr & 0x000000FF);
	    subid[len - 1] = rport;

	    // Set variables
	    subid[len - 11] = tcpConnLocalAddress;
	    ip = saddr;
	    a = &ip;
	    tcpsa->conntable->set(&oi, &a);

	    subid[len - 11] = tcpConnRemAddress;
	    ip = raddr;
	    tcpsa->conntable->set(&oi, &a);

	    subid[len - 11] = tcpConnLocalPort;
	    i = sport;
	    a = &i;
	    tcpsa->conntable->set(&oi, &a);

	    subid[len - 11] = tcpConnRemPort;
	    i = rport;
	    tcpsa->conntable->set(&oi, &a);

	    subid[len - 11] = tcpConnValid;
	    i = validCounter;
	    tcpsa->conntable->set(&oi, &a);

	    subid[len - 11] = tcpConnState;
	    klseek((off_t) pcb.inp_ppcb);
	    if (kread(&tcpcb, sizeof tcpcb) < 0)
		break;
	    i = stateTable[tcpcb.t_state];
	    tcpsa->conntable->set(&oi, &a);
	}
    }

    // Remove invalid entries
    // XXX - What if 0.0.0.0 port 0 to 0.0.0.0 port 0 is an entry?
    for (n = 10; n != 0; n--)
	subid[len - n] = 0;
    subid[len - 11] = tcpConnValid;
    for ( ; ; ) {
	rc = tcpsa->conntable->formNextOID(&oi);
	if (rc != SNMP_ERR_noError)
	    break;
	rc = tcpsa->conntable->get(&oi, &a);
	if (rc == SNMP_ERR_noError
	    && ((asnInteger *) a)->getValue() == validCounter) {
	    delete (asnInteger *) a;
	    continue;
	}
	delete (asnInteger *) a;
	a = 0;

	// Entries is invalid, delete it
	for (subID s = tcpConnState; s <= tcpConnValid; s++) {
	    subid[len - 11] = s;
	    tcpsa->conntable->set(&oi, &a);
	}
    }
}
