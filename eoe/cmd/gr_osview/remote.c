/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * Remote access module.
 *
 * Remote protocol that is used.
 *
 * 1-> local host invokes daemon on reemote host, quits if this fails
 * 2-> local host waits for version string to be sent back, and
 *	verifies that versions match, quits if this fails
 * 3-> local host forks a RPC child to do the remote's wishes and passes
 *	the setup file to the remote host
 * 4-> remote host parses setup file, initializes colorblocks, then
 *	pushes copies to local host.
 * 5-> local host simply continues with colorblock calls as necessary.
 */

# include	"grosview.h"

# include	<sys/time.h>
# include	<math.h>
# include	<string.h>
# include	<setjmp.h>
# include	<signal.h>
# include	<stdlib.h>
# include	<stdio.h>
# include	<errno.h>
# include	<unistd.h>
# include	<gl/device.h>

# define	RMTCMD		"/usr/sbin/gr_osview"
# define	MAXRMTWAIT	30
# define	RETRYWAIT	30
# define	MAXHARDERROR	3
# define	getcb(s)	{int i; if ((i = nextint((char *) s)) < 0) \
					continue; else cb = cbptr(i); }

FILE		*rmtpipe;
FILE		*rmtwrite;
static char	copybuf[RMTBUF];
static char	*pstring;

int		errchan;
FILE		*errfd;

int		pullcbinfo(colorblock_t *);

static int	fdgets(char *, int, int, FILE *, int);
static int	rfgets(char *, int, int);
static int	pullthecb(colorblock_t *, FILE *);
static int	getremoteinfo(void);
static int	contact(char *, char *, char *, char *);
static int	getremote(int);

extern int	rcmd(char **, u_short, char *, char *, char *, int *);

void
remote_text_start(void) {}	/* beginning of text to lock down */

/*
 * This is the RPC daemon.
 */
void
passdata(void)
{
	if (debug)
		printf("RPC daemon starting\n");
	getremoteinfo();
	exit(0);
}

/*
 * For recovery from timeouts.
 */
jmp_buf		nonsense;

void
giveup()
{
	longjmp(nonsense, 1);
}

void
pushoptions(FILE *fd)
{
	if (!barborder)
		fprintf(fd, "=%d %d\n", S_SETOPT, SW_NOBORDER);
	if (arbsize)
		fprintf(fd, "=%d %d\n", S_SETOPT, SW_ARBSIZE);
	if (width)
		fprintf(fd, "=%d %d %d\n", S_SETOPT, SW_WIDTH,
			width);
	if (prefpos)
		fprintf(fd, "=%d %d %d %d\n", S_SETOPT, SW_ORIGIN,
			prefxpos, prefypos);
	if (prefwsize)
		fprintf(fd, "=%d %d %d %d\n", S_SETOPT,SW_WINSIZE,
			prefxsize, prefysize);
	if (fontface) {
		fprintf(fd, "=%d %d\n", S_SETOPT, SW_FONT);
		fprintf(fd, "%s\n", fontface);
	}
	if (interval)
		fprintf(fd, "=%d %d %d\n", S_SETOPT, SW_INTERVAL,
			interval);
	fprintf(fd, "=%d %d %d\n", S_SETOPT, SW_BACKCOLOR,
		backcolor);
	fprintf(fd, "=%d %d %d\n", S_SETOPT, SW_FRONTCOLOR,
		frontcolor);
}

void
doanoption(FILE *fd) {
   int		tmp1;
   int		tmp2;
   int		i;
   char		sbuf[RMTBUF];

	if ((i = nextint((char *) 0)) < 0)
		return;
	switch (i) {
	case SW_NOBORDER:
		barborder = 0;
		break;
	case SW_WIDTH:
		if ((tmp1 = nextint((char *) 0)) < 0)
			return;
		width = tmp1;
		break;
	case SW_ARBSIZE:
		arbsize = 1;
		break;
	case SW_ORIGIN:
		if ((tmp1 = nextint((char *) 0)) < 0)
			return;
		if ((tmp2 = nextint((char *) 0)) < 0)
			return;
		prefxpos = tmp1;
		prefypos = tmp2;
		prefpos = 1;
		break;
	case SW_WINSIZE:
		if ((tmp1 = nextint((char *) 0)) < 0)
			return;
		if ((tmp2 = nextint((char *) 0)) < 0)
			return;
		prefxsize = tmp1;
		prefysize = tmp2;
		prefwsize = 1;
		break;
	case SW_FONT:
		if (fdgets(sbuf, RMTBUF, 0, fd, MAXRMTWAIT) != 0) {
			fontface = strdup(sbuf);
			if (fontface[strlen(fontface)-1] == '\n')
				fontface[strlen(fontface)-1] = '\0';
		}
		break;
	case SW_BACKCOLOR:
		if ((tmp1 = nextint((char *) 0)) < 0)
			return;
		backcolor = tmp1;
		break;
	case SW_FRONTCOLOR:
		if ((tmp1 = nextint((char *) 0)) < 0)
			return;
		frontcolor = tmp1;
		break;
	case SW_INTERVAL:
		if ((tmp1 = nextint((char *) 0)) < 0)
			return;
		interval = tmp1;
		break;
	}
}

static int
pullthecb(colorblock_t *cb, FILE *nfd) {
   int		c;
   int		i;
   char		buf[RMTBUF];

	if (debug)
		fprintf(stderr, "Retrieving colorblock %d\n", cbindex(cb));
	cbresetbar(cb);

	/*
	 * In case he goes away.
	 */
	if (setjmp(nonsense)) {
		if (debug)
			fprintf(stderr, "pullthecb: timeout\n");
		return(1);
	}
	alarm(MAXRMTWAIT);

	/*
	 * Get the strings.
	 */
	if (pullcbinfo(cb))
		return(1);
	if ((c = getc(nfd)) == 'n') {
		if (fdgets(buf, RMTBUF, 0, nfd, MAXRMTWAIT) == 0)
			return(1);
		if (scriptfd) {
			fputc(c, scriptfd);
			fputs(buf, scriptfd);
		}
		buf[strlen(buf)-1] = '\0';
		cb->cb_name = strdup(buf);
		c = getc(nfd);
	}
	if (c == 'h') {
		if (fdgets(buf, RMTBUF, 0, nfd, MAXRMTWAIT) == 0)
			return(1);
		if (scriptfd) {
			fputc(c, scriptfd);
			fputs(buf, scriptfd);
		}
		buf[strlen(buf)-1] = '\0';
		cb->cb_header = strdup(buf);
		c = getc(nfd);
	}
	if (c == 'l') {
		i = 0;
		do {
			if (fdgets(buf, RMTBUF, 0, nfd, MAXRMTWAIT) == 0)
				return(1);
			if (scriptfd) {
				fputc(c, scriptfd);
				fputs(buf, scriptfd);
			}
			buf[strlen(buf)-1] = '\0';
			cb->cb_legend[i] = strdup(buf);
			i++;
		} while ((c = getc(nfd)) == 'l');
	}
	if (c == EOF)
		return(1);
	ungetc(c, nfd);
	alarm(0);
	return(0);
}

int
pulldraw(colorblock_t *cb)
{
   double	fa[MAXSECTS];
   int		i;

	for (i = 0; i < cb->cb_nsects; i++)
		if ((fa[i] = nextfloat((char *) 0)) < 0)
			fa[i] = 0;
	/*
	if (debug) {
		printf("cbdraw(");
		for (i = 0; i < cb->cb_nsects; i++)
			printf("%.3f,", fa[i]);
		printf(")\n");
	}
	*/
	CBDRAW(cb, fa[0], fa[1], fa[2], fa[3], fa[4],
		fa[5], fa[6], fa[7], fa[8], fa[9]);
	return(0);
}

/*
 * Auto detect of a script file. Set up from it.
 */
int
scanscript(FILE *fd) {
   char		mbuf[RMTBUF];
   char		*s;
   int		type;
   int		i;
   colorblock_t	*cbp;

	while (sgets(mbuf, RMTBUF, fd) != NULL) {
		if (debug)
			fprintf(stderr, "scanscript: received %s", mbuf);
		if (*mbuf != '=')
			return(1);
		if ((type = nextint(mbuf + 1)) < 0)
			return(1);
		switch (type) {
		case S_LAYOUT:
			if (sgets(mbuf, RMTBUF, fd) == NULL)
				return(1);
			sname = strdup(mbuf);
			if ((s = strchr(sname, '\n')) != NULL)
				*s = '\0';
			goto doit;
		case S_NSLAYOUT:
			goto doit;
		case S_SETOPT:
			doanoption(fd);
			break;
		case S_PUSHCB:
			if ((i = nextint((char *) 0)) < 0)
				return(1);
			if (pullthecb(cbinitindex(i), fd))
				return(1);
			cbp = cbptr(i);
			switch (cbp->cb_btype) {
			case bdisk:
				if (cbp->cb_name == 0)
					break;
				if ((cbp->cb_disk = diskinfo(cbp->cb_name)) < 0)
					return(1);
				break;
			case bnetip:
				if (cbp->cb_name == 0)
					break;
				if ((cbp->cb_subtype=netifinfo(cbp->cb_name))
					< 0)
					return(1);
				gi_if = 1;
				break;
			case bnetudp:
				gi_udp = 1;
				break;
			case bnetipr:
				gi_ip = 1;
				break;
			case bnet:
				gi_tcp = 1;
				break;
			case bswps:
				gi_swap = 1;
				break;
			}
			break;
		case S_TLIMIT: {
		   int		cb;
		   float	tlim;

			if ((cb = nextint((char *) 0)) < 0)
				return(1);
			if ((tlim = nextfloat((char *) 0)) < 0)
				return(1);
			if (debug)
				fprintf(stderr, "scanscript: tlimit %d %f\n",
					cb, tlim);
			cbptr(cb)->cb_tlimit = tlim;
			if (logfd)
				fprintf(logfd, "=%d %d %f\n", S_TLIMIT, cb,
					tlim);
			break;
		}
		}
	}
	return(1);
    doit:
	return(0);
}

void
startremote(void)
{
	fprintf(rmtwrite, "=%d\n", S_GO);
	fflush(rmtwrite);
}

void
waitstart(void)
{
   char		rbuf[RMTBUF];
   int		type;
   colorblock_t	*cb;
   int		i;

	while (sgets(rbuf, RMTBUF, stdin) != NULL) {
		if (*rbuf != '=') {
			fprintf(stderr, "%s: protocol error with master\n",
				pname);
			exit(1);
		}
		if ((type = nextint(rbuf+1)) == S_GO)
			return;
		else if (type < 0)
			exit(1);
		else if (type == S_TLIMIT) {
			if ((i = nextint((char *) 0)) < 0)
				continue;
			else
				cb = cbptr(i);
			cb->cb_tlimit = nextfloat((char *) 0);
		}
		else if (debug)
			printf("waitstart: bad message\n");
	}
	exit(1);
}

/*
 * Called to wait for and read the next remote data packet.
 */

static int
getremoteinfo(void)
{
   char		pbuf[RMTBUF];
   char		sbuf[RMTBUF];
   char		*buf;
   int		type;
   int		i;
   colorblock_t	*cb;
   int		harderror = 0;

	if (scriptfd) {
		fprintf(scriptfd, "=%d %d %d\n", S_SETOPT, SW_INTERVAL,
			interval);
	}
    tryagain:
	while (rfgets(pbuf, RMTBUF, 0) != 0) {
		/*
		 * See if any interesting events have happened.
		 */
		event_scan(1);

		/*
		 * If not a remote protocol packet, just print it
		 * on the output.
		 */
		if (*pbuf != '=') {
			if (!debug)
				printf(pbuf);
			continue;
		}
		buf = pbuf + 1;

		/*
		 * Figure out what the guy wants, and do it.
		 */
		if ((type = nextint(buf)) < 0)
			continue;
		if (debug && (type != S_DRAW && type != S_PAUSE))
			printf("executing RPC command type %d\n", type);
		switch (type) {
		/*
		 * Sleeping for the next interval.
		 */
		case S_PAUSE:
			if (logfd)
				fprintf(logfd, "=%d\n", S_PAUSE);
			break;
		/*
		 * Draw the values for a bar.
		 */
		case S_DRAW: {
			harderror = 0;
			getcb(0);
			if (pulldraw(cb))
				goto blam;
			break;
		}
		/*
		 * Draw the legend.
		 */
		case S_DRAWLEGEND:
			getcb(0);
			CBDRAWLEGEND(cb);
			break;
		/*
		 * Modify tlimit value.
		 */
		case S_TLIMIT:
			if ((i = nextint((char *) 0)) < 0)
				return(1);
			else
				cb = cbptr(i);
			cb->cb_tlimit = nextfloat((char *) 0);
			if (scriptfd)
				fprintf(scriptfd, "=%d %d %.3f\n", S_TLIMIT,
					i, cb->cb_tlimit);
			if (logfd)
				fprintf(logfd, "=%d %d %.3f\n", S_TLIMIT,
					i, cb->cb_tlimit);
			break;
		/*
		 * Get a colorblock to use.
		 */
		case S_INIT:
			cb = CBINIT();
			fprintf(rmtwrite, "%d %d\n", S_INITREPLY, cbindex(cb));
			fflush(rmtwrite);
			break;
		/*
		 * Put a message in the box.
		 */
		case S_MESSAGE:
			getcb(0);
			if (rfgets(sbuf, RMTBUF, 0) != 0)
				CBMESSAGE(cb, sbuf);
			else
				goto blam;
			break;
		/*
		 * Layout the window.
		 */
		case S_LAYOUT:
			if (rfgets(sbuf, RMTBUF, 0) != 0) {
				sname = strdup(sbuf);
				reinitbars();
			}
			else
				goto blam;
			if (scriptfd) {
				fprintf(scriptfd, "=%d\n", S_LAYOUT);
				fputs(sbuf, scriptfd);
			}
			startremote();
			break;
		/*
		 * No title window layout.
		 */
		case S_NSLAYOUT:
			if (scriptfd)
				fprintf(scriptfd, "=%d\n", S_NSLAYOUT);
			reinitbars();
			startremote();
			break;
		/*
		 * Handle option switches.
		 */
		case S_SETOPT:
			if (scriptfd) {
				fprintf(scriptfd, "=%d ", S_SETOPT);
				fputs(pstring, scriptfd);
			}
			doanoption(rmtpipe);
			break;
		/*
		 * Shut up and go home.
		 */
		case S_TAKEDOWN:
			cbtakedown();
			break;
		/*
		 * Read back a color bar from the other side and set
		 * up the local one.
		 */
		case S_PUSHCB:
			if (scriptfd) {
				fprintf(scriptfd, "=%d ", S_PUSHCB);
				fputs(pstring, scriptfd);
			}
			if ((i = nextint((char *) 0)) < 0)
				return(1);
			if (pullthecb(cbptr(i), rmtpipe))
				goto blam;
			break;
		}
	}
	i = 0;
    blam:
	{
	   char			mbuf[RMTBUF];
	   static FILE		*stash = 0;
	   colorblock_t		*cbp;

		fclose(rmtpipe);
		fclose(rmtwrite);
		if (!cbptr(0) || cbptr(0)->cb_strwid == 0) {
			printf("%s: Lost connection to remote host.\n",
				pname);
			exit(1);
		}
		if (++harderror > MAXHARDERROR) {
			printf("%s: Can't re-contact remote host.\n", pname);
			exit(1);
		}
		sprintf(mbuf, "Lost connection to remote host; %d retries left",
				MAXHARDERROR - harderror);
		CBMESSAGE(cbptr(0), mbuf);
		i = 1;
		event_scan(1);
		while (callremote(1)) {
			event_scan(1);
			if (cbptr(0)) {
				sprintf(mbuf,
				"Can't establish connection: Retry %d", i);
				CBMESSAGE(cbptr(0), mbuf);
			}
			i++;
		}
		if (cbptr(0)) {
			sprintf(mbuf, "Connection re-established ...");
			CBMESSAGE(cbptr(0), mbuf);
		}
		if (debug)
			printf("sending restart data; contents of file:\n");
		if (stash == 0) {
			stash = scriptfd;
			scriptfd = 0;
		}
		rewind(stash);
		while (fgets(mbuf, RMTBUF, stash) != NULL) {
			/*
			if (debug)
				printf("+ %s", mbuf);
			*/
			fputs(mbuf, rmtwrite);
		}
		for (;;) {
			if (rfgets(pbuf, RMTBUF, 0) == 0)
				goto blam;
			if (*pbuf != '=')
				continue;
			if ((type = nextint(pbuf+1)) < 0)
				continue;
			if (type == S_LAYOUT) {
				if (rfgets(mbuf, RMTBUF, 0) == 0)
					goto blam;
				break;
			}
			if (type == S_NSLAYOUT)
				break;
		}
		cbp = nextbar(1);
		while (cbp != 0) {
			cbresetbar(cbp);
			cbp = nextbar(0);
		}
		drawbarreset();
		CBLAYOUT(0);
		CBREDRAW();
		startremote();
		goto tryagain;
	}
}


/*
 * Push the colorbar entries back over the wire.
 */
# define	flout(x)	fprintf(fd, " %.3f", cbp->cb_ ##x)
# define	hxout(y)	fprintf(fd, " %#x", cbp->cb_ ##y)
# define	flayout(x)	{int i; \
				 for (i=0;i<cbp->cb_nsects;i++) \
				 flout(x[i]); }
# define	hxayout(x)	{int i; \
				 for (i=0;i<cbp->cb_nsects;i++) \
				 hxout(x[i]); }
# define	flin(x)		if ((cbp->cb_ ##x = nextfloat((char *) 0))<0)\
					return(1);
# define	hxin(x)		if ((int)(cbp->cb_ ##x = nextint((char *) 0))<0)\
					return(1);
# define	flayin(x)	{int i; \
				 for (i=0;i<cbp->cb_nsects;i++) \
				 flin(x[i]); }
# define	hxayin(x)	{int i; \
				 for (i=0;i<cbp->cb_nsects;i++) \
				 hxin(x[i]); }

void
pushcbinfo(colorblock_t *cbp, FILE *fd) {
	hxout(flags);
	hxout(type);
	hxout(interval);
	hxout(count);
	flout(tlimit);
	flout(upmove);
	hxout(nsects);
	hxout(nmask);
	hxout(avgtick);
	hxout(avgcnt);
	hxout(maxtick);
	hxout(maxcnt);
	hxout(nsamp);
	hxout(tick);
	hxout(bigtick);
	hxout(limcol.back);
	hxout(limcol.front);
	hxout(maxcol.back);
	hxout(maxcol.front);
	hxout(sumcol.back);
	hxout(sumcol.front);
	hxout(interval);
	hxout(count);
	hxout(subtype);
	hxout(disk);
	hxout(btype);
	hxayout(colors);
}

int
pullcbinfo(colorblock_t *cbp) {
	hxin(flags);
	hxin(type);
	hxin(interval);
	hxin(count);
	flin(tlimit);
	flin(upmove);
	hxin(nsects);
	hxin(nmask);
	hxin(avgtick);
	hxin(avgcnt);
	hxin(maxtick);
	hxin(maxcnt);
	hxin(nsamp);
	hxin(tick);
	hxin(bigtick);
	hxin(limcol.back);
	hxin(limcol.front);
	hxin(maxcol.back);
	hxin(maxcol.front);
	hxin(sumcol.back);
	hxin(sumcol.front);
	hxin(interval);
	hxin(count);
	hxin(subtype);
	hxin(disk);
	hxin(btype);
	hxayin(colors);
	return(0);
}

void
pushbackcb(FILE *fd)
{
   colorblock_t	*cp;
   int		i;

	if (fd == 0)
		fd = stdout;
	cp = nextbar(1);
	while (cp != 0) {
		fprintf(fd, "=%d %d ", S_PUSHCB, cp->cb_rb);
		if (debug)
			fprintf(stderr, "pushbackcb: %d\n", cp->cb_rb);
		pushcbinfo(cp, fd);
		fprintf(fd, "\n");
		if (cp->cb_name != 0)
			fprintf(fd, "n%s\n", cp->cb_name);
		if (cp->cb_header != 0)
			fprintf(fd, "h%s\n", cp->cb_header);
		for (i = 0; i < cp->cb_nsects; i++)
			fprintf(fd, "l%s\n", cp->cb_legend[i]);
		cp = nextbar(0);
	}
}

/*
 * Get back an address to use for an init request.
 */
int
rgetcb(void)
{
   char		buf[RMTBUF];
   int		type;

	if (fgets(buf, RMTBUF, stdin) == NULL) {
		fprintf(stderr, "%s: lost connection to master\n", pname);
		exit(1);
	}
	if ((type = nextint(buf)) < 0 || type != S_INITREPLY) {
		fprintf(stderr, "%s protocol error with master\n", pname);
		exit(1);
	}
	return(nextint((char *) 0));
}

/*
 * Remote handlers.
 */

int
nextint(char *buf)
{
   char		*tok;
   int		val;

	if ((tok = strtok(buf, " \n")) == NULL)
		return(-1);
	if (buf != 0)
		pstring = buf + strlen(tok) + 1;
	else
		pstring += strlen(tok) + 1;
	val = strtol(tok, (char **) 0, 0);
	return(val);
}

char *
nextptr(char *buf)
{
   char		*tok;
   char 	*val;

	if ((tok = strtok(buf, " \n")) == NULL)
		return(0);
	if (buf != 0)
		pstring = buf + strlen(tok) + 1;
	else
		pstring += strlen(tok) + 1;
	val = (char *) strtol(tok, (char **) 0, 0);
	return(val);
}

double
nextfloat(char *buf)
{
   char		*tok;
   double	val;

	if ((tok = strtok(buf, " \n")) == NULL)
		return(-1);
	val = atof(tok);
	return(val);
}

static int
rfgets(char *b, int l, int wait)
{
	return(fdgets(b, l, wait, rmtpipe, MAXRMTWAIT));
}

static int
fdgets(char *b, int l, int wait, FILE *fd, int timeo)
{
	if (!wait) {
		if (setjmp(nonsense)) {
			if (debug)
				printf("fdgets: timeout\n");
			return(0);
		}
		alarm(timeo);
	}

	if (fgets(b, l, fd) == NULL) {
		if (debug)
			printf("fdgets: null read from remote, err = %d\n",
				errno);
		return(0);
	}
	if (!wait)
		alarm(0);

	if (debug) {
	   char		*s;
	   char		*prefix;

		if ((b[1] == '3' && b[2] == ' ') ||
		    (b[1] == '1' && b[2] == '5' && b[3] == '\n'))
			return(1);
		strcpy(copybuf, b);
		if ((s = strchr(copybuf, '\n')) != NULL)
			*s = '\0';
		prefix = (slave ? "*RR> " : "*LR> ");
		printf("%s %s\n", prefix, copybuf);
	}
	return(1);
}

void
remote_text_end(void) {}	/* end of text to lock down */

/*
 * Build a remote command line from the options.
 */
static char *
mkrmtcmd(void)
{
   static char	cmdbuf[256];
   char *rmtcmd;

	if ((rmtcmd = getenv("GROSVIEWCMD")) && *rmtcmd)
		strcpy(cmdbuf, rmtcmd);
	else
		strcpy(cmdbuf, RMTCMD);
	strcat(cmdbuf, " -R -D-");
	if (debug)
		strcat(cmdbuf, " -d");
	return(cmdbuf);
}

/*
 * Try to contact the remote and set up the daemon; fail if not possible.
 */
int
callremote(int x)
{
   char		user[100];
   char		host[100];
   char		luser[L_cuserid];
   char		*cmd;
   char		*tmp;
   int		rfd;
   int		sfd;
   int		grcode;

	sigset(SIGALRM, giveup);
	cmd = mkrmtcmd();
	if ((tmp = strchr(remotehost, '@')) == NULL) {
		strncpy(host, remotehost, 100);
		strcpy(user, "guest");
	}
	else {
		strncpy(host, tmp+1, 100);
		strncpy(user, remotehost,
			(tmp-remotehost>100?100:tmp-remotehost));
		user[tmp-remotehost] = '\0';
	}
	if (cuserid(luser) == NULL) {
		fprintf(stderr, "%s: can't identify you!\n", pname);
		exit(1);
	}
	if (debug)
		printf("<execute> %s\n", cmd);
	if (!x)
		printf(" ... attempting to contact %s\n", host);
	if (x) {
		if (setjmp(nonsense))
			return(1);
		alarm(RETRYWAIT);
	}
	if ((rfd = contact(host, user, luser, cmd)) == -1) {
		if (x && errno == EINTR) {
			alarm(0);
			return(1);
		}
		exit(1);
	}
	if (x)
		alarm(0);
	sfd = dup(rfd);
	if ((rmtpipe = fdopen(rfd, "r")) == NULL) {
		fprintf(stderr,
			"%s: Unable to attach standard I/O to remote stream\n",
				pname);
		exit(1);
	}
	/* Really inefficient 
	setbuf(rmtpipe, NULL);
	*/
	if ((rmtwrite = fdopen(sfd, "w")) == NULL) {
		fprintf(stderr,
			"%s: Unable to attach standard I/O to remote stream\n",
				pname);
		exit(1);
	}
	setbuf(rmtwrite, NULL);
	if (debug)
		printf("<channel to remote open>\n");
	if ((grcode = getremote(!x))) {
		if (!x)
			exit(1);
		else if (grcode >= 10) {
			fprintf(stderr,
				"%s: cannot reliably reconnect to remote\n",
				pname);
			fprintf(stderr, "%s: exiting\n", pname);
			exit(1);
		}
		else
			return(1);
	}
	if (!x)
		printf(" ... connection open\n");
	return(0);
}

static int
getremote(int showerr)
{
   char		tbuf[RMTBUF];
   char		buf[RMTBUF];
   void		giveup();
   int		vers = VERS_1;

	if (rfgets(buf, RMTBUF, 0) == 0) {
		if (showerr)
			fprintf(stderr, "%s: read from remote system failed\n",
				pname);
		return(1);
	}
	switch (nextint(buf)) {
	case S_VERSION:
		if ((vers = nextint((char *) 0)) <= 0)
			goto err;
		if (debug)
			printf("getremote: version %ld\n", vers);
		break;
	default:
		if (!debug && showerr)
			fprintf(stderr, "%s: couldn't understand remote\n",
				pname);
		goto err;
	}
	if (vers < myvers) {
		if (showerr) {
			fprintf(stderr,
				"%s: remote version mismatch\n", pname);
			fprintf(stderr,
				"%s: you must be running at least IRIX 4.0\n",
				pname);
			fprintf(stderr, "%s:   on both systems\n", pname);
		}
		return(10);
	}
	if (debug)
		printf("getremote: in sync\n");
	return(0);
    err:
	if (showerr && debug) {
		while (fdgets(tbuf, RMTBUF, 1, errfd, 2) != 0)
			fprintf(stderr, "\"%s\"", tbuf);
		fprintf(stderr, "\n");
	}
	return(10);
}

/*
 * Call the remote system using a nice modern interface through 
 * rexec.
 */

# include	<stdio.h>
# include	<netdb.h>

static int
contact(char *node, char *user, char *luser, char *cmd)
{
   struct servent	*seb;
   int			fd;
   char			nbuf[128];
   char			*nnbuf;
   int			*wanterr = 0;

	if ((seb = getservbyname("shell", "tcp")) == 0) {
		fprintf(stderr, "Unable to contact name server\n");
		return(-1);
	}
	strcpy(nbuf, node);
	nnbuf = nbuf;
	if (debug)
		wanterr = &errchan;
	if (debug)
		printf("contact: host %s port %d luser %s user %s\n", nnbuf,
			seb->s_port, luser, user);
	if ((fd = rcmd(&nnbuf, seb->s_port, luser, user, cmd, wanterr)) == -1) {
		fprintf(stderr, "Unable to invoke remote demon\n");
		return(-1);
	}
	/*
	 * Server to output remote information to local caller.
	 */
	if (debug) {
		errfd = fdopen(errchan, "r");
		setbuf(errfd, NULL);
		if (fork() == 0) {
		   char		buf[RMTBUF];

			fclose(stdin);
			fclose(stdout);
			close(fd);

			while (fgets(buf, RMTBUF, errfd) != NULL) {
				fputs("*RE> ", stderr);
				fputs(buf, stderr);
			}

			exit(0);
		}
	}
	return(fd);
}
