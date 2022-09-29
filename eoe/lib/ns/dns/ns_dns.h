/*
*
* Copyright 1997-1999 Silicon Graphics, Inc.
* All Rights Reserved.
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

#ifndef __NS_DNS_H_
#define __NS_DNS_H_

typedef struct servers {
	u_int32_t	addr;
	struct servers	*next;
} server_t;

typedef struct domains {
	struct domains	*next;
	int		len;
	char		name[1];
} domain_t;

typedef struct addrlist {
	struct in_addr addr;
	struct addrlist *next;
} addrlist_t;

typedef struct sortlist {
	struct in_addr	addr;
	struct in_addr	netmask;
	struct sortlist *next;
	addrlist_t	 *sublist;
} sort_t;

typedef struct {
	unsigned	flags;
	int		max_tries;
	int		timeout;
	uint16_t	xid;
	int		retries;
	char		*map;
	u_int32_t	type;
	u_int32_t	class;
	server_t	*servers;
	domain_t	*domains;
	char		key[MAXDNAME];
} dnsrequest_t;

#define DNS_PARALLEL	1
#define DNS_BUFSIZ	1024

extern int init(char *);
extern domain_t *dns_domain_parse(char *);
extern domain_t *dns_search_parse(char *);
extern server_t *dns_servers_parse(char *);
extern int dns_config(void);

#endif
