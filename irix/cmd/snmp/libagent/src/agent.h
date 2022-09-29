/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	SNMP Agent
 *
 *	$Revision: 1.1 $
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

#define AUTH_FAIL_DATAFILE "/etc/snmpd.authfail"

class asnObjectIdentifier;
class snmpPacket;
class snmpMessageHandler;
class subAgent;
class subAgentTable;
class remoteSubAgentHandler;
/*
 * MIB-II SNMP statistics
 */
struct snmpstats {
	unsigned int	snmpInPkts;
	unsigned int	snmpOutPkts;
	unsigned int	snmpInBadVersions;
	unsigned int	snmpInBadCommunityNames;
	unsigned int	snmpInBadCommunityUses;
	unsigned int	snmpInASNParseErrs;
	unsigned int	snmpInTooBigs;
	unsigned int	snmpInNoSuchNames;
	unsigned int	snmpInBadValues;
	unsigned int	snmpInReadOnlys;
	unsigned int	snmpInGenErrs;
	unsigned int	snmpInTotalReqVars;
	unsigned int	snmpInTotalSetVars;
	unsigned int	snmpInGetRequests;
	unsigned int	snmpInGetNexts;
	unsigned int	snmpInSetRequests;
	unsigned int	snmpInGetResponses;
	unsigned int	snmpInTraps;
	unsigned int	snmpOutTooBigs;
	unsigned int	snmpOutNoSuchNames;
	unsigned int	snmpOutBadValues;
	unsigned int	snmpOutGenErrs;
	unsigned int	snmpOutGetRequests;
	unsigned int	snmpOutGetNexts;
	unsigned int	snmpOutSetRequests;
	unsigned int	snmpOutGetResponses;
	unsigned int	snmpOutTraps;
	unsigned int	snmpEnableAuthenTraps;
};


/*
 * SNMP Agent
 */
class snmpAgent {
	friend class snmpSubAgent;
	friend class remoteSubAgent;

private:
	// Message handler

	snmpMessageHandler	*mh;
	
	remoteSubAgentHandler	*rsa;

	// Request and Response packets

	snmpPacket		request;
	snmpPacket		response;
	
	// Address of requestor

	struct sockaddr_in	addr;

	// Sub-agent Table

	subAgentTable		sat;

	// Statistics

	struct snmpstats	stats;

	// Debugging

	int			dumpPacket;
	int			dumpRequest;
	int			dumpResponse;
	int			foreground;

	// Clean up after each request

	void			clean(void);

	// Make port configurable

	char *			service;
	int			port;

public:
	snmpAgent(void);
	snmpAgent(int);
	snmpAgent(char *);

	// Debugging

	void			setDumpPacket(int d)
					{ dumpPacket = d; }
	void			setDumpRequest(int d)
					{ dumpRequest = d; }
	void			setDumpResponse(int d)
					{ dumpResponse = d; }
	void			setForeground(int d)
					{ foreground = d; }

	// Import a variable from a sub-agent.

	int			import(asnObjectIdentifier *, subAgent *);
	int			unimport(asnObjectIdentifier *);

	// Service requests.

	void			run(void);
 
	// Configure port and service

	void			setService(char* );
	void			setPort(int p);

	// Support snmpEnableAuthenTrap flag

	int			getSnmpEnableAuthenTraps()
					{ return stats.snmpEnableAuthenTraps; }

	void			setSnmpEnableAuthenTraps(int d)
					{ stats.snmpEnableAuthenTraps = d; }

	void			incSnmpOutTraps()
					{ stats.snmpOutTraps++; } 
};
