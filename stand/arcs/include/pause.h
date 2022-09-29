#ident	"include/pause.h:  $Revision: 1.1 $"

/*
 *  pause.h - header for libsc pause function
 */

#define P_INTERRUPT	0
#define P_EXPEDITE	1
#define P_TIMEOUT	2

extern int pause (int secs, char *expedite, char *interrupt);
