#ifndef __QUERY_H__
#define __QUERY_H__
/*
 * Copyright 1992, 1994 Silicon Graphics, Inc.  All rights reserved.
 *
 *	SNMP Query Handler
 *
 *	$Revision: 1.5 $
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

#include <message.h>

#define QUERY_ERR_hostnameError	(SNMP_ERR_genErr + 1)
#define QUERY_ERR_encodeError	(SNMP_ERR_genErr + 2)
#define QUERY_ERR_decodeError	(SNMP_ERR_genErr + 3)
#define QUERY_ERR_sendError	(SNMP_ERR_genErr + 4)
#define QUERY_ERR_recvError	(SNMP_ERR_genErr + 5)
#define QUERY_ERR_timeout	(SNMP_ERR_genErr + 6)

/*
 * A handler to perform SNMP queries.
 */
class snmpQueryHandler {
public:
	snmpQueryHandler(void);
	~snmpQueryHandler(void);

	// These routines will send an SNMP getRequest, getNextRequest, or
	// setRequest to the host named in dst with the variables given in v.
	// They will wait for a reply.  If a valid reply is received, the
	// reply values will be placed in v and SNMP_noError will be
	// returned.  If an error is received, the error will be returned
	// and v will be unchanged.  If a timeout occurs, QUERY_ERR_timeout
	// will be returned and v will be unchanged.
	int get(const char *dst, const varBindList *arg, varBindList *result);
	int getNext(const char *dst, const varBindList *arg,
		    varBindList *result);
	int set(const char *dst, const varBindList *arg, varBindList *result);

	// Same as above but the destination is passed as a struct in_addr.
	// This avoid having to do name resolution on every query.
	int get(const struct in_addr *a, const varBindList *arg,
		varBindList *result);
	int getNext(const struct in_addr *a, const varBindList *arg,
		varBindList *result);
	int vget(const struct in_addr *a, const varBindList *arg,
		varBindList **result, int *);
	int vgetNext(const struct in_addr *a, const varBindList *arg,
		varBindList **result, int *);
	int set(const struct in_addr *a, const varBindList *arg,
		varBindList *result);
	// For power users return the snmp error index. Let the
	// user optionally specify the request ID to use.
	int get(const struct in_addr *a, const varBindList *arg,
		varBindList *result, int *errIdx, int reqID = 0);
	int getNext(const struct in_addr *a, const varBindList *arg,
		varBindList *result, int *errIdx, int reqID = 0);
	int set(const struct in_addr *a, const varBindList *arg,
		varBindList *result, int *errIdx, int reqID = 0);

	// These are an even simpler interface than the routines above meant
	// for the 1 variable case.  Pass as arguments the host name in dst,
	// a character string of the object id, and a pointer to a pointer to
	// a varBind, which will be filled in with the result for the query.
	// The error status will be the return value.
	//
	// If the operation is a set, then a character string for the value
	// of the set must also be passed as an argument.  The character
	// string will be transformed to an asnObject given internal
	// conversion rules as follows:
	//
	//	1. If the string contains a ':', then it is converted to an
	//	   asnOctetString as a physical address.
	//	2. If the string contains a '.' as its first character, then
	//	   it is converted to an asnObjectIdentifier.
	//	3. If the string contains a '.' as other than its first
	//	   character, then it is converted to an snmpIPaddress.
	//	4. If the string is all digits, then it is converted to an
	//	   asnInteger.
	//	5. Otherwise, the value is converted to an asnOctetString.
	//
	// If the internal convertions of character strings is unacceptable,
	// an application can alternatively pass an asnObject for the value
	// of the set.
	int get(const char *dst, const char *objid, varBind **value);
	int getNext(const char *dst, const char *objid, varBind **value);
	int set(const char *dst, const char *objid, const char *objval,
		varBind **value);
	int set(const char *dst, const char *objid, const asnObject *objval,
		varBind **value);

	// Same as above but the destination is passed as a struct in_addr.
	// This avoids having to do name resolution on every query.
	int get(const struct in_addr *a, const char *objid, varBind **value);
	int getNext(const struct in_addr *a, const char *objid,
		varBind **value);
	int set(const struct in_addr *a, const char *objid,
		const char *objval, varBind **value);
	int set(const struct in_addr *a, const char *objid,
		const asnObject *objval, varBind **value);

	// Change the community name used in the query
	void setCommunity(const char *c);
	// Change the number of times the packet is tried
	void setTries(unsigned int t);

	// Change the starting interval between tries.  The time between tries
	// starts at the interval and is doubled each try.
	void setInterval(unsigned int i);
	
	inline void setDestPort (unsigned int port);

protected:
	int queryOne(const struct in_addr *, const char *, const char *,
		     varBind **, snmpPDU *);
	int query(const struct in_addr *, const varBindList *,
		  varBindList *, snmpPDU *, int reqID = 0,
		  int *errIdx = 0);
	int vquery(const struct in_addr *, const varBindList *,
		  varBindList **, snmpPDU *, int *);

private:
	snmpMessageHandler mh;
	unsigned int tries;
	unsigned int interval;
	unsigned int reqid;
	int sock;
	char *community;
	char *requestBuffer;
	char *responseBuffer;
};

//inline functions
void
snmpQueryHandler::setDestPort (unsigned int port) {
    mh.setDestPort(port);
}
#endif /* !__QUERY_H__ */
