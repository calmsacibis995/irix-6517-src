#include	"qpage.h"


#ifndef lint
static char	sccsid[] = "@(#)readmail.c  1.15  05/16/98  tomiii@qpage.org";
#endif


/*
** parseaddr()
**
** Parse an e-mail address.  The two forms of addresses that this function
** will accept are:
**
**	[some stuff] "<" user@host ">" [more stuff]
** and
**	["(" some stuff ")"] user@host ["(" more stuff ")"]
**
**
**	Input:
**		line - the line to parse
**
**	Returns:
**		a pointer to the e-mail address
**
**	Note:
**		The parsed e-mail address is returned in a buffer allocated
**		via malloc().  It is the caller's responsibility to free
**		this memory when it is no longer needed.
*/
char *
parseaddr(char *line)
{
	char	*buff;
	char	*ptr;
	char	*tmp;
	int	incomment;
	int	inquote;
	int	bracketaddr;
	int	done;
	int	i;


	i = strlen(line);
	buff = (void *)malloc(i+1);
	tmp = buff;


	done = 0;
	inquote = 0;
	incomment = 0;
	bracketaddr = 0;
	for (ptr=line; *ptr; ) {
		switch (*ptr) {
			case '\\':
				ptr++;
				break;

			case '"':
				inquote = !inquote;
				break;

			case '(':
				if (!inquote) {
					incomment++;

					/*
					** skip over the comment
					*/
					while (*ptr && incomment) {
						if (*ptr == '(') {
							incomment++;
							ptr++;
							continue;
						}

						if (*ptr == ')')
							incomment--;
						else
							ptr++;
					}

					*tmp++ = ' ';
					ptr++;
					continue;
				}
				break;

			case '<':
				if (!inquote) {
					bracketaddr = 1;
					tmp = buff;
					ptr++;
					continue;
				}
				break;

			case '>':
				if (!inquote && bracketaddr)
					done = 1;
				break;
		}

		if (done)
			break;

		*tmp++ = *ptr++;
	}

	*tmp-- = '\0';

	/*
	** trim trailing whitespace
	*/
	while (isspace(*tmp) && tmp >= buff)
		*tmp-- = '\0';


	/*
	** trim leading whitespace
	*/
	ptr = buff;
	while (*ptr && isspace(*ptr))
		ptr++;

	if (buff != ptr)
		(void)strcpy(buff, ptr);

	return(buff);
}


/*
** getheader()
**
** This function reads an RFC-822 message header from the specified
** file pointer.  Folded lines of any length are handled correctly.
**
**	Input:
**		fp - the file pointer to read from
**		unwrap - whether or not to remove embedded newlines
**
**	Returns:
**		the complete message header without the trailing newline
*/
char *
getheader(FILE *fp, int unwrap)
{
	static char	*buf;
	static int	buflen;

	char		*ptr;
	int		len;
	int		c;


	if (feof(fp))
		return(NULL);

	len = 0;
	ptr = buf;

	while (feof(fp) == 0) {
		if (buflen-len < BUFCHUNKSIZE) {
			buf = my_realloc(buf, buflen+BUFCHUNKSIZE);
			buflen += BUFCHUNKSIZE;
			ptr = buf + len;
		}

		c = getc(fp);
		*ptr = c;

		if (c == '\n') {
			/*
			** see if this is the blank line header separator
			*/
			if (ptr == buf)
				break;

			if (unwrap)
				*ptr = '\0';

			/*
			** check for folded lines
			*/
			c = getc(fp);
			if ((c == ' ') || (c == '\t')) {
				if (unwrap) {
					*ptr = ' ';

					while ((c == ' ') || (c == '\t'))
						c = getc(fp);
				}

				ptr++;
				len++;

				(void)ungetc(c, fp);
				continue;
			}

			(void)ungetc(c, fp);
			break;
		}

		ptr++;
		len++;
	}

	*ptr = '\0';

	return(buf);
}


/*
** read_mail()
**
** Read an RFC-822 e-mail message from the standard input and construct
** a page.  The page will be a concatenation of the following items:
**
**	1) The message sender (the "From:" line)
**	2) The subject, enclosed by "(Subj:" and ")"
**	3) The body of the e-mail message
**
** In the event a MIME message is passed as input, only the first part
** encoded as "text/plain" will be processed.  The rest of the message
** will be discarded.  If the message type is "X-sun-attachment" then
** only the from and subject information will be processed; the entire
** message body will be discarded.  I really hate X-sun-attachments.
**
**	Input:
**		p - a pointer to the current PAGE structure
**
**	Returns:
**		nothing (the buck stops here)
*/
void
read_mail(PAGE *p)
{
	char		buff[10240];
	char		*message;
	char		*boundary;
	char		*fromaddr;
	char		*replyaddr;
	char		*subject;
	char		*header;
	char		*type;
	char		*ptr;
	char		*tmp;
	int		inheader;
	int		found_boundary;
	int		got_part;
	int		mime;


	message = NULL;
	boundary = NULL;
	fromaddr = NULL;
	replyaddr = NULL;
	subject = NULL;
	type = NULL;
	mime = FALSE;

	/*
	** Make sure the message starts with a From_ line.
	*/
	if ((header = getheader(stdin, 0)) != NULL && *header) {
		if (strncmp(header, "From ", 5)) {
			fprintf(stderr, "Message does not start with From_\n");
			return;
		}
	}
	else
		return;

	/*
	** Read the message headers from standard input.  Process
	** the ones we care about and throw the rest away.
	*/
	while ((header = getheader(stdin, 0)) != NULL && *header) {
		/*
		** determine the message sender
		*/
		if (strncasecmp(header, "reply-to:", 9) == 0) {
			replyaddr = parseaddr(header+9);
			continue;
		}

		if (!fromaddr && strncasecmp(header, "from:", 5) == 0) {
			header += 5;

			while (isspace(*header))
				header++;

			if (*header)
				fromaddr = strdup(header);

			if (!replyaddr)
				replyaddr = parseaddr(header);

			continue;
		}

		if (strncasecmp(header, "subject:", 8) == 0) {
			header += 8;

			while (isspace(*header))
				header++;

			if (*header)
				subject = strdup(header);

			continue;
		}

		if (strncasecmp(header, "mime-version:", 13) == 0) {
			mime = TRUE;
			continue;
		}

		/*
		** The Content-Type: header needs extra parsing.
		** I really REALLY hate the x-sun-attachment type
		** since MIME is so much better, but popular demand
		** requires that I at least make some attempt to
		** handle it.  Did I mention that I hate mailtool?
		*/
		if (strncasecmp(header, "content-type:", 13) == 0) {
			type = header+13;

			while (isspace(*type))
				type++;

			for (ptr=type; *ptr; ptr++) {
				if (isspace(*ptr) || *ptr == ';') {
					*ptr++ = '\0';
					break;
				}
			}

			if (strcasecmp(type, "text") == 0) {
				type = NULL;
				continue;
			}

			if (strcasecmp(type, "text/plain") == 0) {
				type = NULL;
				continue;
			}

			if (strcasecmp(type, "multipart/mixed") == 0) {
				while (isspace(*ptr))
					ptr++;

				if (strncasecmp(ptr, "boundary=", 9) != 0) {
					fprintf(stderr, "Content-Type is multipart/mixed but no boundary is specified\n");
					continue;
				}

				ptr += 9;
				boundary = (void *)malloc(strlen(ptr)+3);
				tmp = boundary;
				*tmp++ = '-';
				*tmp++ = '-';

				if (*ptr == '"') {
					ptr++;
					while (*ptr && *ptr != '"')
						*tmp++ = *ptr++;
				}
				else {
					while (*ptr && !strchr(" \t;", *ptr))
						*tmp++ = *ptr++;
				}

				*tmp = '\0';
				continue;
			}

			/*
			** I've changed my mind.  Anyone who insists on
			** using the x-sun-attachment type deserves not
			** to have their messages processed correctly.
			** I'll compromise for now and pass on the subject
			** line, but these people really need to grow up
			** and use a real mailer.
			*/
			if (strcasecmp(type, "x-sun-attachment") == 0) {
				fprintf(stderr, "X-sun-attachments are NOT supported--use MIME instead.\n");
				continue;
			}

			if (Debug > 1)
				printf("Unknown content type: <%s>\n", type);
		}
	}

	if (Debug > 1) {
		if (fromaddr)
			printf("Message sender is: <%s>\n", fromaddr);

		if (replyaddr)
			printf("Reply address is: <%s>\n", replyaddr);

		if (subject)
			printf("Message subject is: <%s>\n", subject);
	}

#ifndef EXCLUDE_FROM
	if (fromaddr) {
		message = strjoin(message, fromaddr);
		message = strjoin(message, "\n");
	}
#endif

	if (subject) {
		message = strjoin(message, "(Subj: ");
		message = strjoin(message, subject);
		message = strjoin(message, ")\n");
	}

	found_boundary = FALSE;
	got_part = FALSE;
	while (fgets(buff, sizeof(buff), stdin)) {
		if ((ptr = strchr(buff, '\n')) != NULL)
			*ptr = '\0';

		/*
		** If this is a MIME message, we need to skip to the first
		** part whose content-type is "text/plain".  Hopefully this
		** will stop us from trying to send binaries to pagers.
		*/
		if (mime && boundary) {
			if (strcmp(buff, boundary) == 0) {
				found_boundary = TRUE;
				inheader = TRUE;

				if (Debug > 1)
					printf("Found boundary: <%s>\n",
						boundary);

				/*
				** we only want the first text part
				*/
				if (got_part)
					break;

				continue;
			}

			if (!found_boundary)
				continue;

			if (inheader && strncasecmp(buff, "content-type:", 13) == 0) {
				if (strncasecmp(buff+13, " text", 5)) {
					if (Debug > 1)
						printf("Skipping type=<%s>\n",
							buff+13);

					found_boundary = FALSE;
				}

				continue;
			}

			if (Debug > 1) {
				if (inheader)
					printf("Mimeheader: <%s>\n", buff);
				else
					printf("Mimetext: <%s>\n", buff);
			}

			if (buff[0] == '\0')
				inheader = FALSE;

			if (!inheader) {
				if (buff[0] == '-' && buff[1] == '-')
					break;

				message = strjoin(message, buff);
				message = strjoin(message, "\n");

				got_part = TRUE;
			}

			continue;
		}

		if (Debug > 1)
			printf("Text: <%s>\n", buff);

		if (buff[0] == '-' && buff[1] == '-')
			break;

		message = strjoin(message, buff);
		message = strjoin(message, "\n");
	}

	/*
	** Keep reading until EOF so the writer doesn't get SIGPIPE.
	*/
	while (fgets(buff, sizeof(buff), stdin))
		;

	/*
	** If no "from" address was specified on the command line, set
	** it to the return e-mail address from the message headers.
	*/
	if (p->from == NULL) {
		if (replyaddr)
			p->from = parseaddr(replyaddr);
		else
			p->from = strdup("anonymous");
	}

	p->message = message;

	if (Debug)
		printf("Message is: <%s>\n", p->message);

	return;
}
