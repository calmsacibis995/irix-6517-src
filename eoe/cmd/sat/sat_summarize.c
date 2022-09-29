/*
 * Copyright 1997, Silicon Graphics, Inc.
 * All Rights Reserved.
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

/*
 * sat_interpret -	convert audit records from binary to human
 *			readable form
 */

#ident  "$Revision: 1.18 $"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <sys.s>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <netinet/in.h>			/* required for ip.h */
#include <sys/mac.h>
#include <sys/capability.h>
#include <sys/acl.h>
#include <sat.h>
#include <getopt.h>
#include <string.h>
#include <time.h>
#include <netdb.h>
#include <sys/stat.h>
#include <sys/syssgi.h>
#include <assert.h>
#include <net/if.h>
#include <net/if_sec.h>
#include <net/route.h>
#include <sys/ioctl.h>			/* also gets <net/soioctl.h> */
#include <sys/sesmgr_if.h>
#include <sys/stropts.h>
#include <sys/termios.h>
#include <netinet/in_systm.h>		/* required for ip.h */
#include <netinet/ip.h>
#include <netinet/ip_var.h>		/* required for tcpip.h */
#include <netinet/tcp.h>		/* required for tcpip.h */
#include <netinet/tcpip.h>
#include <netinet/in.h>			/* for sockaddr_in */
#include <arpa/inet.h>			/* for inet_ntoa() */
#include <sys/sat_compat.h>
#include <sys/extacct.h>
#include "sat_token.h"

extern	int	errno;

typedef	struct	user_list {
	struct	user_list *	next;
	struct	user_list *	prev;
	uid_t			uid;
	int			total;
}	user_list_t;

#define	MAX_REC_SIZE	2408
#define ONE_MINUTE	60

#define ALIGN(x) ((x) + align[(int)(x) % 4])
int align[4] = { 0, 3, 2, 1 };

#ifdef IFSEC_VECTOR
#define SEC(ifr)	((ifsec_t *)((ifr)->ifr_base))
#else /* IFSEC_VECTOR */
#define SEC(ifr)	(&(ifr)->ifr_sec)
#endif /* IFSEC_VECTOR */

/*
 * Output modes: verbose, brief, linear
 */
#define OM_BRIEF	0x00	/* display option -b */
#define OM_VERBOSE	0x01	/* display option -v */
#define OM_LINEAR	0x02	/* display option -l */
#define OM_BY_EVENT	0x04	/* display option -e */
#define OM_BY_USER	0x08	/* display option -u */
#define OM_BY_TIME	0x10	/* display option -t */
#define OM_ECHO_IN	0x20	/* display option -o */

int format	= OM_VERBOSE;	/* output format */
int debug	= SAT_FALSE;	/* include debugging output? */
int fdmap	= 0;		/* map file descriptors to file names? */
int titles	= SAT_TRUE;	/* put titles on the data */
int by_event	= SAT_FALSE;	/* gather statistics by event */
int by_time	= SAT_FALSE;	/* gather statistics by time interval */
int by_user	= SAT_FALSE;	/* gather statistics by user */
int echo_input	= SAT_FALSE;	/* echo sat_file info to output */

char *dotline =	"\n.....................................................\n\n";

int mac_enabled = SAT_TRUE;	/* was mac enabled when the file was produced? */
int cap_enabled = SAT_TRUE;	/* was cap enabled when the file was produced? */
int cipso_enabled = SAT_TRUE; /* was cipso enabled when the file was produced? */
int normalize = SAT_FALSE;	/* normalized output? */

struct sat_list_ent **users = NULL;	/* username <-> userid table */
struct sat_list_ent **groups = NULL;	/* groupname <-> groupid table */
struct sat_list_ent **hosts = NULL;	/* hostname <-> hostid table */
int n_users, n_groups, n_hosts;

int file_major, file_minor;		/* version number of current file */

/* prototypes */
char *	sat_str_outcome( struct sat_hdr_info * );
void	sat_debug_magic( char * );
void	sat_print_header( struct sat_hdr_info * );
char *	sat_str_domain( short );
char *	sat_str_socktype( short );
void	sat_print_sockaddr( short, char * );
char *	sat_str_mode( mode_t );
const char *sat_str_id( unsigned int, struct sat_list_ent **, int );
char *	sat_str_label( mac_t );
char *	sat_str_cap( cap_t );
char *	sat_str_umask( mode_t );
char *	sat_str_open_flags( int );
char *	sat_str_hex( unsigned char *, int );
char *	sat_str_signame( int );
char *	sat_str_dev( dev_t );
void	sat_print_ioctl( int, int, struct ifreq * );
void	sat_print_body( struct sat_hdr_info *, char * );
void	sat_print_sat_control( struct sat_hdr_info *, char * );
void    sat_print_acct_timers( struct acct_timers * );
void    sat_print_acct_counts( struct acct_counts * );
void	sat_user_summ (uid_t, user_list_t *);

#define	sat_str_user(uid)	sat_str_id((uid), users, n_users)
#define	sat_str_group(gid)	sat_str_id((gid), groups, n_groups)
#define	sat_str_host(hostid)	sat_str_id((hostid), hosts, n_hosts)

/* SGIF */
#define NSGIF 200	/* max number of fields */
int	rlen;
#define ALIGN_REC   	4      /* alignment number */

/*
 *  More detailed info for sys_call routine.
 */

char *
sat_sys_call(int major, int minor)
{
	static char buffer[256];

	/* let sys_call handle everything but ioctl() */
	if ((major+SYSVoffset) != SYS_ioctl)
		return sys_call(major,minor);

	/* Convert common ioctls to strings */
	switch (minor & 0xFFFF) {
	default:
                sprintf( buffer, "ioctl('%c'<<8|%d)",
                        (minor&0xff00)>>8, minor&0xff);
		break;
	case (FIOASYNC & 0xFFFF):
		strcpy(buffer,"ioctl(FIOASYNC)");
		break;
	case (FIOGETOWN & 0xFFFF):
		strcpy(buffer,"ioctl(FIOGETOWN)");
		break;
	case (FIONBIO & 0xFFFF):
		strcpy(buffer,"ioctl(FIONBIO)");
		break;
	case (FIONREAD & 0xFFFF):
		strcpy(buffer,"ioctl(FIONREAD)");
		break;
	case (FIOSETOWN & 0xFFFF):
		strcpy(buffer,"ioctl(FIOSETOWN)");
		break;
	case (I_STR & 0xFFFF):
		strcpy(buffer,"ioctl(I_STR)");
		break;
	case (SIOCADDMULTI & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCADDMULTI)");
		break;
	case (SIOCATMARK & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCATMARK)");
		break;
	case (SIOCDARP & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCDARP)");
		break;
	case (SIOCDELMULTI & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCDELMULTI)");
		break;
	case (SIOCGARP & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGARP)");
		break;
	case (SIOCGENADDR & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGENADDR)");
		break;
	case (SIOCGENPSTATS & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGENPSTATS)");
		break;
	case (SIOCGETLABEL & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGETLABEL)");
		break;
	case (SIOCGETNAME & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGETNAME)");
		break;
	case (SIOCGETPEER & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGETPEER)");
		break;
	case (SIOCGETRCVUID & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGETRCVUID)");
		break;
	case (SIOCGETSYNC & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGETSYNC)");
		break;
	case (SIOCGETUID & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGETUID)");
		break;
	case (SIOCGHIWAT & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGHIWAT)");
		break;
	case (SIOCGIFADDR & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGIFADDR)");
		break;
	case (SIOCGIFBRDADDR & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGIFBRDADDR)");
		break;
	case (SIOCGIFCONF & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGIFCONF)");
		break;
	case (SIOCGIFDSTADDR & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGIFDSTADDR)");
		break;
	case (SIOCGIFFLAGS & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGIFFLAGS)");
		break;
	case (SIOCGIFLABEL & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGIFLABEL)");
		break;
	case (SIOCGIFMEM & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGIFMEM)");
		break;
	case (SIOCGIFMETRIC & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGIFMETRIC)");
		break;
	case (SIOCGIFMTU & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGIFMTU)");
		break;
	case (SIOCGIFNETMASK & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGIFNETMASK)");
		break;
	case (SIOCGIFSTATS & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGIFSTATS)");
		break;
	case (SIOCGIFUID & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGIFUID)");
		break;
	case (SIOCGLOWAT & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGLOWAT)");
		break;
	case (SIOCGPGRP & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGPGRP)");
		break;
	case (SIOCIFDETACH & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCIFDETACH)");
		break;
	case (SIOCLOWER & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCLOWER)");
		break;
	case (SIOCNREAD & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCNREAD)");
		break;
	case (SIOCPROTO & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCPROTO)");
		break;
	case (SIOCSARP & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSARP)");
		break;
	case (SIOCSETLABEL & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSETLABEL)");
		break;
	case (SIOCSETSYNC & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSETSYNC)");
		break;
	case (SIOCSETUID & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSETUID)");
		break;
	case (SIOCSHIWAT & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSHIWAT)");
		break;
	case (SIOCSIFADDR & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSIFADDR)");
		break;
	case (SIOCSIFBRDADDR & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSIFBRDADDR)");
		break;
	case (SIOCSIFDSTADDR & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSIFDSTADDR)");
		break;
	case (SIOCSIFFLAGS & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSIFFLAGS)");
		break;
	case (SIOCSIFHEAD & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSIFHEAD)");
		break;
	case (SIOCSIFLABEL & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSIFLABEL)");
		break;
	case (SIOCSIFMEM & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSIFMEM)");
		break;
	case (SIOCSIFMETRIC & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSIFMETRIC)");
		break;
	case (SIOCSIFMTU & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSIFMTU)");
		break;
	case (SIOCSIFNAME & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSIFNAME)");
		break;
	case (SIOCSIFNETMASK & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSIFNETMASK)");
		break;
	case (SIOCSIFSTATS & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSIFSTATS)");
		break;
	case (SIOCSIFUID & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSIFUID)");
		break;
	case (SIOCSLGETREQ & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSLGETREQ)");
		break;
	case (SIOCSLOWAT & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSLOWAT)");
		break;
	case (SIOCSLSTAT & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSLSTAT)");
		break;
	case (SIOCSOCKSYS & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSOCKSYS)");
		break;
	case (SIOCSPGRP & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSPGRP)");
		break;
	case (SIOCSPROMISC & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSPROMISC)");
		break;
	case (SIOCSSDSTATS & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSSDSTATS)");
		break;
	case (SIOCSSESTATS & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSSESTATS)");
		break;
	case (SIOCTOSTREAM & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCTOSTREAM)");
		break;
	case (SIOCUPPER & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCUPPER)");
		break;
	case (SIOCX25RCV & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCX25RCV)");
		break;
	case (SIOCX25TBL & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCX25TBL)");
		break;
	case (SIOCX25XMT & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCX25XMT)");
		break;
	case (SIOCXPROTO & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCXPROTO)");
		break;
	case (TCBLKMD & 0xFFFF):
		strcpy(buffer,"ioctl(TCBLKMD)");
		break;
	case (TCDSET & 0xFFFF):
		strcpy(buffer,"ioctl(TCDSET)");
		break;
	case (TCFLSH & 0xFFFF):
		strcpy(buffer,"ioctl(TCFLSH)");
		break;
	case (TCGETA & 0xFFFF):
		strcpy(buffer,"ioctl(TCGETA)");
		break;
	case (TCGETS & 0xFFFF):
		strcpy(buffer,"ioctl(TCGETS)");
		break;
	case (TCIOFF & 0xFFFF):
		strcpy(buffer,"ioctl(TCIOFF)");
		break;
	case (TCION & 0xFFFF):
		strcpy(buffer,"ioctl(TCION)");
		break;
	case (TCOOFF & 0xFFFF):
		strcpy(buffer,"ioctl(TCOOFF)");
		break;
	case (TCOON & 0xFFFF):
		strcpy(buffer,"ioctl(TCOON)");
		break;
	case (TCSADRAIN & 0xFFFF):
		strcpy(buffer,"ioctl(TCSADRAIN)");
		break;
	case (TCSAFLUSH & 0xFFFF):
		strcpy(buffer,"ioctl(TCSAFLUSH)");
		break;
	case (TCSANOW & 0xFFFF):
		strcpy(buffer,"ioctl(TCSANOW)");
		break;
	case (TCSBRK & 0xFFFF):
		strcpy(buffer,"ioctl(TCSBRK)");
		break;
	case (TCSETA & 0xFFFF):
		strcpy(buffer,"ioctl(TCSETA)");
		break;
	case (TCSETAF & 0xFFFF):
		strcpy(buffer,"ioctl(TCSETAF)");
		break;
	case (TCSETAW & 0xFFFF):
		strcpy(buffer,"ioctl(TCSETAW)");
		break;
	case (TCSETLABEL & 0xFFFF):
		strcpy(buffer,"ioctl(TCSETLABEL)");
		break;
	case (TCXONC & 0xFFFF):
		strcpy(buffer,"ioctl(TCXONC)");
		break;
	case (TFIOC & 0xFFFF):
		strcpy(buffer,"ioctl(TFIOC)");
		break;
	case (TIOCCBRK & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCCBRK)");
		break;
	case (TIOCCDTR & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCCDTR)");
		break;
	case (TIOCEXCL & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCEXCL)");
		break;
	case (TIOCFLUSH & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCFLUSH)");
		break;
	case (TIOCGETC & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCGETC)");
		break;
	case (TIOCGETD & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCGETD)");
		break;
	case (TIOCGETP & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCGETP)");
		break;
	case (TIOCGLTC & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCGLTC)");
		break;
	case (TIOCGPGRP & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCGPGRP)");
		break;
	case (TIOCGSID & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCGSID)");
		break;
	case (TIOCGWINSZ & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCGWINSZ)");
		break;
	case (TIOCHPCL & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCHPCL)");
		break;
	case (TIOCLBIC & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCLBIC)");
		break;
	case (TIOCLBIS & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCLBIS)");
		break;
	case (TIOCLGET & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCLGET)");
		break;
	case (TIOCLSET & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCLSET)");
		break;
	case (TIOCMBIC & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCMBIC)");
		break;
	case (TIOCMBIS & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCMBIS)");
		break;
	case (TIOCMGET & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCMGET)");
		break;
	case (TIOCMSET & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCMSET)");
		break;
	case (TIOCNXCL & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCNXCL)");
		break;
	case (TIOCOUTQ & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCOUTQ)");
		break;
	case (TIOCPKT & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCPKT)");
		break;
	case (TIOCREMOTE & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCREMOTE)");
		break;
	case (TIOCSBRK & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCSBRK)");
		break;
	case (TIOCSDTR & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCSDTR)");
		break;
	case (TIOCSETC & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCSETC)");
		break;
	case (TIOCSETD & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCSETD)");
		break;
	case (TIOCSETN & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCSETN)");
		break;
	case (TIOCSETP & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCSETP)");
		break;
#ifdef	NOT_SUPPORTED_AS_OF_1997_05_19
	case (TIOCSIGNAL & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCSIGNAL)");
		break;
#endif	/* NOT_SUPPORTED_AS_OF_1997_05_19 */
	case (TIOCSLTC & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCSLTC)");
		break;
	case (TIOCSPGRP & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCSPGRP)");
		break;
	case (TIOCSSID & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCSSID)");
		break;
	case (TIOCSTART & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCSTART)");
		break;
	case (TIOCSTI & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCSTI)");
		break;
	case (TIOCSTOP & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCSTOP)");
		break;
	case (TIOCSWINSZ & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCSWINSZ)");
		break;
	}
	return buffer;
}

/*
 *  outcome (success or failure, and the reason why or why not) is displayed
 */
char *
sat_str_outcome (struct sat_hdr_info *h) {
	static char outstring [400];
	int outcome = h->sat_outcome;
	cap_value_t sat_cap = h->sat_cap;

	switch (format) {
	case OM_BRIEF:
	case OM_VERBOSE:
		if (SAT_SUCCESS & outcome)
			strcpy (outstring, "Success");
		else
			strcpy (outstring, "Failure");

		if (SAT_CHK & outcome) {
			strcat (outstring, "/privilege=");
			if (SAT_CAPABILITY & outcome)
				strcat (outstring, "capability");
			else {
				if (SAT_SUSER & outcome)
					strcat (outstring, "superuser");
				else
					strcat (outstring, "none");
			}
		}

		if (cap_enabled && sat_cap != 0) {
			char *cp = cap_value_to_text(sat_cap);

			strcat (outstring, "/capability=");
			strcat (outstring, cp);
			cap_free(cp);
		}

		if (SAT_DAC & outcome)
			strcat (outstring, " (DAC checked)");

		if (SAT_MAC & outcome)
			strcat (outstring, " (MAC checked)");
		break;
	case OM_LINEAR:
		if (SAT_SUCCESS & outcome)
			strcpy (outstring, "+");
		else
			strcpy (outstring, "-");

		if (SAT_CHK & outcome) {
			strcat (outstring, "/p=");
			if (SAT_CAPABILITY & outcome)
				strcat (outstring, "c");
			else {
				if (SAT_SUSER & outcome)
					strcat (outstring, "s");
				else
					strcat (outstring, "n");
			}
		}

		if (cap_enabled && sat_cap != 0) {
			char *cp = cap_value_to_text(sat_cap);

			strcat (outstring, "/c=");
			strcat (outstring, cp);
			cap_free(cp);
		}

		if (SAT_DAC & outcome)
			strcat (outstring, " DAC");

		if (SAT_MAC & outcome)
			strcat (outstring, " MAC");
		break;
	}
	return (outstring);
}


/*
 *  sat_debug_magic - display header in hex if misaligned - a debugging aid
 */
void
sat_debug_magic (char *h)
{
	int     l;		/* indexes bytes of header		*/
	int     n;		/* indexes bytes of hex header buffer 	*/
	int     nibl;		/* nibble of a header byte		*/
	char    hhb[72];	/* hex header buffer			*/

	printf("INVALID MAGIC NUMBER!!\n");

	for (n=0,l=0; l < sizeof(struct sat_hdr) && n < sizeof(hhb); l++) {
		if ((!(l%4))&&l)
			hhb[n++] = ' ';	/* insert blanks after each word */
		nibl = 0xf & (h[l] >> 4);
		hhb[n++] = (0xa > nibl) ? '0' + nibl : 'a' + nibl - 0xa;
		nibl = 0xf &  h[l];
		hhb[n++] = (0xa > nibl) ? '0' + nibl : 'a' + nibl - 0xa;
	}
	hhb[n] = '\0';
	printf ("Header           = %s\n",  hhb );
}

/*
 * descriptor/name mapping.
 */
struct fdnamemap {
	struct fdnamemap	*fn_next;
	int			fn_pid;		/* Process ID */
	int			fn_fd;		/* File Descriptor */
	char			*fn_path;	/* Associated Object Name */
};

struct fdnamemap *fdnamemap;

void
sat_set_fdmap_path(int pid, int fd, char *path)
{
	struct fdnamemap *fp;
	struct fdnamemap *unused = NULL;

	if (!fdmap)
		return;

	for (fp = fdnamemap; fp; fp = fp->fn_next) {
		/*
		 * Remember the first unused entry for later.
		 */
		if (unused == NULL && fp->fn_pid == -1)
			unused = fp;

		if (fp->fn_pid != pid)
			continue;
		if (fd != -1 && fp->fn_fd != fd)
			continue;

		if (fp->fn_path) {
			free(fp->fn_path);
			fp->fn_path = NULL;
		}

		/*
		 * If a path is given set the entry, otherwise mark it
		 * as available for other use.
		 */
		if (path)
			fp->fn_path = strdup(path);
		else
			fp->fn_pid = -1;
		return;
	}
	/*
	 * Don't add entries for the clear cases.
	 */
	if (fd == -1 || path == NULL)
		return;
	/*
	 * Mapping not found. Add to the list, using existing list
	 * entry if available.
	 */
	if (unused) {
		fp = unused;
	}
	else {
		fp = (struct fdnamemap *)malloc(sizeof(struct fdnamemap));
		fp->fn_next = fdnamemap;
		fdnamemap = fp;
	}
	fp->fn_pid = pid;
	fp->fn_fd = fd;
	fp->fn_path = strdup(path);
}

/*
 * If path is NULL clear the entry.
 * If fd is -1 set (clear) all entries for this pid.
 */
void
sat_set_fdmap(int pid, int fd, char *body)
{
	mac_t label;
	char *reqname = NULL;
	char *actname;
	char *buf = body;
	struct sat_pathname pn;

	if (body && sat_intrp_pathname(&buf, &pn, &reqname, &actname, &label,
		file_major, file_minor) < 0) {
		exit(1);
	}
	sat_set_fdmap_path(pid, fd, reqname);
}

char *
sat_get_fdmap(int pid, int fd)
{
	static char result[MAXPATHLEN];
	struct fdnamemap *fp;

	if (!fdmap)
		return (NULL);

	for (fp = fdnamemap; fp; fp = fp->fn_next) {
		if (fp->fn_pid != pid || fp->fn_fd != fd)
			continue;
		if (fp->fn_path == NULL)
			break;
		sprintf(result, "%s", fp->fn_path);
		return (result);
	}
	return (NULL);
}

void
sat_copy_fdmap(int pid, int newpid)
{
	struct fdnamemap *fp;

	if (!fdmap)
		return;

	for (fp = fdnamemap; fp; fp = fp->fn_next)
		if (fp->fn_pid == pid)
			sat_set_fdmap_path(newpid, fp->fn_fd, fp->fn_path);
}

/*
 * sat_normalize_pid keeps a list even if normalization is not being done.
 */
int
sat_normalize_pid(int pid)
{
	static int normalizer;
	static struct pids {
		struct pids	*next;
		int		pid;
		int		npid;
	} *pidlist;
	struct pids *pp;

	/*
	 * Look up the real pid in the list and return the normalized pid.
	 */
	for (pp = pidlist; pp; pp = pp->next)
		if (pp->pid == pid)
			return (pp->npid);
	/*
	 * Create a new entry in the normalized pid list.
	 */
	pp = (struct pids *)malloc(sizeof(struct pids));
	pp->pid = pid;

	if (normalize)
		pp->npid = ++normalizer;
	else
		pp->npid = pid;

	pp->next = pidlist;
	pidlist = pp;

	/*
	 * While we're here, add mappings for std{in,out,err}
	 */
	sat_set_fdmap_path(pid, 0, "<stdin>");
	sat_set_fdmap_path(pid, 1, "<stdout>");
	sat_set_fdmap_path(pid, 2, "<stderr>");

	return (pp->npid);
}

/*
 * print user events with proper spacing
 */
void
sat_print_ae( int ae, const char *message )
{
	switch (format) {
	case OM_VERBOSE:
	case OM_BRIEF:
		switch (ae) {
		case SAT_AE_AUDIT:
			printf("Audit adm event  = %s", message);
			break;
		case SAT_AE_IDENTITY:
			printf("Identity event   = %s", message);
			break;
		case SAT_AE_CUSTOM:
			printf("Custom event     = %s", message);
			break;
		case SAT_AE_MOUNT:
			printf("Mount event      = %s", message);
			break;
		case SAT_AE_DBEDIT:
			printf("DB edit event    = %s", message);
			break;
		case SAT_AE_LP:
			printf("LP event         = %s", message);
			break;
		case SAT_AE_X_ALLOWED:
			printf("X access allowed = %s", message);
			break;
		case SAT_AE_X_DENIED:
			printf("X access denied  = %s", message);
			break;
		}
		/*
		 * Width of description is constant (18).
		 */
		if (strlen(message) < 52)
			printf(" %s\n", message);
		else
			printf("\n\t%s\n", message);
		break;
	case OM_LINEAR:
		switch (ae) {
		case SAT_AE_AUDIT:
			printf("ae_audit:\"%s\" ", message);
			break;
		case SAT_AE_IDENTITY:
			printf("ae_identity:\"%s\" ", message);
			break;
		case SAT_AE_CUSTOM:
			printf("ae_custom:\"%s\" ", message);
			break;
		case SAT_AE_MOUNT:
			printf("ae_mount:\"%s\" ", message);
			break;
		case SAT_AE_DBEDIT:
			printf("ae_dbedit:\"%s\" ", message);
			break;
		case SAT_AE_LP:
			printf("ae_lp:\"%s\" ", message);
			break;
		case SAT_AE_X_ALLOWED:
			printf("ae_x_allowed:\"%s\" ", message);
			break;
		case SAT_AE_X_DENIED:
			printf("ae_x_denied:\"%s\" ", message);
			break;
		}
		break;
	}
}

/*
 *  communications domains are translated from binary to ascii
 */
char *
sat_str_domain (short doma)
{
#define	SAT_NUM_DOMA	19
	static struct doma_names {
		char *  name;
	} doma_name [] = {
		"unspecified",			/* 00 */
		"unix same host",		/* 01 */
		"internet",			/* 02 */
		"IMP link",			/* 03 */
		"PUP",				/* 04 */
		"MIT CHAOS",			/* 05 */
		"XEROX NS",			/* 06 */
		"NBS",				/* 07 */
		"ECMA",				/* 08 */
		"ATT datakit",			/* 09 */
		"CCITT wide area",		/* 10 */
		"IBM SNA",			/* 11 */
		"DEC net",			/* 12 */
		"direct link interface",	/* 13 */
		"LAT",				/* 14 */
		"NSC hyperchannel",		/* 15 */
		"Apple Talk",			/* 16 */
		"CS net serial",		/* 17 */
		"raw",				/* 18 */
		"eXpress Transfer Protocol",	/* 19 */
	};

	static  char    doma_name_str[72];

	if ((SAT_NUM_DOMA <= doma) || (0 > doma))
		sprintf (doma_name_str, "unknown");
	else
		strcpy  (doma_name_str, doma_name[doma].name);
	return (doma_name_str);
}

/*
 *  network socket types are translated from binary to ascii
 */
char *
sat_str_socktype (short styp)
{
#define	SAT_NUM_STYP	5
	static struct styp_names {
		char *  name;
	} styp_name [] = {
		"unspecified",
		"stream",
		"datagram",
		"raw",
		"reliably-delivered",
		"sequenced-packet",
	};

	static  char    styp_name_str[72];

	if ((SAT_NUM_STYP <= styp) || (0 > styp))
		sprintf (styp_name_str, "unknown");
	else
		strcpy  (styp_name_str, styp_name[styp].name);
	return (styp_name_str);
}

/*
 * Display mode bits like ls -l does
 */
char *
sat_str_mode ( mode_t mode )
{
	static char modestr[11];

	strcpy( modestr, "----------" );

	if (S_ISDIR(mode))
		modestr[0] = 'd';
	else if (S_ISCHR(mode))
		modestr[0] = 'c';
	else if (S_ISBLK(mode))
		modestr[0] = 'b';
	else if (S_ISREG(mode))
		modestr[0] = '-';
	else if (S_ISFIFO(mode))
		modestr[0] = 'p';
	else if (S_ISLNK(mode))
		modestr[0] = 'l';
	else if (S_ISSOCK(mode))
		modestr[0] = 's';
	else
		modestr[0] = ' ';

	if (mode & S_ISUID)
		modestr[3] = 'S';
	if (mode & S_ISGID)
		modestr[6] = 'l';
	if (mode & S_ISVTX)
		modestr[9] = 'T';

	if (mode & S_IRUSR)
		modestr[1] = 'r';
	if (mode & S_IWUSR)
		modestr[2] = 'w';
	if (mode & S_IXUSR)
		modestr[3] = 'x';
	if (mode & S_IRGRP)
		modestr[4] = 'r';
	if (mode & S_IWGRP)
		modestr[5] = 'w';
	if (mode & S_IXGRP)
		modestr[6] = 'x';
	if (mode & S_IROTH)
		modestr[7] = 'r';
	if (mode & S_IWOTH)
		modestr[8] = 'w';
	if (mode & S_IXOTH)
		modestr[9] = 'x';

	if ((mode & S_ISUID) && (mode & S_IXUSR))
		modestr[3] = 's';
	if ((mode & S_ISGID) && (mode & S_IXGRP))
		modestr[6] = 's';
	if ((mode & S_ISVTX) && (mode & S_IXOTH))
		modestr[9] = 't';

	return (modestr);
}

/*
 * sat_list_ent decoder.  Given an id in the list of entries, return
 * the text field associated with that id.  Since the list is sorted
 * by id, use a binary search.
 */
const char *
sat_str_id(unsigned int id, struct sat_list_ent **list, int n_entries)
{
	static char mystery_id[80];
	static char *lastdata;
	static struct sat_list_ent **lastlist;
	static int lastid;
	int first, last, curr;

	if (id == (unsigned short)-1 || id == (unsigned)-1 || id == -1)
		return "N/A";

	if (id == lastid && list == lastlist)
		return lastdata;

	/* binary search */

	first = 0;			/* index of first entry */
	last = n_entries - 1;		/* index of last entry */

	while (last >= first) {
		curr = first + (last - first)/2;

		if (list[curr]->sat_id == id) {
			lastlist = list;
			lastid = id;
			lastdata = list[curr]->sat_name.data;
			return list[curr]->sat_name.data;
		}

		if (list[curr]->sat_id < id)
			first = curr + 1;
		else if (first == last)
			break;
		else
			last = curr;
	}

	/* it wasn't found */

	sprintf (mystery_id, "No name (#%d)", id);
	return mystery_id;
}

char *
sat_str_umask (mode_t mode)
{
	static char buf[MAX_REC_SIZE];
	sprintf (buf, "%#lo", (unsigned long) mode);
	return (buf);
}

/*
 * Return the open flags in textual form.
 */
char *
sat_str_open_flags (int flags)
{
	static char buf[256];

	flags += FOPEN;		/* reverse the -= FOPEN done in the kernel */

	if ((flags & O_ACCMODE) == O_RDONLY)
		strcpy( buf, "O_RDONLY" );
	if ((flags & O_ACCMODE) == O_WRONLY)
		strcpy( buf, "O_WRONLY" );
	if ((flags & O_ACCMODE) == O_RDWR)
		strcpy( buf, "O_RDWR" );

	if (flags & O_APPEND)
		strcat( buf, " | O_APPEND" );
	if (flags & O_CREAT)
		strcat( buf, " | O_CREAT" );
	if (flags & O_EXCL)
		strcat( buf, " | O_EXCL" );
	if (flags & O_NDELAY)
		strcat( buf, " | O_NDELAY" );
	if (flags & O_NOCTTY)
		strcat( buf, " | O_NOCTTY" );
	if (flags & O_NONBLOCK)
		strcat( buf, " | O_NONBLOCK" );
	if (flags & O_SYNC)
		strcat( buf, " | O_SYNC" );
	if (flags & O_TRUNC)
		strcat( buf, " | O_TRUNC" );

	return buf;
}

char *
sat_str_hex( unsigned char * bytes, int len )
{
	static char hex[] = "0123456789ABCDEF";
	static char buf[50];
	char * bufp;
	int tmp;

	buf[0] = 0;
	if ( len <= 0 )
		return buf;
	len = ((len - 1) & 15) + 1;
	bufp = buf;
	do {
		tmp = *bytes++;
		bufp[0] = hex[ tmp >> 4 ];
		bufp[1] = hex[ tmp & 15 ];
		bufp[2] = ' ';
		bufp += 3;
	} while ( --len );
	*--bufp = 0;
	return buf;
}

char *
sat_str_signame( int signal )
{
	static char *signame[] = {
		"SIGNULL",
		"SIGHUP", "SIGINT", "SIGQUIT", "SIGILL", "SIGTRAP", "SIGABRT",
		"SIGEMT", "SIGFPE", "SIGKILL", "SIGBUS", "SIGSEGV", "SIGSYS",
		"SIGPIPE", "SIGALRM", "SIGTERM", "SIGUSR1", "SIGUSR2",
		"SIGCHLD", "SIGPWR", "SIGWINCH", "SIGURG", "SIGIO", "SIGSTOP",
		"SIGTSTP", "SIGCONT", "SIGTTIN", "SIGTTOU", "SIGVTALRM",
		"SIGPROF", "SIGXCPU", "SIGXFSZ", "SIG32", "SIGCKPT",
	};

	/* contiguous signals */
	if (signal >= 0 && signal < (sizeof (signame) / sizeof (signame[0])))
		return signame[signal];

	/* non-contiguous signals */
	switch (signal) {
		case SIGRTMIN:
			return "SIGRTMIN";
		case SIGRTMAX:
			return "SIGRTMAX";
		case SIGPTINTR:
			return "SIGPTINTR";
		case SIGPTRESCHED:
			return "SIGPTRESCHED";
	}
	return "<Bad signal number>";
}


/*
 * print device info
 */
char *
sat_str_dev( dev_t dev )
{
	static char buf[80];

	if (dev == (o_dev_t)-1 || dev == (dev_t)-1)
		return "NODEV";

	if (file_major == 1)
		sprintf(buf, "%d,%d", ((unsigned)dev>>8) & 0x7f, dev & 0xff);
	else
		sprintf(buf, "%d,%d", major(dev), minor(dev));

	return buf;
}

void
sat_print_acct_timers( struct acct_timers *timers )
{
	switch (format) {
	case OM_VERBOSE:
	case OM_BRIEF:
		printf("Times (ns):\n");
		printf("  User time      = %lld\n", timers->ac_utime);
		printf("  System time    = %lld\n", timers->ac_stime);
		printf("  Block I/O wait = %lld\n", timers->ac_bwtime);
		printf("  Raw I/O wait   = %lld\n", timers->ac_rwtime);
		printf("  Run queue wait = %lld\n", timers->ac_qwtime);
		break;
	case OM_LINEAR:
		printf("user:%lld system:%lld ",
		       timers->ac_utime, timers->ac_stime);
		printf("blocki/o:%lld rawi/o:%lld runqwait:%lld ",
		       timers->ac_bwtime, timers->ac_rwtime,
		       timers->ac_qwtime);
		break;
	}
}

void
sat_print_acct_counts( struct acct_counts *counts )
{
	switch (format) {
	case OM_VERBOSE:
	case OM_BRIEF:
		printf("Counts:\n");
		printf("  Memory usage   = %lld\n", counts->ac_mem);
		printf("  swaps          = %lld\n", counts->ac_swaps);
		printf("  bytes read     = %lld\n", counts->ac_chr);
		printf("  bytes written  = %lld\n", counts->ac_chw);
		printf("  blocks read    = %lld\n", counts->ac_br);
		printf("  blocks written = %lld\n", counts->ac_bw);
		printf("  read syscalls  = %lld\n", counts->ac_syscr);
		printf("  write syscalls = %lld\n", counts->ac_syscw);
		break;
	case OM_LINEAR:
		printf("mem:%lld swaps:%lld chr:%lld chw:%lld ",
		       counts->ac_mem, counts->ac_swaps,
		       counts->ac_chr, counts->ac_chw);
		printf("br:%lld bw:%lld syscr:%lld syscw:%lld ",
		       counts->ac_br, counts->ac_bw,
		       counts->ac_syscr, counts->ac_syscw);
		break;
	}
}

/* keep a tally of the number of times each user caused a record generation  */
void
sat_user_summ (uid_t uid, user_list_t * list_head_p)
{
	user_list_t *	cursor;

	for (cursor = list_head_p->next ;; cursor = cursor->next) {
		if (uid == cursor -> uid) {	/* if uid found here  */
			cursor -> total ++;
			/* remove item from list.  outside of loop    */
			/* it'll go back into list, at the very front */
			cursor -> prev -> next = cursor -> next;
			cursor -> next -> prev = cursor -> prev;
			break;
		}
		if (list_head_p == cursor) {	/* if uid not in list */
			/* malloc new item for list.  outside of loop */
			/* it'll go into list, at the very front      */
			cursor = malloc (sizeof (user_list_t));
			cursor -> uid   = uid;
			cursor -> total = 1;
			break;
		}
	}
	cursor -> next = list_head_p -> next;
	list_head_p -> next -> prev = cursor;
	cursor -> prev = list_head_p;
	list_head_p -> next = cursor;
}

uint64_t record_count[100];
uint64_t record_size[100];
uint64_t token_count[100];
uint64_t token_size[100];

static void
update_record_counts(sat_token_t token)
{
	int cursor;
	int32_t magic;
	sat_token_size_t size;
	int8_t rectype;
	int8_t outcome;
	int8_t sequence;
	int8_t error;

	cursor = sat_fetch_token(token, &magic, sizeof(magic), 0);
	cursor = sat_fetch_token(token, &size, sizeof(size), cursor);
	cursor = sat_fetch_token(token, &rectype, sizeof(rectype), cursor);
	cursor = sat_fetch_token(token, &outcome, sizeof(outcome), cursor);
	cursor = sat_fetch_token(token, &sequence, sizeof(sequence), cursor);
	cursor = sat_fetch_token(token, &error, sizeof(error), cursor);

	record_count[rectype]++;
	record_size[rectype] += size;
}

int
main(int argc, char *argv[])
{
	sat_token_t	token;
	char		*token_text;
	FILE	*in;
	char	*filename;
	char	*timezone = NULL;	/* override audit file timezone */
	int	c;		/* getopt */
	int	record_sum = 0;	/* number of audit records processed */
	int	byte_sum = 0;	/* number of audit record bytes processed */
	int	elapsed_time;	/* time in seconds for writing audit log */
	int	gotuid;		/* uid for audit record has been obtained */
	uid_t	uid;		/* uid from audit record */
	gid_t	gid;		/* gid from audit record */
	int	rate_rcrd;
	int	rate_byte;
	float	mbytes_hr;
	int	days;
	int	hours;
	int	mins;
	int	secs;
	struct	passwd * pw_entry;
	user_list_t	user_list_head;
	user_list_t *	user_curs;

	struct	sat_file_info	finfo;	/* audit file header */

	/* initialize user list head    */
	user_list_head.next  = & user_list_head;
	user_list_head.prev  = & user_list_head;
	user_list_head.uid   = -1;
	user_list_head.total = -1;

	while ((c = getopt(argc, argv, "bdeflnotTuvz:")) != -1) {
		switch (c) {
		case 'b':
			format = OM_BRIEF;
			break;
		case 'd':
			debug = SAT_TRUE;
			setlinebuf(stderr);
			setlinebuf(stdout);
			break;
		case 'e':
			by_event = SAT_TRUE;
			break;
		case 'f':
			fdmap = SAT_TRUE;
			break;
		case 'l':
			format = OM_LINEAR;
			break;
		case 'n':
			normalize = SAT_TRUE;
			break;
		case 'o':
			echo_input = SAT_TRUE;
			break;
		case 't':
			by_time = SAT_TRUE;
			break;
		case 'T':
			titles = SAT_FALSE;
			break;
		case 'u':
			by_user = SAT_TRUE;
			break;
		case 'v':
			format = OM_VERBOSE;
			break;
		case 'z':
			timezone = optarg;
			break;
		default:
			fprintf(stderr,
			    "usage: %s [-bdeflnotTuv] [-z timezone] [filename]\n",
			    argv[0]);
			exit(1);
		}
	}
	if (( by_event == SAT_FALSE)
		&& (by_user == SAT_FALSE)
		&& (by_time == SAT_FALSE))
		{
		by_event = SAT_TRUE;
	}

	/* check for an optional input file in the command line */
	if (optind < argc) {
		filename = argv[optind];
		in = fopen( filename, "r" );
		if (in == NULL) {
			fprintf (stderr, "Cannot open %s for input: ",
			    filename);
			perror(NULL);
			exit(1);
		}
	} else {
		in = stdin;
		filename = "<stdin>";
	}

	/* clear users and groups lists, if necessary */

	if (users) {
		for (c = 0; users[c]; c++)
			free(users[c]);
		free(users);
	}
	if (groups) {
		for (c = 0; groups[c]; c++)
			free(groups[c]);
		free(groups);
	}

	/*
	 * Read file header
	 */
	if (echo_input == SAT_TRUE) {
		if (sat_read_file_info(in, stdout, &finfo, SFI_ALL)
			== SFI_ERROR) {
			fprintf (stderr, "Bad header in %s: ", filename);
			perror(NULL);
			exit(1);
		}
	} else {
		if (sat_read_file_info(in, NULL, &finfo, SFI_ALL) == SFI_ERROR) {
			fprintf (stderr, "Bad header in %s: ", filename);
			perror(NULL);
			exit(1);
		}
	}

	n_users = finfo.sat_user_entries;
	users = finfo.sat_users;

	n_groups = finfo.sat_group_entries;
	groups = finfo.sat_groups;

	n_hosts = finfo.sat_host_entries;
	hosts = finfo.sat_hosts;

	file_major = finfo.sat_major;
	file_minor = finfo.sat_minor;
	mac_enabled = finfo.sat_mac_enabled;
	cap_enabled = finfo.sat_cap_enabled;
	cipso_enabled = finfo.sat_cipso_enabled;

	if (debug) {
		printf("debug: file header:\n");
		printf("  sat_major = %d\n", finfo.sat_major);
		printf("  sat_minor = %d\n", finfo.sat_minor);
		printf("  sat_start_time = %d\n", finfo.sat_start_time);
		printf("  sat_stop_time = %d\n", finfo.sat_stop_time);
		printf("  sat_host_id = %#x\n", finfo.sat_host_id);
		printf("  sat_mac_enabled = %d\n", finfo.sat_mac_enabled);
		printf("  sat_cap_enabled = %d\n", finfo.sat_cap_enabled);
		printf("  sat_cipso_enabled = %d\n", finfo.sat_cipso_enabled);
		printf("  sat_user_entries = %d\n", finfo.sat_user_entries);
		printf("  sat_group_entries = %d\n", finfo.sat_group_entries);
		printf("  sat_host_entries = %d\n", finfo.sat_host_entries);
	}

	if (file_major > SAT_VERSION_MAJOR ||
	    (file_major == SAT_VERSION_MAJOR &&
	     file_minor > SAT_VERSION_MINOR)) {
		fprintf(stderr,
"Error: this file is version %d.%d; this version of sat_interpret can only\n\
interpret file versions up to and including %d.%d\n",
			file_major, file_minor,
			SAT_VERSION_MAJOR, SAT_VERSION_MINOR);
		exit(1);
	} else if (file_major < SAT_VERSION_MAJOR) {
		if (execv("/usr/bin/sat_summarize31",&argv[0]) != 0) {
			perror("sat_summarize failed to exec sat_summarize31");
			exit(1);
		}
	}

	/* set our timezone to that of the file */
	/* don't free finfo.sat_timezone, putenv keeps it! */
	if (timezone) {
		char *tz;
		tz = malloc(strlen(timezone) + 4);
		strcpy(tz, "TZ=");
		strcat(tz, timezone);
		putenv(tz);
	} else {
		putenv(finfo.sat_timezone);
	}
	tzset();

	token_text = malloc(128);
	printf("File version %d.%d\n", finfo.sat_major, finfo.sat_minor);
	cftime(token_text, "%a %b %e %T %Z %Y", &finfo.sat_start_time);
	printf("Created %s\n", token_text);
	if (finfo.sat_stop_time != 0) {
		cftime(token_text, "%a %b %e %T %Z %Y", &finfo.sat_stop_time);
		printf("Closed  %s\n", token_text);
	} else
		printf("Closed  <unknown>\n");
	free(token_text);

	if (n_hosts > 1) {
		struct in_addr addr;
		printf("Merged from the following hosts:\n");
		for (c = 0; c < n_hosts; c++) {
			addr.s_addr = hosts[c]->sat_id;
			printf("    %s, hostid %s\n", hosts[c]->sat_name.data,
			       inet_ntoa(addr));
		}
		putchar('\n');
	} else {
		struct in_addr addr;
		addr.s_addr = finfo.sat_host_id;
		printf("Written on host %s", finfo.sat_hostname);
		if (*finfo.sat_domainname)
			printf(".%s", finfo.sat_domainname);
		free(finfo.sat_hostname);
		free(finfo.sat_domainname);
		printf(", hostid %s\n\n", inet_ntoa(addr));
	}

	/*
	 * Read all of the tokens.
	 */
	while (token = sat_fread_token(in)) {

		token_count[token->token_header.sat_token_id]++;
		token_size[token->token_header.sat_token_id] +=
		    token->token_header.sat_token_size;

		switch (token->token_header.sat_token_id) {
		case SAT_RECORD_HEADER_TOKEN:
			update_record_counts(token);
			gotuid = 0;
			break;
		case SAT_UGID_TOKEN:
			if (by_user == SAT_TRUE && gotuid == 0) {
				sat_fetch_ugid_token(token, &uid, &gid);
				sat_user_summ (uid, &user_list_head);
				gotuid = 1;
			}
			break;
		default:
			break;
		}
	}

	if (by_event == SAT_TRUE) {
	    printf("\nRecords:\n");
	    for (c = 0; c < 100; c++) {
		if (record_count[c] > 0) {
		     printf("%-30s\t%ld\t%ld\n", sat_eventtostr(c),
				(int)record_count[c], (int)record_size[c]);
		}
	    }

	    printf("\nTokens:\n");
	    for (c = 0; c < 100; c++) {
		if (token_count[c] > 0) {
		    printf("%-30s\t%ld\t%ld\n",
			    sat_token_name(c), (int)token_count[c], 
			    (int)token_size[c]);
		}
	    }
	}
	if (by_time == SAT_TRUE) {
		for (c = 0; c < 100; c++) {
			if (record_count[c] > 0) {
				record_sum += record_count[c];
				byte_sum += record_size[c];
			}
		}
		elapsed_time = finfo.sat_stop_time - finfo.sat_start_time;
		if (elapsed_time == 0)
			elapsed_time = 1;	/* prevent zero divide */

		/* divide first to prevent possible overflow */

		rate_rcrd = ONE_MINUTE * (record_sum / elapsed_time);
		rate_byte = ONE_MINUTE * (byte_sum / elapsed_time);
		secs = elapsed_time;
		days = secs / (60*60*24);
		secs %= (60*60*24);
		hours = secs / (60*60);
		secs %= (60*60);
		mins = secs / 60;
		secs %= 60;

		printf("\nRecords: %7d total, %6d per minute\n",
			record_sum, rate_rcrd);
		printf("Bytes:   %7d total, %6d per minute\n",
			byte_sum, rate_byte);
		if (days > 0)
			printf(" %d day%s", days, days>1?"s":"");
		if (hours > 0)
			printf(" %2d:%02d", hours, mins);
		else if (mins > 0)
			printf(" %d minute%s", mins, mins>1?"s":"");
		else
			printf("%d second%s", secs, secs>1?"s":"");
		mbytes_hr = rate_byte *( 60.0 / 1024.0*1024.0);
		printf (" (%.1f megabytes per hour)\n\n", mbytes_hr);
	}
	if (by_user == SAT_TRUE) {
		printf("\nBy user:\n");
		for (user_curs = user_list_head.next;
				-1 != user_curs->total;
				user_curs =  user_curs -> next) {
			if ((0 == user_curs->total) && (format != OM_VERBOSE))
				continue;
			if (pw_entry = getpwuid ((uid_t) user_curs->uid))
				printf ("%s\t%d\n", pw_entry->pw_name,
					user_curs->total);
			else
				printf ("User #%d\t%d\n",
					user_curs->uid, user_curs->total);
		}
	}
	exit(0);
	/* NOTREACHED */
}
