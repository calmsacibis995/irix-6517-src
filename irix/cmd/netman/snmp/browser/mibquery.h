#ifndef __MIBQUERY_H__
#define __MIBQUERY_H__
/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	SNMP  MIB Query Handler
 *
 *	$Revision: 1.3 $
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

//#include <sys/socket.h>

//#include "message.h"
//#include "packet.h"
#include "pdu.h"
//#include "snmp.h"

class tuTimer;

#define MIBQUERY_ERR_encodeError	(SNMP_ERR_genErr + 101)
#define MIBQUERY_ERR_decodeError	(SNMP_ERR_genErr + 102)
#define MIBQUERY_ERR_sendError		(SNMP_ERR_genErr + 103)
#define MIBQUERY_ERR_recvError		(SNMP_ERR_genErr + 104)
#define MIBQUERY_ERR_timeout		(SNMP_ERR_genErr + 105)
#define MIBQUERY_ERR_mallocError        (SNMP_ERR_genErr + 106)
#define MIBQUERY_ERR_noError            (SNMP_ERR_genErr + 107)
#define MIBQUERY_ERR_snmpError          (SNMP_ERR_genErr + 108)

#define MIB_GETrequest    1
#define MIB_SETrequest    2

// #define TRUE              1
// #define FALSE             0

#define MAXREQS           20  // maximum request pdus in a transaction
#define MAXIDS            15   // Maximum variables in a pdu

// structures that the mibQueryHandler uses 

struct result    {

                        varBindList      goodResult;
                        varBindList      notfoundResult;
                        varBindList      badvalueResult;
                        varBindList      readonlyResult;
                        varBindList      generrorResult;
                        varBindList      timeexpiredResult;

		};			


struct reqInfo   {
			unsigned int 	reqId;
			unsigned int    requestType;
			snmpPDU      	requestpdu;
			snmpPDU      	responsepdu;
			unsigned int 	tries;
			unsigned int 	received;
			tuTimer         *reqTimer;
		 };

struct transInfo {
			int              tranId;
                        int              totalReqs;
                        int              totalReceived;
			int 		 totalList;

                        char            *hostname;
                        char            *community;
			int 		 timeout;
			int 		 maxRetries;

			struct reqInfo   *reqBlock[MAXREQS]; 

			struct result    *tranResult;
			struct transInfo *next;
		 };
/*
 * A handler to perform SNMP queries.
 */
class mibQueryHandler {
private:
	snmpMessageHandler	mh;
	int			sock;
//	char *			community;
	char *			requestBuffer;
	char *			responseBuffer;

	struct transInfo        *transList;


public:
	mibQueryHandler(void);
	~mibQueryHandler(void);


	int	get(char *hostname, char *community,
		    int timeout, int retries,
		    varBindList *arg,unsigned int transId);
        int     getNext(char *hostname, char *community,
		    int timeout, int retries,
                    varBindList *arg,unsigned int transId);

	int	set(char *hostname, char *community,
		    int timeout, int retries,
		    varBindList *arg,unsigned int transId);

        int     query(char *hostname, char *community,
		    int timeout, int retries,
                    int reqtype,  varBindList *arg, int transId);

	void    addToList(struct transInfo *transAdd);

	void    removeFromList(struct transInfo *transRemove);

	void    response();
	int 	resend(struct transInfo *transptr, int blocknum,
                       int error, int index, unsigned int reqtype);

        int     bigResend(struct transInfo *transptr, int blocknum,
                          unsigned int reqtype);

	void 	timer(int requestId);

};

#endif /* !__MIBQUERY_H__ */
