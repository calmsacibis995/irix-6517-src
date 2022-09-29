#ifdef __STDC__
	#pragma weak ether_ntoa = _ether_ntoa
	#pragma weak ether_aton = _ether_aton
	#pragma weak ether_ntohost = _ether_ntohost
	#pragma weak ether_hostton = _ether_hostton
	#pragma weak ether_line = _ether_line
#endif

#include "synonyms.h"
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>
#include <malloc.h>
#include <ns_api.h>

#define _SGI_REENTRANT_FUNCTIONS

#define ETHBUFLEN	1024
#define MAXETHMAGIC	128
#define LOCALFILE	"/etc/ethers"

static ns_map_t _map_byname _INITBSSS;
static ns_map_t _map_byaddr _INITBSSS;

extern int _getXbyY_no_stat;

struct ether_addr {
	unsigned char ether_addr_octet[6];
};

/*
** This routine will split the source string into two strings, the
** address and name.
*/
int
ether_line(char *l, struct ether_addr *e, char *hostname)
{
	int i;
	unsigned int t[6];

	if (! l || ! e || ! hostname) {
		return 1;
	}
	
	i = sscanf(l, " %x:%x:%x:%x:%x:%x %s",
		&t[0], &t[1], &t[2], &t[3], &t[4], &t[5], hostname);

	if (i != 7) {
		return (7 - i);
	}

	for (i = 0; i < 6; i++) {
		e->ether_addr_octet[i] = (unsigned char)t[i];
	}

	return 0;
}

/*
** This routine just prints the ethernet address into the buffer provided.
*/
char *
ether_ntoa_r(struct ether_addr *e, char *buffer)
{
	*buffer = (char)0;

#define EI(i)   (unsigned int)(e->ether_addr_octet[(i)])
	sprintf(buffer, "%x:%x:%x:%x:%x:%x",
		EI(0), EI(1), EI(2), EI(3), EI(4), EI(5));
	
	return buffer;
}

/*
** This routine prints the ethernet address into static space.
*/
char *
ether_ntoa(struct ether_addr *e)
{
	static char s[18] _INITBSSS;
	
	return ether_ntoa_r(e, s);
}

/*
** This routine expands the string into the provided address structure.
*/
struct ether_addr *
ether_aton_r(char *s, struct ether_addr *e) {
	int i;
	unsigned int t[6];

	i = sscanf(s, " %x:%x:%x:%x:%x:%x",
		&t[0], &t[1], &t[2], &t[3], &t[4], &t[5]);

	if (i < 6) {
		return (struct ether_addr *)0;
	}

	for (i = 0; i < 6; i++) {
		e->ether_addr_octet[i] = (unsigned char)t[i];
	}
	return e;
}

/*
** This routine expands the string into static space.
*/
struct ether_addr *
ether_aton(char *s)
{
	static struct ether_addr e _INITBSSS;

	return ether_aton_r(s, &e);
}

/*
** This routine looks up the ethernet line by hostname, and returns the
** data into the space provided.
*/
int
ether_hostton(char *hostname, struct ether_addr *e)
{
	char ebuf[ETHBUFLEN];
	char currenthost[256];
	int status;
	struct stat sbuf;
	FILE *fp;

	if (! hostname || ! e) {
		return -1;
	}

	if (! _getXbyY_no_stat && stat(LOCALFILE, &sbuf) == 0) {
		_map_byname.m_version = sbuf.st_mtim.tv_sec;
	}

	status = ns_lookup(&_map_byname, 0, "ethers.byname", hostname, 0,
	    ebuf, ETHBUFLEN);
	switch (status) {
	    case NS_SUCCESS:
		if ((ether_line(ebuf, e, currenthost) == 0) &&
		    (strcasecmp(currenthost, hostname) == 0)) {
			return 0;
		}
	    case NS_FATAL:
		/*
		** Fallback to local files.
		*/
		status = -1;
		fp = fopen(LOCALFILE, "r");
		if (! fp) {
			return -1;
		}
		while (fscanf(fp, "%[^\n] ", ebuf) == 1) {
			if ((ether_line(ebuf, e, currenthost) == 0) &&
			    (strcasecmp(currenthost, hostname) == 0)) {
				status = 0;
				break;
			}
		}
		fclose(fp);
		return status;
	}

	return -1;
}

/*
** This routine looks up the ethernet line by address, and returns the
** data into the space provided.
*/
int
ether_ntohost(char *hostname, struct ether_addr *e)
{
	char ebuf[ETHBUFLEN], es[18];
	struct ether_addr currente;
	int status;
	struct stat sbuf;
	FILE *fp;

	if (! hostname || ! e) {
		return -1;
	}

	ether_ntoa_r(e, es);

	if (! _getXbyY_no_stat && stat(LOCALFILE, &sbuf) == 0) {
		_map_byname.m_version = sbuf.st_mtim.tv_sec;
	}

	status = ns_lookup(&_map_byaddr, 0, "ethers.byaddr", es, 0,
	    ebuf, ETHBUFLEN);
	switch (status) {
	    case NS_SUCCESS:
		if ((ether_line(ebuf, &currente, hostname) == 0) &&
		    (memcmp(e, &currente, sizeof(currente)) == 0)) {
			return 0;
		}
	    case NS_FATAL:
		/*
		** Fallback to local files.
		*/
		status = -1;
		fp = fopen(LOCALFILE, "r");
		if (! fp) {
			return -1;
		}
		while (fscanf(fp, "%[^\n] ", ebuf) == 1) {
			if ((ether_line(ebuf, &currente, hostname) == 0) &&
			    (memcmp(e, &currente, sizeof(currente)) == 0)) {
				status = 0;
				break;
			}
		}
		fclose(fp);
		return status;
	}

	return -1;
}
