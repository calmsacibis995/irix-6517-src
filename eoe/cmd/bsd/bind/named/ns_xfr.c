#if !defined(line) && !defined(SABER)
static char sccsid[] = "@(#)ns_xfr.c	1.1 (Rutgers) 7/16/93";
static char rcsid[] = "$Id:";
#endif /* not lint */
/*
 *
 * ns_xfr.c - support for specifying the program to be used to do
 * zone transfers.  This isn't obvious, but this is actually support
 * for shuffling address records, cnames, etc - anything you want.
 *
 * Note that if this code is turned on, then the sorting of responses
 * is actively turned off -- since these are order dependent - then
 * such things as sorting responses tends to bung things up a bit.
 *
 * This codes enables the "transfer" keyword in the named.boot
 * file.  There you may use the syntax:
 *	    transfer	    <zone-name>	    <program name>
 *
 * Of course, inside of that program, you can do *anything* you
 * want to do.  Currently I'm actually using a shell script that
 * does a "round robin" ordering of addresses.  With the right
 * ancillary programs - you could even have it page you everytime
 * it does a zone transfer.
 *
 * This is the result of a long and arduous trek through the IETF,
 * a working group, and a lot of frustration.  All of this for
 * load-balancing. - TpB (brisco@noc.rutgers.edu)
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <syslog.h>
#include <signal.h>
#include <resolv.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include "named.h"

#ifdef SETTRANSFER
#define MAXTRANDEFS 128
struct xfr_element {
    char * domain;
    char * prog;
};
static struct xfr_element xfr_table[MAXTRANDEFS];
static int no_xfrs;

#ifdef sgi
void
#endif
setXfer(dname, progname)
char * dname;
char * progname;
{
    char *d, *p;
    extern int debug;
    if (no_xfrs >= MAXTRANDEFS) {
	if (debug)
	    dprintf(1, (ddt,"Set Transfer:  max# exceeded.  Dropping.\n"));
	return;
    }
    d = xfr_table[no_xfrs].domain = malloc(strlen(dname)+1);
    p = xfr_table[no_xfrs].prog = malloc(strlen(progname)+1);
    strcpy(d, dname);
    strcpy(p, progname);
    if (debug) fprintf(ddt,"Set transfer[%d] for domain %s to %s\n",
		    no_xfrs,d,p);
    no_xfrs++;
}

char *
getXfer(dname)
char * dname;
{
    char * p;
    int i;
    p = (char *)NULL;
    for (i=0; i<no_xfrs && p==(char*)NULL; i++) {
	if (!strcasecmp(xfr_table[i].domain,dname))
	    p = xfr_table[i].prog;
    }
    if (debug) fprintf(ddt,"Transfer for domain %s is %s\n",
		    dname,p?p:_PATH_XFER);
    return(p);
}
#endif /* SETTRANSFER */
