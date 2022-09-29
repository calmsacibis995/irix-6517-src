#include	"qpage.h"


/*
** Global variables
*/
#ifndef lint
static char	sccsid[] = "@(#)ixo.c  1.38  07/26/98  tomiii@qpage.org";
#endif
static int	HangupDetected;


/*
** checksum()
**
** Calculate the checksum of a packet.
**
**	Input:
**		pk - the packet
**
**	Returns:
**		a character representation of the checksum for the packet
**
**	Note:
**		This function came from the "tpage" package, written
**		by Tom Limoncelli <tal@lucent.com>.
*/
char *
checksum(char *pk)
{
	static char	check[10];
	int		sum;


	for (sum=0;*pk; pk++)
		sum += *pk;

	check[2] = '0' + (sum & 15); sum = sum >> 4;
	check[1] = '0' + (sum & 15); sum = sum >> 4;
	check[0] = '0' + (sum & 15);
	check[3] = 0;

	return(check);
}


/*
** getpacket()
**
** Read a packet (a string terminated by '\r') from the modem.
**
**	Input:
**		seconds - how long to wait before timing out
**
**	Returns:
**		a complete packet or NULL if a timeout occurred
**
**	Note:
**		The timer is reset for each character received.
*/
char *
getpacket(int fd, int seconds)
{
	static char	buff[1024];
	static int	got_full_packet;

#ifdef HAVE_POLL
	struct pollfd	fds;
#else
	struct timeval	timeout;
	fd_set		readfds;
#endif
	char		*ptr;
	int		i;


	if (got_full_packet) {
		got_full_packet = 0;
		ptr = buff;
		*ptr = '\0';
	}
	else {
		ptr = buff+strlen(buff);
	}


	do {
#ifdef HAVE_POLL
		fds.fd = fd;
		fds.events = POLLIN;
		fds.revents = 0;

		i = poll(&fds, 1, seconds * 1000);

		if (fds.revents & POLLHUP)
			HangupDetected = TRUE;
#else
		FD_ZERO_LINTED(&readfds);
		FD_SET(fd, &readfds);

		timeout.tv_sec = seconds;
		timeout.tv_usec = 0;

		i = select(FD_SETSIZE, &readfds, 0, 0, &timeout);

		/*
		** AIX USERS BEWARE!  Many AIX users have reported a
		** problem with select() returning early (and with a
		** non-zero return code) even though the status has not
		** changed on any of the specified file descriptors.
		** One report said the bug only started happening after
		** some "Year-2000 patches" were installed.  Another
		** report said that IBM has a patch specifically for
		** this problem (although I don't know the patch number).
		** I don't have access to the AIX platform so I can't
		** test this myself.
		*/
#endif /* not HAVE_POLL */

		switch (i) {
			case 0: /* timer expired */
				return(NULL);

			case 1: /* modem is waiting to be read */
#ifdef HAVE_POLL
				if ((fds.revents & POLLIN) == 0)
					break;
#else
				if (FD_ISSET(fd, &readfds) == 0)
					break;
#endif
				errno = 0;
				if ((i = read(fd, ptr, 1)) == 1) {

					if (*ptr == '\r') {
						*ptr = '\0';
						got_full_packet++;
					}
					else {
						/*
						** SunOS 4.1.4 reportedly
						** sometimes reads null
						** characters here.  We
						** don't want these copied
						** to the buffer.  Thanks
						** to Tim Steele for finding
						** the problem.
						*/
						if (*ptr && *ptr != '\n')
							ptr++;

						*ptr = '\0';
					}
				}
				else {
					if (i == 0 || errno != EINTR)
						return(NULL);
				}
				break;

			default:
				if (errno == EINTR)
					break;

#ifdef HAVE_POLL
				qpage_log(LOG_DEBUG, "poll() failed: %s",
					strerror(errno));
#else
				qpage_log(LOG_DEBUG, "select() failed: %s",
					strerror(errno));
#endif

				return(NULL);
		}
	}
	while (!got_full_packet);

	if (Debug)
		qpage_log(LOG_DEBUG, "got %d bytes: <%s>", strlen(buff),
			safe_string(buff));

	return(buff);
}


/*
** lookfor()
**
** Read packets from the modem until the packet matches a given
** string or until getpacket() times out.
**
**	Input:
**		str - the string to look for
**		timeout - a timeout value to pass to getpacket()
**
**	Returns:
**		an integer status (1=found, 0=notfound)
*/
int
lookfor(int fd, char *str, int timeout)
{
	char	*ptr;


	if (Debug)
		qpage_log(LOG_DEBUG, "looking for <%s>", safe_string(str));

	do {
		ptr = getpacket(fd, timeout);

		if (ptr && !strcmp(ptr, str)) {
			if (Debug) {
				qpage_log(LOG_DEBUG, "found <%s>",
					safe_string(str));
			}

			return(1);
		}
	}
	while (ptr);

	if (Debug)
		qpage_log(LOG_DEBUG, "didn't find <%s>", safe_string(str));

	return(0);
}


/*
** write_modem()
**
** Write a string to the modem.
**
**	Input:
**		str - the string to write
**
**	Returns:
**		nothing
*/
void
write_modem(int fd, char *str)
{
	if (str == NULL)
		return;

	if (Debug)
		qpage_log(LOG_DEBUG, "sending <%s>", safe_string(str));

	while (write(fd, str, strlen(str)) < 0 && errno == EINTR)
		continue;
}


/*
** carrier_detect()
**
** This function determines whether the RS-232 "carrier detect"
** signal is asserted or not.
**
**	Input:
**		fd - the device's open file descriptor
**
**	Returns:
**		a boolean status indicator
*/
int
carrier_detect(int fd)
{
#ifdef TIOCM_CAR
	struct termios	ti;

	if (tcgetattr(fd, &ti) < 0) {
		if(errno != ENXIO) {
			qpage_log(LOG_DEBUG, "carrier_detect tcgetattr(): %s", strerror(errno));
		}
		return(0);
	}

	return(ti.c_lflag & TIOCM_CAR);
#else
	return(0);
#endif
}


/*
** closemodem()
**
** Close the modem device.  The IXO/TAP protocol requires XON/XOFF
** flow control.  Occasionally, line noise received by the modem
** when the other end disconnects contains the XOFF character.  On
** some platforms, this prevents close() from completing until the
** matching XON character is received.  This function attempts to
** work around the problem by flushing any data in the stream and
** putting the file descriptor in non-blocking mode before calling
** close(). 
*/
void
closemodem(int fd)
{
	(void)tcflush(fd, TCIOFLUSH);
	(void)fcntl(fd, F_SETFL, O_NONBLOCK|O_NDELAY);
	(void)close(fd);
}


/*
** hangup_modem()
**
** This function attempts to hang up the modem, first by dropping DTR
** and then if that doesn't work, sending +++ followed by ATH0.
**
**	Input:
**		fd - the device's open file descriptor
**
**	Returns:
**		nothing
*/
void
hangup_modem(int fd)
{
	
	struct termios	ti;

	if (Debug || Interactive)
		qpage_log(LOG_DEBUG, "hanging up modem");

#ifndef QUICK_HANGUP
	/*
	** Get the modem status
	*/
	if (tcgetattr(fd, &ti) < 0) {
		qpage_log(LOG_DEBUG, "hangup_modem tcgetattr(): %s", strerror(errno));
		closemodem(fd);
		return;
	}

#ifdef TIOCM_DTR
	ti.c_lflag &= ~TIOCM_DTR;
#endif
	(void)cfsetispeed(&ti, 0);
	(void)cfsetospeed(&ti, 0);

	/*
	** Attempt to hang up the "nice" way by setting the line speed
	** to zero and dropping DTR.
	*/
	if (tcsetattr(fd, TCSANOW, &ti) < 0) {
		qpage_log(LOG_DEBUG, "tcsetattr(): %s", strerror(errno));
		closemodem(fd);
		return;
	}

	/*
	** Give the modem a chance to reset and then get the status of
	** the carrier-detect signal again.  Note: I've been told that
	** IRIX users might have to increase this sleep time to as much
	** as 10 seconds.
	*/

 	(void)sleep(5);

	if (carrier_detect(fd)) {
		qpage_log(LOG_INFO, "dropping DTR did not hang up the modem!");

		/*
		** try hanging up the old fashioned way
		*/
		write_modem(fd, "+++");
		(void)lookfor(fd, "OK", 5);
		write_modem(fd, "ATH0\r");

		/*
		** read() is probably going to return 0 from now on because
		** a hangup occurred on the stream (see read(2) for details)
		** but we'll give it a try anyway.
		*/
		(void)lookfor(fd, "OK", 5);
	}
#endif /* not QUICK_HANGUP */

	closemodem(fd);
}


/*
** openmodem()
**
** Open and initialize the modem device.
**
**	Input:
**		s - the service specifying the device name, speed, and parity
**
**	Returns:
**		an open file descriptor or -1 on error
*/
int
openmodem(char *path)
{
	int	flags;
	int	fd;

	/*if (Debug)*/
		qpage_log(LOG_DEBUG, "trying modem device %s", path);

	if (lock_modem(path) < 0)
		return(-1);

	if ((fd = open(path, O_RDWR|O_NONBLOCK, 0)) < 0) {
		qpage_log(LOG_INFO, "cannot open %s: %s",
			path, strerror(errno));

		unlock_modem(path);
		return(-1);
	}

	/*
	** attempt to flush any data currently in the system buffers
	*/
	(void)tcflush(fd, TCIOFLUSH);

	/*
	** Make sure modem isn't already connected
	*/
	if (carrier_detect(fd)) {
		qpage_log(LOG_INFO, "carrier detect is already present!");
		hangup_modem(fd);
		unlock_modem(path);
	
		if (lock_modem(path) < 0)
			return(-1);

		if ((fd = open(path, O_RDWR|O_NONBLOCK, 0)) < 0) {
			qpage_log(LOG_INFO, "cannot open %s: %s",
				path, strerror(errno));

			unlock_modem(path);
			return(-1);
		}
	}

	/*
	** get the file descriptor status flags
	*/
	if ((flags = fcntl(fd, F_GETFL, 0)) < 0) {
		qpage_log(LOG_DEBUG, "fcntl(F_GETFL): %s", strerror(errno));
		closemodem(fd);
		unlock_modem(path);
		return(-1);
	}

	/*
	** turn off non-blocking mode
	*/
	flags &= ~(O_NONBLOCK|O_NDELAY);
	if (fcntl(fd, F_SETFL, flags) < 0) {
		qpage_log(LOG_DEBUG, "fcntl(F_SETFL): %s", strerror(errno));
		closemodem(fd);
		unlock_modem(path);
		return(-1);
	}

	return(fd);
}


/*
** init_tty()
*/
int
init_tty(int fd, service_t *s)
{
	struct termios		ti;


	/*
	** get current tty settings
	*/
	if (tcgetattr(fd, &ti) < 0) {
		qpage_log(LOG_DEBUG, "init_tty tcgetattr(): %s", strerror(errno));
		return(-1);
	}

	/*
	** set the desired terminal speed
	*/
	(void)cfsetispeed(&ti, s->baudrate);
	(void)cfsetospeed(&ti, s->baudrate);

	/*
	** set the desired parity
	*/
	switch (s->parity) {
		case 0:
			ti.c_cflag &= ~CSIZE;	/* clear char size bits */
			ti.c_cflag |= CS8;	/* 8-bit bytes */
			ti.c_cflag &= ~PARENB;	/* no parity */
			break;

		case 1:
			ti.c_cflag &= ~CSIZE;	/* clear char size bits */
			ti.c_cflag |= CS7;	/* 7-bit bytes */
			ti.c_cflag |= PARENB;	/* parity enable */
			ti.c_cflag |= PARODD;	/* odd parity */
			break;

		case 2: /* fall through */
		default:
			ti.c_cflag &= ~CSIZE;	/* clear char size bits */
			ti.c_cflag |= CS7;	/* 7-bit bytes */
			ti.c_cflag |= PARENB;	/* parity enable */
			ti.c_cflag &= ~PARODD;	/* even parity */
			break;
	}

	/*
	** For the following code:
	**
	**	|= OPTION	turns the option on
	**	&= ~OPTION	turns the option off
	*/
	ti.c_iflag |= IGNBRK;		/* ignore breaks */
/*	ti.c_iflag |= BRKINT;		signal interrupt on break */
/*	ti.c_iflag |= IGNPAR;		ignore parity */
/*	ti.c_iflag |= PARMRK;		mark parity errors */
	ti.c_iflag &= ~INPCK;		/* enable input parity check */
	ti.c_iflag &= ~ISTRIP;		/* strip 8th bit */
	ti.c_iflag |= INLCR;		/* map nl->cr */
	ti.c_iflag &= ~IGNCR;		/* ignore cr */
	ti.c_iflag &= ~ICRNL;		/* map cr->nl */
/*	ti.c_iflag |= IUCLC;		map uppercase to lowercase */
	ti.c_iflag |= IXON;		/* enable start/stop output control */
	ti.c_iflag &= ~IXANY;		/* enable any char to restart output */
	ti.c_iflag |= IXOFF;		/* enable start/stop input control */
/*	ti.c_iflag |= IMAXBEL;		echo BEL on input line too long */

	ti.c_oflag &= ~OPOST;		/* post-process */

	ti.c_cflag &= ~CSTOPB;		/* one stop bit */
/*	ti.c_cflag |= CSTOPB;		two stop bit */

	ti.c_cflag |= CREAD;		/* enable receiver */
	ti.c_cflag |= HUPCL;		/* hang up on last close */
	ti.c_cflag &= ~CLOCAL;		/* modem line */
/*	ti.c_cflag |= CRTSCTS;		enable RTS/CTS flow control */

	ti.c_lflag &= ~ISIG;		/* enable signals */
	ti.c_lflag &= ~ICANON;		/* enable erase & kill processing */
/*	ti.c_lflag |= XCASE;		canon upper/lower presentation */
	ti.c_lflag &= ~ECHO;		/* enable echo */
/*	ti.c_lflag |= ECHOE;		echo erase as bs-sp-bs */
/*	ti.c_lflag |= ECHOK;		echo nl after kill */
/*	ti.c_lflag |= ECHONL;		echo nl */
/*	ti.c_lflag |= NOFLSH;		disable flush after interrupt */
/*	ti.c_lflag |= TOSTOP;		send SIGTTOU for background output */
/*	ti.c_lflag |= ECHOCTL;		echo ctrl chars as ^A */
/*	ti.c_lflag |= ECHOPRT;		echo erase as char erased */
/*	ti.c_lflag |= ECHOKE;		bs-sp-bs entire line on line kill */
/*	ti.c_lflag |= FLUSHO;		output is being flushed */
/*	ti.c_lflag |= PENDIN;		retype pending input at next read */
/*	ti.c_lflag |= IEXTEN;		recognize all specials, else POSIX */

	ti.c_cc[VMIN] = 0;		/* must read one char or timeout */
	ti.c_cc[VTIME] = 10;		/* read() times out at 1.0 second */

	/*
	** set new tty settings
	*/
	if (tcsetattr(fd, TCSANOW, &ti) < 0) {
		qpage_log(LOG_DEBUG, "tcsetattr(): %s", strerror(errno));
		return(-1);
	}

	return(0);
}


/*
** remote_connect()
**
** Dial the remote paging terminal and log in.
**
**	Input:
**		s - the paging service to connect to
**
**	Returns:
**		zero or more flags indicating success or reasons for failure
*/
int
remote_connect(int fd, service_t *s, modem_t *m)
{
	char		buff[1024];
	char		*ptr;
	char		*initcmd;
	char		*dialcmd;
	int		done;
	int		gotnak;
	char		c;
	int		i;


	if (m)
		initcmd = m->initcmd;
	else
		initcmd = DEFAULT_INITCMD;

	if (s->dialcmd) {
		dialcmd = s->dialcmd;
	}
	else {
		if (m)
			dialcmd = m->dialcmd;
		else
			dialcmd = DEFAULT_DIALCMD;
	}

	if (Debug || Interactive)
		qpage_log(LOG_DEBUG, "initializing modem with %s", initcmd);

	write_modem(fd, "\r");
	(void)sleep(1);
	write_modem(fd, initcmd);
	write_modem(fd, "\r");

	if (!lookfor(fd, "OK", 5)) {
		qpage_log(LOG_NOTICE, "cannot initialize modem");
		return(F_NOMODEM);
	}

	/*
	** Some braindamaged modems (like the Hayes 1200) return "OK" before
	** they are actually ready to accept commands.
	*/
	(void)sleep(1);

	if (Debug || Interactive)
		qpage_log(LOG_DEBUG, "dialing service %s with %s%s",
			s->name, dialcmd, s->phone ? s->phone : "");

	/*
	** dial paging terminal
	*/
	(void)sprintf(buff, "%s%s", dialcmd, s->phone ? s->phone : "");
	write_modem(fd, buff);
	write_modem(fd, "\r");

	if (!lookfor(fd, buff, 2)) {
		qpage_log(LOG_NOTICE, "cannot dial modem");
		return(F_NOMODEM);
	}

	if (Debug || Interactive)
		qpage_log(LOG_DEBUG, "waiting for connection");

	/*
	** wait for connection
	*/
	for (;;) {
		/*
		** use a very long timeout here so the modem's S7 register
		** can determine the real timeout
		*/
		if ((ptr = getpacket(fd, 255)) == NULL)
			break;

		if (strstr(ptr, "CONNECT")) {
			qpage_log(LOG_INFO, "connected to remote, service=%s",
				s->name);
			break;
		}

		if (strstr(ptr, "BUSY")) {
			qpage_log(LOG_INFO, "remote is busy, service=%s",
				s->name);
			return(F_BUSY);
		}

		if (strstr(ptr, "NO DIALTONE")) {
			qpage_log(LOG_NOTICE,
				"no answer from remote, service=%s", s->name);
			return(F_NOMODEM);
		}

		if (strstr(ptr, "NO CARRIER")) {
			qpage_log(LOG_NOTICE,
				"no answer from remote, service=%s", s->name);
			return(F_NOCARRIER);
		}

		if (strstr(ptr, "OK")) {
			qpage_log(LOG_NOTICE, "dial string failed, service=%s",
				s->name);
			return(F_NOCARRIER);
		}

	}

	if (!ptr) {
		qpage_log(LOG_NOTICE,
			"timeout waiting for CONNECT, service=%s", s->name);
		return(F_UNKNOWN);
	}

	/*
	** look for "ID=" prompt from paging service
	*/
	for (i=0; i<10; i++) {
		(void)sleep(1);
		write_modem(fd, "\r");
		done = FALSE;
		ptr = "ID=";

		while (read(fd, &c, 1) == 1) {
			if (Debug) {
				buff[0] = c;
				buff[1] = '\0';
				qpage_log(LOG_DEBUG, "got 1 byte: <%s>",
					safe_string(buff));
			}

			if (c != *ptr++)
				ptr = "ID=";

			if (*ptr == '\0') {
				done = TRUE;
				break;
			}
		}

		if (done)
			break;
	}

	if (!done) {
		qpage_log(LOG_NOTICE, "no login prompt from %s", s->name);
		return(F_NOPROMPT);
	}

	if (Debug || Interactive)
		qpage_log(LOG_DEBUG, "logging into %s", s->name);

	write_modem(fd, "\033PG1");
	write_modem(fd, s->password);
	write_modem(fd, "\r");

	gotnak = 0;

	for (;;) {
		HangupDetected = FALSE;

		if ((ptr = getpacket(fd, 10)) == NULL)
			break;

		if (HangupDetected || strstr(ptr, "NO CARRIER")) {
			qpage_log(LOG_NOTICE, "unexpected hangup by %s",
				s->name);

			return(F_NOCARRIER);
		}

		if (strstr(ptr, "\006")) { /* ACK */
			if (Debug || Interactive)
				qpage_log(LOG_DEBUG, "login accepted");

			break;
		}

		if (strstr(ptr, "\025")) { /* NAK */
			/*
			** suggested by Doron Shikmoni, Bar-Ilan University
			*/
			if (gotnak++ > 5) {
				qpage_log(LOG_NOTICE,
					"too many login attempts for %s",
					s->name);

				return(F_UNKNOWN);
			}

			if (Debug || Interactive)
				qpage_log(LOG_DEBUG, "login requested again");

			write_modem(fd, "\033PG1");
			write_modem(fd, s->password);
			write_modem(fd, "\r");
		}

		if (strstr(ptr, "\033\004")) { /* ESC-EOT */
			qpage_log(LOG_NOTICE, "forced disconnect from %s",
				s->name);

			return(F_FORCED);
		}
	}

	if (!ptr) {
		qpage_log(LOG_NOTICE, "login failed (timeout waiting for ACK)");
		return(F_UNKNOWN);
	}

	if (!lookfor(fd, "\033[p", 5)) {
		qpage_log(LOG_NOTICE, "no go-ahead from %s", s->name);
		return(F_UNKNOWN);
	}

	if (Debug || Interactive)
		qpage_log(LOG_DEBUG, "%s says ok to proceed", s->name);

	return(0);
}


/*
** remote_disconnect()
**
** Tell the paging terminal we're all done with this session, then
** hang up the modem.
**
**	Input:
**		s - the paging service to disconnect
**
**	Returns:
**		nothing
*/
void
remote_disconnect(int fd, service_t *s)
{
	char		*ptr;


	if (Debug || Interactive)
		qpage_log(LOG_DEBUG, "logging out from %s", s->name);

	write_modem(fd, "\004\r");

	do {
		ptr = getpacket(fd, 5);

		if (ptr && strstr(ptr, "\036")) { /* RS */
			qpage_log(LOG_WARNING, "warning: %s sent <RS>",
				s->name);
		}
	}
	while (ptr && !strstr(ptr, "\033\004"));
}


/*
** xmit_message()
**
** Transmit a page to the remote paging terminal.  This function sends
** an entire message (breaking it into multi-page pieces if necessary)
** to one recipient.
**
**	Input:
**		job - the structure containing the job to transmit
**
**	Returns:
**		an integer status code:
**			 0 = success, send another page if desired
**			 1 = success, hang up now
**			-1 = failure, page not accepted
*/
int
xmit_message(int fd, job_t *job)
{
	char	*buf;
	char	*field1;
	char	*field2;
	char	*message;
	char	*chunk;
	char	*response;
	char	*countp;
	char	*ptr;
	char	*from;
	int	retries;
	int	status;
	int	max_size;
	int	msghdrlen;
	int	field1len;
	int	field2len;
	int	parts;


	field1len = strlen(job->pager->pagerid) + 2;
	field2len = job->service->maxmsgsize + 2;

	field1 = (void *)malloc(field1len);
	field2 = (void *)malloc(field2len);
	buf = (void *)malloc(field1len + field2len + 10);

	if (job->p->from == NULL) {
		if (job->service->identfrom && job->p->ident)
			from = strdup(job->p->ident);
		else
			from = NULL;
	}
	else
		from = strdup(job->p->from);

	/*
	** Did the administrator turn off prefixes?
	*/
	if (job->service->msgprefix == FALSE)
		from = NULL;

	/*
	** If "from" is an e-mail address, only use the userid part
	** for the message prefix (i.e. chop off the hostname).
	*/
	if (from && (ptr = strchr(from, '@')) != NULL)
		*ptr = '\0';

	(void)sprintf(field1, "%c%s", CHAR_STX, job->pager->pagerid);

	if (from)
		(void)sprintf(field2, "%s:0: ", from);
	else
		(void)strcpy(field2, "0: ");

	msghdrlen = strlen(field2);
	countp = field2 + msghdrlen - 3;

	max_size = job->service->maxmsgsize - msghdrlen - field1len;
	message = job->p->message;

	parts = 0;
	chunk = message;

	/*
	** sanity check
	*/
	if (max_size < 10) {
		qpage_log(LOG_ERR, "maxmsgsize too small");
		status = -1;
		chunk = NULL;
	}

	response = NULL;

	while (chunk) {
		/*
		** increment the message count indicator
		*/
		(*countp)++;
		parts++;

		chunk = msgcpy(field2+msghdrlen, chunk, max_size);

		if (!chunk) {
			if (*countp == '1') {
				/*
				** The whole message fits in one page
				** so let's rewrite field2 to get rid
				** of the split-message indicator.
				*/
				if (from)
					(void)sprintf(field2, "%s: %s", from,
						message);
				else
					(void)strcpy(field2, message);
			}
			else {
				/*
				** this is the last page in a series
				*/
				*countp = 'e';

				/*
				** Tack on "-oo-" at the end of the message,
				** but only if it fits in the remaining space.
				** We didn't do this earlier because we don't
				** want to send a whole extra page just for
				** the -oo- indicator.
				*/
				if (job->service->maxmsgsize-strlen(field2) > 5)
					(void)strcat(field2, " -oo-");
			}
		}

		if (chunk && (*countp == ('0' + job->service->maxpages))) {
			/*
			** There is more to this message but this
			** is the last piece we're allowed to send.
			** We'll change the split-message indicator
			** to a 't' so they know it's a truncated
			** message.
			*/
			*countp = 't';
			chunk = NULL;
		}

		(void)sprintf(buf, "%s\r%s\r%c", field1, field2, CHAR_ETX);

		(void)strcat(buf, checksum(buf));
		(void)strcat(buf, "\r");

		write_modem(fd, buf);

		status = -1; /* assume failure */
		retries = 5;
		response = NULL;
		do {
			HangupDetected = FALSE;

			if ((ptr = getpacket(fd, 15)) == NULL)
				break;

			/*
			** Make sure they didn't hang up on us.  If they
			** did, treat it as a forced-disconnect.
			*/
			if (HangupDetected || strstr(ptr, "NO CARRIER")) {
				qpage_log(LOG_ERR, "unexpected hangup by remote");
				status = 1;
				break;
			}

			if (strstr(ptr, "\006")) { /* ACK */
				if (Debug || Interactive)
					qpage_log(LOG_DEBUG, "message accepted");

				status = 0;
				break;
			}

			if (strstr(ptr, "\025")) { /* NAK */
				if (Debug || Interactive)
					qpage_log(LOG_DEBUG, "message requested again");

				if (--retries == 0) {
					qpage_log(LOG_ERR, "too many retries");
					break;
				}

				write_modem(fd, buf);
				status = -1;
				continue;
			}

			if (strstr(ptr, "\036")) { /* RS */
				if (Debug || Interactive)
					qpage_log(LOG_DEBUG, "message rejected");

				status = -1;
				break;
			}

			if (strstr(ptr, "\033\004")) { /* ESC-EOT */
				if (Debug || Interactive)
					qpage_log(LOG_DEBUG, "message not accepted, disconnect now");

				status = 1;
				break;
			}

			/*
			** This must be an informational message from the
			** paging service.  Trim leading whitespace.
			*/
			while (isspace(*ptr))
				ptr++;

			if (*ptr) {
				my_free(response);
				response = strdup(ptr);
			}
		}
		while (ptr);


		if (!ptr) {
			if (Debug || Interactive)
				qpage_log(LOG_DEBUG, "no valid response from %s", job->service->name);

			status = -1;
			break;
		}

		/*
		** It's a fatal error if the status is non-zero.  Abort
		** the rest of this page.
		*/
		if (status)
			break;
	}

	switch (status) {
		case 0:
			qpage_log(LOG_NOTICE,
				"page delivered, id=%s, from=%s, to=%s, parts=%d",
				job->p->messageid, from ? from : "[anonymous]",
				job->rcpt->pager, parts);
			break;

		case 1:
			qpage_log(LOG_NOTICE,
				"page status unknown, id=%s, from=%s, to=%s, parts=%d : %s",
				job->p->messageid, from ? from : "[anonymous]",
				job->rcpt->pager, parts,
				response ? response : "<NULL>");
			break;

		default:
			qpage_log(LOG_NOTICE,
				"page failed, id=%s, from=%s, to=%s, parts=%d : %s",
				job->p->messageid, from ? from : "[anonymous]",
				job->rcpt->pager, parts,
				response ? response : "<NULL>");
			break;
	}

	free(field1);
	free(field2);
	free(buf);

	my_free(response);
	my_free(from);

	return(status);
}


/*
** send_pages()
**
** Send all the pages in a page list.
**
**	Input:
**		jobs - an ordered linked list of jobs to send
**
**	Returns:
**		nothing
*/
void
send_pages(job_t *jobs)
{
	service_t	*service;
	modem_t		*modem;
	job_t		*tmp;
	char		*device;
	char		*ptr;
	char		*last;
	char		*res;
	int		fd;
	int		i;


	service = NULL;
	device = NULL;
	res = NULL;
	fd = -1;

	while (jobs) {
		jobs->rcpt->tries++;
		jobs->rcpt->lasttry = time(NULL);
		jobs->rcpt->flags &= ~(CALLSTATUSFLAGS);

		/*
		** Initialize the modem and log into the paging terminal.
		*/
		if ((device == NULL) || (service != jobs->service)) {
			if (Debug || Interactive)
				qpage_log(LOG_DEBUG, "new service: %s",
					jobs->service->name);

			/*
			** If we're connected to a paging service then we
			** need to let them know we're finished before we
			** can call the next one.
			*/
			if (device) {
				remote_disconnect(fd, service);
				hangup_modem(fd);
				unlock_modem(device);
				device = NULL;
			}

			service = jobs->service;
			res = my_realloc(res, strlen(service->device)+1);
			ptr = safe_strtok(service->device, ",", &last, res);

			do {
				modem = lookup(Modems, ptr);

				if (modem)
					device = modem->device;
				else
					device = ptr;

				if (device[0] != '/') {
					qpage_log(LOG_ERR, "%s: no such "
						"modem", device);
				}
				else
					fd = openmodem(device);

				ptr = safe_strtok(NULL, ",", &last, res);
			}
			while (fd < 0 && ptr);

			/*
			** If openmodem() failed, move on to the next job.
			*/
			if (fd < 0 || init_tty(fd, service) < 0) {
				if (fd >= 0) {
					closemodem(fd);
					unlock_modem(device);
				}

				device = NULL;
				jobs->rcpt->flags |= F_NOMODEM;
				tmp = jobs;
				jobs = jobs->next;
				free(tmp);
				continue;
			}

			i = remote_connect(fd, service, modem);

			if (i) {
				/*
				** If we failed for some reason other
				** than resetting the modem or getting
				** a busy signal, it's possible we are
				** paying for a phone call.  Bump up
				** the "good" try counter so we don't
				** get stuck in an endless loop racking
				** up phone charges.
				*/
				if ((i & (F_BUSY|F_NOMODEM)) == 0)
					jobs->rcpt->goodtries++;

				jobs->rcpt->flags |= i;
				hangup_modem(fd);
				unlock_modem(device);
				device = NULL;
				tmp = jobs;
				jobs = jobs->next;
				free(tmp);
				continue;
			}
		}

		/*
		** we're definitely using the phone line at this point
		*/
		jobs->rcpt->goodtries++;

		switch (xmit_message(fd, jobs)) {
			case 0: /* success */
				jobs->rcpt->flags |= F_SENT;
				tmp = jobs;
				jobs = jobs->next;
				free(tmp);
				break;


			case 1: /* forced disconnect, hang up now */

				/*
				** If we got a forced disconnect, it
				** probably means we've exceeded the
				** paging service's maximum pages per
				** phone call.  There might be more jobs
				** in the job list for this particular
				** service.  If we set the F_FORCED flag
				** now and move on, we will have the
				** unexpected result of having every Nth
				** page skipped to be retried on the
				** next queue run.  To avoid this yucky
				** behavior, if this is the first time
				** we've tried this page during this
				** queue run we will keep it at the
				** current position in the job list.
				** Otherwise, it's a real failure and
				** we should move on to the next job.
				**
				** Thanks to Keith Parks for suggesting
				** this fix.
				*/
				if (jobs->rcpt->flags & F_FORCED) {
					tmp = jobs;
					jobs = jobs->next;
					free(tmp);
				}
				else {
					jobs->rcpt->flags |= F_FORCED;
				}

				hangup_modem(fd);
				unlock_modem(device);
				device = NULL;
				break;


			default: /* failure, page not accepted */
				jobs->rcpt->flags |= F_REJECT;
				tmp = jobs;
				jobs = jobs->next;
				free(tmp);
				break;
		}


		/*
		** If this was the last job, tell the paging terminal
		** we're finished.
		*/
		if (device && jobs == NULL) {
			remote_disconnect(fd, service);
			hangup_modem(fd);
			unlock_modem(device);
			device = NULL;
		}
	}

	my_free(res);
}
