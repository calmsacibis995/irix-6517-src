/*
 *	hostreg.c 	7/27/88		initial release
 */

#ifdef __STDC__
        #pragma weak registerhost = _registerhost
        #pragma weak registerinethost = _registerinethost
        #pragma weak renamehost = _renamehost
        #pragma weak unregisterhost = _unregisterhost
#endif
#include "synonyms.h"

#include <stdio.h>
#include <assert.h>
#include <rpc/rpc.h>
#include <netdb.h>
#include <sys/socket.h>
#include <rpcsvc/ypclnt.h>
#include <netinet/in.h>
#include <arpa/inet.h>		/* prototype for inet_addr() */
#include <string.h> 		/* prototype for strlen() */
#include <errno.h>
#include "hostreg.h"
#include "priv_extern.h"

static char	hostbyname[] = "hosts.byname";

int registerhost(char *, int, char *, int);

int
registerhost(char *name, int family, char *idata, int datalen)
{
	char		*domain;
	int		error;

	switch (family) {
	case AF_INET:
		error = yp_get_default_domain(&domain);
		if ( error)
			return (error);
		error = yp_update(domain, hostbyname, YPOP_INSERT,
			name, (int)strlen(name), idata, datalen);
		return (error);

	default:
		return (EPROTONOSUPPORT);
	}
} /* registerhost */


int
registerinethost( char *namep, char *network, char *netmask, 
	struct in_addr *inaddr, char *aliases)
{
	char	*domain, *outstr;
	int	rlen, error;
	struct reg_info idata;

	if ( netmask ) {
		idata.info_netmask = inet_addr(netmask);
		idata.info_network = inet_addr(network);
	}
	else {
		idata.info_network = inet_network(network);
		idata.info_netmask = 0;
	}

	idata.action = REG_NEWNAME;
	if ( aliases ) {
		if ( strlen(aliases) > MAX_ALIASES )
			return (E2BIG);
		strcpy(idata.aliases, aliases);
	}
	else
		idata.aliases[0] = 0;

	error = registerhost(namep, AF_INET, (char *)&idata, sizeof idata);
	if (!error && inaddr ) {
		error = yp_get_default_domain(&domain);
		if ( error )
			return (error);
		error = yp_match(domain, hostbyname, namep, (int)strlen(namep),
			&outstr, &rlen);
		inaddr->s_addr = (u_int32_t)inet_addr(outstr);
	}
	return (error);
} /* registerinethost */


int
renamehost(char *oldname, char *newname, char *aliases, char *passwd)
{
	int	error;
	char	*domain;
	struct reg_info	idata;

	error = yp_get_default_domain(&domain);
	if ( error ) {
		return (error);
	}
	idata.action = REG_RENAME;
	if ( strlen(newname) > 31 )
		return (E2BIG);
	strcpy(idata.info_rename, newname);
	if ( passwd )
		strncpy(idata.info_passwd, passwd, 8);
	else
		idata.info_passwd[0] = 0;

	if ( aliases ) {
		if ( strlen(aliases) > MAX_ALIASES )
			return (E2BIG);
		strcpy(idata.aliases, aliases);
	}
	else
		idata.aliases[0] = 0;

	error = yp_update(domain, hostbyname, YPOP_CHANGE, oldname,
			(int)strlen(oldname), (char *)&idata, sizeof idata);
	return (error);
} /* renamehost */


int
unregisterhost(char *name, char *passwd)
{
	int	error;
	char	*domain;
	struct reg_info	idata;

	error = yp_get_default_domain(&domain);
	if ( error )
		return (error);

	idata.action = REG_DELETE;

	if ( passwd )
		strncpy(idata.info_passwd, passwd, 8);
	else
		idata.info_passwd[0] = 0;

	error = yp_update(domain, hostbyname, YPOP_DELETE, name,
			(int)strlen(name), (char *)&idata, sizeof idata);
	return (error);
} /* unregisterhost*/
