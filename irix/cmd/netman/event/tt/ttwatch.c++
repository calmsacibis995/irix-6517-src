/* ttwatch.c++  - implentation of the server classe for the nveventd
 * 
 *	ttsession watchdog process. It forces all netvis eventlib users to
 *      join the same specified session.
 *
 *	$Revision: 1.2 $
 *
 */
 
/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#include <sys/types.h>
#include <unistd.h>
#include <sys/syslog.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
/* #include <getopt.h> */
#include <bstring.h>
#include <stdio.h>
#include <sys/stat.h>
#include <osfcn.h>
#include <syslog.h>


extern "C" {
    void syslog (int p, const char *m, ...);   
    void (*signal (int sig, void (*func)(int, ...)))(int, ...);
};

static void sigHandler(int, ...);
void setsignals(void);
static char *usage = "ttwatch [-d] <ttsessionidFile>";
int debugging = 0;

#define TTSESSION_CMD "ttsession -p -s"
#define MAX_SESSION_ID_SIZE 128;
#define WATCH_LOCK_FILE "/usr/tmp/.TTWATCH_LOCK"


class watcher {
public:
    watcher (void);
    void cmdline (int argc,  char **argv, char **env);
    int startPrivateSession (void);
    void doStdOut(void);
    void cleanup(void);
protected:
    char *idFile;
    char sessionID[128];
    char ssid[1];
    FILE *ttStdOut; 
    FILE * idf;
    FILE *wlf; 
    int ttrunning;
};

watcher watch;

watcher::watcher(void) {
    struct stat statbuf;
    
    idFile = NULL; 
    ttStdOut = NULL; 
    idf = NULL; 
    ttrunning = 0;
    if (!stat(WATCH_LOCK_FILE, &statbuf)) {
	fprintf(stderr,  "ttwatch: watcher: there is already one of me\n");
	exit (1);
    }
    else {
	wlf = fopen(WATCH_LOCK_FILE, "w");
	fprintf(wlf, "%d %d\n", getpid(), getuid());
	fflush(wlf);
    }
}

void
watcher::cmdline(int argc, char **argv, char **env) {
    int arg;
    extern char *optarg;
    extern int optind;
    
    while ((arg = getopt(argc, argv, "d")) != -1) {
	switch (arg) {
	    case 'd':
		debugging = 1;
	    break;
	    default:
	    break;
	}
    }
    
    if (optind != argc - 1) {
        fprintf(stderr, "ttwatch: usage: %s\n", usage);
	cleanup();
	exit(1);
    }


    idFile = strdup(argv[optind]);

}

int
watcher::startPrivateSession(void) {
    int rslt = -1;
    int len = 0;
    if (debugging)
	fprintf(stderr, "ttwatch: startPrivateSession: about to popen %s\n", 
		TTSESSION_CMD);
    ttStdOut = popen(TTSESSION_CMD, "r");
    if (ttStdOut) {
	ttrunning = 1;
	len = read(fileno(ttStdOut), (void *) sessionID, sizeof sessionID);
	if (len < 0) {
	    perror("ttwatch: read of ttsession pipe failed");
	    goto done;
	}
	if (debugging)
	    fprintf(stderr, "ttwatch: startPrivateSession: read sessionID (%s)\n", 
		    sessionID);
	idf  = fopen (idFile, "w");
	if (idf) {
	    if (debugging)
		fprintf(stderr, "ttwatch: startPrivateSession: wrote sessionID to %s\n", 
			idFile);
	    fputs(sessionID, idf);
	    fputc('\n', idf);
	    fflush(idf);
	}
	else {
	    perror("ttwatcher: fopen of sessionID file failed.");
	    goto done;
	}
    }
    else {
	perror("ttwatcher: popen failed");
	goto done;
    }
    rslt = 0;
done:
    return (rslt);
}

void
watcher::doStdOut(void) {
    char outbuf[1024];
    int wlen = 0, len = 0;
    
    while (ttrunning) {
reread:
	len = read(fileno(ttStdOut), (void *)outbuf, sizeof outbuf);
	if (debugging)
	    fprintf(stderr, "ttwatch: doStdOut: read %d bytes: %s\n", 
		    len, outbuf);
	if (len < 0) {
	    if (errno != EINTR) {
		perror("ttwatcher: doStdOut: read failed");
		break;
	    }
	    else {
		goto reread;
	    }
	}
	else if (len == 0) {  
	    fprintf(stderr, "ttwatch: doStdout: ttsession is gone!\n");
	    break;
	}
rewrite:
	wlen = write(fileno(stdout), (void *)outbuf, len);
	if (debugging)
	    fprintf(stderr, "ttwatch: doStdOut: wrote %d bytes: %s\n", 
		    len, outbuf);
	if (wlen != len) {
	    if (errno != EINTR) {
		perror("ttwatcher: doStdOut: write failed");
		break;
	    }
	    else 
		goto rewrite;	    
	}
	(void) fflush(stdout);
	bzero((void *)outbuf,  sizeof outbuf);	
    }
    fprintf(stderr, "ttwatch: doStdOut: exiting\n");
    cleanup();
    exit(1);
}
void
watcher::cleanup(void) {
    fclose (idf);
    unlink (idFile);
    fclose (wlf);
    unlink (WATCH_LOCK_FILE);
    ttrunning = 0;        

}

static void
sigHandler(int sig, ...) {
    if (debugging)
	fprintf(stderr, "ttwatch: sigHandler: caught signal %d\n", sig);
    switch (sig) {
	case SIGINT:
	case SIGTERM:
	    watch.cleanup();
	    exit(0);
    	break;
	default:
	    if (debugging)
		fprintf(stderr, 
			 "ttwatch: don't do anything with this signal yet\n");
	break;
    }
}

void
setsignals(void) {
    signal(SIGINT, sigHandler);
    signal(SIGHUP, sigHandler);
    signal (SIGTERM, sigHandler);
    signal (SIGUSR1, sigHandler);
    signal (SIGUSR2, sigHandler);    
}

main (int argc,  char **argv, char **environ) {
    
    setsignals();
    watch.cmdline(argc, argv, environ);
    watch.startPrivateSession();
    watch.doStdOut();
}
