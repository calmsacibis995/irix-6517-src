/*
** QuickPage
**
**	This program sends pages to an alpha-numeric pager.
**
**	Author:		Thomas Dwyer III <tomiii@qpage.org>
**	Version:	3.3beta6
**
*/
#include	"qpage.h"


/*
** Global variables
*/
#ifndef lint
static char	sccsid[] = "@(#)qpage.c  3.35  07/07/98  tomiii@qpage.org";
#endif
char		*ConfigFile = NULL;
int		Debug = 0;
int		Interactive = FALSE;
int		Silent = FALSE;


/*
** do_version()
*/
void
do_version(void)
{
	printf("\n");
	printf("QuickPage v%s, Copyright 1995-97 by Thomas Dwyer III\n",
		VERSION);
	printf("\n");
}


/*
** do_usage()
*/
void
do_usage(char *prog)
{
	do_version();
	printf("Usage: %s [ options ] [ pagerid ] [ message ]\n", prog);
	printf("\n");
	printf("Options:\n");
	printf("    -a [+]hhmm    send the page at the specified time\n");
	printf("    -c coverage   coverage area (service name) for pager\n");
	printf("    -C config     use an alternate configuration file\n");
	printf("    -d            debug mode\n");
	printf("    -f from       who the page is from\n");
	printf("    -h            help (this screen)\n");
#ifndef CLIENT_ONLY
	printf("    -i            interactive mode\n");
#endif
	printf("    -l level      level of service\n");
	printf("    -m            read an e-mail message as client input\n");
	printf("    -p pager      pager id to recieve the message\n");
	printf("    -P pager      same as -p but don't reset -[acl]\n");
#ifndef CLIENT_ONLY
	printf("    -q xxx        process the queue every xxx seconds\n");
	printf("    -Q            show the pages currently in the queue\n");
#endif
	printf("    -s server     name of SNPP the server (default=%s)\n",
		SNPP_SERVER);
	printf("    -v            print qpage version and exit\n");
	printf("\n");
}


/*
** command_line_message()
**
** Build a message from the remaining command line arguments.  If
** there is no message on the command line, prompt the user to enter
** the message from standard input.
**
**	Input:
**		argc - the number of arguments
**		argv - an array of argument strings
**
**	Returns:
**		the message text
*/
char *
command_line_message(int argc, char **argv)
{
	char	*buf;
	char	*src;
	char	*dst;
	int	buflen;
	int	len;
	int	i;


	buf = NULL;
	dst = NULL;
	len = 0;
	buflen = 0;

	/*
	** Loop through each argument, appending it (followed by a space)
	** to the message buffer.
	*/
	for (i=0; i<argc; i++) {
		src = argv[i];
		len = strlen(src);

		/*
		** make sure the buffer is big enough to hold this argument
		*/
		while (buflen-(dst-buf) <= len + 1) {
			buf = (char *)my_realloc(buf, buflen+BUFCHUNKSIZE);
			buflen += BUFCHUNKSIZE;

			if (dst == NULL)
				buf[0] = '\0';

			dst = buf + strlen(buf);
		}

		(void)strcpy(dst, src);
		dst += len;
		*dst++ = ' ';
		*dst = '\0';
	}

	/*
	** nuke the trailing space
	*/
	if (dst)
		*(dst-1) = '\0';

	/*
	** check for an empty message
	*/
	if (buf && *buf == '\0') {
		free(buf);
		buf = NULL;
	}

	if (buf == NULL) {
		if (Silent == FALSE)
			printf("Enter the message; end with '.' on a "
				"line by itself\n");

		buf = getinput(stdin, FALSE);

		if (Silent == FALSE && buf && *buf)
			printf("Ok.\n");
	}

	if (buf && *buf)
		return(buf);
	else
		return(NULL);
}


/*
** add_recipients()
**
** This function adds recipients to the PAGE structure.
**
**	Input:
**		p - the PAGE structure
**		str - the string of recipients to parse
**		coverage - the coverage in effect
**		holduntil - the hold-time in effect
**		level - the service level in effect
**
**	Returns:
**		nothing
*/
static void
add_recipients(PAGE *p, char *str, char *coverage, time_t holduntil, int level)
{
	char		*ptr;
	rcpt_t		*tmp;


	if ((ptr = strtok(str, ",/:")) == NULL)
		return;

	do {
		tmp = (void *)malloc(sizeof(*tmp));
		(void)memset((char *)tmp, 0, sizeof(*tmp));
		tmp->next = p->rcpts;
		tmp->pager = strdup(ptr);
		tmp->holduntil = holduntil;
		tmp->coverage = coverage ? strdup(coverage) : NULL;
		tmp->level = level;

		p->rcpts = tmp;
	}
	while ((ptr = strtok(NULL, ",/:")) != NULL);
}


#ifndef CLIENT_ONLY
/*
** expand_recipients()
**
** This function expands a group into individual recipients.  This
** function is only called when using qpage in interactive mode.
**
**	Input:
**		p - the page containing the recipient list to expand
**
**	Returns:
**		nothing
*/
void
expand_recipients(PAGE *p)
{
	pgroup_t	*group;
	member_t	*member;
	rcpt_t		*list;
	rcpt_t		*tmp;


	list = p->rcpts;

	while (list) {
		if ((group = lookup(Groups, list->pager)) == NULL) {
			list = list->next;
			continue;
		}

		if (group->members) {
			free(list->pager);
			list->pager = strdup(group->members->pager->name);
			member = group->members->next;

			while (member) {
				tmp = (void *)malloc(sizeof(*tmp));
				(void)memset((char *)tmp, 0, sizeof(*tmp));
				tmp->pager = strdup(member->pager->name);
				tmp->next = list->next;
				list->next = tmp;
				list = tmp;

				member = member->next;
			}
		}

		list = list->next;
	}
}
#endif /* CLIENT_ONLY */


/*
** main()
*/
int
main(int argc, char **argv)
{
#ifndef CLIENT_ONLY
	struct sigaction	action;
	job_t			*joblist;
	short			port;
	char			msgid[100];
	int			sleeptime;
	int			showq;
#endif
	PAGE			p;
	time_t			t;
	time_t			holduntil;
	char			*coverage;
	char			*server;
	int			needpager;
	int			level;
	int			readmail;
	int			daemon;
	int			c;
	int			i;

	extern int		optind;
	extern char		*optarg;


	if (isatty(fileno(stdin)) == 0)
		Silent = TRUE;

#ifndef CLIENT_ONLY
	sleeptime = 0;
	port = 0;

	(void)memset((char *)&action, 0, sizeof(action));

#ifdef SA_NODEFER
	action.sa_flags |= SA_NODEFER;
#endif
#ifdef SA_NOCLDSTOP
	action.sa_flags |= SA_NOCLDSTOP;
#endif

	/*
	** Use sigaction() instead of signal() because we don't
	** want the SA_RESETHAND flag set.  That is, we want the
	** signal handler to remain in effect after the signal
	** is caught.
	*/
	action.sa_handler = (RETSIGTYPE(*)())sigalrm;
	if (sigaction(SIGALRM, &action, NULL) < 0)
		perror("sigaction(SIGALRM)");

	action.sa_handler = (RETSIGTYPE(*)())sigterm;
	if (sigaction(SIGTERM, &action, NULL) < 0)
		perror("sigaction(SIGTERM)");

	action.sa_handler = (RETSIGTYPE(*)())sigchld;
	if (sigaction(SIGCHLD, &action, NULL) < 0)
		perror("sigaction(SIGCHLD)");

	action.sa_handler = (RETSIGTYPE(*)())sighup;
	if (sigaction(SIGHUP, &action, NULL) < 0)
		perror("sigaction(SIGHUP)");

	action.sa_handler = (RETSIGTYPE(*)())sigusr1;
	if (sigaction(SIGUSR1, &action, NULL) < 0)
		perror("sigaction(SIGUSR1)");

	openlog(PROGRAM_NAME, LOG_PID|LOG_NDELAY|LOG_NOWAIT, SYSLOG_FACILITY);
#endif /* CLIENT_ONLY */

	server = NULL;
	coverage = NULL;
	holduntil = 0;
	level = DEFAULT_LEVEL;
	readmail = FALSE;
	daemon = FALSE;
	needpager = FALSE;
#ifndef CLIENT_ONLY
	showq = FALSE;
#endif

	(void)memset((char *)&p, 0, sizeof(p));

	while ((c = getopt(argc, argv, "a:c:C:df:hil:mp:P:q:Qs:t:v")) != -1) {
		switch (c) {
			case 'a':
			case 't':
				if ((t = parse_time(optarg)) == INVALID_TIME) {
					fprintf(stderr,
						"Invalid time: %s\n", optarg);
					return(-1);
				}

				holduntil = t;
				needpager = TRUE;
				break;

			case 'c':
				my_free(coverage);
				coverage = strdup(optarg);
				needpager = TRUE;
				break;

			case 'C':
				ConfigFile = strdup(optarg);
				break;

			case 'd':
				Debug++;
				break;

			case 'f':
				my_free(p.from);
				p.from = strdup(optarg);
				break;

			case 'h':
			case '?':
				do_usage(argv[0]);
				return(-1);

			case 'i':
				if (daemon) {
					fprintf(stderr, "Error: -i not valid with -q option\n");
					return(-1);
				}
#ifndef CLIENT_ONLY
				Interactive = TRUE;
				break;
#else
				fprintf(stderr, "Interactive mode not allowed.\n");
				return(-1);
#endif /* CLIENT_ONLY */

			case 'l':
				i = atoi(optarg);
				if ((i < 0) || (i > 11)) {
					fprintf(stderr, "Level must be between 0 and 11\n");
					return(-1);
				}

				level = i;
				needpager = TRUE;
				break;

			case 'm':
				readmail = TRUE;
				break;

			case 'p':
			case 'P':
				add_recipients(&p, optarg,
					coverage, holduntil, level);

				if (c == 'p') {
					my_free(coverage);
					coverage = NULL;
					holduntil = 0;
					level = DEFAULT_LEVEL;
				}
				needpager = FALSE;
				break;

			case 'q':
#ifndef CLIENT_ONLY
				if (Interactive) {
					fprintf(stderr, "Error: -q not valid with -i option\n");
					return(-1);
				}

				daemon = TRUE;
				sleeptime = atoi(optarg);
				break;
#else
				fprintf(stderr, "Server mode not allowed.\n");
				return(-1);
#endif /* CLIENT_ONLY */

			case 'Q':
#ifndef CLIENT_ONLY
				showq = TRUE;
				break;
#else
				fprintf(stderr, "Error: -Q support not compiled in.\n");
				return(-1);
#endif /* CLIENT_ONLY */

			case 's':
				server = optarg;
				break;

			case 'v':
				do_version();
				return(0);

			default:
				fprintf(stderr, "Internal program error\n");
				return(-1);
		}
	}

	if (ConfigFile != NULL) {
		if (daemon == FALSE && Interactive == FALSE) {
			fprintf(stderr,
				"Error: -C not allowed in client mode\n");

			return(-1);
		}
	}
	else
		ConfigFile = strdup(QPAGE_CONFIG);

#ifndef CLIENT_ONLY
	if (showq) {
		drop_root_privileges();

		if (get_qpage_config(ConfigFile) != 0) {
			fprintf(stderr, "Error reading configuration file\n");
			return(-1);
		}

		i = showqueue();
		printf("\n");
		return(i);
	}

	if (daemon) {
		if (sleeptime == 0) {
			drop_root_privileges();

			if (get_qpage_config(ConfigFile) != 0) {
				fprintf(stderr,
					"Error reading configuration file\n");
				return(-1);
			}

			(void)runqueue();
			return(0);
		}

		/*
		** Use a port specified on the command line.  This is
		** undocumented and unsupported.  Use at your own risk.
		*/
		if (optind < argc)
			port = atoi(argv[optind]);

		(void)become_daemon(sleeptime, port);

		/* NOT REACHED */
		return(-1);
	}
#endif /* CLIENT_ONLY */

	if (p.rcpts == NULL && optind < argc) {
		add_recipients(&p, argv[optind++], coverage, holduntil, level);
		needpager = FALSE;
	}

	if (p.rcpts == NULL) {
		fprintf(stderr, "No recipients!\n");
		return(-1);
	}

	if (needpager) {
		fprintf(stderr, "The -a, -c and -l options must come *before* the pagerids they affect\n");
		return(-1);
	}


	if (readmail)
		read_mail(&p);
	else
		p.message = command_line_message(argc-optind, &argv[optind]);

	if (p.message == NULL) {
		fprintf(stderr, "No message!\n");
		return(-1);
	}

	/*
	** fix the "from" address if necessary
	*/
	if (p.from == NULL)
		p.from = strdup(get_user());

	if (p.from[0] == '\0') {
		free(p.from);
		p.from = NULL;
	}


#ifndef CLIENT_ONLY
	if (Interactive) {
		/*
		** Previous versions let people use the -i
		** flag as root.  This resulted in lots of
		** confusion about why qpage worked fine
		** with the -i flag but not in regular
		** daemon mode.  I think it's better to
		** just make them both work the same.
		*/
		drop_root_privileges();

		if (get_qpage_config(ConfigFile) != 0) {
			fprintf(stderr, "Error reading configuration file\n");
			return(-1);
		}

		expand_recipients(&p);

		strip(&p.message);

		newmsgid(msgid);
		(void)strcat(msgid, "I");

		p.messageid = strdup(msgid);
		joblist = NULL;
		(void)insert_jobs(&joblist, &p);
		send_pages(joblist);
		i = 0;
	}
	else {
#endif /* CLIENT_ONLY */
		if (server == NULL)
			server = getenv("SNPP_SERVER");

#ifdef SNPP_SERVER_FILE
		if (server == NULL) {
			struct stat	statbuf;
			int		fd;


			if ((fd = open(SNPP_SERVER_FILE, O_RDONLY, 0)) >= 0) {
				if (fstat(fd, &statbuf) < 0) {
					perror("fstat() failed");
					return(-1);
				}

				i = statbuf.st_size;
				server = (void *)malloc(i+1);

				if (read(fd, server, i) != i) {
					perror("read() failed");
					return(-1);
				}

				server[i] = '\0';

				(void)close(fd);
			}
		}
#endif

		if (server == NULL)
			server = SNPP_SERVER;

		i = submit_page(&p, server);
#ifndef CLIENT_ONLY
	}

	closelog();
#endif
	return(i);
}
