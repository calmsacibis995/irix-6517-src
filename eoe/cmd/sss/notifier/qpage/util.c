#include	"qpage.h"


/*
** global variables
*/
#ifndef lint
static char	sccsid[] = "@(#)util.c  1.31  07/07/98  tomiii@qpage.org";
#endif
#ifndef CLIENT_ONLY
jmp_buf		TimeoutEnv;
int		JobsPending = FALSE;
int		ReReadConfig = FALSE;


/*
** qpage_log()
**
** This function logs messages for qpage either via syslog() or by
** printing them to stdout or stderr, depending on the Interactive
** and Debug flags.
**
**	Input:
**		pri - the syslog priority (if used)
**		fmt - a printf format string
**		... - a variable argument list (see "man va_start")
**
**	Returns:
**		nothing
**
**	Note:
**		Some messages should never be logged to stdout.  We
**		will abuse LOG_ALERT to kludge this condition.
*/
void
qpage_log(int pri, char *fmt, ...)
{
	FILE		*where;
	va_list		ap;
	int		tmp;


	/*
	** KLUDGE ALERT -- DANGER WILL ROBINSON
	**
	** There are some messages we always want logged.  The calling
	** functions will use LOG_ALERT to signal us of this condition,
	** and we'll just remap it to LOG_INFO.
	*/
	tmp = Interactive;
	if (pri == LOG_ALERT) {
		Interactive = FALSE;
		pri = LOG_INFO;
	}

	va_start(ap, fmt);

	/*
	** There are several possibilities here, depending on the
	** Interactive and Debug flags.
	**
	**	I D | output
	**	----+-------
	**	0 0 | sylog()
	**	0 1 | stderr
	**	1 0 | stdout
	**	1 1 | stdout
	*/
	if (Debug || Interactive) {
		where = Interactive ? stdout : stderr;

		(void)vfprintf(where, fmt, ap);
		fprintf(where, "\n");
		(void)fflush(where);
	}
	else {
#ifdef HAVE_VSYSLOG
		vsyslog(pri, fmt, ap);
#else
#ifdef HAVE_VPRINTF
		char buff[1024];
		(void)vsprintf(buff, fmt, ap);
		syslog(pri, buff);
#else
		/*
		** bummer, your system really stinks.
		*/
		syslog(pri, "%s", buff);
#endif
#endif
	}

	va_end(ap);

	Interactive = tmp;
}


/*
** log_child_status()
**
** This function attempts to determine why a child process exited.
*/
void
log_child_status(int pid, int status)
{
	char	*signame;
	int	signum;
	int	core;


	signame = "unknown signal";
	core = 0;

	/*
	** It's probably not cool to be printing messages from
	** an interrupt context (libc is not always thread safe)
	** so we'll only do this if we're debugging.
	*/
	if (Debug) {
#ifdef WIFSIGNALED
		if (WIFSIGNALED(status)) {
			signum = WTERMSIG(status);
#ifdef SYS_SIGLIST_DECLARED
			signame = (char *)sys_siglist[signum];
#else
#ifdef SOLARIS2
			/*
			** Solaris 2.x is weird.
			*/
			if (signum < _sys_siglistn)
				signame = (char *)_sys_siglistp[signum];
#endif
#endif /* SYS_SIGLIST_DECLARED */

#ifdef WCOREDUMP
			core = WCOREDUMP(status);
#endif
			qpage_log(LOG_NOTICE,
				"Child pid %u exited on signal %d (%s%s)",
				pid, signum, signame,
				core ? ", core dumped" : "");
		}
#endif /* WIFSIGNALED */

		if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
			qpage_log(LOG_INFO,
				"Child pid %u exited with status %d",
				pid, WEXITSTATUS(status));
	}
}


/*
** sigalrm()
**
** This function catches SIGALRM.  The alarm is used to implement
** timeouts for incoming SNPP commands.
*/
void
sigalrm(void)
{
	longjmp(TimeoutEnv, 1);
}


/*
** sigchld()
**
** This function is a signal handler for SIGCHLD.  It is
** called to clean up after a child process exits.
*/
void
sigchld(void)
{
	int	pid;
	int	status;


#ifdef HAVE_WAITPID
	while ((pid = waitpid((pid_t)-1, &status, WNOHANG)) > 0)
		log_child_status(pid, status);
#else
	if ((pid = wait(&status)) > 0)
		log_child_status(pid, status);
#endif
}


/*
** sighup()
**
** This function is a signal handler for SIGHUP.  This signal means
** the configuration files have changed and we should reread them as
** soon as possible.
*/
void
sighup(void)
{
	ReReadConfig = TRUE;
}


/*
** sigterm()
**
** This function is a signal handler for SIGTERM.
*/
void
sigterm(void)
{
	qpage_log(LOG_INFO, "caught SIGTERM, exiting...");
	exit(0);
}


/*
** sigusr1()
**
** This function is a signal handler for SIGUSR1.  This signal means
** the queue should be processed immediately without waiting for the
** normal "sleeptime" to expire.
*/
void
sigusr1(void)
{
	JobsPending = TRUE;
}
#endif /* CLIENT_ONLY */


/*
** my_realloc()
**
** This function is exactly like realloc() but it allows the incoming
** pointer to be NULL, in which case malloc() is called.
**
**	Input:
**		ptr - the buffer to reallocate
**		len - the new size
**
**	Returns:
**		a pointer to the new buffer (or NULL on error)
*/
void *
my_realloc(void *ptr, int len)
{
	if (ptr == NULL)
		return(malloc(len));
	else
		return(realloc(ptr, len));
}


/*
** my_free()
**
** This function makes sure the pointer is not NULL before calling free().
*/
void
my_free(void *ptr)
{
	if (ptr)
		free(ptr);
}


/*
** my_ctime()
**
** This function returns the string the real ctime() returns, except
** without the trailing newline.  What idiot decided it would be nice
** to supply that extra newline character, anyway?
**
**	Input:
**		clock - the number of seconds since 1970
**
**	Returns:
**		a string representation of time, or NULL on failure
*/
char *
my_ctime(time_t *clock)
{
	char	*ptr;
	char	*tmp;


	ptr = ctime(clock);

	if (ptr && ((tmp = strchr(ptr, '\n')) != NULL))
		*tmp = '\0';

	return(ptr);
}


/*
** strjoin()
**
** This function joins two strings by allocating a buffer, copying
** the two strings into it, and freeing the pointer to the first
** string.
**
**	Input:
**		str1 - the first string to join
**		str2 - the second string to join
**
**	Returns:
**		a pointer to the newly allocated buffer containing
**		the joined strings
*/
char *
strjoin(char *str1, char *str2)
{
	char	*new;
	int	len1;
	int	len2;


	if (str1 == NULL)
		return(strdup(str2));

	len1 = strlen(str1);
	len2 = strlen(str2);

	new = (void *)malloc(len1 + len2 + 1);

	(void)strcpy(new, str1);
	(void)strcpy(new + len1, str2);

	free(str1);

	return(new);
}


/*
** getinput()
**
** This function reads input characters from the specified stream.
** If the "oneline" flag is TRUE, characters are read until CRLF
** (or just LF) is received.  Otherwise, characters are read one
** at a time until the sequence "CRLF.CRLF" is received (the CR
** characters are optional).  All contiguous whitespace is reduced
** to a single space character.  Leading and trailing whitespace
** is removed.
**
**	Input:
**		fp - a pointer to the input stream
**		oneline - whether to return only a single line
**
**	Returns:
**		a null terminated message with all occurrences of
**		space characters compressed down to a single space.
**
**	Note:
**		The buffer returned by this function is dynamically
**		allocated via malloc().  It is the responsibility of
**		the caller to free this memory when it is no longer
**		needed.
*/
char *
getinput(FILE *fp, int oneline)
{
	char		*buf;
	char		*ptr;
	char		lastc;
	int		buflen;
	int		almosteom;	/* true if "CRLF." was seen */
	int		len;
	int		c;


	buflen = 0;
	buf = NULL;
	ptr = NULL;
	len = 0;
	lastc = '\n';
	almosteom = 0;

#ifndef CLIENT_ONLY
	/*
	** Set the alarm so we don't hang.  Since this function is
	** also used by the client to read the initial message from
	** the user, we'll only set the timer if we're not reading
	** from standard input.
	*/
	if (fp != stdin)
		(void)alarm(SNPPTimeout);

	/*
	** check to see if a timeout occurred
	*/
	if (setjmp(TimeoutEnv) > 0) {
		my_free(buf);
		return("");
	}
#endif

	while (!feof(fp)) {
		/*
		** make sure the buffer is big enough
		*/
		if (buflen-len < 10) {
			buf = (char *)my_realloc(buf, buflen+BUFCHUNKSIZE);
			buflen += BUFCHUNKSIZE;
			ptr = buf + len;
		}

		if ((c = getc(fp)) == EOF)
			break;

		/*
		** if this is CR and the next char is LF, skip the CR
		*/
		if (c == '\r') {
			if ((c = getc(fp)) == EOF)
				break;

			if (c != '\n')
				(void)ungetc(c, fp);
		}

		/*
		** check for the end of the message
		*/
		if ((almosteom || oneline) && (c == '\n' || c == '\r')) {
			/*
			** if this is a message, nuke the trailing dot
			*/
			if (almosteom)
				len--;

			break;
		}
		else
			if (c == '.' && lastc == '\n')
				almosteom = 1;
			else
				almosteom = 0;

		if (isspace(c)) {
			if (!isspace(lastc)) {
				*ptr++ = ' ';
				len++;
			}
		}
		else {
			*ptr++ = c;
			len++;
		}

		lastc = c;
	}

#ifndef CLIENT_ONLY
	/*
	** we're out of danger; turn off the alarm
	*/
	(void)alarm(0);
#endif

	/*
	** clean up the end of the message
	*/
	if (buf)
		buf[len] = '\0';

	if (len > 0 && buf[len-1] == ' ')
		buf[len-1] = '\0';

	if (buf && *buf == '\0') {
		free(buf);
		buf = NULL;
	}

	return(buf);
}


/*
** msgcpy()
**
** Like strncpy() but break at whitespace.
**
**	Input:
**		dst - the destination string
**		src - the source string
**		n - the maximum number of bytes to copy
**
**	Returns:
**		a pointer to the first byte not copied or NULL if there
**		is nothing left to copy
*/
char *
msgcpy(char *dst, char *src, int n)
{
	char	*start;
	char	*end;


	start = NULL;
	end = NULL;

	/*
	** skip leading whitespace
	*/
	while (*src && isspace(*src))
		src++;

	/*
	** save room for the null terminator
	*/
	if (n-- < 1)
		return(src);

	while (*src && n) {
		if (isspace(*src)) {
			end = dst;
			*dst++ = ' ';
			n--;

			while (*src && isspace(*src))
				src++;

			start = src;
		}
		else {
			*dst++ = *src++;
			n--;
		}
	}

	*dst = '\0';

	/*
	** Now make sure we didn't chop a word in the middle.
	*/
	if (*src && end) {
		*end++ = '\0';
		src = start;
	}

	/*
	** It's possible that the only characters left over at this
	** point are whitespace characters.  We should get rid of them
	** here and return a null pointer so we don't end up sending
	** an "empty" page next time.
	*/
	while (*src && isspace(*src))
		src++;

	if (*src)
		return(src);
	else
		return(NULL);
}


/*
** snpptime()
**
** This function checks to see if its argument is a valid SNPP time
** specification.  If so, an equivalent local time value is returned.
**
**	Input:
**		arg - the argument string
**
**	Returns:
**		the equivalent local time
*/
time_t
snpptime(char *arg)
{
	struct tm	tm;
	struct tm	*tmp;
	time_t		now;
	time_t		timespec;
	time_t		gmtoffset;
	int		year2;
	int		i;


	tzset();
	now = time(NULL);
	year2 = localtime(&now)->tm_year % 100;

	/*
	** check for "YYMMDDHHMMSS" length
	*/
	if (strlen(arg) < 12) {
		return(INVALID_TIME);
	}

	for (i=0; i<12; i++)
		if (!isdigit(arg[i]))
			return(INVALID_TIME);

	if ((i != 12) || (arg[i] && !isspace(arg[i])))
		return(INVALID_TIME);

	(void)memset((char *)&tm, 0, sizeof(tm));
	tm.tm_isdst = -1;
	tm.tm_year += (*arg++ - '0') * 10;
	tm.tm_year += (*arg++ - '0');
	tm.tm_mon  += (*arg++ - '0') * 10;
	tm.tm_mon  += (*arg++ - '0');
	tm.tm_mday += (*arg++ - '0') * 10;
	tm.tm_mday += (*arg++ - '0');
	tm.tm_hour += (*arg++ - '0') * 10;
	tm.tm_hour += (*arg++ - '0');
	tm.tm_min  += (*arg++ - '0') * 10;
	tm.tm_min  += (*arg++ - '0');
	tm.tm_sec  += (*arg++ - '0') * 10;
	tm.tm_sec  += (*arg++ - '0');

	/*
	** check for Y2K problems
	*/
	if (tm.tm_year < 50 && year2 >= 50) {
		tm.tm_year += 100;
	}

	/*
	** do some sanity checking on the user-supplied values
	*/
	tm.tm_mon--;
	if ((tm.tm_mon < 0) || (tm.tm_mon > 11))
		return(INVALID_TIME);

	if ((tm.tm_mday < 1) || (tm.tm_mday > 31))
		return(INVALID_TIME);

	if ((tm.tm_hour < 0) || (tm.tm_hour > 24))
		return(INVALID_TIME);

	if ((tm.tm_min < 0) || (tm.tm_min > 60))
		return(INVALID_TIME);

	if ((tm.tm_sec < 0) || (tm.tm_sec > 60))
		return(INVALID_TIME);

	/*
	** verify that we're at the end of a token
	*/
	if (*arg && !isspace(*arg))
		return(INVALID_TIME);

	/*
	** skip whitespate
	*/
	while (*arg && isspace(*arg))
		arg++;

	/*
	** check for optional GMT offset
	*/
	if (*arg) {
		i = atoi(arg);

		/*
		** all GMT offsets are multiples of 100
		*/
		if (i % 100)
			return(INVALID_TIME);

		tmp = gmtime(&now);
		tmp->tm_isdst = -1;
		gmtoffset = (now - mktime(tmp)) / 3600;
		tm.tm_hour += gmtoffset - i/100;
	}

	timespec = mktime(&tm);
	return(timespec);
}


/*
** parse_time()
**
** Convert a string into the number of seconds represented by the string.
** If the first character is a '+' then the result is added to the current
** time.  If the specified time already happened today, assume the user
** means the same time tomorrow.
**
**	Input:
**		str - the string to parse
**
**	Returns:
**		the time in seconds since 1970 that the page should be sent
*/
time_t
parse_time(char *str)
{
	struct tm	*tm;
	time_t		result;
	time_t		offset;
	time_t		now;
	char		*ptr;
	char		*tmp;
	int		days;
	int		i;


	days = 0;
	offset = 0;
	now = time(NULL);

	/*
	** As a convenience to the user, first check to see if they
	** specified the time in SNPP format.
	**
	** (Hi Bill, I hope this makes you happy now!)
	*/
	if ((result = snpptime(str)) != INVALID_TIME)
		return(result);


	if (*str == '+') {
		/*
		** They specified a time relative to the current time.
		** No problem, this is easy.
		*/
		str++;
		result = now;
	}
	else {
		/*
		** They specified an absolute time.  This means we must
		** figure out what time() would have returned at 12am
		** this morning.
		*/
		if ((tm = localtime(&now)) == NULL)
			return(INVALID_TIME);

		tm->tm_hour = 0;
		tm->tm_min = 0;
		tm->tm_sec = 0;

		if ((result = mktime(tm)) == INVALID_TIME)
			return(INVALID_TIME);
	}

	/*
	** see if they specified a number of days
	*/
	if ((tmp = strchr(str, '+')) != NULL && isdigit(*str)) {
		*tmp++ = '\0';
		days = atoi(str);
		str = tmp;
	}

	i = strlen(str);

	if (i > 4)
		return(INVALID_TIME);

	/*
	** reverse the string and take it apart one digit at a time
	*/
	ptr = (void *)malloc(i+1);
	tmp = ptr;
	while (i--) {
		if (!isdigit(*str)) {
			free(ptr);
			return(INVALID_TIME);
		}

		*tmp++ = *(str+i);
	}
	*tmp = '\0';

	tmp = ptr;
	if (*tmp) offset += ((*tmp++) - '0') * 60;
	if (*tmp) offset += ((*tmp++) - '0') * 600;
	if (*tmp) offset += ((*tmp++) - '0') * 3600;
	if (*tmp) offset += ((*tmp++) - '0') * 36000;
	free(ptr);

	result += (offset + (days * 24 * 60 * 60));

	/*
	** If the specified time already happened, they probably
	** meant the same time tomorrow.
	*/
	if (result < now)
		result += (24 * 60 * 60);

	return(result);
}


/*
** get_user()
**
** Return the username of the person running this program.
**
**	Input:
**		none
**
**	Returns:
**		the userid of the person running this program
*/
char *
get_user(void)
{
	static char	username[255];
	struct passwd	*p;


	if ((p = getpwuid(geteuid())) != NULL)
		(void)strcpy(username, p->pw_name);
	else
		(void)sprintf(username, "UID=%lu", geteuid());


	return(username);
}


#ifndef CLIENT_ONLY
/*
** on_duty()
**
** This function returns a boolean status of whether the specified
** time falls inside the specified schedule.  A schedule has the
** format DaylistStarttime-Endtime (the same syntax as the UUCP
** /etc/uucp/Systems file).  For example, MoThFr1600-1800 specifies
** a two hour shift three days a week starting at 4pm.
** 
**	Input:
**		schedule - the schedule to check
**		when - the time to use when checking the schedule
**
**	Returns:
**		an integer status code
*/
int
on_duty(char *schedule, time_t when)
{
	struct tm	*tm;
	char		*days[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
	int		start;
	int		end;
	int		now;


	if (schedule == NULL)
		return(TRUE);

	if (when == 0)
		when = time(NULL);

	/*
	** It's an error if localtime() fails, but we'll send
	** the page out anyway.  It's probably better to page
	** someone by mistake than to drop the page on the floor.
	*/
	if ((tm = localtime(&when)) == NULL)
		return(-1);

	now = (tm->tm_hour * 100) + tm->tm_min;

	if (sscanf(schedule, "%*[^0123456789]%d-%d", &start, &end) != 2) {
		qpage_log(LOG_ERR, "invalid schedule: %s", schedule);
		return(-1);
	}

	if (start == 2400)
		start = 0;

	if (end == 0)
		end = 2400;

	if (!strstr(schedule, days[tm->tm_wday]) && !strstr(schedule, "Any"))
		return(FALSE);

	if (now >= start && now <= end)
		return(TRUE);

	return(FALSE);
}


/*
** safe_string()
**
** Make a string safe for printing (i.e. change control chars to \nnn).
**
**	Input:
**		str - the string to translate
**
**	Returns:
**		a buffer containing the safe string
**
**	Side effects:
**		The buffer is declared static and is overritten each time
**		this function is called.
*/
char *
safe_string(char *str)
{
	static char     buf[4096];
	char            *ptr = buf;
	int             i;


	*ptr = '\0';

	while (*str) {
		if (isprint(*str))
			*ptr++ = *str++;
		else {
			switch(*str) {
				case ' ':
					*ptr++ = *str;
					break;

				case 27:
					*ptr++ = '\\';
					*ptr++ = 'e';
					break;

				case '\t':
					*ptr++ = '\\';
					*ptr++ = 't';
					break;

				case '\n':
					*ptr++ = '\\';
					*ptr++ = 'n';
					break;

				case '\r':
					*ptr++ = '\\';
					*ptr++ = 'r';
					break;

				default:
					i = *str;
					(void)sprintf(ptr, "\\%03o", i);
					ptr += 4;
					break;
			}

			str++;
		}

		*ptr = '\0';

	}

	return(buf);
}


/*
** lock_file()
**
** This function creates a kernel lock on a file descriptor.
**
**	Input:
**		fd - the file descriptor
**		mode - O_RDONLY or O_RDWR
**		block - block if the lock cannot be granted immediately
**
**	Returns:
**		an integer status code (0=success, -1=failure)
*/
int
lock_file(int fd, int mode, int block)
{
	struct flock	lock;
	int		i;


	(void)memset((char *)&lock, 0, sizeof(lock));

	lock.l_type = (mode == O_RDONLY ? F_RDLCK : F_WRLCK);
	lock.l_pid = getpid();

	i = fcntl(fd, (block ? F_SETLKW : F_SETLK), &lock);

	return(i);
}


/*
** lock_queue()
**
** This function creates a kernel lock on a lock file in the current
** working directory (assumed to be the the page queue).  The purpose
** of this function is to ensure that only one qpage process has the
** queue under its control at any given time.
**
**	Input:
**		nothing
**
**	Returns:
**		a locked open file descriptor, or -1 on failure
*/
int
lock_queue(void)
{
	int	fd;


	if ((fd = open("lock", O_CREAT|O_WRONLY, 0666)) < 0) {
		qpage_log(LOG_ERR, "cannot open queue lockfile: %s",
			strerror(errno));

		return(-1);
	}

	if (lock_file(fd, O_RDWR, FALSE) < 0) {
		/*
		** The lock attempt will fail here if another process
		** owns the queue lock.  This is normal, don't complain.
		** But if the failure is caused by any other condition
		** then we should let someone know.
		*/
		if (errno != EAGAIN && errno != EACCES)
			qpage_log(LOG_NOTICE, "cannot lock queue lockfile: %s",
				strerror(errno));

		(void)close(fd);
		return(-1);
	}

	return(fd);
}


/*
** lookup()
**
** This function searches a linked list for the node matching the
** specified name.  A NULL pointer is returned if no such node exists.
**
**	Input:
**		list - a linked list (service_t *) or (pager_t *).
**		name - the name to search for
**
**	Returns:
**		a pointer to the found structure or NULL if not found
**
**	Note:
**		The linked list passed to this function must have the
**		firsts two fields be a pointer to the next node and
**		a pointer to a name, respectively.
*/
void *
lookup(void *list, char *name)
{
	struct {
		void	*next;
		char	*name;
	} *ptr;


	for (ptr=list; ptr; ptr=ptr->next) {
		if (!strcmp(ptr->name, name))
			return(ptr);
	}

	return(NULL);
}


/*
** get_lockname()
**
** This function determines the name of the lockfile to use when
** locking the modem.
**
**	Input:
**		buff - the buffer to store the filename
**		device - the name of the modem device
**
**	Returns:
**		an integer status code (0=success, -1=failure)
*/
int
get_lockname(char *buff, char *device)
{
#ifdef SOLARIS2
	struct stat     statbuf;
#else
	char		*ptr;
#endif


#ifdef SOLARIS2
	if (stat(device, &statbuf) < 0) {
		qpage_log(LOG_WARNING, "cannot stat() modem (%s): %s",
			device, strerror(errno));

		return(-1);
	}

	(void)sprintf(buff, "%s/LK.%3.3lu.%3.3lu.%3.3lu", LockDir,
		(unsigned long)major(statbuf.st_dev),
		(unsigned long)major(statbuf.st_rdev),
		(unsigned long)minor(statbuf.st_rdev));
#else
	if ((ptr = strrchr(device, '/')) == NULL)
		return(-1);

	ptr++;
	(void)sprintf(buff, "%s/LCK..%s", LockDir, ptr);
#endif

	return(0);
}


/*
** lock_modem()
**
** This function locks the modem so that "tip" and friends don't
** stomp on us.
**
**	Input:
**		device - the name of the device to lock
**
**	Returns:
**		an integer status code (0=success, -1=error)
*/
int
lock_modem(char *device)
{
	FILE		*fp;
	mode_t		mode;
	pid_t		pid;
	char		buff[100];
	char		tmpfile[255];
	char		lockfile[255];
	int		fd;


	if (Debug || Interactive)
		qpage_log(LOG_DEBUG, "locking %s", device);

	if (get_lockname(lockfile, device) < 0) {
		qpage_log(LOG_WARNING, "cannot determine lock name for %s",
			device);
		return(-1);
	}

	(void)sprintf(tmpfile, "%s/LTMP.%lu", LockDir, getpid());

	/*
	** Override the default umask because everyone should be
	** able to read this lockfile.
	*/
	mode = umask(0);
	fd = creat(tmpfile, 0444);
	(void)umask(mode);

	if (fd < 0) {
		qpage_log(LOG_WARNING, "cannot create lockfile: %s",
			strerror(errno));

		return(-1);
	}

	(void)sprintf(buff, "%10lu\n", getpid());
	(void)write(fd, buff, strlen(buff));
	(void)close(fd);

	if (link(tmpfile, lockfile) < 0) {
		if (errno != EEXIST) {
			qpage_log(LOG_WARNING, "cannot create lockfile: %s",
				strerror(errno));

			(void)unlink(tmpfile);
			return(-1);
		}

		if ((fp = fopen(lockfile, "r")) == NULL) {
			qpage_log(LOG_WARNING, "cannot open existing lockfile");
			(void)unlink(tmpfile);
			return(-1);
		}

		buff[0] = '\0';
		(void)fgets(buff, sizeof(buff), fp);
		(void)fclose(fp);

		pid = atoi(buff);

		/*
		** Does the lock owner exist?
		*/
		errno = 0;
		if (pid > 0 && ((kill(pid, 0) == 0) || (errno != ESRCH))) {
			qpage_log(LOG_INFO, "%s already in use", device);
			(void)unlink(tmpfile);
			return(-1);
		}

		/*
		** The process that owns the lock must have died.
		** Blast the existing lock file and create a new one.
		*/
		if (unlink(lockfile) < 0) {
			qpage_log(LOG_WARNING, "cannot remove lockfile: %s",
				strerror(errno));

			(void)unlink(tmpfile);
			return(-1);
		}

		if (link(tmpfile, lockfile) < 0) {
			qpage_log(LOG_WARNING, "cannot create lockfile: %s",
				strerror(errno));

			(void)unlink(tmpfile);
			return(-1);
		}
	}

	(void)unlink(tmpfile);
	return(0);
}


/*
** unlock_modem()
**
** This function unlocks the modem.
**
**	Input:
**		device - the name of the device to unlock
**
**	Returns:
**		nothing
*/
void
unlock_modem(char *device)
{
	char		lockfile[255];


	if (Debug || Interactive)
		qpage_log(LOG_DEBUG, "unlocking %s", device);

	if (get_lockname(lockfile, device) < 0) {
		qpage_log(LOG_WARNING, "cannot determine lock name for %s",
			device);
		return;
	}

	if (unlink(lockfile) < 0)
		qpage_log(LOG_WARNING, "cannot unlock lock %s: %s",
			device, strerror(errno));
}


/*
** drop_root_privileges()
**
** This function attempts to drop root privileges.  If a username
** was specified at compile time, that user's privileges (including
** all associated group memberships) are used.  Otherwise if a
** numeric UID was specified at compile time, that UID is used and
** the run-time group membership remains unchanged.
**
**	Input:
**		nothing
**
**	Returns:
**		nothing
*/
void
drop_root_privileges(void)
{
	struct passwd	*p;
	uid_t		uid;


	if (geteuid() == 0) {
		qpage_log(LOG_INFO, "giving up root privileges");

		if ((p = getpwnam(DAEMON_USER)) != NULL) {
			if (Debug)
				qpage_log(LOG_DEBUG, "becoming user '%s'",
					p->pw_name);

			(void)initgroups(p->pw_name, p->pw_gid);
			(void)setuid(p->pw_uid);
		}
		else {
			uid = (uid_t)atol(DAEMON_USER);

			if (uid && (p = getpwuid(uid)) != NULL) {
				if (Debug)
					qpage_log(LOG_DEBUG,
						"becoming user '%s'",
						p->pw_name);

				(void)initgroups(p->pw_name, p->pw_gid);
				(void)setuid(p->pw_uid);
			}
			else {
				if (uid && Debug)
					qpage_log(LOG_DEBUG,
						"becoming uid %d", uid);

				if (uid)
					(void)setuid(uid);
			}
		}

		if (geteuid() == 0)
			qpage_log(LOG_WARNING,
				"warning: daemon is running as root!");
	}
}
#endif /* CLIENT_ONLY */


/*
** strip
**
** This function strips all characters in the input string to 7 bits.
** All occurances of whitespace are reduced to a single space.  All
** control characters are converted to two bytes: 0x1A followed by
** the printable ASCII value obtained by adding 0x40 to the value of
** the control character (i.e. Ctrl-A is replaced with 0x1A and 'A').
** 
**	Input:
**		str - the string to be stripped
**
**	Returns:
**		nothing
**
**	Side effects:
**		The memory used by the original string is freed.  A
**		new buffer is allocated to hold the result.  Upon
**		return from this function, the original pointer will
**		point to the processed version of the input string. 
*/
void
strip(char **data)
{
	char	*str;
	char	*buf;
	char	*ptr;
	char	c;
	int	len;


	str = *data;
	len = 0;

	/*
	** figure out how big to make the buffer
	*/
	while (*str) {
		c = *str++ & 0x7f;

		/*
		** handle the 0x80 case
		*/
		if (c == '\0')
			continue;

		/*
		** compress whitespace
		*/
		if (isspace(c)) {
			len++;

			while (isspace(*str & 0x7f))
				str++;

			continue;
		}

		/*
		** escape control characters
		*/
		if (c < 0x20) {
			len++;
			continue;
		}

		len++;
	}

	buf = (void *)malloc(len+1);
	ptr = buf;
	str = *data;

	/*
	** Now copy the data to the new buffer, compressing
	** whitespace and escaping control characters.
	*/
	while (*str) {
		c = *str++ & 0x7f;

		/*
		** handle the 0x80 case
		*/
		if (c == '\0')
			continue;

		/*
		** compress whitespace
		*/
		if (isspace(c)) {
			*ptr++ = ' ';

			while (isspace(*str & 0x7f))
				str++;

			continue;
		}

		/*
		** escape control characters
		*/
		if (c < 0x20) {
			*ptr++ = 0x1a;
			*ptr++ = c + 0x40;
			continue;
		}

		*ptr++ = c;
	}

	/*
	** delete possible trailing space
	*/
	if (ptr > buf && *(ptr-1) == ' ')
		ptr--;

	*ptr = '\0';

	free(*data);
	*data = buf;
}


/*
** safe_strtok()
**
** This function works just like the real strtok() but it has the
** advantage that it does not keep any state and therefore it can
** be invoked multiple times from any context.  Also, the original
** string is never modified (unlike the real strtok() which sprinkes
** NULL characters all over the place).
**
**	Input:
**		str - the string to be parsed (NULL on successive calls)
**		sep - the list of separater characters
**		last - where we left off parsing on the last call
**		res - a buffer to store the result.
**
**	Returns:
**		the parsed token, or NULL if no tokens remain
*/
char *
safe_strtok(char *str, char *sep, char **last, char *res)
{
	char	*a;
	char	*b;

	if (str == NULL)
		str = *last;

	if (str == NULL)
		return(NULL);

	if ((a = str + strspn(str, sep)) == NULL)
		return(NULL);

	if ((b = strpbrk(a, sep)) == NULL) {
		(void)strcpy(res, a);
		*last = NULL;
	}
	else {
		(void)strncpy(res, a, b-a);
		res[b-a] = '\0';
		*last = b + 1;
	}

	return(res);
}
