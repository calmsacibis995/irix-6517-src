/*
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 *
 * Index operations for IP addresses, used by several protocols.
 */
#include <sys/types.h>
#include <netinet/in.h>

unsigned int
ip_hashaddr(struct in_addr *addr)
{
	u_long hash;

	hash = addr->s_addr;
	return ((hash >> 8 ^ hash) >> 8 ^ hash) >> 8 ^ hash;
}

int
ip_cmpaddrs(struct in_addr *addr1, struct in_addr *addr2)
{
	return addr1->s_addr != addr2->s_addr;
}
