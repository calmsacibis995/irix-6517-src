#include	"qpage.h"


/*
** global variables
*/
#ifndef lint
static char	sccsid[] = "@(#)sendmail.c  1.15  07/26/98  tomiii@qpage.org";
#endif


/*
** invoke_sendmail()
**
** This function invokes sendmail and uses a pipe to redirect stdout
** from this process to stdin of the sendmail process.
**
**	Input:
**		addr - the destination e-mail address
**
**	Returns:
**		the child process ID, or -1 on failure
**
**	Note:
**		We cannot use popen() because pclose() calls wait().
**		Our SIGCHLD handler also calls wait().  Only one of
**		these waits will succeed.  We don't want the other
**		one to block trying to reap the status of a different
**		child.
*/
FILE *
invoke_sendmail(char *addr)
{
#ifdef SENDMAIL_PATH
	FILE	*fp;
	int	fds[2];


	/*
	** temporary files are evil--create a pipe for sendmail instead
	*/
	if (pipe(fds) < 0) {
		qpage_log(LOG_ERR, "pipe() failed, cannot send e-mail: %s",
			strerror(errno));
		return(NULL);
	}

	switch (fork()) {
		case (pid_t)-1:
			qpage_log(LOG_ERR,
				"fork() failed, cannot send e-mail: %s",
				strerror(errno));
			return(NULL);

		case 0: /* child */
			(void)close(fds[1]);

			if (fds[0] != fileno(stdin)) {
				(void)dup2(fds[0], fileno(stdin));
				(void)close(fds[0]);
			}

			(void)execl(SENDMAIL_PATH, "sendmail",
				"-f", "<>", addr, NULL);

			qpage_log(LOG_ERR, "exec(%s) failed: %s",
				SENDMAIL_PATH, strerror(errno));

			_exit(-1);
			break;

		default: /* parent */
			(void)close(fds[0]);
			break;
	}

	fp = fdopen(fds[1], "w");

	return(fp);
#else
	return(NULL);
#endif /* SENDMAIL_PATH */
}


/*
** add_signature()
**
** Add a custom signature to the bottom of an e-mail status notification.
**
**	Input:
**		fp - the file pointer for the notification message
**		sigfile - the name of the signature file
**
**	Returns:
**		an integer status (zero=success, nonzero=failure)
*/
int
add_signature(FILE *fp, char *sigfile)
{
	FILE *sig;
	char buf[100];


	if (sigfile == NULL)
		return(-1);

	if ((sig = fopen(sigfile, "r")) == NULL) {
		qpage_log(LOG_WARNING, "cannot open signature file <%s>\n",
			sigfile);

		return(-1);
	}

	/*
	** Don't check to see if the file actually contains any
	** text.  An empty sigfile just means the administrator
	** doesn't want any signature at all.
	*/
	while (fgets(buf, sizeof(buf), sig))
		(void)fprintf(fp, "%s", buf);

	(void)fclose(sig);
	return(0);
}


/*
** notify_administrator()
**
** This function sends e-mail notification to the qpage administrator
** whenever a page fails.
**
**	Input:
**		p - a page structure
**
**	Returns:
**		nothing
*/
void
notify_administrator(PAGE *p)
{
	rcpt_t	*rcpt;
	FILE	*fp;
	int	pages;
	int	still_trying;


	/*
	** Only send e-mail for failed pages.
	*/
	pages = 0;
	for (rcpt=p->rcpts; rcpt; rcpt=rcpt->next) {
		if ((rcpt->flags & (F_FAILED|F_SENTADMIN)) == F_FAILED) {
			rcpt->flags |= F_SENTADMIN;
			pages++;
		}
	}

	if (pages == 0) {
		if (Debug)
			qpage_log(LOG_DEBUG, "no failed pages for id=%s",
				p->messageid);
		return;
	}

	qpage_log(LOG_INFO, "notifying administrator of failed page id=%s",
		p->messageid);

	if ((fp = invoke_sendmail(Administrator)) == NULL)
		return;

	fprintf(fp, "From: QuickPage Daemon <%s>\n", get_user());
	fprintf(fp, "To: QuickPage Administrator <%s>\n", Administrator);
	fprintf(fp, "Subject: A page has failed.\n");
	fprintf(fp, "\n");

	fprintf(fp, "The following page could not be delievered "
		"to one or more recipients:\n\n"); 

	fprintf(fp, "\tDate:  %s\n", my_ctime(&p->created));

	if (p->messageid)
		fprintf(fp, "\tMsgID: %s\n", p->messageid);

	if (p->filename)
		fprintf(fp, "\tFile:  %s\n", p->filename);

	if (p->from)
		fprintf(fp, "\tFrom:  %s\n", p->from);

	if (p->auth)
		fprintf(fp, "\tAuth:  %s\n", p->auth);

	if (p->ident)
		fprintf(fp, "\tIdent: %s\n", p->ident);

	if (p->hostname)
		fprintf(fp, "\tHost:  %s\n", p->hostname);

	fprintf(fp, "\n");

	fprintf(fp, "\t%-30s%s\n", "Pager", "Status");
	fprintf(fp, "\t%-30s%s\n", "-----", "------");

	still_trying = 0;
	for (rcpt=p->rcpts; rcpt; rcpt=rcpt->next) {
		fprintf(fp, "\t%-30s", rcpt->pager);

		if (rcpt->flags & F_SENT) {
			fprintf(fp, "Delivered\n");
			continue;
		}

		if (rcpt->flags & F_FAILED) {
			fprintf(fp, "Failed\n");
			continue;
		}

		fprintf(fp, "Still trying\n");
		still_trying++;
	}

	fprintf(fp, "\n");

	if (still_trying == 0)
		fprintf(fp, "This page should be manually removed "
			"from the page queue.\n");

	fprintf(fp, "\n\nRegards,\nQuickPage\n");

	/*
	** tell sendmail we're done
	*/
	fprintf(fp, "\n.\n");
	(void)fclose(fp);
}


/*
** notify_submitter()
**
** This function sends e-mail notification to a page submitter
** concerning the status of the page.  Mail is sent whenever the
** status changes to "sent" or "failed" for one or more recipients.
**
**	Input:
**		p - a page structure
**
**	Returns:
**		nothing
*/
void
notify_submitter(PAGE *p)
{
	rcpt_t	*rcpt;
	FILE	*fp;
	char	buff[100];
	char	*append;
	char	*address;
	char	*msg;
	int	pages;


	/*
	** Only send e-mail if the page was just sent or the page
	** just failed.  Also, only send e-mail about a success or
	** failure once, unless we're already sending mail because
	** of another recipient.  That is, only send e-mail if:
	**
	**	F_SENT & ~F_SENTMAIL
	** or
	**	F_FAILED & ~F_SENTMAIL
	** or
	**	one or more recipients meets the above two conditions.
	*/
	pages = 0;
	for (rcpt=p->rcpts; rcpt; rcpt=rcpt->next) {
		if ((rcpt->flags & F_SENDMAIL) == 0)
			continue;

		if ((rcpt->flags & F_SENTMAIL) == 0) {
			if (rcpt->flags & (F_SENT|F_FAILED)) {
				rcpt->flags |= F_SENTMAIL;
				pages++;
			}
		}
	}

	if (pages == 0) {
		if (Debug)
			qpage_log(LOG_DEBUG, "no e-mail to send for id=%s",
				p->messageid);
		return;
	}

	qpage_log(LOG_INFO, "sending e-mail status for id=%s", p->messageid);

	/*
	** If necessary, make sure the "from" address is qualified
	** with a hostname.  If a hostname was specified (i.e. the
	** "forcehostname" option starts with '@') then use it,
	** otherwise use the name of the machine where the page was
	** submitted from.
	*/
	if (ForceHostname) {
		if (ForceHostname[0] == '@')
			append = &ForceHostname[1];
		else
			append = p->hostname;

		if (append && strchr(p->from, '@') == NULL) {
			address = (void *)malloc(strlen(p->from) +
				strlen(append) + 2);

			(void)sprintf(address, "%s@%s", p->from, append);
		}
		else
			address = strdup(p->from);
	}
	else
		address = strdup(p->from);

	if ((fp = invoke_sendmail(address)) == NULL)
		return;

	fprintf(fp, "From: QuickPage Daemon <%s>\n", get_user());
	fprintf(fp, "To: QuickPage User <%s>\n", address);
	fprintf(fp, "Subject: The status of your page\n");

	if (Administrator)
		fprintf(fp, "Reply-To: %s\n", Administrator);

	fprintf(fp, "\n");

	fprintf(fp, "The page you submitted has the following status:\n\n");

	fprintf(fp, "\tDate:    %s\n", my_ctime(&p->created));

	if (p->from)
		fprintf(fp, "\tFrom:    %s\n", p->from);

	if (p->auth)
		fprintf(fp, "\tAuth:    %s\n", p->auth);

	if (p->ident)
		fprintf(fp, "\tIdent:   %s\n", p->ident);

	if (p->hostname)
		fprintf(fp, "\tHost:    %s\n", p->hostname);

	fprintf(fp, "\n");

	msg = msgcpy(buff, p->message, 50);

	fprintf(fp, "\tMessage: %s\n", buff);

	while (msg) {
		msg = msgcpy(buff, msg, 50);
		fprintf(fp, "\t         %s\n", buff);
	}

	fprintf(fp, "\n");

	fprintf(fp, "\t%-30s%s\n", "Pager", "Status");
	fprintf(fp, "\t%-30s%s\n", "-----", "------");

	for (rcpt=p->rcpts; rcpt; rcpt=rcpt->next) {
#ifdef ONLY_SPECIFIED_RECIPIENTS
		if ((rcpt->flags & F_SENDMAIL) == 0)
			continue;
#endif

		fprintf(fp, "\t%-30s", rcpt->pager);

		if (rcpt->flags & F_SENT) {
			fprintf(fp, "Delivered\n");
			continue;
		}

		if (rcpt->flags & F_FAILED) {
			fprintf(fp, "Failed\n");
			continue;
		}

		fprintf(fp, "Still trying\n");
	}

	fprintf(fp, "\n");

	/*
	** Add the administrator-defined signature if we can, otherwise
	** just use something simple.
	*/
	if (add_signature(fp, SigFile) != 0)
		fprintf(fp, "Regards,\nQuickPage\n");

	/*
	** tell sendmail we're done
	*/
	fprintf(fp, "\n.\n");
	(void)fclose(fp);

	free(address);
}
