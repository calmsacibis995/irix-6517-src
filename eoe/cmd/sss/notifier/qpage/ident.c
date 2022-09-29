#include	"qpage.h"


/*
** global variables
*/
#ifndef lint
static char	sccsid[] = "@(#)ident.c  1.11  03/12/98  tomiii@qpage.org";
#endif


/*
** trim()
**
** This function trims leading and trailing whitespace from a string.
** The string is modified in place.  Since the result will be less than
** or equal to the size of the original string, this should not be a
** problem.
**
**	Input:
**		str - the string to trim
**
**	Returns:
**		nothing
*/
void
trim(char *str)
{
	char			*start;
	char			*end;


	for (start=str; *start && isspace(*start); start++)
		continue;

	end = start + strlen(start) - 1;

	while (end >= start && isspace(*end))
		*end-- = '\0';

	if (str != start)
		(void)strcpy(str, start);
}


/*
** ident()
**
** This function queries the remote host's ident server to find the
** userid at the other end of the connection.
**
**	Input:
**		peer - the open socket for this connection
**
**	Returns:
**		the userid returned from the ident query, or NULL on error
**
**	Note:
**		The string returned by this function is obtained via
**		malloc().  It is therefore the caller's responsibility
**		to free this memory when it is no longer needed.
*/
char *
ident(int peer)
{
	struct sockaddr_in	addr;
	struct servent		*svc;
	char			buff[1024];
	char			type[1024];
	char			os[1024];
	char			user[1024];
	char			*ptr;
	int			sock;
	int			theirport;
	int			ourport;
	int			left;
	int			len;
	int			i;


	/*
	** get our port number
	*/
	len = sizeof(addr);
	if (getsockname(peer, (struct sockaddr *)&addr, &len) < 0) {
		if (Debug)
			qpage_log(LOG_DEBUG, "ident.getsockname() failed: %s",
				strerror(errno));

		return(NULL);
	}
	ourport = ntohs(addr.sin_port);


	/*
	** get their port number
	*/
	len = sizeof(addr);
	if (getpeername(peer, (struct sockaddr *)&addr, &len) < 0) {
		if (Debug)
			qpage_log(LOG_DEBUG, "ident.getpeername() failed: %s",
				strerror(errno));

		return(NULL);
	}
	theirport = ntohs(addr.sin_port);


	if ((svc = getservbyname("ident", "tcp")) != NULL)
		addr.sin_port = svc->s_port;
	else
		addr.sin_port = htons(113);


	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		if (Debug)
			qpage_log(LOG_DEBUG, "ident.socket() failed: %s",
				strerror(errno));

		return(NULL);
	}

	/*
	** set the alarm so we don't hang
	*/
	(void)alarm(IdentTimeout);

	/*
	** set the alarm so we don't hang
	*/
	if (setjmp(TimeoutEnv) > 0) {
		(void)close(sock);
		return(NULL);
	}

	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		if (Debug) {
			if (errno == ECONNREFUSED) {
				qpage_log(LOG_DEBUG,
					"RFC-1413 query refused by remote");
			}
			else
				qpage_log(LOG_DEBUG,
					"ident.connect() failed: %s",
					strerror(errno));
		}

		(void)close(sock);
		(void)alarm(0);
		return(NULL);
	}

	(void)sprintf(buff, "%u, %u\r\n", theirport, ourport);

	if (write(sock, buff, strlen(buff)) < 0) {
		if (Debug)
			qpage_log(LOG_DEBUG, "ident.write() failed: %s",
				strerror(errno));

		(void)close(sock);
		(void)alarm(0);
		return(NULL);
	}

	ptr = buff;
	left = sizeof(buff) - 1;

	while ((i = read(sock, ptr, left)) > 0) {
		ptr += i;
		left -= i;
	}

	(void)close(sock);

	/*
	** we're safe, cancel the alarm and restore the signal handler
	*/
	(void)alarm(0);

	if ((i < 0) || (ptr == buff)) {
		if (Debug)
			qpage_log(LOG_DEBUG, "no valid ident response");

		return(NULL);
	}

	*ptr-- = '\0';

	if (*ptr == '\n')
		*ptr-- = '\0';

	if (*ptr == '\r')
		*ptr-- = '\0';

	i = sscanf(buff, "%*[^:]:%[^:]:%[^:]:%[^\r\n]", type, os, user);

	if (i != 3) {
		if (Debug)
			qpage_log(LOG_DEBUG, "no valid ident response");

		return(NULL);
	}

	trim(type);
	trim(os);
	trim(user);

	if (Debug)
		qpage_log(LOG_DEBUG, "ident: type=<%s>, os=<%s>, user=<%s>",
			type, os, user);

	if (!strcasecmp(type, "userid") && strcasecmp(os, "other")) {
		ptr = strdup(user);
		return(ptr);
	}

	return(NULL);
}
