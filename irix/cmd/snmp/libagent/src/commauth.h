/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Community-based SNMP Authorization
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

/*
 * Linked list of hosts
 */
class commAuthHost {
	friend class commAuthRule;

private:
	commAuthHost *		next;
	struct in_addr		addr;

	commAuthHost(void);

	unsigned int		set(const char *);
};

/*
 * Linked list of communities
 */
class commAuthCommunity {
	friend class commAuthRule;

private:
	commAuthCommunity *	next;
	char *			community;
	unsigned int		length;

	commAuthCommunity(void);
	~commAuthCommunity(void);

	unsigned int		set(const char *);
};

/*
 * Valid operations
 */
class commAuthOperation {
	friend class commAuthRule;

private:
	int			op;

	commAuthOperation(void);

	unsigned int		set(const char *);
};

/*
 * A rule
 */
class commAuthRule {
	friend class communityAuth;

private:
	commAuthRule *		next;
	commAuthHost *		hosts;
	commAuthCommunity * 	communities;
	commAuthOperation	operations;

	commAuthRule(void);
	~commAuthRule(void);

	int			parse(char *);
	int			match(struct in_addr *, asnOctetString *, int);
};

/*
 * The community-based authorization module
*/
class communityAuth {
private:
	commAuthRule *		accept;
	commAuthRule *		reject;
	char *			file;
	time_t			readTime;

	void			clearRules(void);
	void			readFile(void);
	char *			getword(char *, char **);

public:
	// Constructor takes the name of the authorization file as
	// an argument.

	communityAuth(const char *file);
	~communityAuth(void);

	// authorize() takes as arguments the incoming address, the community,
	// and the PDU type and return non-zero if accepted.

	int	authorize(struct in_addr *a, asnOctetString *c, int pdutype);
};
