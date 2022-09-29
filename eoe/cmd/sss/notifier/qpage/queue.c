#include	"qpage.h"


/*
** global variables
*/
#ifndef lint
static char	sccsid[] = "@(#)queue.c  1.22  07/07/98  tomiii@qpage.org";
#endif


/*
** insert_jobs()
**
** This function inserts a page into the job list.  Since each page
** may contain multiple recipients each with a possibly different
** paging service, each recipient is considered a separate job.  The
** job list is sorted first by service name and then by level within
** each service.  This allows all pages for a particular service to
** be sent with one phone call (if possible).  Note that each job
** simply contains pointers to the respective elements of a page
** structure.  Therefore, a job in this context does not actually
** contain any data, only pointers to data.
**
**	Input:
**		joblist - a pointer to the head node in the list
**		p - the new page to be added to the list.
**
**	Returns:
**		the number of pending jobs in the specified page
**
**	Note:
**		Jobs scheduled for the future (i.e. seen here but
**		not added to the job list) are counted in the number
**		returned by this function.
*/
int
insert_jobs(job_t **joblist, PAGE *p)
{
	job_t		*curr;
	job_t		*prev;
	job_t		*tmp;
	rcpt_t		*rcpt;
	service_t	*service;
	pager_t		*pager;
	int		jobcount;
	int		i;


	jobcount = 0;

	for (rcpt=p->rcpts; rcpt; rcpt=rcpt->next) {
		/*
		** skip pages we've already sent
		*/
		if (rcpt->flags & F_SENT)
			continue;

		/*
		** skip pages which are scheduled for the future
		*/
		if (rcpt->holduntil > time(NULL)) {
			if (Debug || Interactive)
				qpage_log(LOG_DEBUG, "skipping %s until %s",
					rcpt->pager,
					my_ctime(&rcpt->holduntil));

			jobcount++;
			continue;
		}

		pager = lookup(Pagers, rcpt->pager);

		/*
		** If this is a raw pagerid we need to kludge something
		*/
		if (pager == NULL && (rcpt->flags & F_RAWPID)) {
			pager = (void *)malloc(sizeof(*pager));
			(void)memset((char *)pager, 0, sizeof(*pager));
			pager->name = strdup(rcpt->pager);
			pager->pagerid = strdup(rcpt->pager);
			pager->flags = rcpt->flags;
		}

		/*
		** "pager" better not be NULL at this point
		*/
		if (pager == NULL) {
			qpage_log(LOG_ERR, "no such pager %s", rcpt->pager);
			continue;
		}

		/*
		** If they specified a coverage, use it, otherwise
		** use the default coverage for this pager.
		*/
		if (rcpt->coverage)
			service = lookup(Services, rcpt->coverage);
		else
			service = pager->service;

		if (service == NULL) {
			qpage_log(LOG_ERR, "no such service %s",
				rcpt->coverage);

			continue;
		}

#ifdef REJECT_IS_FAILURE
		/*
		** do not retry rejected pages
		*/
		if (rcpt->flags & F_REJECT) {
			rcpt->flags |= F_FAILED;
			continue;
		}
#endif

		/*
		** limit retries to the number specified by the paging service
		*/
		if (rcpt->goodtries >= service->maxtries) {
			rcpt->flags |= F_FAILED;

			qpage_log(LOG_ERR, "too many retries for %s in %s",
					rcpt->pager, p->filename);

			continue;
		}

		/*
		** build a new job for this recipient
		*/
		tmp = (void *)malloc(sizeof(*tmp));
		tmp->next = NULL;
		tmp->p = p;
		tmp->rcpt = rcpt;
		tmp->service = service;
		tmp->pager = pager;

		if (Debug) {
			qpage_log(LOG_DEBUG, "pager=%s, pagerid=%s, service=%s",
				pager->name, pager->pagerid, service->name);
		}

		curr = *joblist;
		prev = NULL;

		/*
		** Scan through the job list to find an appropriate
		** place to insert this recipient.
		*/
		while (curr) {
			i = strcmp(curr->service->name, tmp->service->name);

			if (i == 0)
				i = curr->rcpt->level - tmp->rcpt->level;

			if (i == 0)
				i = curr->p->created - tmp->p->created;

			if (i>0)
				break;

			prev = curr;
			curr = curr->next;
		}

		if (prev == NULL) {
			/*
			** insert the job at the beginning of the list
			*/
			tmp->next = *joblist;
			*joblist = tmp;
		}
		else {
			/*
			** insert the job somewhere after the first node
			*/
			tmp->next = prev->next;
			prev->next = tmp;
		}

		jobcount++;
	}

	/*
	** At this point, a jobcount of zero means all the recipients
	** of this page fall into one of these two categories:
	**
	**	- we already successfully sent the page
	**	- we failed to send the page and we should stop trying
	**
	** If there are no valid jobs here now, there won't be any in
	** the future so let's nuke this page.
	*/
	if (jobcount == 0)
		p->flags |= F_BADPAGE;

	return(jobcount);
}


/*
** read_page()
**
** This function reads a page from the page queue.
**
**	Input:
**		file - the filename to read from
**
**	Returns:
**		a page structure, or NULL on failure
*/
PAGE *
read_page(char *file)
{
	rcpt_t		*tmp;
	FILE		*fp;
	PAGE		*p;
	time_t		holduntil;
	time_t		lasttry;
	char		keyword[255];
	char		coverage[255];
	char		status[255];
	char		name[255];
	char		msgid[255];
	char		hostname[257];
	char		*buf;
	char		*ptr;
	int		tries;
	int		goodtries;
	int		version;
	int		buflen;
	int		gotmarker;
	int		line;
	int		level;
	int		flags;
	int		bytes;
	int		n;


	gotmarker = 0;
	line = 0;
	coverage[0] = '\0';
	level = DEFAULT_LEVEL;
	holduntil = 0;
	lasttry = 0;
	flags = 0;
	tries = 0;
	goodtries = 0;

	if ((fp = fopen(file, "r")) == NULL) {
		qpage_log(LOG_NOTICE, "cannot open file %s for reading", file);
		return(NULL);
	}

	if (lock_file(fileno(fp), O_RDONLY, TRUE) < 0) {
		qpage_log(LOG_ERR, "cannot lock %s: %s", file,
			strerror(errno));

		return(NULL);
	}

	p = (void *)malloc(sizeof(*p));
	(void)memset((char *)p, 0, sizeof(*p));

	buf = (void *)malloc(BUFCHUNKSIZE);
	buflen = BUFCHUNKSIZE;

	while (fgets(buf, buflen, fp)) {
		line++;

		if ((ptr = strchr(buf, '\n')) == NULL) {
			qpage_log(LOG_ERR,
				"short read (this should never happen)");
		}
		else
			*ptr = '\0';

		if (sscanf(buf, "%s %n", keyword, &n) != 1) {
			qpage_log(LOG_ERR,
				"no keyword (this should never happen)");

			continue;
		}

		switch (keyword[0]) {
			case '#': /* comment */
				break;

			case '-': /* end-of-recipients marker */
				gotmarker++;
				break;

			case 'B': /* bytes (size of message) */
				(void)sscanf(&buf[n], "%d", &bytes);
				if (bytes > buflen) {
					buf = (void *)realloc(buf, bytes);
					buflen = bytes;
				}
				break;

			case 'C': /* created/coverage */
				if (gotmarker) {
					(void)sscanf(&buf[n], "%ld",
						&p->created);
				}
				else {
					(void)sscanf(&buf[n], "%s", coverage);
				}
				break;

			case 'F': /* from/flags */
				if (gotmarker) {
					/*
					** We can't use sscanf() here
					** because there may be embedded
					** whitespace in the CALLerid
					** information.
					*/
					while (buf[n] && isspace(buf[n]))
						n++;

					p->from = strdup(&buf[n]);
				}
				else {
					(void)sscanf(&buf[n], "%d", &flags);
				}
				break;

			case 'G': /* goodtries */
				(void)sscanf(&buf[n], "%d", &goodtries);
				break;

			case 'H': /* hostname/holduntil */
				if (gotmarker) {
					(void)sscanf(&buf[n], "%s",
						hostname);

					p->hostname = strdup(hostname);
				}
				else {
					(void)sscanf(&buf[n], "%ld",
						&holduntil);
				}
				break;

			case 'I': /* ident */
				p->ident = strdup(&buf[n]);
				break;

			case 'L': /* lasttry */
				(void)sscanf(&buf[n], "%ld", &lasttry);
				break;

			case 'M': /* message */
				p->message = strdup(&buf[n]);
				break;

			case 'P': /* pager */
				name[0] = '\0';
				(void)sscanf(&buf[n], "%s", name);

				tmp = (void *)malloc(sizeof(*tmp));
				(void)memset((char *)tmp, 0, sizeof(*tmp));
				tmp->next = p->rcpts;
				p->rcpts = tmp;

				tmp->pager = strdup(name);

				if (coverage[0])
					tmp->coverage = strdup(coverage);

				tmp->holduntil = holduntil;
				tmp->lasttry = lasttry;
				tmp->goodtries = goodtries;
				tmp->tries = tries;
				tmp->level = level;
				tmp->flags = flags;

				coverage[0] = '\0';
				level = DEFAULT_LEVEL;
				holduntil = 0;
				lasttry = 0;
				goodtries = 0;
				tries = 0;
				flags = 0;

				break;

			case 'S': /* status/servicelevel */
				if (gotmarker) {
					(void)sscanf(&buf[n], "%s", status);
				}
				else {
					(void)sscanf(&buf[n], "%d", &level);
				}
				break;

			case 'T': /* tries */
				(void)sscanf(&buf[n], "%d", &tries);
				break;

			case 'U': /* unique id */
				(void)sscanf(&buf[n], "%s", msgid);
				p->messageid = strdup(msgid);
				break;

			case 'V': /* version */
				(void)sscanf(&buf[n], "%d", &version);
				if (version != 3) {
					qpage_log(LOG_ERR, "FATAL ERROR: incompatible version of queue file");
					clear_page(p, FALSE);
					free(p);
					return(NULL);
				}
				break;

			default:
				qpage_log(LOG_NOTICE, "%s line %d: unknown meaning (%s)",
					file, line, buf);
				break;
		}
	}

	free(buf);
	(void)fclose(fp);

	p->filename = strdup(file);

	if (p->messageid == NULL)
		p->messageid = strdup("[none]");

	return(p);
}


/*
** write_page()
**
** This function writes a page to the page queue.  If the filename
** field of the page structure is not null, it is assumed to point
** to the name of the file the page was read from.  In this case,
** if the page has been successfully delivered to all the recipients,
** the file is removed.  Otherwise the page is written back to that
** file.  If the filename field of the page structure is NULL, this
** function assumes this is a new page.  A new filename is created
** based on the current time.
**
**	Input:
**		p - a page structure
**		new - whether this is a new page
**
**	Returns:
**		an integer status code:
**			 0 = queue file was removed/renamed
**			 1 = page was written to queue file
**			-1 = error occurred; status unknown
*/
int
write_page(PAGE *p, int new)
{
	rcpt_t		*tmp;
	FILE		*fp;
	char		filename[255];
	char		*badname;
	int		fd;
	int		ext;
	int		doit;


	/*
	** send e-mail notification (if needed) of the page status
	*/
	if (new == FALSE) {
		if (Administrator)
			notify_administrator(p);

		if (p->from)
			notify_submitter(p);
	}

	/*
	** first verify whether this page should be written back or not
	*/
	doit = FALSE;
	for (tmp=p->rcpts; tmp; tmp=tmp->next) {
		if (tmp->flags & F_SENT)
			continue;

		doit = TRUE;
	}

	if (doit == FALSE) {
		if (p->filename) {
			if (Debug)
				qpage_log(LOG_DEBUG, "unlinking %s",
					p->filename);

			if (unlink(p->filename) < 0)
				qpage_log(LOG_WARNING, "unlink failed for %s: %s",
					p->filename, strerror(errno));
		}

		return(0);
	}

	if (p->filename == NULL) {
		ext = 0;

		do {
			(void)sprintf(filename, "P%lu.%03u", time(NULL), ext++);

			fd = open(filename, O_CREAT|O_EXCL|O_WRONLY, 0644);

			if (fd >= 0)
				break;

			if (errno != EEXIST) {
				qpage_log(LOG_NOTICE, "cannot create %s: %s",
					filename, strerror(errno));
			}
		}
		while (ext < 100);

		if (fd < 0) {
			qpage_log(LOG_ERR, "cannot create file %s: %s",
				filename, strerror(errno));

			return(-1);
		}

		p->filename = strdup(filename);
	}
	else {
		if ((fd = open(p->filename, O_WRONLY, 0644)) < 0) {
			qpage_log(LOG_ERR, "cannot open file %s: %s",
				p->filename, strerror(errno));

			return(-1);
		}
	}

	if (lock_file(fd, O_RDWR, TRUE) < 0) {
		qpage_log(LOG_ERR, "cannot lock %s: %s", p->filename,
			strerror(errno));

		return(-1);
	}

	/*
	** explicitly truncate the file now that it's locked
	*/
	(void)ftruncate(fd, (off_t)0);

	if ((fp = fdopen(fd, "w")) == NULL) {
		qpage_log(LOG_ERR, "cannot reopen file %s", p->filename);
		(void)close(fd);
		return(-1);
	}

	fprintf(fp, "Version: 3\n");

	for (tmp=p->rcpts; tmp; tmp=tmp->next) {
		if (tmp->coverage)
			fprintf(fp, "Coverage: %s\n", tmp->coverage);

		if (tmp->holduntil)
			fprintf(fp, "Holduntil: %lu %s\n", tmp->holduntil,
				my_ctime(&tmp->holduntil));

		if (tmp->lasttry)
			fprintf(fp, "Lasttry: %lu %s\n", tmp->lasttry,
				my_ctime(&tmp->lasttry));

		fprintf(fp, "Tries: %d\n", tmp->tries);

		if (tmp->goodtries)
			fprintf(fp, "Goodtries: %d\n", tmp->goodtries);

		if (tmp->level != DEFAULT_LEVEL)
			fprintf(fp, "Servicelevel: %d\n", tmp->level);

		if (tmp->flags) {
			fprintf(fp, "Flags: %d (", tmp->flags);

			if (tmp->flags & F_SENT)
				fprintf(fp, " F_SENT");

			if (tmp->flags & F_FAILED)
				fprintf(fp, " F_FAILED");

			if (tmp->flags & F_BUSY)
				fprintf(fp, " F_BUSY");

			if (tmp->flags & F_NOCARRIER)
				fprintf(fp, " F_NOCARRIER");

			if (tmp->flags & F_NOMODEM)
				fprintf(fp, " F_NOMODEM");

			if (tmp->flags & F_FORCED)
				fprintf(fp, " F_FORCED");

			if (tmp->flags & F_NOPROMPT)
				fprintf(fp, " F_NOPROMPT");

			if (tmp->flags & F_UNKNOWN)
				fprintf(fp, " F_UNKNOWN");

			if (tmp->flags & F_REJECT)
				fprintf(fp, " F_REJECT");

			if (tmp->flags & F_RAWPID)
				fprintf(fp, " F_RAWPID");

			if (tmp->flags & F_SENDMAIL)
				fprintf(fp, " F_SENDMAIL");

			if (tmp->flags & F_SENTMAIL)
				fprintf(fp, " F_SENTMAIL");

			if (tmp->flags & F_SENTADMIN)
				fprintf(fp, " F_SENTADMIN");

			fprintf(fp, " )\n");
		}

		fprintf(fp, "Pager: %s\n", tmp->pager);
	}

	fprintf(fp, "-\n");

	if (p->from)
		fprintf(fp, "From: %s\n", p->from);

	if (p->ident)
		fprintf(fp, "Ident: %s\n", p->ident);

	if (p->hostname)
		fprintf(fp, "Hostname: %s\n", p->hostname);

	/*
	** Tell the reader how big the buffer has to be in order to read
	** the next line.  This includes the message length plus the word
	** "Message:" plus a space, a newline, and a null character.
	*/
	fprintf(fp, "Bytes: %d\n", strlen(p->message)+11);
	fprintf(fp, "Message: %s\n", p->message);
	fprintf(fp, "Created: %lu %s\n", p->created, my_ctime(&p->created));
	fprintf(fp, "UniqueID: %s\n", p->messageid);

	if (p->status)
		fprintf(fp, "Status: %s\n", p->status);

	/*
	** Before we close the file (which releases the lock), we
	** should check to see if this page is worth reading again
	** in the future.  If not, rename the file to start with 'B'.
	*/
	badname = NULL;
	if (p->flags & F_BADPAGE) {
		badname = strdup(p->filename);
		badname[0] = 'B';

		qpage_log(LOG_NOTICE, "renaming bad page to %s", badname);

		if (rename(p->filename, badname) < 0) {
			qpage_log(LOG_WARNING, "cannot rename %s to %s: %s",
				p->filename, badname, strerror(errno));
		}
	}

	if (fclose(fp)) {
		qpage_log(LOG_WARNING, "error writing queue file: %s",
			strerror(errno));

		return(-1);
	}

	if (badname) {
		free(badname);
		return(0);
	}

	return(1);
}


/*
** read_queue()
**
** This function reads the filenames in the page queue.  Any file which
** starts with a 'P' is considered a page.  All other files are ignored.
** Files containing pages are passed to read_page() to be read.
**
**	Input:
**		bad - a boolean flag indicating whether to read bad pages
**
**	Returns:
**		a linked list of pages, linked in the order they are read
*/
PAGE *
read_queue(int bad)
{
	struct dirent	*entry;
	PAGE		*head;
	PAGE		*curr;
	PAGE		*tmp;
	DIR		*dirp;


	head = NULL;
	curr = NULL;

	if ((dirp = opendir(".")) == NULL) {
		qpage_log(LOG_ERR, "cannot read current directory");
		return(NULL);
	}

	while ((entry = readdir(dirp)) != NULL) {
		if (entry->d_name[0] != 'P') {
			if (bad == FALSE || entry->d_name[0] != 'B')
				continue;
		}

		if ((tmp = read_page(entry->d_name)) != NULL) {
			if (head == NULL)
				head = tmp;
			else
				curr->next = tmp;

			curr = tmp;
		}
	}

	(void)closedir(dirp);

	return(head);
}


/*
** showqueue()
**
** This function shows the pages currently in the queue.
**
**	Input:
**		nothing
**
**	Returns:
**		the number of pages in the page queue
*/
int
showqueue(void)
{
	PAGE		*pagelist;
	PAGE		*tmp;
	rcpt_t		*rcpt;
	int		count;
	time_t		now;


	count = 0;
	now = time(NULL);

	pagelist = read_queue(TRUE);

	for (tmp=pagelist; tmp; tmp=tmp->next) {
		printf("ID=%s%s\n", tmp->messageid,
			tmp->filename[0] == 'B' ? " (*** bad page ***)" : "");

		printf("\t  Date: %s\n", my_ctime(&tmp->created));
		printf("\t  File: %s\n", tmp->filename);
		printf("\t  From: %s\n", tmp->from ? tmp->from : "[anonymous]");

		if (tmp->hostname)
			printf("\t  Host: %s\n", tmp->hostname);

		printf("\tLength: %d bytes\n", strlen(tmp->message));

		for (rcpt=tmp->rcpts; rcpt; rcpt=rcpt->next) {
			printf("\t    To: pager=%s", rcpt->pager);

			if (rcpt->tries)
				printf(", goodtries/tries=%d/%d",
					rcpt->goodtries, rcpt->tries);

			if (rcpt->holduntil > now)
				printf(", holduntil=%s",
					my_ctime(&rcpt->holduntil));

			if (rcpt->flags & F_SENT)
				printf(", status=SENT");

			if (rcpt->flags & F_FAILED)
				printf(", status=FAILED");

			printf("\n");
		}

		if (tmp->next)
			printf("\n");

		count++;
	}

	if (count == 0)
		printf("The page queue is empty.\n");

	return(count);
}


/*
** runqueue()
**
** This function reads the pages in the page queue, sorts them into
** a job list, sends the jobs, and writes pages back to the page queue.
** The number of pages remaining in the page queue is returned.  This
** number includes pages which were not sent because of retry counts
** being exceeded.
**
**	Input:
**		nothing
**
**	Returns:
**		an integer status (0=success)
**
**	Note:
**		This function creates memory leaks.  Fortunately,
**		it is only called from within a sub-process and
**		the memory will be reclaimed by the operating
**		system when the process exits.
*/
int
runqueue(void)
{
	PAGE		*pagelist;
	PAGE		*tmp;
	job_t		*joblist;
	int		count;
	int		lock;


	count = 0;
	joblist = NULL;

	/*
	** lock the page queue
	*/
	if ((lock = lock_queue()) < 0)
		return(-1);

	pagelist = read_queue(FALSE);

	if (Debug)
		qpage_log(LOG_DEBUG, "getting job list");

	for (tmp=pagelist; tmp; tmp=tmp->next) {
		if ((tmp->flags & F_BADPAGE) == 0)
			count += insert_jobs(&joblist, tmp);
	}

	if (Debug)
		qpage_log(LOG_DEBUG, "pending jobs: %d", count);

	if (joblist) {
		if (Debug)
			qpage_log(LOG_DEBUG, "sending job list");

		send_pages(joblist);
	}

	/*
	** We must write the pages back out regardless of whether
	** any jobs were processed.  This is because we may have
	** changed some flags (e.g. F_FAILED) and we need to ensure
	** that the changes are seen on the next iteration.
	*/
	if (Debug && pagelist)
		qpage_log(LOG_DEBUG, "writing job list");

	for (tmp=pagelist; tmp; tmp=tmp->next) {
		if (write_page(tmp, FALSE) < 0) {
			qpage_log(LOG_WARNING, "lost page id=%s",
			tmp->messageid);
		}
	}

	/*
	** unlock the page queue
	*/
	(void)close(lock);

	return(0);
}
