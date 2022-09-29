/*
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Id: record.c,v 2.21 1998/11/15 08:35:24 kenmcd Exp $"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <libgen.h>
#include <sys/resource.h>
#if defined(__linux__)
#define _USE_BSD
#endif
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include "pmapi.h"
#include "impl.h"

/*
 * some extended state, make sure these values are different to
 * the PM_REC_* macros in <pmapi.h>
 */
#define PM_REC_BEGIN	81
#define PM_REC_HOST	82

static int	state = PM_REC_OFF;	/* where you are up to ... */
static char	*dir;		/* directory containing Archive Folio files */
static char	*base;			/* unique basename */
static FILE	*f_folio;		/* Archive Folio goes here */
static FILE	*f_replay;		/* current replay config file */
static int	_replay;		/* can replay? */
static char	*_folio = NULL;		/* remember the folio name */
static char	*_creator = NULL;	/* remember the creator name */
static int	_seendefault;		/* seen default host? */

typedef struct _record {
    struct _record	*next;
    int			state;
    char		*host;		/* host name */
    int			isdefault;	/* is this the default host? */
    pmRecordHost	public;		/* exposed to the caller */
    char		*base;		/* archive base */
    char		*logfile;	/* for -l ... not to be confused */
					/* with public.logfile which is the */
					/* full pathname */
    char		*config;	/* for -c */
    int			argc;
    char		**argv;
} record_t;

static record_t	*record = NULL;
static int	n_record;
static int	n_alive;
static char	tbuf[MAXPATHLEN];	/* used for mktemp(), messages, ... */

extern int	errno;

/*
 * initialize, and retrun stdio stream for writing replay config
 * (if any)
 */
FILE *
pmRecordSetup(const char *folio, const char *creator, int replay)
{
    char	*p;
    char	c;
    time_t	now;
    int		sts;
    int		fd = -1;
    static char	host[MAXHOSTNAMELEN];
    record_t	*rp;

    if (state != PM_REC_OFF) {
	/* already begun w/out end */
	errno = EINVAL;
	return NULL;
    }

    if (access(folio, F_OK) == 0) {
	errno  = EEXIST;
	return NULL;
    }

    if (_folio != NULL)
	free(_folio);
    if ((_folio = strdup(folio)) == NULL)
	return NULL;

    if (_creator != NULL)
	free(_creator);
    if ((_creator = strdup(creator)) == NULL)
	return NULL;

    if ((f_folio = fopen(folio, "w")) == NULL)
	return NULL;

    dir = NULL;
    base = NULL;

    /*
     * have folio file created ... get unique base string
     */
    tbuf[0] = '\0';
    if ((p = strrchr(folio, '/')) != NULL) {
	/* folio name contains a slash */
	p++;
	c = *p;
	*p = '\0';
	if (strcmp(folio, "./") != 0) {
	    if (dir != NULL)
		free(dir);
	    if ((dir = strdup(folio)) == NULL)
		goto failed;
	    strcpy(tbuf, dir);
	    strcat(tbuf, "/");
	}
	*p = c;
    }
    strcat(tbuf, "XXXXXX");
    mktemp(tbuf);

    if ((fd = open(tbuf, O_CREAT | O_EXCL | O_RDWR, 0644)) < 0)
	goto failed;

    if (dir == NULL)
	p = tbuf;
    else
	p =  strrchr(tbuf, '/') + 1;

    if ((base = strdup(p)) == NULL)
	goto failed;

    /*
     * folio preamble ...
     */
    fprintf(f_folio, "PCPFolio\nVersion: 1\n");
    fprintf(f_folio, "# use pmafm(1) to process this PCP Archive Folio\n#\n");
    time(&now);
    (void)gethostname(host, MAXHOSTNAMELEN);
    host[MAXHOSTNAMELEN-1] = '\0';
    fprintf(f_folio, "Created: on %s at %s", host, ctime(&now));
    fprintf(f_folio, "Creator: %s", creator);
    if (replay)
	fprintf(f_folio, " %s", base);
    fprintf(f_folio, "\n#               Host                    Basename\n#\n");

    _replay = replay;
    if (_replay) {
	f_replay = fdopen(fd, "w");
	if (f_replay == NULL)
	    goto failed;
    }
    else
	f_replay = fopen("/dev/null", "r");

    n_record = 0;
    for (rp = record; rp != NULL; rp = rp->next) {
	rp->state = PM_REC_BEGIN;
	rp->isdefault = 0;
	if (rp->host != NULL) {
	    free(rp->host);
	    rp->host = NULL;
	}
	if (rp->base != NULL) {
	    free(rp->base);
	    rp->base = NULL;
	}
	if (rp->logfile != NULL) {
	    free(rp->logfile);
	    rp->logfile = NULL;
	}
	if (rp->config != NULL) {
	    free(rp->config);
	    rp->config = NULL;
	}
	if (rp->argv != NULL) {
	    free(rp->argv);
	    rp->argv = NULL;
	}
	rp->argc = 0;
	if (rp->public.f_config != NULL) {
	    fclose(rp->public.f_config);
	    rp->public.f_config = NULL;
	}
	if (rp->public.fd_ipc != -1) {
	    close(rp->public.fd_ipc);
	    rp->public.fd_ipc = -1;
	}
	if (rp->public.logfile != NULL) {
	    free(rp->public.logfile);
	    rp->public.logfile = NULL;
	}
	rp->public.pid = 0;
	rp->public.status = -1;
    }

    state = PM_REC_BEGIN;
    _seendefault = 0;

    return f_replay;

failed:
    sts = errno;
    if (dir != NULL)
	free(dir);
    if (base != NULL)
	free(base);
    unlink(folio);
    fclose(f_folio);
    if (fd >= 0)
	close(fd);
    errno = sts;
    return NULL;
}

/*
 * need to log another host in this folio ... must come here
 * at least once, but may be more than once before the recording
 * commences
 */
int
pmRecordAddHost(const char *host, int isdefault, pmRecordHost **rhp)
{
    char	*p;
    int		c;
    int		sts;
    record_t	*rp;

    *rhp = NULL;	/* in case of errors */

    if (state != PM_REC_BEGIN && state != PM_REC_HOST)
	/* botched order of calls ... */
	return -EINVAL;

    if (isdefault && _seendefault)
	/* only one default allowed per session! */
	return -EINVAL;

    for (rp = record; rp != NULL; rp = rp->next) {
	if (rp->state == PM_REC_BEGIN)
	    break;
    }
    if (rp == NULL) {
	/* need another one */
	if ((rp = (record_t *)malloc(sizeof(record_t))) == NULL)
	    return -errno;
	rp->next = record;
	record = rp;
	rp->isdefault = 0;
	rp->host = NULL;
	rp->base = NULL;
	rp->logfile = NULL;
	rp->config = NULL;
	rp->argv = NULL;
	rp->argc = 0;
	rp->public.f_config = NULL;
	rp->public.fd_ipc = -1;
	rp->public.logfile = NULL;
	rp->public.pid = 0;
	rp->public.status = -1;
    }

    rp->isdefault = isdefault;

    if (dir != NULL)
	strcpy(tbuf, dir);
    else
	tbuf[0] = '\0';
    strcat(tbuf, base);
    p = &tbuf[strlen(tbuf)];
    strcat(tbuf, ".");
    strcat(tbuf, host);
    strcat(tbuf, ".config");
    c = '\0';
    if (access(tbuf, F_OK) == 0) {
	p[0] = 'a';
	p[1] = '.';
	p[2] = '\0';
	strcat(p, host);
	strcat(p, ".config");
	while (p[0] <= 'z') {
	    if (access(tbuf, F_OK) != 0) {
		c = p[0];
		break;
	    }
	    p[0]++;
	}
	if (c == '\0') {
	    errno = EEXIST;
	    goto failed;
	}
    }

    if ((rp->host = strdup(host)) == NULL)
	goto failed;

    if ((rp->public.f_config = fopen(tbuf, "w")) == NULL)
	goto failed;

    if ((rp->base = malloc(strlen(base)+1+strlen(host)+3)) == NULL)
	goto failed;
    strcpy(rp->base, base);
    p = &rp->base[strlen(rp->base)];
    if (c != '\0') {
	*p++ = c;
    }
    *p++ = '.';
    *p = '\0';
    strcat(p, host);

    if ((rp->logfile = malloc(strlen(rp->base)+5)) == NULL)
	goto failed;
    strcpy(rp->logfile, rp->base);
    strcat(rp->logfile, ".log");

    if ((rp->config = malloc(strlen(rp->base)+8)) == NULL)
	goto failed;
    strcpy(rp->config, rp->base);
    strcat(rp->config, ".config");

    /* construct full pathname */
    if (dir != NULL && dir[0] == '/') {
	rp->public.logfile = malloc(MAXPATHLEN);
	if (rp->public.logfile != NULL)
	    strcpy(rp->public.logfile, dir);
    }
    else
	rp->public.logfile = getcwd(NULL, MAXPATHLEN);

    if (rp->public.logfile != NULL) {
	if (rp->public.logfile[strlen(rp->public.logfile)-1] != '/')
	    strcat(rp->public.logfile, "/");
	if (strncmp(rp->logfile, "./", 2) == 0)
	    strcat(rp->public.logfile, &rp->logfile[1]);
	else
	    strcat(rp->public.logfile, rp->logfile);
    }

    n_record++;
    state = rp->state = PM_REC_HOST;
    *rhp = &rp->public;

    return 0;

failed:
    sts = -errno;
    if (rp->public.f_config != NULL) {
	unlink(tbuf);
	fclose(rp->public.f_config);
	rp->public.f_config = NULL;
    }
    if (rp->host != NULL)
	free(rp->host);
    if (rp->base != NULL)
	free(rp->base);
    if (rp->logfile != NULL)
	free(rp->logfile);
    if (rp->config != NULL)
	free(rp->config);
    *rhp = NULL;
    return sts;
}

/*
 * simple control protocol between here and pmlogger
 *  - only write from app to pmlogger
 *  - no ack
 *  - commands are
 *	V<number>\n	- version
 *	F<folio>\n	- folio name
 *	P<name>\n	- launcher's name
 *	R[<msg>]\n	- launcher knows how to replay
 *	D[<msg>]\n	- detach from launcher
 *	Q[<msg>]\n	- quit pmlogger
 *	?[<msg>]\n	- display session status
 */
static int
xmit_to_logger(int fd, char tag, const char *msg)
{
    int		sts;
    SIG_PF	user_onpipe;

    if (fd < 0)
	return PM_ERR_IPC;

    user_onpipe = signal(SIGPIPE, SIG_IGN);
    sts = (int)write(fd, &tag, 1);
    if (sts != 1)
	goto fail;

    if (msg != NULL) {
	int	len = (int)strlen(msg);
	sts = (int)write(fd, msg, len);
	if (sts != len)
	    goto fail;
    }

    sts = (int)write(fd, "\n", 1);
    if (sts != 1)
	goto fail;

    signal(SIGPIPE, user_onpipe);
    return 0;

fail:
    if (errno == EPIPE)
	sts = PM_ERR_IPC;
    else
	sts = -errno;
    signal(SIGPIPE, user_onpipe);
    return sts;
}

int
pmRecordControl(pmRecordHost *rhp, int request, const char *msg)
{
    pid_t	pid;
    record_t	*rp;
    int		sts;
    int		ok;
    int		cmd;
    int		mypipe[2];
    static int	maxseenfd = -1;

    /*
     * harvest old and smelly pmlogger instances
     */
    while ((pid = waitpid(-1, &sts, WNOHANG)) > 0) {
	for (rp = record; rp != NULL; rp = rp->next) {
	    if (pid == rp->public.pid) {
		rp->public.status = sts;
		if (rp->public.fd_ipc != -1) {
		    close(rp->public.fd_ipc);
		    rp->public.fd_ipc = -1;
		}
		break;
	    }
	}
    }

    sts = 0;

    switch (request) {

    case PM_REC_ON:
	    if (state != PM_REC_HOST || rhp != NULL) {
		sts = -EINVAL;
		break;
	    }

	    for (rp = record; rp != NULL; rp = rp->next) {
		if (rp->state != PM_REC_HOST)
		    continue;
		if (rp->isdefault) {
		    fprintf(f_folio, "%-15s %-23s %s\n", "Archive:", rp->host, rp->base);
		    break;
		}
	    }

	    for (rp = record; rp != NULL; rp = rp->next) {
		if (rp->state != PM_REC_HOST || rp->isdefault)
		    continue;
		fprintf(f_folio, "%-15s %-23s %s\n", "Archive:", rp->host, rp->base);
	    }

	    fflush(stderr);
	    fflush(stdout);
	    if (_replay)
		fflush(f_replay);
	    fflush(f_folio);

	    for (rp = record; rp != NULL; rp = rp->next) {
		if (rp->state != PM_REC_HOST)
		    continue;
		fclose(rp->public.f_config);
		if (pipe(mypipe) < 0) {
		    sts = -errno;
		    break;
		}
		if (mypipe[1] > maxseenfd)
		    maxseenfd = mypipe[1];
		if (mypipe[0] > maxseenfd)
		    maxseenfd = mypipe[0];

		if ((rp->public.pid = fork()) == 0) {
		    /* pmlogger */
		    char	fdnum[4];
		    int		fd;
		    int		i;

		    close(mypipe[1]);
		    sprintf(fdnum, "%d", mypipe[0]);
		    if (dir != NULL) {
			/* trim trailing '/' */
			dir[strlen(dir)-1] = '\0';
			if (chdir(dir) < 0) {
			    /* not good! */
			    exit(1);
			}
		    }
		    /*
		     * leave stdin, tie stdout and stderr together, leave
		     * the ipc fd and close all other fds
		     */
		    dup2(2, 1);	/* stdout -> stderr */
		    for (fd = 3; fd <= maxseenfd; fd++) {
			if (fd != mypipe[0])
			    close(fd);
		    }
		    /*
		     * have:
		     *	argv[0] ... argv[argc-1] from PM_REC_SETARG
		     * want:
		     *	argv[0]				"pmlogger"
		     *  argv[1] ... argv[argc]		from PM_REC_SETARG
		     *  argv[argc+1], argv[argc+2]	"-x" fdnum
		     *  argv[argc+3], argv[argc+4]	"-h" host
		     *  argv[argc+5], argv[argc+6]	"-l" log
		     *  argv[argc+7], argv[argc+8]	"-c" config
		     *  argv[argc+9]			basename
		     *  argv[argc+10]			NULL
		     */
		    rp->argv = (char **)realloc(rp->argv, (rp->argc+11)*sizeof(rp->argv[0]));
		    if (rp->argv == NULL) {
			__pmNoMem("pmRecordControl: argv[]", (rp->argc+11)*sizeof(rp->argv[0]), PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    for (i = rp->argc; i > 0; i--)
			rp->argv[i] = rp->argv[i-1];
		    rp->argv[0] = "pmlogger";
		    i = rp->argc+1;
		    rp->argv[i++] = "-x";
		    rp->argv[i++] = fdnum;
		    rp->argv[i++] = "-h";
		    rp->argv[i++] = rp->host;
		    rp->argv[i++] = "-l";
		    rp->argv[i++] = rp->logfile;
		    rp->argv[i++] = "-c";
		    rp->argv[i++] = rp->config;
		    rp->argv[i++] = rp->base;
		    rp->argv[i++] = NULL;
#if DESPERATE
fprintf(stderr, "Launching pmlogger:");
for (i = 0; i < rp->argc+11; i++) fprintf(stderr, " %s", rp->argv[i]);
fputc('\n', stderr);
#endif
		    execv("/usr/pcp/bin/pmlogger", rp->argv);

		    /* this is really bad! */
		    exit(1);
		}
		else if (rp->public.pid < 0) {
		    sts = -errno;
		    break;
		}
		else {
		    /* the application launching pmlogger */
		    close(mypipe[0]);
		    rp->public.fd_ipc = mypipe[1];
		    /* send the protocol version */
		    ok = xmit_to_logger(rp->public.fd_ipc, 'V', "0");
		    if (ok < 0) {
			/* remember last failure */
			sts = ok;
			rp->public.fd_ipc = -1;
			continue;
		    }
		    /* send the folio name */
		    ok = xmit_to_logger(rp->public.fd_ipc, 'F', _folio);
		    if (ok < 0) {
			/* remember last failure */
			sts = ok;
			rp->public.fd_ipc = -1;
			continue;
		    }
		    /* send the my name */
		    ok = xmit_to_logger(rp->public.fd_ipc, 'P', _creator);
		    if (ok < 0) {
			/* remember last failure */
			sts = ok;
			rp->public.fd_ipc = -1;
			continue;
		    }
		    /* do we know how to replay? */
		    if (_replay) {
			ok = xmit_to_logger(rp->public.fd_ipc, 'R', NULL);
			if (ok < 0) {
			    /* remember last failure */
			    sts = ok;
			    rp->public.fd_ipc = -1;
			    continue;
			}
		    }
		}
		rp->state = PM_REC_ON;
	    }

	    if (sts < 0) {
		for (rp = record; rp != NULL; rp = rp->next) {
		    if (rp->public.fd_ipc >= 0) {
			close(rp->public.fd_ipc);
			rp->public.fd_ipc = -1;
		    }
		}
	    }
	    else {
		if (_replay)
		    fclose(f_replay);
		fclose(f_folio);
		state = PM_REC_ON;
		n_alive = n_record;
	    }
	    break;

    case PM_REC_STATUS:
	    cmd = '?';
	    goto broadcast;

    case PM_REC_OFF:
	    cmd = 'Q';
	    goto broadcast;

    case PM_REC_DETACH:
	    cmd = 'D';

broadcast:
	    if (state != PM_REC_ON) {
		sts = -EINVAL;
		break;
	    }

	    for (rp = record; rp != NULL; rp = rp->next) {
		if (rhp != NULL && rhp != &rp->public)
		    continue;
		if (rp->state != PM_REC_ON) {
		    if (rhp != NULL)
			/* explicit pmlogger, should be "on" */
			sts = -EINVAL;
		    continue;
		}
		if (rp->public.fd_ipc >= 0) {
		    ok = xmit_to_logger(rp->public.fd_ipc, cmd, msg);
		    if (ok < 0) {
			/* remember last failure */
			sts = ok;
			rp->public.fd_ipc = -1;
		    }
		}
		else
		    sts = PM_ERR_IPC;

		if (cmd != '?') {
		    n_alive--;
		    rp->state = PM_REC_OFF;
		    if (rp->public.fd_ipc != -1) {
			close(rp->public.fd_ipc);
			rp->public.fd_ipc = -1;
		    }
		}
	    }

	    if (n_alive <= 0)
		state = PM_REC_OFF;

	    break;

    case PM_REC_SETARG:
	    if (state != PM_REC_HOST) {
		sts = -EINVAL;
		break;
	    }

	    for (rp = record; rp != NULL; rp = rp->next) {
		if (rhp != NULL && rhp != &rp->public)
		    continue;
		rp->argv = (char **)realloc(rp->argv, (rp->argc+1)*sizeof(rp->argv[0]));
		if (rp->argv == NULL) {
		    sts = -errno;
		    rp->argc = 0;
		}
		else {
		    rp->argv[rp->argc] = strdup(msg);
		    if (rp->argv[rp->argc] == NULL)
			sts = -errno;
		    else
			rp->argc++;
		}
	    }
	    break;

    default:
	    sts = -EINVAL;
	    break;
    }

    return sts;
}
