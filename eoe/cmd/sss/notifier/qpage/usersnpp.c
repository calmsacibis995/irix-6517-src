#include	"qpage.h"


#define		CHECKRESULT(x)		if ((x) < 0) return(-1); \
					if ((x) > 0) continue;


/*
** global variables
*/
#ifndef lint
static char	sccsid[] = "@(#)usersnpp.c  1.17  03/12/98  tomiii@qpage.org";
#endif


/*
** get_response()
**
** This function reads a response from the SNPP server.  In the case
** of a multi-line response (the 214 code) all but the last line,
** which must be a 250 code, is ignored.
**
**	Input:
**		fp - the file pointer to read from
**
**	Returns:
**		the first digit of the SNPP response code
*/
int
get_response(FILE *fp)
{
	static char	buff[1024];
	int		code;


	for (;;) {
		if (fgets(buff, sizeof(buff), fp) == NULL)
			return(4); /* fatal error */

		if (Debug)
			printf("%s", buff);

		code = atoi(buff);

		switch (code) {
			case 0:
				fprintf(stderr, "Bogus response: %s", buff);
				break;

			case 214:
				break;

			default:
				return(code / 100);
		}
	}
}


/*
** send_command()
**
** This function sends a command to the SNPP server and reads the
** response.
**
**	Input:
**		in - the input FILE pointer
**		out - the output FILE pointer
**		cmd - the command to be sent
**		err - the error message to print in case of failure
**
**	Returns:
**		an integer status code as follows:
**
**			status < 0 : a 4xx code was received
**			status > 0 : a 5xx code was received
**			status = 0 : any other code was received
*/
int
send_command(FILE *in, FILE *out, char *cmd, char *err)
{
	if (Debug)
		printf(">>> %s\n", cmd);

	fprintf(out, "%s\r\n", cmd);
	(void)fflush(out);

	switch (get_response(in)) {
		case 4:
			fprintf(stderr, "Fatal error, aborting\n");
			(void)fclose(in);
			(void)fclose(out);
			return(-1);

		case 5:
			if (err)
				fprintf(stderr, "%s\n", err);
			return(1);

		default:
			return(0);
	}
}


/*
** open_connection()
**
** This function opens a connection to the SNPP server.
**
**	Input:
**		server - the server's hostname
**
**	Returns:
**		an open socket, or -1 on error
*/
int
open_connection(char *server)
{
	struct sockaddr_in	addr;
	struct hostent		*hp;
	struct servent		*svc;
	short			port;
	int			sock;


	if (Debug)
		printf("Connecting to SNPP server at %s\n", server);

	if ((hp = gethostbyname(server)) == NULL) {
		fprintf(stderr, "Unknown host: %s\n", server);
		return(-1);
	}

	/*
	** Get the SNPP port number from the /etc/services map.
	** Note that the port number is returned in network byte
	** order so no call to htons() is required.
	*/
	if ((svc = getservbyname("snpp", "tcp")) != NULL)
		port = svc->s_port;
	else
		port = htons(SNPP_SVC_PORT);

	/*
	** There should be code here to pick the best of multiple
	** addresses returned by the name lookup.
	*/
	(void)memcpy((char *)&addr.sin_addr.s_addr, hp->h_addr_list[0],
		hp->h_length);

	addr.sin_family = AF_INET;
	addr.sin_port = port;

	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Cannot create socket");
		return(-1);
	}

	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "Cannot connect to SNPP server at %s: ",
			server);
		(void)close(sock);
		perror(NULL);
		return(-1);
	}

	if (Debug)
		printf("Connected\n");

	return(sock);
}


/*
** close_connection()
**
** This function closes the connection to the SNPP server.
**
**	Input:
**		in - the input FILE pointer
**		out - the output FILE pointer
**
**	Returns:
**		nothing
*/
void
close_connection(FILE *in, FILE *out)
{
	(void)send_command(in, out, "QUIT", (char *)NULL);
	(void)fclose(in);
	(void)fclose(out);
}


/*
** submit_page()
**
** This function submits a page to the SNPP server.
**
**	Input:
**		p - the page to submit
**		server - the SNPP server's hostname
**
**	Returns:
**		an integer status code
*/
int
submit_page(PAGE *p, char *server)
{
	struct tm	*tm;
	FILE		*in;
	FILE		*out;
	rcpt_t		*rcpt;
	char		*command;
	char		*last;
	char		*result;
	char		*ptr;
	char		errormsg[1024];
	char		buff[100];
	int		goodpage;
	int		fd;
	int		i;


	if ((i = strlen(p->message)) < 1024)
		i = 1024;

	/*
	** The command buffer has to be big enough to hold the entire
	** message plus strlen("MESSage ").
	*/
	command = (void *)malloc(i+20);

	result = (void *)malloc(strlen(server)+1);
	ptr = safe_strtok(server, ",: \t\r\n", &last, result);
	while (ptr) {
		if ((fd = open_connection(ptr)) >= 0)
			break;

		ptr = safe_strtok(NULL, ",: \t\r\n", &last, result);
	}
	free(result);

	if (fd < 0)
		return(-1);

	if ((in = fdopen(fd, "r")) == NULL) {
		fprintf(stderr, "Cannot reopen socket for reading\n");
		(void)close(fd);
		return(-1);
	}

	if ((out = fdopen(dup(fd), "w")) == NULL) {
		fprintf(stderr, "Cannot reopen socket for writing\n");
		(void)fclose(in);
		return(-1);
	}

	setbuf(out, NULL);

	if (get_response(in) != 2) {
		fprintf(stderr, "Fatal error, aborting\n");
		close_connection(in, out);
		return(-1);
	}

	/*
	** identify ourselves to the remote end
	*/
	if (p->from && p->from[0]) {
		(void)sprintf(command, "CALLerid %s", p->from);
		(void)sprintf(errormsg, "CALLerid %s not accepted", p->from);

		if (send_command(in, out, command, errormsg) < 0)
			return(-1);
	}

	/*
	** spin through each recipient
	*/
	goodpage = FALSE;
	for (rcpt=p->rcpts; rcpt; rcpt=rcpt->next) {
		if (rcpt->holduntil) {
			if ((tm = localtime(&rcpt->holduntil)) == NULL) {
				/*
				** this should never happen
				*/
				fprintf(stderr, "localtime() failed\n");
				continue;
			}

			(void)sprintf(buff, "%02u%02u%02u%02u%02u%02u",
				tm->tm_year % 100, tm->tm_mon+1, tm->tm_mday,
				tm->tm_hour, tm->tm_min, tm->tm_sec);

			(void)sprintf(command, "HOLDuntil %s", buff);

			(void)sprintf(errormsg,
				"Cannot hold page until %s (skipping %s)",
				buff, rcpt->pager);

			i = send_command(in, out, command, errormsg);

			CHECKRESULT(i);
		}

		if (rcpt->coverage) {
			(void)sprintf(command, "COVErage %s", rcpt->coverage);

			(void)sprintf(errormsg,
				"Cannot set coverage to %s (skipping %s)",
				rcpt->coverage, rcpt->pager);

			i = send_command(in, out, command, errormsg);

			CHECKRESULT(i);
		}

		if (rcpt->level != DEFAULT_LEVEL) {
			(void)sprintf(command, "LEVEl %d", rcpt->level);

			(void)sprintf(errormsg,
				"Cannot set level to %d (skipping %s)",
				rcpt->level, rcpt->pager);

			i = send_command(in, out, command, errormsg);

			CHECKRESULT(i);
		}

		(void)sprintf(command, "PAGEr %s", rcpt->pager);
		(void)sprintf(errormsg, "Cannot send page to %s", rcpt->pager);

		i = send_command(in, out, command, errormsg);

		CHECKRESULT(i);

		goodpage = TRUE;
	}

	/*
	** Finish sending the rest of the page only if at least one
	** recipient was passed successfully.
	*/
	if (goodpage) {
		/*
		** Try the DATA command first but fall back to use MESSage
		** if the command is rejected.  If the SNPP server accepts
		** the DATA command but then rejects the message, this is
		** a fatal error and we should not continue.
		*/
		if ((i = send_command(in, out, "DATA", (char *)NULL)) == 0) {
			/*
			** Although the qpage server can handle unlimited
			** line lengths, we may be talking to someone else's
			** implementation which does not support unlimited
			** line lengths.  Since RFC-1861 does not specify
			** a maximum line length, we will use 70 characters
			** so that it looks nice in debug mode.
			*/
			ptr = p->message;

			while (ptr) {
				ptr = msgcpy(buff, ptr, 70);

				if (Debug)
					printf(">>> %s\n", buff);

				fprintf(out, "%s\r\n", buff);
			}

			i = send_command(in, out, ".",
				"Cannot send message; entire page is canceled");
		}
		else {
			/*
			** Make sure they didn't tell us to go away.
			*/
			if (i < 0) {
				close_connection(in, out);
				return(-1);
			}

			(void)sprintf(command, "MESSage %s", p->message);

			i = send_command(in, out, command,
				"Cannot send message; entire page is canceled");
		}

		/*
		** If we got either a 4xx or 5xx response, the page failed.
		*/
		if (i) {
			close_connection(in, out);
			return(-1);
		}

		i = send_command(in, out, "SEND", "Page failed");

		/*
		** If we got either a 4xx or 5xx response, the page failed.
		*/
		if (i) {
			close_connection(in, out);
			return(-1);
		}
	}

	close_connection(in, out);

	return(goodpage == FALSE);
}
