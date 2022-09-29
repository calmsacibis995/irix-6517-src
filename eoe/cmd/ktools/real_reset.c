/* $Modified: Fri Feb 10 11:32:40 1995 by cwilson $ */
#include "resetter.h"
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <rpcsvc/ypclnt.h>

#define LOG_FILE	"/usr/annex/resetter.log"
#define RESETTER	"/dev/reset"

#define LINE_SIZE	128

char *ypdomain = NULL;
char default_domain_name[64];
#define YPMAP "ktools"
#define TRUE 1
#define FALSE 0

main(argc, argv)
int argc;
char *argv[];
{
	int fd;
	char *mach;
	int port = 0;
	int found;
	char name[50];
	FILE *conf;
	char line[LINE_SIZE];
	int 	i;
	int 	err;
	char 	*val;
	char 	*scratch;
	int	len;

	if (argc != 2) {
		fprintf(stderr, "Usage: reset <machine_name>\n");
		exit(1);
	}

	mach = argv[1];

	/* yp setup */
	if (!getdomainname(default_domain_name, 64) ) {
	  ypdomain = default_domain_name;
	} else {
	  (void) fprintf(stderr, "real_reset: invalid domainname.\n");
	  exit(1);
	}
    
	if (strlen(ypdomain) == 0) {
	  (void) fprintf(stderr, "real_reset: invalid domainname.\n");
	  exit(1);
	}
	
	/* look up mach in yp map */

	if ((err = ypmatch(mach, &val, &len))) {
	  fprintf(stderr, "real_reset: unable to locate %s in ktools NIS map.\n", 
		  mach);
		log(mach, "No NIS record: %s", err);
		exit(1);
	}

/* step through info till we get what we want */
	i = 0;
	found = 0;
	while ( i < len && found != 16 ) {
	  if (val[i++] == '`')
	    found++;
	} 

	/* advanced to reset machine, now advance to reset line */
	/* (format is reset_machine:portno ) */

	if ((sscanf( val+i, "%[A-z.]:%d", name, &port)) != 2) {
	  fprintf(stderr, "real_reset: unable to determine reset line.\n");
	  exit(1);
	}

	printf("found it:  %s, port %d\n", name, port);

	if ((fd = open(RESETTER, O_RDONLY)) == -1) {
		perror(RESETTER);
		log(mach, "%s: %s", RESETTER, strerror(errno));
		exit(1);
	}

	if (ioctl(fd, RESET_PORT, port)== -1) {
		perror("RESET_PORT");
		log(mach, "RESET_PORT cmd: %s", strerror(errno));
		exit(1);
	}

	log(mach, "successful");
	exit(0);
}

log(mach, fmt, a0, a1, a2, a3, a4)
char *mach;
char *fmt;
int a0, a1, a2, a3, a4;
{
	FILE *log;
	time_t now;
	char *username;
	char *time_str;

	if ((log = fopen(LOG_FILE, "a")) == NULL) {
		perror(LOG_FILE);
		return;
	}

	if ((username = getenv("REMOTEUSER")) == NULL)
		username = getenv("USER");

	now = time(NULL);
	time_str = ctime(&now);
	time_str[strlen(time_str) - 1] = '\0';	/* get rid of newline */

	fprintf(log, "Reset %s requested by %s(%d)@%s,\n\t%s, ", mach, 
		username, getuid(), getenv("REMOTEHOST"), time_str);
	fprintf(log, fmt, a0, a1, a2, a3, a4);
	fputs("\n", log);

	fclose(log);
}

ypmatch(char *key, char **val, int *len) {
  int err;
  int error = FALSE;

  *val = NULL;
  *len = 0;
  err = yp_match(ypdomain, YPMAP, key, strlen(key), val, len);

  if (err == YPERR_KEY) {
    err = yp_match(ypdomain, YPMAP, key, (strlen(key) + 1),
		   val, len);
  }
  
  if (err) {
/*
    (void) fprintf(stderr,
		   "Can't match key %s in map %s.  Reason: %s.\n", key, YPMAP,
		   yperr_string(err) );
*/
    error = TRUE;
  }
  return (error);
}
