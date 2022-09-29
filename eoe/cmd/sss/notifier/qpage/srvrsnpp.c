/*
** NOTICE:
**
**	Portions of this file contain code based on ideas derived
**	from the source code of "sendmail" written by Eric Allman
**	at the University of California, Berkeley.
*/
#include	"qpage.h"

#ifdef TCP_WRAPPERS
#include	"tcpd.h"
#endif


/*
** Level 1 commands
*/
#define		CMDERROR	0	/* bad command */
#define		CMDPAGE 	1	/* specify a pagerid */
#define		CMDMESS		2	/* specify the message text */
#define		CMDRESE		3	/* reset all settings */
#define		CMDSEND		4	/* send the page */
#define		CMDQUIT		5	/* disconnect */
#define		CMDHELP		6	/* send help information */

/*
** Level 2 commands
*/
#define		CMDDATA		7	/* prompt for message text */
#define		CMDLOGI		8	/* authenticate remote user */
#define		CMDLEVE		9	/* specify service level */
#define		CMDALER		10	/* alert (not implemented) */
#define		CMDCOVE		11	/* specify alternate coverage */
#define		CMDHOLD		12	/* hold page until some future time */
#define		CMDCALL		13	/* specify callerid */
#define		CMDSUBJ		14	/* subject (not implemented) */

/*
** Level 3 commands (not implemented)
*/
#define		CMD2WAY		15	/* begin 2-way paging */
#define		CMDPING		16	/* ping a pager */
#define		CMDEXPT		17	/* change expiration time */
#define		CMDNOQU		18	/* don't queue message */
#define		CMDACKR		19	/* read acknowledgment */
#define		CMDRTYP		20	/* reply type */
#define		CMDMCRE		21	/* multiple choice responce codes */
#define		CMDMSTA		22	/* message status */
#define		CMDKTAG		23	/* kill message */

/*
** Other commands which are not part of the SNPP protocol
**
** Note: These commands are not documented, not supported, and may very
**       well disappear in a future release.  They do not produce results
**       consistant with RFC-1861.  Use at your own risk.
*/
#define		CMDXDEB		24	/* activate debug mode */
#define		CMDXCON		25	/* print configuration file */
#define		CMDXQUE		26	/* show the page queue contents */
#define		CMDXWHO		27	/* show the known pagers/groups */


/*
** global variables
*/
#ifndef lint
static char	sccsid[] = "@(#)srvrsnpp.c  1.45  07/07/98  tomiii@qpage.org";
#endif

#ifdef TCP_WRAPPERS
int allow_severity = LOG_INFO;
int deny_severity = LOG_WARNING;
#endif

static struct cmd {
	char		*cmdname;
	int		cmdcode;
} CmdTab[] = {
	{"PAGEr",	CMDPAGE},
	{"MESSage",	CMDMESS},
	{"RESEt",	CMDRESE},
	{"SEND",	CMDSEND},
	{"QUIT",	CMDQUIT},
	{"HELP",	CMDHELP},
	{"DATA",	CMDDATA},
	{"LOGIn",	CMDLOGI},
	{"LEVEl",	CMDLEVE},
	{"ALERt",	CMDALER},
	{"COVErage",	CMDCOVE},
	{"HOLDuntil",	CMDHOLD},
	{"CALLerid",	CMDCALL},
	{"SUBJect",	CMDSUBJ},
	{"2WAY",	CMD2WAY},
	{"PING",	CMDPING},
	{"EXPTag",	CMDEXPT},
	{"NOQUEUEing",	CMDNOQU},
	{"ACKRead",	CMDACKR},
	{"RTYPe",	CMDRTYP},
	{"MCREsponse",	CMDMCRE},
	{"MSTAtus",	CMDMSTA},
	{"KTAG",	CMDKTAG},
	{"XDEBug",	CMDXDEB},
	{"XCONfig",	CMDXCON},
	{"XQUEue",	CMDXQUE},
	{"XWHO",	CMDXWHO},
	{NULL,		CMDERROR},
};


/*
** message()
**
** This function sends a string followed by CRLF to the standard output.
**
**	Input:
**		text - the string to print
**
**	Returns:
**		nothing
*/
void
message(char *text)
{
	if (Debug) {
		fprintf(stderr, ">>> %s\r\n", text);
		(void)fflush(stderr);
	}

	printf("%s\r\n", text);
	(void)fflush(stdout);
}


/*
** clear_page()
**
** This function frees the memory allocated to a page during the
** SNPP session.  Memory allocated before the session started (such
** as the socket's file descriptor) may or may not be freed,
** depending on the save parameter.
**
**	Input:
**		p - a pointer to the PAGE structure
**		save - whether to save certain fields
**
**	Returns:
**		nothing
*/
void
clear_page(PAGE *p, int save)
{
	FILE		*peer;
	rcpt_t		*tmp;
	char		*ident;
	char		*msgid;
	char		*hostname;


	/*
	** free all the recipients
	*/
	while (p->rcpts) {
		tmp = p->rcpts;
		p->rcpts = p->rcpts->next;

		my_free(tmp->pager);
		my_free(tmp->coverage);
		free(tmp);
	}

	if (save) {
		/*
		** save a few things
		*/
		peer = p->peer;
		ident = p->ident;
		msgid = p->messageid;
		hostname = p->hostname;
	}

	/*
	** free the page
	*/
	my_free(p->filename);
	my_free(p->message);
	my_free(p->auth);
	my_free(p->from);
	my_free(p->status);

	if (save) {
		(void)memset((char *)p, 0, sizeof(*p));

		/*
		** restore the saved fields
		*/
		p->peer = peer;
		p->ident = ident;
		p->messageid = msgid;
		p->hostname = hostname;
	}
	else
		free(p);
}


/*
** newmsgid()
**
** This function generates a unique message id.  Note that the client
** is not limited to one page per connection, so this message id will
** have to be updated later on after the SEND command is received.
**
**	Input:
**		buf - where to store the new id (must hold >7 characters)
**
**	Returns:
**		nothing
**
**	Note:
**		This assumes we'll never have more than 1000 incoming
**		SNPP connections per minute.  Probably a fair assumption,
**		but if this ever happens we'll end up with duplicate
**		message IDs.
*/
void
newmsgid(char *buf)
{
	static char		idchars[] =
					"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
					"abcdefghijklmnopqrstuvwxyz"
					"0123456789";

	static int		count;
	struct tm		*tm;
	time_t			now;


	now = time(NULL);

	/*
	** use gmtime() so time never goes backwards
	*/
	tm = gmtime(&now);

	buf[0] = idchars[tm->tm_mon];
	buf[1] = idchars[tm->tm_mday-1];
	buf[2] = idchars[tm->tm_hour];
	buf[3] = idchars[tm->tm_min];

	/*
	** make it unique
	*/ 
	(void)sprintf(&buf[4], "%03d", count++);
	count %= 1000;
}


/*
** dohelp()
**
** This function prints the list of SNPP commands supported by this program.
*/
void
dohelp(void)
{
	message("214 ");
	message("214 Level 1 commands accepted:");
	message("214 ");
	message("214     PAGEr <pager ID>");
	message("214     MESSage <alpha or numeric message>");
	message("214     RESEt");
	message("214     SEND");
	message("214     QUIT");
	message("214     HELP");
	message("214 ");
	message("214 Level 2 commands accepted:");
	message("214 ");
	message("214     DATA");
	message("214     LOGIn <loginid> [password]");
	message("214     LEVEl <ServiceLevel>");
	message("214     COVErage <AlternateArea>");
	message("214     HOLDuntil <YYMMDDHHMMSS> [+/-GMTdifference]");
	message("214     CALLerid <CallerID>");
	message("214 ");
	message("214 Level 3 commands accepted:");
	message("214 ");
	message("214     none");
	message("214 ");
	message("250 OK");
}


/*
** dump_pagers()
**
** This function prints the list of pagers/groups known by this server.
** Only those with text descriptions are printed.
*/
void
dump_pagers(void)
{
	pager_t		*pager;
	pgroup_t	*group;
	char		buff[1024];


	for (pager=Pagers; pager; pager=pager->next) {
		if (pager->text == NULL)
			continue;

		(void)sprintf(buff, "214 %s %s", pager->name, pager->text);
		message(buff);
	}

	for (group=Groups; group; group=group->next) {
		if (group->text == NULL)
			continue;

		(void)sprintf(buff, "214 %s %s", group->name, group->text);
		message(buff);
	}
}


/*
** authorize()
**
** This function verifies a userid/password supplied as arguments
** to the SNPP LOGIN command.
**
**	Input:
**		str - the userid and password, separated by whitespace
**
**	Returns:
**		the userid if verification succeeds, NULL if it doesn't
*/
char *
authorize(char *str)
{
	/*
	** add site-specific code here for authorization
	*/
	
	return(strdup(str));
}


/*
** find_recipients()
**
** This function adds a pager to the page's recipient list.  If the pager
** specifies a page group, the group is expanded and each member is added
** to the recipient list.
**
**	Input:
**		p - the page structure
**		str - the pager to be added
**		service - the default paging service
**		holduntil - the default hold time
**		level - the default service level
**
**	Returns:
**		an integer status code (0=success, 1=no one on duty, -1=error)
*/
static int
find_recipients(PAGE *p, char *str, service_t *service, time_t holduntil, int level)
{
	pgroup_t	*group;
	member_t	*member;
	rcpt_t		*tmp;
	char		*ptr;
	int		flags;


	tmp = NULL;
	flags = 0;

	/*
	** interpret the service level
	*/
	switch (level) {
		case LEVEL_SENDMAIL:
			flags |= F_SENDMAIL;
			break;
	}

	/*
	** see if they specified a group name
	*/
	if ((group = lookup(Groups, str)) != NULL) {
		for (member=group->members; member; member=member->next) {
			/*
			** Check the time range for this member to make
			** sure he/she is "on-duty" when the page goes out.
			*/
			if (!on_duty(member->schedule, holduntil)) {
				if (Debug)
					qpage_log(LOG_DEBUG,
						"skipping %s (off-duty)",
						member->pager->name);

				continue;
			}

			/*
			** Members can be listed in a group more than once
			** if they are on-duty during different times of
			** the week.  Check to make sure that this recipient
			** isn't already on the list (i.e. time ranges
			** overlap).
			*/
			for (tmp=p->rcpts; tmp; tmp=tmp->next) {
				if (!strcmp(member->pager->name, tmp->pager)) {
					member = NULL;
					break;
				}
			}

			if (member == NULL)
				continue;

			tmp = (void *)malloc(sizeof(*tmp));
			(void)memset((char *)tmp, 0, sizeof(*tmp));
			tmp->pager = strdup(member->pager->name);
			tmp->holduntil = holduntil;

			if (service)
				tmp->coverage = strdup(service->name);

			tmp->level = level;
			tmp->next = p->rcpts;
			tmp->flags = flags;
			p->rcpts = tmp;
		}

		/*
		** Return success if we found one or more valid recipients.
		*/
		if (tmp)
			return(0);
		else
			return(1);
	}

	/*
	** it wasn't a group name, try an individual pager
	*/
	if (lookup(Pagers, str) == NULL) {
		/*
		** The pager doesn't exist.  Let's
		** see if they gave us a pagerid
		** rather than a username.
		*/
		for (ptr=str; *ptr; ptr++)
			if (!isdigit(*ptr))
				return(-1);

		if (*ptr)
			return(-1);

		if (service == NULL) {
			service = lookup(Services,
				"default");

			if (service == NULL)
				return(-1);
		}

		if (service->allowpid == FALSE)
			return(-1);

		flags |= F_RAWPID;
	}

	tmp = (void *)malloc(sizeof(*tmp));
	(void)memset((char *)tmp, 0, sizeof(*tmp));
	tmp->pager = strdup(str);
	tmp->holduntil = holduntil;

	if (service)
		tmp->coverage = strdup(service->name);

	tmp->level = level;
	tmp->next = p->rcpts;
	tmp->flags = flags;
	p->rcpts = tmp;


	return(0);
}


/*
** snpp()
**
** This function does most all the communication between
** the client and the server.
**
**	Input:
**		p - an empty PAGE structure
**
**	Returns:
**		0 on success, -1 on error
**
**	Note:
**		Calls to qpage_log() from within this function should
**		be treated as though XDEBug never existed.
*/
int
snpp(PAGE *p)
{
	struct cmd	*c;
	service_t	*service;
	time_t		holduntil;
	time_t		now;
	job_t		*joblist;
#ifdef DEBUG
	FILE		*fp;
#endif
	char		buff[1024];
	char		*cmdbuf;
	char		*errmsg;
	char		*a;
	char		*b;
	int		i;
	int		badarg;
	int		gotpager;
	int		gotmessage;
	int		badcommands;
	int		level;
	int		pagecount;


	service = NULL;
	gotpager = 0;
	holduntil = 0;
	gotmessage = 0;
	badcommands = 0;
	pagecount = 0;
	level = DEFAULT_LEVEL;

	now = time(NULL);
	(void)sprintf(buff, "220 QuickPage v%s SNPP server ready at %s",
		VERSION, my_ctime(&now));

	message(buff);

	for (;;) {
		while ((cmdbuf = getinput(p->peer, TRUE)) == NULL) {
			if (++badcommands > MAXBADCOMMANDS) {
				message("421 Too many errors, goodbye");
				return(-1);
			}

			message("500 Command unrecognized");
		}

		/*
		** check for a timeout condition
		*/
		if (*cmdbuf == '\0') {
			message("421 Timeout, goodbye");
			return(-1);
		}

		if (Debug) {
			fprintf(stderr, "--> %s\n", cmdbuf);
			(void)fflush(stderr);
		}

		/*
		** figure out which command they gave us
		*/
		for (c=CmdTab; c->cmdname != NULL; c++) {
			if (strncasecmp(c->cmdname, cmdbuf, 4) == 0)
				break;
		}

		/*
		** find the first argument (if any)
		*/
		a = cmdbuf;
		while (*a && !isspace(*a))
			a++;

		/*
		** nuke leading whitespace from the argument list
		*/
		while (*a && isspace(*a))
			a++;

		/*
		** process the command
		*/
		switch (c->cmdcode) {
			/*
			** Level 1 commands
			*/
			case CMDPAGE:
				errmsg = "550 Error, invalid pager ID";

				if (*a == '\0') {
					message(errmsg);
					break;
				}

				/*
				** find the end of the argument
				*/
				for (b=a; *b && !isspace(*b); b++)
					continue;

				*b = '\0';
				while (*b && isspace(*b))
					b++;

				/*
				** We don't support the level 2 syntax yet
				*/
				if (*b) {
					message(errmsg);
					break;
				}

				i = find_recipients(p, a, service, holduntil,
					level);

				if (i == 1)
					errmsg = "550 Error, no group members on duty";

				if (i) {
					message(errmsg);
					break;
				}

				gotpager++;

				if (holduntil)
					message("250 Pager ID accepted, message will be delayed");
				else
					message("250 Pager ID accepted, message will not be delayed");

				/*
				** set the per-pager options to their defaults
				*/
				service = NULL;
				holduntil = 0;
				level = DEFAULT_LEVEL;
				break;

			case CMDMESS:
				if (gotmessage) {
					message("503 Error, message already entered");
					break;
				}

				if (*a == '\0') {
					message("550 Empty message not allowed");
					break;
				}

				/*
				** trim trailing whitespace
				*/
				b = &a[strlen(a)-1];
				while (b > a && isspace(*b))
					*b-- = '\0';

				p->message = strdup(a);
				strip(&p->message);
				gotmessage++;

				message("250 Message ok");
				break;

			case CMDRESE:
				clear_page(p, TRUE);
				service = NULL;
				gotpager = 0;
				holduntil = 0;
				gotmessage = 0;
				badcommands = 0;
				level = DEFAULT_LEVEL;

				message("250 Reset ok");
				break;

			case CMDSEND:
				if (!gotpager) {
					message("503 Error, no pager ID");
					clear_page(p, TRUE);
					break;
				}

				if (!gotmessage) {
					message("503 Error, no message");
					clear_page(p, TRUE);
					break;
				}

				p->created = time(NULL);
				(void)sprintf(buff, "%d", pagecount++);
				(void)strcat(p->messageid, buff);

				qpage_log(LOG_ALERT, "page submitted, id=%s, from=%s",
					p->messageid,
					p->from ? p->from : "[anonymous]");

				/*
				** If the XDEBug command was issued, send
				** the page out now rather than queueing it.
				** Set the interactive flag so the remote
				** user can see what is going on.
				**
				** KLUDGE ALERT--we're sucking down memory
				** here without ever giving it back.  Good
				** thing this is a separate process that
				** will exit soon.
				*/
				if (Interactive) {
					message("214 Sending message");
					joblist = NULL;
					(void)insert_jobs(&joblist, p);
					send_pages(joblist);
					message("250 Done sending message");
					clear_page(p, TRUE);
					break;
				}

				if (write_page(p, TRUE) < 0) {
					message("554 Message failed (error writing queue file)");
					qpage_log(LOG_ALERT, "write_page() failed for id=%s", p->messageid);
					clear_page(p, TRUE);
					break;
				}

				/*
				** Tell the parent there's work to do.
				*/
				if (Synchronous)
					(void)kill(getppid(), SIGUSR1);

				clear_page(p, TRUE);
				service = NULL;
				gotpager = 0;
				holduntil = 0;
				gotmessage = 0;
				badcommands = 0;
				level = DEFAULT_LEVEL;

				(void)sprintf(buff,
					"250 Message %s queued for processing",
					p->messageid);

				message(buff);
				break;

			case CMDQUIT:
				message("221 OK, goodbye");
				return(0);

			case CMDHELP:
				dohelp();
				break;

			/*
			** Level 2 commands
			*/
			case CMDDATA:
				if (gotmessage) {
					message("503 Error, message already entered");
					break;
				}

				message("354 Begin input; end with <CRLF>'.'<CRLF>");

				p->message = getinput(p->peer, FALSE);

				if (p->message == NULL) {
					message("550 Empty message not allowed");
					break;
				}

				/*
				** check for a timeout condition
				*/
				if (p->message[0] == '\0') {
					message("421 Timeout, goodbye");
					return(-1);
				}

				strip(&p->message);
				gotmessage++;

				message("250 Message ok");
				break;

			case CMDLOGI:
				errmsg = "550 Error, invalid login or password";

				if ((p->auth = authorize(a)) == NULL) {
					message(errmsg);
					break;
				}

				message("250 Login accepted");
				break;

			case CMDLEVE:
				errmsg = "550 Error, invalid service level";

				if (*a == '\0') {
					message(errmsg);
					break;
				}

				badarg = 0;
				for (b=a; *b && !isspace(*b); b++) {
					if (!isdigit(*b)) {
						badarg++;
						break;
					}
				}

				*b = '\0';
				while (*b && isspace(*b))
					b++;

				if (*b)
					badarg++;

				i = atoi(a);

				if ((i < 0) || (i > 11))
					badarg++;

				if (badarg) {
					message(errmsg);
					break;
				}

				level = i;
				message("250 OK, alternate service level accepted");
				break;

			case CMDALER:
				message("500 Command not implemented");
				break;

			case CMDCOVE:
				errmsg = "550 Error, invalid alternate region";

				if (*a == '\0') {
					message(errmsg);
					break;
				}

				/*
				** find the end of the argument
				*/
				for (b=a; *b && !isspace(*b); b++)
					continue;

				*b = '\0';
				while (*b && isspace(*b))
					b++;

				if (*b) {
					message(errmsg);
					break;
				}

				if ((service = lookup(Services, a)) == NULL) {
					message(errmsg);
					break;
				}

				message("250 Alternate coverage selected");
				break;

			case CMDHOLD:
				if ((holduntil = snpptime(a)) == INVALID_TIME) {
					message("550 Error, invalid delivery date/time");
					holduntil = 0;
					break;
				}

				if ((b = my_ctime(&holduntil)) == NULL) {
					message("554 ctime() failed");
					break;
				}

				(void)sprintf(buff, "250 Message for next PAGEr will be delayed until %s", b);
				message(buff);
				break;

			case CMDCALL:
				if (*a == '\0') {
					message("550 Error, invalid caller ID");
					break;
				}

				/*
				** trim trailing whitespace
				*/
				b = &a[strlen(a)-1];
				while (b > a && isspace(*b))
					*b-- = '\0';

				my_free(p->from);
				p->from = strdup(a);

#ifndef NOIDENT
				if (p->ident && strcasecmp(p->ident, p->from))
					message("250 You're a liar, but I'll trust you this time");
				else
#endif
					message("250 Caller ID accepted");
				break;

			case CMDSUBJ:
				message("500 Command not implemented");
				break;

			/*
			** Level 3 commands
			*/
			case CMD2WAY:
			case CMDPING:
			case CMDEXPT:
			case CMDNOQU:
			case CMDACKR:
			case CMDRTYP:
			case CMDMCRE:
			case CMDMSTA:
			case CMDKTAG:
				message("500 Command not implemented");
				break;

#ifdef DEBUG
			/*
			** Other commands not part of SNPP protocol
			*/
			case CMDXDEB:
				qpage_log(LOG_ALERT, "debug mode entered");
				Debug = TRUE;
				Interactive = TRUE;
				setbuf(stdout, NULL);
				message("250 Debug mode entered");
				break;

			case CMDXCON:
				qpage_log(LOG_ALERT, "configuration file requested");

				dump_qpage_config(ConfigFile);

				(void)fflush(stdout);
				(void)fclose(fp);

				message("250 Command complete");
				break;

			case CMDXQUE:
				qpage_log(LOG_ALERT, "page queue requested");

				printf("---------- start queue ----------\n");
				i = showqueue();
				printf("---------- end queue ----------\n");
				(void)fflush(stdout);

				message("250 Command complete");
				break;
#endif /* DEBUG */

			case CMDXWHO:
				qpage_log(LOG_ALERT, "pagerid list requested");
				dump_pagers();
				message("250 Command complete");
				break;

			case CMDERROR:
			default:
				if (++badcommands > MAXBADCOMMANDS) {
					message("421 Too many errors, goodbye");
					return(-1);
				}

				message("500 Command unrecognized");
				break;
		}
	}
}


void
accept_connection(int sock)
{
#ifdef TCP_WRAPPERS
	struct request_info	request;
	char			*ptr;
#endif
	struct sockaddr_in	addr;
	struct hostent		*hp;
	pid_t			pid;
	FILE			*in;
	FILE			*out;
	PAGE			*p;
	char			msgid[100];
	int			len;
	int			s;


	len = sizeof(addr);
	if ((s = accept(sock, (struct sockaddr *)&addr, &len)) < 0) {
#ifdef ERESTART
		if (errno != EINTR && errno != ERESTART)
#else
		if (errno != EINTR)
#endif
			qpage_log(LOG_ERR, "accept() failed: %s",
				strerror(errno));

		return;
	}

	/*
	** Generate a new message ID before the fork() to ensure
	** that it will be somewhat unique.
	*/
	newmsgid(msgid);

	pid = fork();

	if (pid != 0) {
		if (pid < 0)
			qpage_log(LOG_ERR, "fork() failed: %s",
				strerror(errno));

		(void)close(s);
		return;
	}

	/* CHILD */

	/*
	** create a new page structure
	*/
	p = (void *)malloc(sizeof(*p));
	(void)memset((char *)p, 0, sizeof(*p));
	p->messageid = strdup(msgid);

#ifdef TCP_WRAPPERS
	if (request_init(&request, RQ_DAEMON, PROGRAM_NAME, RQ_FILE,
		s, NULL) != NULL) {

		fromhost(&request);

		ptr = eval_user(&request);

		if (ptr && strcmp(ptr, STRING_UNKNOWN) != 0)
			p->ident = strdup(ptr);

		ptr = eval_hostinfo(&request.client);

		if (ptr && strcmp(ptr, STRING_UNKNOWN) != 0)
			p->hostname = strdup(ptr);
	}

	if (!hosts_access(&request)) {
		ptr = "421 Permission Denied by Administrator\r\n";
		(void)write(s, ptr, strlen(ptr));
		(void)close(s);

		qpage_log(LOG_INFO, "connection refused from %s@%s",
			p->ident ? p->ident : "[anonymous]", p->hostname);
		_exit(2);
	}
#endif

	if (p->hostname == NULL) {
		/*
		** figure out where this connection came from
		*/
		hp = gethostbyaddr((char *)&addr.sin_addr.s_addr,
			sizeof(addr.sin_addr.s_addr), AF_INET);

		if (hp == NULL) {
			/*
			** allocate enough room for [nnn.nnn.nnn.nnn]
			*/
			p->hostname = (void *)malloc(17+1);
			(void)sprintf(p->hostname, "[%s]",
				inet_ntoa(addr.sin_addr));
		}
		else
			p->hostname = strdup(hp->h_name);
	}

	if (IdentTimeout && p->ident == NULL)
		p->ident = ident(s);

	qpage_log(LOG_INFO, "connection from %s@%s",
		p->ident ? p->ident : "[anonymous]", p->hostname);


	if ((in = fdopen(s, "r")) == NULL) {
		qpage_log(LOG_ERR, "cannot reopen socket");
		_exit(-1);
	}

	if ((out = fdopen(dup(s), "w")) == NULL) {
		qpage_log(LOG_ERR, "cannot reopen socket");
		_exit(-1);
	}

	/*
	** stdout goes to the socket
	*/
	if (fileno(out) != fileno(stdout))
		(void)dup2(fileno(out), fileno(stdout));

	p->peer = in;
	(void)snpp(p);

	qpage_log(LOG_INFO, "disconnect from %s@%s",
		p->ident ? p->ident : "[anonymous]", p->hostname);

	(void)fclose(stdout);
	(void)fclose(out);
	(void)fclose(in);
	(void)close(s);
	_exit(0);
}


/*
** become_daemon()
**
** This function opens a socket on the SNPP port and waits for
** incoming connections.  When a connection is established, this
** function forks.  The parent continues to listen for incoming
** connections while the child is handed off to snpp().
**
** A background process is also started which continually processes
** the page queue.  If there are jobs remaining in the queue after
** processing (such as jobs scheduled to run in the future), the
** background process waits for a specified number of seconds and
** starts over again.  Otherwise, the background process goes to
** sleep until a new job is submitted to the queue.
**
**	Input:
**		sleeptime - the time interval to sleep between iterations
**
**	Returns:
**		-1 on error, otherwise never
*/
int
become_daemon(int sleeptime, short port)
{
	struct sockaddr_in	addr;
	struct servent		*svc;
	struct timeval		timeout;
#ifdef HAVE_POLL
	struct pollfd		fds;
#else
	fd_set			readfds;
#endif
	time_t			lastrun;
	time_t			now;
	pid_t			childpid;
	char			pid[30];
	int			dontsend;
	int			sock;
	int			len;
	int			on;
	int			fd;
	int			i;


	if (sleeptime < 0) {
		dontsend = TRUE;
		sleeptime = 0;
	}
	else
		dontsend = FALSE;

	lastrun = 0;
	childpid = 0;

	if (port == 0) {
		/*
		** Get the SNPP port number from the /etc/services map.
		** Note that the port number is returned in network byte
		** order so no call to htons() is required.
		*/
		if ((svc = getservbyname("snpp", "tcp")) != NULL)
			port = svc->s_port;
		else
	
			port = htons(SNPP_SVC_PORT);
	}
	else
		port = htons(port);

	/*
	** Make sure we have permission to bind to this port.
	*/
	if (ntohs(port) < 1024 && geteuid() != 0)
		fprintf(stderr, "Warning: daemon must be started as root\n");

	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "socket() failed: %s\n", strerror(errno));
		return(-1);
	}

	/*
	** Attempt to set REUSEADDR but it's no big deal if this fails.
	*/
	on = 1;
	len = sizeof(on);
	(void)setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&on, len);

	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_family = AF_INET;
	addr.sin_port = port;

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "bind() failed: %s\n", strerror(errno));
		(void)close(sock);
		return(-1);
	}

	if (listen(sock, 5) < 0) {
		fprintf(stderr, "listen() failed: %s\n", strerror(errno));
		(void)close(sock);
		return(-1);
	}

	/*
	** Give up all root permissions now that we're bound to the SNPP port
	*/
	drop_root_privileges();

	if (get_qpage_config(ConfigFile) != 0) {
		fprintf(stderr, "Error reading configuration file\n");
		return(-1);
	}

	if (!Debug) {
		/*
		** Detatch ourselves from the controling tty.
		*/
		switch (fork()) {
			case (pid_t)-1:
				qpage_log(LOG_ERR, "fork() failed: %s",
					strerror(errno));
				return(-1);

			case 0:
				/*
				** Ensure that we will never have a
				** controlling terminal.
				*/
				(void)setsid();
				if (fork())
					_exit(-1);
				break;

			default:
				_exit(0);
		}
	}

	/*
	** Attempt to write our PID to a file specified by the
	** administrator.  Note that we do not know the filename
	** until after we've read the configuration file and we
	** can't read the configuration file until after we've
	** dropped our root permissions (because otherwise the
	** access() calls in config.c will be using the wrong
	** permissions).  Therefore, it is safe to use O_CREAT
	** and O_TRUNC because we know it's not possible for
	** someone to trick us into doing the wrong thing by
	** feeding us a symbolic link.
	**
	** The obvious downside here is that the file will most
	** likely want to be in some system directory such as /etc,
	** which means if the file doesn't already exist then the
	** O_CREAT flag probably won't do us any good.  Such is
	** life, I guess.
	*/
	if (PIDfile) {
		/*
		** let the current umask determine the default permissions
		*/
		fd = open(PIDfile, O_WRONLY|O_CREAT|O_TRUNC, 0666);

		if (fd >= 0) {
			(void)sprintf(pid, "%d\n", (int)getpid());
			(void)write(fd, pid, strlen(pid));
			(void)close(fd);
		}
	}

	if (Debug)
		qpage_log(LOG_DEBUG, "waiting for incoming connections");

	/*
	** spin forever, waiting for connections and processing the queue
	*/
	for (;;) {
		timeout.tv_sec = sleeptime;
		timeout.tv_usec = 0;

		if (JobsPending)
			timeout.tv_sec = 0;

#ifdef HAVE_POLL
		fds.fd = sock;
		fds.events = POLLIN;
		fds.revents = 0;

		i = poll(&fds, 1, timeout.tv_sec * 1000);
#else
		FD_ZERO_LINTED(&readfds);
		FD_SET(sock, &readfds);

		i = select(FD_SETSIZE, &readfds, 0, 0,
			sleeptime ? &timeout : NULL);
#endif

		if (i < 0) {
#ifdef ERESTART
			if (errno != EINTR && errno != ERESTART)
#else
			if (errno != EINTR)
#endif
#ifdef HAVE_POLL
				qpage_log(LOG_ERR, "poll() failed: %s",
					strerror(errno));
#else
				qpage_log(LOG_ERR, "select() failed: %s",
					strerror(errno));
#endif
		}

		if (ReReadConfig) {
			ReReadConfig = FALSE;
			qpage_log(LOG_NOTICE, "rereading configuration");

			if (get_qpage_config(ConfigFile)) {
				qpage_log(LOG_NOTICE,
					"new configuration has errors");
			}
		}

#ifdef HAVE_POLL
		if (fds.revents & POLLIN)
			accept_connection(sock);
#else
		if (i > 0 && FD_ISSET(sock, &readfds))
			accept_connection(sock);
#endif

		now = time(NULL);

		/*
		** If there's a living child processing the page queue,
		** we should just go back to waiting for more incoming
		** network requests.
		*/
		if (childpid > 0 && kill(childpid, 0) == 0)
			/*
			** Make sure the child hasn't wedged itself.
			** It shouldn't take more than 5 minutes to
			** do a queue run.
			*/
			if (now - lastrun > 300) {
				qpage_log(LOG_WARNING, "killing stuck child");
				(void)kill(childpid, SIGKILL);
			}
			else
				continue;

		/*
		** hack to process new jobs without waiting for sleeptime
		*/
		if (JobsPending) {
			JobsPending = FALSE;
			lastrun = 0;
		}

		if (now - lastrun >= sleeptime && dontsend == FALSE) {
			lastrun = now;

/*			qpage_log(LOG_DEBUG, "processing the page queue"); */

			childpid = fork();

			switch (childpid) {
				case (pid_t)-1:
					qpage_log(LOG_ERR,
						"fork() failed: %s",
						strerror(errno));
					break;

				case 0:
					i = runqueue();
					_exit(i);
			}
		}
	}
}
