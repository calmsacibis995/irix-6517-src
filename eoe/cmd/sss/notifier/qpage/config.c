#include	"qpage.h"


/*
** global variables
*/
#ifndef lint
static char	sccsid[] = "@(#)config.c  1.27  07/26/98  tomiii@qpage.org";
#endif
service_t	*Services = NULL;
service_t	*DefaultService = NULL;
pager_t		*Pagers = NULL;
pgroup_t	*Groups = NULL;
modem_t		*Modems = NULL;
char		*PIDfile = NULL;
char		*Administrator = NULL;
char		*ForceHostname = NULL;
char		*SigFile = NULL;
char		*QueueDir = NULL;
char		*LockDir = NULL;
int		IdentTimeout = DEFAULT_IDENTTIMEOUT;
int		SNPPTimeout = DEFAULT_SNPPTIMEOUT;
int		Synchronous = TRUE;


/*
**
** init_service_defaults()
**
** This function initializes a paging service to the default values
** from the default paging service.
**
**	Input:
**		service - the service to set the defaults for
**
**	Returns:
**		nothing
*/
void
init_service_defaults(service_t *service)
{
	if (DefaultService->device)
		service->device = strdup(DefaultService->device);

	if (DefaultService->dialcmd)
		service->dialcmd = strdup(DefaultService->dialcmd);

	if (DefaultService->phone)
		service->phone = strdup(DefaultService->phone);

	if (DefaultService->password)
		service->password = strdup(DefaultService->password);

	service->baudrate   = DefaultService->baudrate;
	service->parity     = DefaultService->parity;
	service->maxmsgsize = DefaultService->maxmsgsize;
	service->maxpages   = DefaultService->maxpages;
	service->maxtries   = DefaultService->maxtries;
	service->identfrom  = DefaultService->identfrom;
	service->allowpid   = DefaultService->allowpid;
	service->msgprefix  = DefaultService->msgprefix;
}


/*
** check_device()
**
** This function verifies whether a service's device specification is
** valid or not.
**
**	Input:
**		filename - the name of the configuration file being read
**		lineno - the current line number being read
**		value - the value of the "device" keyword
**
**	Returns:
**		The number of errors found (i.e. 0=success, >0=failure)
*/
int
check_device(char *filename, int lineno, char *value)
{
	char	*ptr;
	char	*last;
	char	*res;
	int	ok;


	ok = FALSE;

	res = (void *)malloc(strlen(value)+1);
	ptr = safe_strtok(value, ",", &last, res);
	do {
		if (lookup(Modems, ptr) != NULL) {
			ok = TRUE;
			continue;
		}

		if (ptr[0] != '/') {
			qpage_log(LOG_WARNING, "%s(%d): undefined modem %s",
				filename, lineno, ptr);

			continue;
		}

		if (access(ptr, R_OK|W_OK) < 0) {
			qpage_log(LOG_WARNING, "%s(%d): cannot access %s: %s",
				filename, lineno, ptr, strerror(errno));

			continue;
		}

		ok = TRUE;
	}
	while ((ptr = safe_strtok(NULL, ",", &last, res)) != NULL);
	free(res);

	return(ok);
}


/*
** check_modem()
**
** This function verifies that a modem specification has all the
** required information.
**
**	Input:
**		filename - the name of the configuration file being read
**		lineno - the current line number being read
**		modem - the modem specification to be checked
**
**	Returns:
**		The number of errors found (i.e. 0=success, >0=failure)
*/
int
check_modem(char *filename, int lineno, modem_t *modem)
{
	if (modem == NULL)
		return(0);

	if (modem->device != NULL)
		return(0);

	if (modem->name[0] == '/') {
		if (access(modem->name, R_OK|W_OK) == 0) {
			modem->device = strdup(modem->name);
			return(0);
		}

		qpage_log(LOG_ERR, "%s(%d): cannot access %s: %s",
			filename, lineno, modem->name, strerror(errno));
	}
	else {
		qpage_log(LOG_ERR, "%s(%d): no device for modem=%s",
			filename, lineno, modem->name);
	}

	return(1);
}


/*
** check_service()
**
** This function verifies that a service specification has all the
** required information.  If a required field is missing, a bogus
** string is substituted and an error is returned.  We still keep
** the service entry around because otherwise it would cause errors
** for pager entries that reference them (it's misleading to users).
**
**	Input:
**		filename - the current filename
**		lineno - the current line number
**		service - the service specification to be checked
**
**	Returns:
**		The number of errors found (i.e. 0=success, >0=failure)
*/
int
check_service(char *filename, int lineno, service_t *service)
{
	char	*ptr;
	char	*last;
	char	*res;
	int	errors;


	if (service == NULL)
		return(0);

	if (service->device == NULL) {
		qpage_log(LOG_ERR, "%s(%d): no modem device for service=%s",
			filename, lineno, service->name);

		service->device = strdup("FATAL_CONFIGURATION_ERROR");
		return(1);
	}

	errors = 0;

	/*
	** make sure we have enough information to dial the modem
	*/
	if (service->dialcmd == NULL) {
		if (service->phone == NULL) {
			qpage_log(LOG_ERR, "%s(%d): no phone number for "
				"service=%s", filename, lineno, service->name);

			errors++;
		}

		res = (void *)malloc(strlen(service->device)+1);
		ptr = safe_strtok(service->device, ",", &last, res);
		do {
			if (lookup(Modems, ptr) != NULL)
				continue;

			qpage_log(LOG_ERR, "%s(%d): no dial command for "
				"service=%s, device=%s", filename, lineno,
				service->name, ptr);

			errors++;
		}
		while ((ptr = safe_strtok(NULL, ",", &last, res)) != NULL);
		free(res);
	}

	return(errors);
}


/*
** check_pager()
**
** This function verifies that a pager specification has all the
** required information.  If a required field is missing, the entry
** for this pager is freed.  Note that we are aways passed the head
** of a linked list so we don't need to worry about changing a pointer
** pointing at this pager.
**
**	Input:
**		filename - the current filename
**		lineno - the current line number
**		pager - a pointer to the pager structure
**
**	Returns:
**		The number of errors found (i.e. 0=success, >0=failure)
*/
int
check_pager(char *filename, int lineno, pager_t **pager)
{
	pager_t	*tmp;


	tmp = *pager;

	if (tmp && tmp->pagerid == NULL) {
		qpage_log(LOG_ERR, "%s(%d): no pagerid for pager=%s",
			filename, lineno, tmp->name);
		*pager = tmp->next;
		free(tmp->name);
		my_free(tmp->text);
		free(tmp);
		return(1);
	}

	return(0);
}


/*
** check_group()
**
** This function verifies that a group specification has all the
** required information.
**
**	Input:
**		filename - the current filename
**		lineno - the current line number
**		group - the group specification to be checked
**
**	Returns:
**		The number of errors found (i.e. 0=success, >0=failure)
*/
int
check_group(char *filename, int lineno, pgroup_t *group)
{
	if (group && group->members == NULL) {
		qpage_log(LOG_ERR, "%s(%d): group %s has no members",
			filename, lineno, group->name);

		return(1);
	}

	return(0);
}


/*
** read_config_file()
**
** This function reads the configuration file.
**
**	Input:
**		the name of a configuration file
**
**	Returns:
**		an integer status code
**
**	Side effects:
**		This function changes the current working directory to
**		that of the page queue as specified in the configuration
**		file.  It is an error if no such specification is present.
**
**		The following global variables are modified:
**
**			Pagers
**			Services
**			Groups
**			Modems
**			IdentTimeout
**			SNPPTimeout
**			Administrator
**			ForceHostname
*/
int
read_config_file(char *filename)
{
	FILE		*fp;
	modem_t		*modem;
	service_t	*service;
	pager_t		*pager;
	pgroup_t	*group;
	member_t	*tmp;
	char		buff[1024];
	char		value[1024];
	char		*ptr;
	int		errors;
	int		lineno;
	int		pos;


	modem = NULL;
	service = NULL;
	errors = 0;
	pager = NULL;
	group = NULL;
	lineno = 0;
	pos = 0;

	if (Services == NULL) {
		Services = (void *)malloc(sizeof(service_t));
		(void)memset((char *)Services, 0, sizeof(*Services));
		Services->name = strdup("default");
		Services->dialcmd = NULL;
		Services->baudrate = DEFAULT_BAUDRATE;
		Services->parity = DEFAULT_PARITY;
		Services->maxmsgsize = DEFAULT_MAXMSGSIZE;
		Services->maxpages = DEFAULT_MAXPAGES;
		Services->maxtries = DEFAULT_MAXTRIES;
		Services->identfrom = DEFAULT_IDENTFROM;
		Services->allowpid = DEFAULT_ALLOWPID;
		Services->msgprefix = DEFAULT_PREFIX;

		DefaultService = Services;
	}

	if (QueueDir == NULL)
		QueueDir = strdup(DEFAULT_QUEUEDIR);

	if ((fp = fopen(filename, "r")) == NULL) {
		qpage_log(LOG_ERR, "cannot open %s: %s", filename,
			strerror(errno));
		return(-1);
	}

	for (;;) {
		if (pos && buff[pos]) {
#ifdef CONFDEBUG
			printf("Leftover characters: <%s>\n", &buff[pos]);
#endif
			(void)strcpy(buff, &buff[pos]);
		}
		else {
			if (fgets(buff, sizeof(buff), fp) == NULL)
				break;

			lineno++;
		}

#ifndef STRICT_COMMENTS
		/*
		** let comments to start anywhere in the line
		*/
		if ((ptr = strchr(buff, '#')) != NULL)
			*ptr = '\0';
#endif

		/*
		** Skip comments and blank lines
		*/
		if ((buff[0] == '#') || (buff[0] == '\n')) {
			pos = 0;
			continue;
		}

		/*
		** strip trailing newlines
		*/
		if ((ptr = strchr(buff, '\n')) != NULL)
			*ptr = '\0';

		/*
		** This is the name of another configuration file we
		** need to process.
		*/
		if (sscanf(buff, "include=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
			printf("include=<%s>\n", value);
#endif
			errors += check_modem(filename, lineno, modem);
			errors += check_service(filename, lineno, service);
			errors += check_pager(filename, lineno, &Pagers);
			errors += check_group(filename, lineno, group);
			modem = NULL;
			service = NULL;
			pager = NULL;
			group = NULL;

			errors += read_config_file(value);
			continue;
		}

		/*
		** This specifies whether a child process handling an
		** SNPP connection should notify its parent that there's
		** work to do.  If false, the parent will wait for the
		** normal sleep timer to expire before processing the
		** page queue.
		*/
		if (sscanf(buff, "synchronous=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
			printf("synchronous=<%s>\n", value);
#endif
			errors += check_modem(filename, lineno, modem);
			errors += check_service(filename, lineno, service);
			errors += check_pager(filename, lineno, &Pagers);
			errors += check_group(filename, lineno, group);
			modem = NULL;
			service = NULL;
			pager = NULL;
			group = NULL;

			if (!strcasecmp(value, "no"))
				Synchronous = FALSE;

			if (!strcasecmp(value, "off"))
				Synchronous = FALSE;

			if (!strcasecmp(value, "false"))
				Synchronous = FALSE;

			continue;
		}

		/*
		** This is the location of the queue directory.  This
		** keyword causes the termination of any current service,
		** pager, or group specification.
		*/
		if (sscanf(buff, "queuedir=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
			printf("queuedir=<%s>\n", value);
#endif
			errors += check_modem(filename, lineno, modem);
			errors += check_service(filename, lineno, service);
			errors += check_pager(filename, lineno, &Pagers);
			errors += check_group(filename, lineno, group);
			modem = NULL;
			service = NULL;
			pager = NULL;
			group = NULL;

			if (access(value, R_OK|W_OK|X_OK) < 0) {
				qpage_log(LOG_ERR,
					"%s(%d): cannot access %s: %s",
					filename, lineno, value,
					strerror(errno));

				errors++;
				continue;
			}

			my_free(QueueDir);
			QueueDir = strdup(value);

			continue;
		}

		/*
		** This is the location of the lock directory.  This
		** keyword causes the termination of any current service,
		** pager, or group specification.
		*/
		if (sscanf(buff, "lockdir=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
			printf("lockdir=<%s>\n", value);
#endif
			errors += check_modem(filename, lineno, modem);
			errors += check_service(filename, lineno, service);
			errors += check_pager(filename, lineno, &Pagers);
			errors += check_group(filename, lineno, group);
			modem = NULL;
			service = NULL;
			pager = NULL;
			group = NULL;

			if (access(value, R_OK|W_OK|X_OK) < 0) {
				qpage_log(LOG_ERR,
					"%s(%d): cannot access %s: %s",
					filename, lineno, value,
					strerror(errno));

				errors++;
				continue;
			}

			my_free(LockDir);
			LockDir = strdup(value);

			continue;
		}

		/*
		** This is the ident timeout in seconds.  This keyword
		** causes the termination of any current service, pager,
		** or group specification.
		*/
		if (sscanf(buff, "identtimeout=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
			printf("identtimeout=<%s>\n", value);
#endif
			errors += check_modem(filename, lineno, modem);
			errors += check_service(filename, lineno, service);
			errors += check_pager(filename, lineno, &Pagers);
			errors += check_group(filename, lineno, group);
			modem = NULL;
			service = NULL;
			pager = NULL;
			group = NULL;

			IdentTimeout = atoi(value);

			if (IdentTimeout < 0)
				IdentTimeout = 0;

			continue;
		}

		/*
		** This is the SNPP command timeout in seconds.  This keyword
		** causes the termination of any current service, pager, or
		** group specification.
		*/
		if (sscanf(buff, "snpptimeout=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
			printf("snpptimeout=<%s>\n", value);
#endif
			errors += check_modem(filename, lineno, modem);
			errors += check_service(filename, lineno, service);
			errors += check_pager(filename, lineno, &Pagers);
			errors += check_group(filename, lineno, group);
			modem = NULL;
			service = NULL;
			pager = NULL;
			group = NULL;

			SNPPTimeout = atoi(value);

			/*
			** make sure they didn't define something stupid
			*/
			if (SNPPTimeout < 2)
				SNPPTimeout = 2;

			continue;
		}

		/*
		** This specifies the filename in which the qpage server
		** should write it's process ID.  This file is not used
		** by qpage for any other purpose; it is provided as a
		** convenience for the administrator.
		*/
		if (sscanf(buff, "pidfile=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
			printf("pidfile=<%s>\n", value);
#endif
			errors += check_modem(filename, lineno, modem);
			errors += check_service(filename, lineno, service);
			errors += check_pager(filename, lineno, &Pagers);
			errors += check_group(filename, lineno, group);
			modem = NULL;
			service = NULL;
			pager = NULL;
			group = NULL;

			my_free(PIDfile);
			PIDfile = strdup(value);

			continue;
		}

		/*
		** This specifies the e-mail addresses of the qpage
		** administrator.  If defined, status notification is
		** sent to this address (regardless of the service level)
		** for all failed pages.  This keyword causes the termination
		** of any current service, pager, or group specification.
		*/
		if (sscanf(buff, "administrator=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
			printf("administrator=<%s>\n", value);
#endif
			errors += check_modem(filename, lineno, modem);
			errors += check_service(filename, lineno, service);
			errors += check_pager(filename, lineno, &Pagers);
			errors += check_group(filename, lineno, group);
			modem = NULL;
			service = NULL;
			pager = NULL;
			group = NULL;

			my_free(Administrator);
			Administrator = strdup(value);

			continue;
		}

		/*
		** This specifies whether qpage should qualify the
		** addresses used to send e-mail notification to
		** page submitters.  If true (and the CALLerid info
		** does not contain "@host.name") then the submitter's
		** hostname will be appended to the notification e-mail
		** address.  If the vaue of this keyword starts with
		** '@' then it is appended as-is to unqualified addresses.
		** This keyword causes the termination of any current
		** service, pager, or group specification.
		**
		** Disclaimer: Using "true" for this keyword should
		** be a last resort.  I strongly believe that all e-mail
		** addresses for a given domain should use the domain
		** name ONLY (with no leading hostname).  Administrators
		** who implement public user@host e-mail addresses (where
		** "host" is the name of the user's workstation) should
		** be forced to scrub toilets for a living.
		*/
		if (sscanf(buff, "forcehostname=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
			printf("forcehostname=<%s>\n", value);
#endif
			errors += check_modem(filename, lineno, modem);
			errors += check_service(filename, lineno, service);
			errors += check_pager(filename, lineno, &Pagers);
			errors += check_group(filename, lineno, group);
			modem = NULL;
			service = NULL;
			pager = NULL;
			group = NULL;

			if (value[0] == '@') {
				ForceHostname = strdup(value);
				continue;
			}

			if (!strcasecmp(value, "on"))
				ForceHostname = strdup("true");

			if (!strcasecmp(value, "yes"))
				ForceHostname = strdup("true");

			if (!strcasecmp(value, "true"))
				ForceHostname = strdup("true");

			continue;
		}

		/*
		** This specifies the signature file to be used when
		** sending e-mail notifications to the page submitter.
		** This keyword causes the termination of any current
		** modem, service, pager, or group specification.
		*/
		if (sscanf(buff, "sigfile=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
			printf("sigfile=<%s>\n", value);
#endif
			errors += check_modem(filename, lineno, modem);
			errors += check_service(filename, lineno, service);
			errors += check_pager(filename, lineno, &Pagers);
			errors += check_group(filename, lineno, group);
			modem = NULL;
			service = NULL;
			pager = NULL;
			group = NULL;

			if (access(value, R_OK) < 0) {
				qpage_log(LOG_ERR,
					"%s(%d): cannot access %s: %s",
					filename, lineno, value,
					strerror(errno));

				errors++;
				continue;
			}

			my_free(SigFile);
			SigFile = strdup(value);

			continue;
		}

		/*
		** This is a modem specification.  Add another node to
		** the modems linked-list.  This keyword causes the
		** causes the termination of any current service, pager,
		** or group specification.
		*/
		if (sscanf(buff, "modem=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
			printf("modem=<%s>\n", value);
#endif
			errors += check_modem(filename, lineno, modem);
			errors += check_service(filename, lineno, service);
			errors += check_pager(filename, lineno, &Pagers);
			errors += check_group(filename, lineno, group);
			modem = NULL;
			service = NULL;
			pager = NULL;
			group = NULL;

			if ((modem = lookup(Modems, value)) == NULL) {
				modem = (void *)malloc(sizeof(*modem));
				(void)memset((char *)modem, 0, sizeof(*modem));
				modem->next = Modems;
				Modems = modem;
				modem->name = strdup(value);
				modem->initcmd = strdup(DEFAULT_INITCMD);
				modem->dialcmd = strdup(DEFAULT_DIALCMD);
			}
			else {
				qpage_log(LOG_ERR,
					"%s(%d): duplicate modem %s",
					filename, lineno, value);

				modem = NULL;
			}

			continue;
		}

		/*
		** This is a paging service specification.  Add another
		** node to the services linked-list.  Note that we are
		** building the linked-list in reverse order.  This keyword
		** causes the termination of any current modem, pager,
		** or group specification.
		*/
		if (sscanf(buff, "service=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
			printf("service=<%s>\n", value);
#endif
			errors += check_modem(filename, lineno, modem);
			errors += check_service(filename, lineno, service);
			errors += check_pager(filename, lineno, &Pagers);
			errors += check_group(filename, lineno, group);
			modem = NULL;
			service = NULL;
			pager = NULL;
			group = NULL;

			if ((service = lookup(Services, value)) == NULL) {
				service = (void *)malloc(sizeof(*service));
				(void)memset((char *)service, 0,
					sizeof(*service));
				service->next = Services;
				Services = service;
				service->name = strdup(value);

				init_service_defaults(service);

				if (!strcmp(value, "default"))
					DefaultService = service;
			}

			continue;
		}

		/*
		** This is a pager specification.  Add another node to the
		** pagers linked-list.  Note that we are building the list
		** in reverse order.  This keyword causes the termination
		** of any current modem, service or group specification.
		*/
		if (sscanf(buff, "pager=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
			printf("pager=<%s>\n", value);
#endif
			errors += check_modem(filename, lineno, modem);
			errors += check_service(filename, lineno, service);
			errors += check_pager(filename, lineno, &Pagers);
			errors += check_group(filename, lineno, group);
			modem = NULL;
			service = NULL;
			pager = NULL;
			group = NULL;

			if (lookup(Groups, value) != NULL) {
				qpage_log(LOG_ERR,
					"%s(%d): pager %s not allowed, "
					"group %s already defined",
					value, value);

				continue;
			}

			if ((pager = lookup(Pagers, value)) == NULL) {
				pager = (void *)malloc(sizeof(*pager));
				(void)memset((char *)pager, 0, sizeof(*pager));
				pager->next = Pagers;
				Pagers = pager;
				pager->name = strdup(value);
				pager->service = DefaultService;
			}
			else {
				qpage_log(LOG_ERR,
					"%s(%d): duplicate pager %s",
					filename, lineno, value);

				pager = NULL;
			}

			continue;
		}

		/*
		** This is a group specification.  Add another node to the
		** groups linked-list.  Note that we are building the list
		** in reverse order.  This keyword causes the termination
		** of any current modem, service or pager specification.
		*/
		if (sscanf(buff, "group=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
			printf("group=<%s>\n", value);
#endif
			errors += check_modem(filename, lineno, modem);
			errors += check_service(filename, lineno, service);
			errors += check_pager(filename, lineno, &Pagers);
			errors += check_group(filename, lineno, group);
			modem = NULL;
			service = NULL;
			pager = NULL;
			group = NULL;

			if (lookup(Pagers, value) != NULL) {
				qpage_log(LOG_ERR,
					"%s(%d): group %s not allowed, "
					"pager %s already defined",
					value, value);

				continue;
			}

			if ((group = lookup(Groups, value)) == NULL) {
				group = (void *)malloc(sizeof(*group));
				(void)memset((char *)group, 0, sizeof(*group));
				group->next = Groups;
				Groups = group;
				group->name = strdup(value);
			}
			else {
				qpage_log(LOG_ERR,
					"%s(%d): duplicate group %s",
					filename, lineno, value);

				group = NULL;
			}

			continue;
		}

		/*
		** This section processes modem specific keywords
		*/
		if (modem && isspace(buff[0])) {
			if (sscanf(buff, " text=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
				printf("%s:text=<%s>\n", modem->name,
					value);
#endif
				/*
				** translate underscores to spaces
				*/
				for (ptr=value; *ptr; ptr++)
					if (*ptr == '_')
						*ptr = ' ';

				modem->text = strdup(value);
				continue;
			}

			if (sscanf(buff, " device=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
				printf("%s:device=<%s>\n", modem->name,
					value);
#endif
				if (access(value, R_OK|W_OK) < 0) {
					qpage_log(LOG_ERR,
						"%s(%d): cannot access %s: %s",
						filename, lineno, value,
						strerror(errno));

					errors++;
					continue;
				}

				my_free(modem->device);
				modem->device = strdup(value);
				continue;
			}

			if (sscanf(buff, " initcmd=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
				printf("%s:initcmd=<%s>\n", modem->name,
					value);
#endif
				my_free(modem->initcmd);
				modem->initcmd = strdup(value);
				continue;
			}

			if (sscanf(buff, " dialcmd=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
				printf("%s:dialcmd=<%s>\n", modem->name,
					value);
#endif
				my_free(modem->dialcmd);
				modem->dialcmd = strdup(value);
				continue;
			}
		}

		/*
		** This section processes service specific keywords
		*/
		if (service && isspace(buff[0])) {
			if (sscanf(buff, " text=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
				printf("%s:text=<%s>\n", service->name,
					value);
#endif
				/*
				** translate underscores to spaces
				*/
				for (ptr=value; *ptr; ptr++)
					if (*ptr == '_')
						*ptr = ' ';

				service->text = strdup(value);
				continue;
			}

			if (sscanf(buff, " device=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
				printf("%s:device=<%s>\n", service->name,
					value);
#endif
				if (check_device(filename, lineno, value) < 0) {
					errors++;
					continue;
				}

				my_free(service->device);
				service->device = strdup(value);
				continue;
			}

			if (sscanf(buff, " dialcmd=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
				printf("%s:dialcmd=<%s>\n", service->name,
					value);
#endif
				my_free(service->dialcmd);
				service->dialcmd = strdup(value);
				continue;
			}

			if (sscanf(buff, " phone=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
				printf("%s:phone=<%s>\n", service->name,
					value);
#endif
				my_free(service->phone);
				service->phone = strdup(value);
				continue;
			}

			if (sscanf(buff, " password=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
				printf("%s:password=<%s>\n", service->name,
					value);
#endif
				service->password = strdup(value);
				continue;
			}

			if (sscanf(buff, " baudrate=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
				printf("%s:baudrate=<%s>\n", service->name,
					value);
#endif
				switch (atoi(value)) {
					case 300:
						service->baudrate = B300;
						break;

					case 1200:
						service->baudrate = B1200;
						break;

					case 2400:
						service->baudrate = B2400;
						break;

					case 9600:
						service->baudrate = B9600;
						break;

					case 19200:
						service->baudrate = B19200;
						break;

					case 38400:
						service->baudrate = B38400;
						break;
#ifdef B57600
					case 57600:
						service->baudrate = B57600;
						break;
#endif
#ifdef B115200
					case 115200:
						service->baudrate = B115200;
						break;
#endif
					default:
						qpage_log(LOG_WARNING,
							"unrecognized baud "
							"rate %s", value);
						service->baudrate = B300;
						break;
				}
				continue;
			}

			if (sscanf(buff, " parity=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
				printf("%s:parity=<%s>\n", service->name,
					value);
#endif
				if (!strcasecmp(value, "even"))
					service->parity = 2;
				else
					if (!strcasecmp(value, "odd"))
						service->parity = 1;
					else
						if (!strcasecmp(value, "none"))
							service->parity = 0;
						else
							qpage_log(LOG_WARNING,
								"unrecognized parity %s", value);

				continue;
			}

			if (sscanf(buff, " maxmsgsize=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
				printf("%s:maxmsgsize=<%s>\n", service->name,
					value);
#endif
				service->maxmsgsize = atoi(value);

				if (service->maxmsgsize > MAXMSGSIZE)
					service->maxmsgsize = MAXMSGSIZE;

				continue;
			}

			if (sscanf(buff, " maxpages=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
				printf("%s:maxpages=<%s>\n", service->name,
					value);
#endif
				service->maxpages = atoi(value);

				if (service->maxpages < 1)
					service->maxpages = 1;

				if (service->maxpages > 9)
					service->maxpages = 9;

				continue;
			}

			if (sscanf(buff, " maxtries=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
				printf("%s:maxtries=<%s>\n", service->name,
					value);
#endif
				service->maxtries = atoi(value);
				continue;
			}

			if (sscanf(buff, " identfrom=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
				printf("%s:identfrom=<%s>\n", service->name,
					value);
#endif
				if (!strcasecmp(value, "no"))
					service->identfrom = FALSE;

				if (!strcasecmp(value, "off"))
					service->identfrom = FALSE;

				if (!strcasecmp(value, "false"))
					service->identfrom = FALSE;

				continue;
			}

			if (sscanf(buff, " allowpid=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
				printf("%s:allowpid=<%s>\n", service->name,
					value);
#endif
				if (!strcasecmp(value, "on"))
					service->allowpid = TRUE;

				if (!strcasecmp(value, "yes"))
					service->allowpid = TRUE;

				if (!strcasecmp(value, "true"))
					service->allowpid = TRUE;

				continue;
			}

			if (sscanf(buff, " msgprefix=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
				printf("%s:msgprefix=<%s>\n", service->name,
					value);
#endif
				if (!strcasecmp(value, "no"))
					service->msgprefix = FALSE;

				if (!strcasecmp(value, "off"))
					service->msgprefix = FALSE;

				if (!strcasecmp(value, "false"))
					service->msgprefix = FALSE;

				continue;
			}
		}

		/*
		** This section processes pager specific keywords
		*/
		if (pager && isspace(buff[0])) {
			if (sscanf(buff, " text=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
				printf("%s:text=<%s>\n", pager->name,
					value);
#endif
				/*
				** translate underscores to spaces
				*/
				for (ptr=value; *ptr; ptr++)
					if (*ptr == '_')
						*ptr = ' ';

				pager->text = strdup(value);
				continue;
			}

			if (sscanf(buff, " pagerid=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
				printf("%s:pagerid=<%s>\n", pager->name,
					value);
#endif
				pager->pagerid = strdup(value);
				continue;
			}

			if (sscanf(buff, " service=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
				printf("%s:service=<%s>\n", pager->name,
					value);
#endif
				pager->service = lookup(Services, value);

				if (pager->service == NULL) {
					errors++;

					qpage_log(LOG_ERR,
						"no service %s for pager=%s",
						value, pager->name);

					Pagers = pager->next;

					my_free(pager->pagerid);
					free(pager->name);
					free(pager);
					pager = NULL;
				}

				continue;
			}
		}

		/*
		** This section processes group specific keywords
		*/
		if (group && isspace(buff[0])) {
			if (sscanf(buff, " text=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
				printf("%s:text=<%s>\n", group->name,
					value);
#endif
				/*
				** translate underscores to spaces
				*/
				for (ptr=value; *ptr; ptr++)
					if (*ptr == '_')
						*ptr = ' ';

				group->text = strdup(value);
				continue;
			}

			if (sscanf(buff, " member=%s%n", value, &pos) == 1) {
#ifdef CONFDEBUG
				printf("%s:member=<%s>\n", group->name,
					value);
#endif
				/*
				** check for a duty schedule
				*/
				if ((ptr = strchr(value, '/')) != NULL) {
					*ptr++ = '\0';

					if (on_duty(ptr, 0) < 0)
						continue;
				}

				if ((pager = lookup(Pagers, value)) != NULL) {
					/*
					** The pager is valid.  Add to group.
					*/
					tmp = (void *)malloc(sizeof(*tmp));
					(void)memset((char *)tmp, 0, sizeof(*tmp));
					tmp->pager = pager;

					if (ptr)
						tmp->schedule = strdup(ptr);

					tmp->next = group->members;
					group->members = tmp;

					pager = NULL;
				}
				else
					qpage_log(LOG_ERR,
						"no such pager %s for group=%s",
						value, group->name);

				continue;
			}
		}

		/*
		** check for a line consisting of nothing but whitespace
		*/
		for (ptr=buff; *ptr; ptr++) {
			if (!isspace(*ptr))
				break;
		}

		if (*ptr == (char)NULL) {
#ifdef CONFDEBUG
			printf("<just whitespace>\n");
#endif
			pos = 0;
			continue;
		}

		qpage_log(LOG_WARNING, "%s(%d): syntax error <%s>",
			filename, lineno, safe_string(ptr));

		errors++;
		pos = 0;
	}

	(void)fclose(fp);

	/*
	** make sure the last entry we read is complete
	*/
	errors += check_modem(filename, lineno, modem);
	errors += check_service(filename, lineno, service);
	errors += check_pager(filename, lineno, &Pagers);
	errors += check_group(filename, lineno, group);

	return(errors);
}


/*
** free_modems()
**
** This function frees all the memory associated with the
** global Modems linked list.
**
**	Input:
**		list - a pointer to the head node of the list
**
**	Returns:
**		nothing
*/
void
free_modems(modem_t *list)
{
	if (list) {
		free_modems(list->next);

		my_free(list->name);
		my_free(list->text);
		my_free(list->device);
		my_free(list->initcmd);
		my_free(list->dialcmd);
		free(list);
	}
}


/*
** free_services()
**
** This function frees all the memory associated with the
** global Services linked list.
**
**	Input:
**		list - a pointer to the head node of the list
**
**	Returns:
**		nothing
*/
void
free_services(service_t *list)
{
	if (list) {
		free_services(list->next);

		my_free(list->name);
		my_free(list->text);
		my_free(list->device);
		my_free(list->dialcmd);
		my_free(list->phone);
		my_free(list->password);
		free(list);
	}
}


/*
** free_pagers()
**
** This function frees all the memory associated with the
** global Pagers linked list.
**
**	Input:
**		list - a pointer to the head node of the list
**
**	Returns:
**		nothing
*/
void
free_pagers(pager_t *list)
{
	if (list) {
		free_pagers(list->next);

		my_free(list->name);
		my_free(list->text);
		my_free(list->pagerid);
		my_free(list->service);
		free(list);
	}
}


/*
** free_groups()
**
** This function frees all the memory associated with the
** global Groups linked list.
**
**	Input:
**		list - a pointer to the head node of the list
**
**	Returns:
**		nothing
*/
void
free_groups(pgroup_t *list)
{
	member_t	*tmp;

	if (list) {
		free_groups(list->next);

		my_free(list->name);
		my_free(list->text);

		while (list->members) {
			tmp = list->members;
			list->members = tmp->next;
			my_free(tmp->schedule);
			free(tmp);
		}

		free(list);
	}
}


/*
** get_qpage_config()
**
** This function is the entry point to functions that read the
** configuration file.  Any buffers allocated in previous calls to
** this function is first freed before allocating new buffers.
**
**	Input:
**		filename - the name of the configuration file to read
**
**	Returns:
**		The number of errors found while reading the configuration 
**
**	Side effects:
**		The current working directory is changed to the value
**		specified by the "queuedir" keyword.
*/
int
get_qpage_config(char *filename)
{
	int	errors;

	free_modems(Modems);
	free_services(Services);
	free_pagers(Pagers);
	free_groups(Groups);

	Modems = NULL;
	Services = NULL;
	Pagers = NULL;
	Groups = NULL;

	my_free(Administrator);
	my_free(ForceHostname);
	my_free(QueueDir);
	my_free(LockDir);

	Administrator = NULL;
	ForceHostname = NULL;
	QueueDir = NULL;
	LockDir = strdup(DEFAULT_LOCKDIR);;
	IdentTimeout = DEFAULT_IDENTTIMEOUT;
	SNPPTimeout = DEFAULT_SNPPTIMEOUT;

	errors = read_config_file(filename);

	if (access(LockDir, R_OK|W_OK|X_OK) < 0) {
		qpage_log(LOG_ERR, "cannot access LockDir(%s): %s", LockDir,
			strerror(errno));

		errors++;
	}

	if (QueueDir != NULL) {
		/*
		** It is not necessary to chdir to the queue directory
		** during interactive mode since the queue is not used.
		*/
		if (Interactive == FALSE) {
			if (access(QueueDir, R_OK|W_OK|X_OK) < 0) {
				qpage_log(LOG_ERR,
					"cannot access QueueDir(%s): %s",
					QueueDir, strerror(errno));

				errors++;
			}

			if (chdir(QueueDir) < 0) {
				qpage_log(LOG_ERR,
					"cannot chdir to QueueDir(%s): %s",
					QueueDir, strerror(errno));

				errors++;
			}
		}
	}
	else {
		qpage_log(LOG_ERR, "no queue directory specified!");
		errors++;
	}

	return(errors);
}


/*
** dump_qpage_config()
**
** Print the qpage configuration file(s) to stdout (for the XCONfig
** command).  This is a hack.  I'm not proud of it.
*/
void
dump_qpage_config(char *filename)
{
	FILE	*fp;
	char	buff[1024];
	char	value[1024];


	if ((fp = fopen(filename, "r")) == NULL) {
		(void)printf("[cannot open %s: %s]\n", filename,
			strerror(errno));

		return;
	}

	(void)printf("[---------- begin %s ----------]\n", filename);

	while (fgets(buff, sizeof(buff), fp)) {

		(void)printf("%s", buff);

		if (sscanf(buff, "include=%s", value) == 1)
			dump_qpage_config(value);
	}

	(void)printf("[---------- end %s ----------]\n", filename);
}
